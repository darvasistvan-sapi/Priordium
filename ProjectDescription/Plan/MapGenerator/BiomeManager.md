# BiomeManager - Fejlesztési Terv

## Áttekintés

A `UBiomeManager` a heightmap alapján minden cellához egy bióm típust rendel. A bióm meghatározza a terep vizuális megjelenését, növényzetét és elérhető erőforrásait. Hőmérséklet és csapadék térképet is generál a realisztikus bióm elosztáshoz.

## Célkitűzések

- Realisztikus bióm térkép generálása magasság, hőmérséklet és csapadék alapján
- Hőmérséklet térkép létrehozása földrajzi helyzet és magasság alapján
- Csapadék térkép létrehozása környezeti faktorok alapján
- Biómok közötti lágy átmenetek biztosítása blend weight-ekkel
- Optimális teljesítmény biztosítása nagy térképeken

## Függőségek

### Előfeltételek
- **MapGeneratorSettings** modul implementálva
- **HeightmapGenerator** modul implementálva
- `EBiomeType` enum definiálva
- `FBiomeCell` struktúra definiálva

### Külső függőségek
- FastNoise/SimplexNoise library (hőmérséklet és csapadék térképhez)
- UE CoreUObject, Engine modulok

## MVP Fejlesztési Struktúra

## MVP 1: FBiomeCell Struktúra és UBiomeManager Váz

### Cél
FBiomeCell alapstruktúra létrehozása BiomeBlendWeights TMap-pel, valamint UBiomeManager komponens váz implementálása getter-ekkel.

### Implementációs Lépések

1. **FBiomeCell struktúra definiálása (MapGeneratorTypes.h)**
```cpp
USTRUCT(BlueprintType)
struct FBiomeCell
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    EBiomeType BiomeType = EBiomeType::Plains;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    float Height = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    float Temperature = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    float Moisture = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    TMap<EBiomeType, float> BiomeBlendWeights;
};
```

2. **UBiomeManager header (BiomeManager.h)**
   - ActorComponent alapú osztály
   - BiomeMap, TemperatureMap, MoistureMap TArray-ek
   - Publikus getter interfész

3. **Alapvető getter-ek implementálása**
   - `GetBiomeAt(int32 X, int32 Y)`
   - `GetBiomeTypeAt(int32 X, int32 Y)`
   - `GetBiomeMap()`
   - Bounds checking

### Automatizált Tesztek (MVP 1)

**Test_BiomeCell.cpp:**
- FBiomeCell létrehozása, alapértelmezett értékek
- TMap BiomeBlendWeights műveletek (Add, FindOrAdd)
- Szerializáció teszt

**Test_BiomeManager_Structure.cpp:**
- UBiomeManager komponens létrehozása
- BiomeMap inicializálás SetNum-mal
- Getterek bounds checking működése
- Invalid index kezelés

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `BP_TestBiomeManager` Actor Blueprint létrehozása UBiomeManager komponenssel, spawolása a szintbe
2. **[AI]** `RunTests.bat` futtatása – `Priordium.MapGenerator.BiomeManager.*` tesztek zöldek legyenek
3. **[Manuális]** PIE-ben futtasd a `MapGen.TestBiomeManager` console command-ot
4. **[Manuális]** Output Log-ban ellenőrizd a FBiomeCell értékeket különböző koordinátákon

### Elfogadási Kritériumok (MVP 1)

- [ ] FBiomeCell struktúra fordul
- [ ] TMap<EBiomeType, float> működik
- [ ] UBiomeManager komponens példányosítható
- [ ] Getter-ek helyes értéket adnak vissza
- [ ] Bounds checking működik
- [ ] Automatizált tesztek zöldek
- [ ] Blueprint-ben hozzáadható a komponens

---

## MVP 2: Hőmérséklet Térkép Generálás

### Cél
GenerateTemperatureMap() metódus implementálása szélességi fok és magasság alapú számítással.

### Implementációs Lépések

1. **GenerateTemperatureMap() implementálás**
   - Szélességi fok hatás: `BaseTemp = 1.0f - (Y / Resolution.Y)`
   - Magassági korrekció: `HeightPenalty = Height * 0.5f`
   - Noise variáció: SimplexNoise2D 0.1x amplitúdóval
   - Normalizálás [0, 1] tartományra

2. **AssignBiomes() metódus kezdete**
   - Resolution beállítás Settings-ből
   - TemperatureMap inicializálás
   - GenerateTemperatureMap() hívása

3. **GetTemperatureAt() getter**
   - X, Y koordinátákra hőmérséklet lekérdezés

### Automatizált Tesztek (MVP 2)

**Test_TemperatureMap.cpp:**
- Északi területek (Y=0) hidegebbek (Temperature < 0.5)
- Déli területek (Y=max) melegebbek (Temperature > 0.5)
- Magasabb pontok hidegebbek
- Minden érték [0, 1] tartományban
- Determinizmus teszt: Ugyanaz a seed → ugyanaz a térkép

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `BP_TestBiomeManager` Blueprint bővítése UHeightmapGenerator komponenssel, BeginPlay → Print String "BiomesAssigned" node-dal
2. **[AI]** `RunTests.bat` futtatása – temperature assignment tesztek zöldek legyenek
3. **[Manuális]** PIE-ben futtasd a `MapGen.ShowTemperatureMap` console command-ot
4. **[Manuális]** Ellenőrizd vizuálisan: északi terület kék, déli terület piros

### Elfogadási Kritériumok (MVP 2)

- [ ] TemperatureMap generálódik
- [ ] Szélességi fok hatása helyes
- [ ] Magasság korrekció működik
- [ ] Noise variáció látható
- [ ] Értékek [0, 1] tartományban
- [ ] Determinizmus biztosított
- [ ] Automatizált tesztek zöldek
- [ ] Vizuálisan ellenőrizhető

---

## MVP 3: Csapadék Térkép Generálás

### Cél
GenerateMoistureMap() implementálása Perlin noise és tengerközelség hatással.

### Implementációs Lépések

1. **GenerateMoistureMap() implementálás**
   - Alap csapadék: SimplexNoise2D (scale 0.01)
   - Tengerközelség: Távolság térkép széltől
   - OceanEffect: Max 0.3 extra csapadék partvidéken
   - Normalizálás [0, 1] tartományra

2. **AssignBiomes() bővítése**
   - MoistureMap inicializálás
   - GenerateMoistureMap() hívása

3. **GetMoistureAt() getter**

### Automatizált Tesztek (MVP 3)

**Test_MoistureMap.cpp:**
- Minden érték [0, 1] tartományban
- Tengerpart közelében magasabb csapadék
- Noise-based változatosság látható
- Determinizmus teszt (seed alapú)

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `RunTests.bat` futtatása – moisture calculation tesztek zöldek legyenek
2. **[Manuális]** PIE-ben futtasd a `MapGen.ShowMoistureMap` console command-ot
3. **[Manuális]** Ellenőrizd vizuálisan: csapadék térkép zöld színskálán, partvidék nedvesebb (zöldebb)
4. **[Manuális]** Hasonlítsd össze a Temperature és Moisture térképeket

### Elfogadási Kritériumok (MVP 3)

- [ ] MoistureMap generálódik
- [ ] Tengerközelség hatása látható
- [ ] Noise-based változatosság
- [ ] Értékek [0, 1] tartományban
- [ ] Determinizmus biztosított
- [ ] Automatizált tesztek zöldek
- [ ] Vizuálisan ellenőrizhető

---

## MVP 4: Bióm Meghatározási Logika

### Cél
DetermineBiomeType() és CalculateBiomes() implementálása Height, Temperature, Moisture alapján.

### Implementációs Lépések

1. **DetermineBiomeType() döntési fa**
   - Height < SeaLevel → Ocean
   - Height < SeaLevel + 0.05 → Beach
   - Height > 0.7 → Mountain
   - Height > 0.5 → Hills
   - Height < 0.35 && Moisture > 0.7 → Swamp
   - Moisture > 0.6 → Forest
   - Alapértelmezett → Plains

2. **CalculateBiomes() implementálás**
   - BiomeMap inicializálás
   - Ciklusban minden cellára DetermineBiomeType() hívása
   - FBiomeCell feltöltése (BiomeType, Height, Temperature, Moisture)

3. **AssignBiomes() finalizálás**
   - CalculateBiomes() hívása a pipeline végén
   - Return true

### Automatizált Tesztek (MVP 4)

**Test_DetermineBiomeType.cpp:**
- Ismert Height/Temp/Moisture → várt BiomeType
- SeaLevel alatt → Ocean
- High Moisture → Forest vagy Swamp
- Mountain logika működik

**Test_BiomeMap_Generation.cpp:**
- AssignBiomes() végrehajt minden lépést
- BiomeMap minden cella kitöltve
- Realisztikus bióm eloszlás (van Ocean, Beach, Mountain, stb.)

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `RunTests.bat` futtatása – biome assignment tesztek zöldek legyenek
2. **[Manuális]** PIE-ben futtasd a `MapGen.ShowBiomeMap` console command-ot
3. **[Manuális]** Ellenőrizd vizuálisan: Ocean=Kék, Beach=Sárga, Mountain=Fehér/Szürke, Forest=Sötétzöld, Plains=Zöld, Swamp=Barna, Hills=Világoszöld

### Elfogadási Kritériumok (MVP 4)

- [ ] DetermineBiomeType() helyes döntéseket hoz
- [ ] CalculateBiomes() minden cellát feldolgoz
- [ ] BiomeMap kitöltve
- [ ] Bióm eloszlás realisztikus
- [ ] Automatizált tesztek zöldek
- [ ] Vizuálisan ellenőrizhető bióm térkép

---

## MVP 5: Blend Weight Számítás

### Cél
CalculateBlendWeights() implementálása szomszédos biómok közötti smooth átmenetekhez.

### Implementációs Lépések

1. **CalculateBlendWeights() algoritmus**
   - BlendRadius = 3 beállítása
   - Minden cellára szomszédos cellák vizsgálata
   - Távolság alapú súly: `Weight = 1.0 - (Distance / BlendRadius)`
   - TMap<EBiomeType, float> feltöltése
   - Normalizálás: súlyok összege = 1.0

2. **AssignBiomes() kiegészítése**
   - CalculateBlendWeights() hívása a végén

3. **GetBlendWeightsAt() getter (opcionális)**

### Automatizált Tesztek (MVP 5)

**Test_BlendWeights.cpp:**
- Minden cellának van BiomeBlendWeights-e
- Súlyok összege ~1.0 (tolerancia 0.01)
- Homogén területen (csak 1 bióm) blend weight = 1.0
- Határon blend weight-ek több bióm között oszlanak meg

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `RunTests.bat` futtatása – blend weight tesztek zöldek legyenek
2. **[Manuális]** PIE-ben futtasd a `MapGen.ShowBlendWeights X Y` console command-ot
3. **[Manuális]** Ellenőrizd: bióm határok mentén blend weight arány helyes, Boundary cellák értékei a Details Panelen

### Elfogadási Kritériumok (MVP 5)

- [ ] CalculateBlendWeights() működik
- [ ] Súlyok összege ~1.0
- [ ] Határok mentén smooth átmenetek
- [ ] Automatizált tesztek zöldek
- [ ] Vizuálisan ellenőrizhető blend weights

---

## Átfogó Tesztelési Követelmények

### Code Coverage Követelmények

**KÖTELEZŐ:** A tesztek code coverage-ének **90% feletti** kell lennie.

### Use Case Lefedettség

| Use Case ID       | Use Case Neve                            | Implementáló Metódus          | Teszt Osztály                    |
|-------------------|------------------------------------------|-------------------------------|----------------------------------|
| UC-MG-02.2        | Biómok kiosztása                         | AssignBiomes()                | Test_BiomeMap_Generation         |
| UC-MG-02.2-A      | Hőmérséklet térkép generálása            | GenerateTemperatureMap()      | Test_TemperatureMap              |
| UC-MG-02.2-B      | Csapadék térkép generálása               | GenerateMoistureMap()         | Test_MoistureMap                 |
| UC-MG-02.2-C      | Bióm típus meghatározása                 | DetermineBiomeType()          | Test_DetermineBiomeType          |
| UC-MG-02.2-D      | Bióm blend súlyok számítása              | CalculateBlendWeights()       | Test_BlendWeights                |

### Code Coverage Céleloszlás Komponensenként

| Komponens/Modul                  | Minimum Coverage | Cél Coverage | Prioritás |
|----------------------------------|------------------|--------------|-----------|
| AssignBiomes()                   | 95%              | 100%         | Kritikus  |
| GenerateTemperatureMap()         | 100%             | 100%         | Kritikus  |
| GenerateMoistureMap()            | 100%             | 100%         | Kritikus  |
| DetermineBiomeType()             | 100%             | 100%         | Kritikus  |
| CalculateBiomes()                | 100%             | 100%         | Kritikus  |
| CalculateBlendWeights()          | 95%              | 100%         | Magas     |
| Getter metódusok                 | 100%             | 100%         | Közepes   |
| **Teljes modul**                 | **90%**          | **96%**      | Kritikus  |

## Kockázatok és Kockázatcsökkentés

| Kockázat                              | Valószínűség | Hatás   | Csökkentési Stratégia                                        |
|---------------------------------------|--------------|---------|--------------------------------------------------------------|
| Bióm határok túl élesek               | Közepes      | Közepes | Blend weight sugár növelése, smooth interpoláció             |
| Teljesítmény probléma blend számításnál | Közepes    | Közepes | Optimalizálás parallel_for-ral                              |
| Irrealisztikus bióm eloszlás          | Közepes      | Közepes | Küszöbértékek iteratív finomhangolása                        |
| TMap overhead BiomeBlendWeights-ben   | Alacsony     | Alacsony | Ha szükséges, fix méretű array használata                   |

## Elfogadási Kritériumok (Teljes Modul)

- [ ] **MVP 1 teljesítve:** FBiomeCell struktúra és UBiomeManager váz kész
- [ ] **MVP 2 teljesítve:** Hőmérséklet térkép generálás működik
- [ ] **MVP 3 teljesítve:** Csapadék térkép generálás működik
- [ ] **MVP 4 teljesítve:** Bióm meghatározási logika helyesen működik
- [ ] **MVP 5 teljesítve:** Blend weight számítás smooth átmeneteket eredményez
- [ ] AssignBiomes() metódus minden pipeline lépést lefuttat
- [ ] Getterek helyesen adják vissza az adatokat
- [ ] Realisztikus bióm eloszlás a térképen
- [ ] Összes automatizált teszt zöld

## Következő Lépések

A BiomeManager implementálása után a következő modulok fejleszthetők:
1. **ClimateZoneManager** - Használja a BiomeMap hőmérséklet adatait
2. **WaterSystemBuilder** - Befolyásolja a BiomeMap-et (River, Lake biómok)
3. **LandscapeBuilder** - Használja a BiomeMap-et a material rétegekhez
4. **ResourceDistributor** - Bióm alapú erőforrás elhelyezés
