# ClimateZoneManager - Fejlesztési Terv

## Áttekintés

A `UClimateZoneManager` felelős a térkép klímazónáinak meghatározásáért a bióm térkép alapján. A klímazónák befolyásolják az időjárási mintákat, az elérhető erőforrásokat és a törzsek életfeltételeit.

## Célkitűzések

- Klímazónák kiszámítása bióm és hőmérséklet térkép alapján
- Alap hőmérsékleti és csapadéki tartományok definiálása zónánként
- Évszak hatásmodell implementálása
- Runtime lekérdezési interfész biztosítása
- Optimalizált teljesítmény

## Függőségek

### Előfeltételek
- **MapGeneratorSettings** modul implementálva
- **BiomeManager** modul implementálva
- `EClimateZone` enum definiálva

### Külső függőségek
- UE CoreUObject, Engine modulok

## MVP Fejlesztési Struktúra

## MVP 1: UClimateZoneManager Váz és Klímazóna Meghatározási Logika

### Cél
UClimateZoneManager komponens vázának létrehozása és DetermineClimateZone() algoritmus implementálása hőmérséklet alapján.

### Implementációs Lépések

1. **UClimateZoneManager header (ClimateZoneManager.h)**
   - ActorComponent alapú osztály
   - ClimateZoneMap TArray
   - Publikus interfész: CalculateClimateZones(), GetClimateZoneAt()

2. **DetermineClimateZone() algoritmus**
```cpp
EClimateZone UClimateZoneManager::DetermineClimateZone(const FBiomeCell& Cell) const
{
    float Temperature = Cell.Temperature;
    if (Cell.Height > 0.7f) Temperature -= 0.2f;

    if (Temperature > 0.75f && Cell.Moisture > 0.5f) return EClimateZone::Tropical;
    else if (Temperature > 0.4f)                     return EClimateZone::Temperate;
    else if (Temperature > 0.2f)                     return EClimateZone::Continental;
    else                                             return EClimateZone::Subarctic;
}
```

### Automatizált Tesztek (MVP 1)

**Test_ClimateZoneManager_Structure.cpp** és **Test_ClimateZone_Logic.cpp:**
- High temp + High moisture → Tropical
- Medium temp → Temperate
- Low temp → Continental/Subarctic
- Magasság korrekció működik

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `BP_TestClimateZoneManager` Actor Blueprint létrehozása UClimateZoneManager komponenssel, spawolása a szintbe
2. **[AI]** `RunTests.bat` futtatása – `Priordium.MapGenerator.ClimateZoneManager.*` tesztek zöldek legyenek
3. **[Manuális]** PIE-ben futtasd a `MapGen.TestClimateZone` console command-ot, ellenőrizd a különböző hőmérsékletű cellákra kapott klímazóna értékeket

### Elfogadási Kritériumok (MVP 1)

- [ ] UClimateZoneManager osztály fordul
- [ ] DetermineClimateZone() helyesen működik
- [ ] Hőmérséklet alapú logika helyes
- [ ] Magasság korrekció működik
- [ ] Automatizált tesztek zöldek
- [ ] Blueprint-ben tesztelhető

---

## MVP 2: CalculateClimateZones() Implementálás

### Cél
CalculateClimateZones() metódus implementálása BiomeMap alapján, ClimateZoneMap feltöltése.

### Implementációs Lépések

1. **CalculateClimateZones() implementálás**
```cpp
bool UClimateZoneManager::CalculateClimateZones(const TArray<FBiomeCell>& BiomeMap, UMapGeneratorSettings* Settings)
{
    if (BiomeMap.Num() == 0) return false;
    Resolution.X = Settings->LandscapeResolution;
    Resolution.Y = Settings->LandscapeResolution;
    ClimateZoneMap.SetNum(BiomeMap.Num());
    for (int32 i = 0; i < BiomeMap.Num(); ++i)
        ClimateZoneMap[i] = DetermineClimateZone(BiomeMap[i]);
    return true;
}
```

### Automatizált Tesztek (MVP 2)

**Test_CalculateClimateZones.cpp:**
- ClimateZoneMap.Num() == BiomeMap.Num()
- Realisztikus klímazóna eloszlás

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `RunTests.bat` futtatása – climate zone calculation tesztek zöldek legyenek
2. **[Manuális]** PIE-ben futtasd a `MapGen.ShowClimateZones` console command-ot
3. **[Manuális]** Ellenőrizd vizuálisan: Tropical=piros, Temperate=zöld, Continental=kék, Subarctic=fehér
4. **[Manuális]** Details Panelen ellenőrizd a ClimateZoneMap array-t és a zónák eloszlását

### Elfogadási Kritériumok (MVP 2)

- [ ] CalculateClimateZones() működik
- [ ] ClimateZoneMap teljes
- [ ] Realisztikus klímazóna eloszlás
- [ ] Automatizált tesztek zöldek
- [ ] Vizuálisan ellenőrizhető

---

## MVP 3: Évszakmodell és GetTemperatureAt()

### Cél
FSeasonProfile struktúra és GetTemperatureAt() implementálása TimeOfYear alapján.

### Implementációs Lépések

1. **FSeasonProfile struktúra**
```cpp
USTRUCT(BlueprintType)
struct FSeasonProfile
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climate")
    float SummerTempModifier = 0.1f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climate")
    float WinterTempModifier = -0.15f;
};
```

2. **CalculateSeasonalTemperature()** és **GetTemperatureAt()** implementálás

### Automatizált Tesztek (MVP 3)

**Test_SeasonProfile.cpp** és **Test_GetTemperatureAt.cpp:**
- Winter (TimeOfYear=0.0) → BaseTemp + WinterTempModifier
- Summer (TimeOfYear=0.5) → BaseTemp
- Smooth átmenet évszakok között

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `RunTests.bat` futtatása – seasonal temperature tesztek zöldek legyenek
2. **[Manuális]** PIE-ben futtasd a `MapGen.TestSeasons` console command-ot
3. **[Manuális]** Output Log-ban ellenőrizd a hőmérséklet értékeket évszakonként
4. **[Manuális]** Hasonlítsd össze: TimeOfYear=0.0 (tél) vs 0.5 (nyár) heatmap

### Elfogadási Kritériumok (MVP 3)

- [ ] FSeasonProfile struktúra fordul
- [ ] CalculateSeasonalTemperature() helyes hőmérsékletet számol
- [ ] GetTemperatureAt() működik
- [ ] Évszakok közötti átmenet smooth
- [ ] Automatizált tesztek zöldek

---

## MVP 4: GetClimateZoneAt() és Debug Vizualizáció

### Cél
GetClimateZoneAt() getter és klímazóna debug vizualizáció.

### Implementációs Lépések

1. **GetClimateZoneAt()** bounds checking-gel
2. **Debug vizualizáció** – Tropical=piros, Temperate=zöld, Continental=kék, Subarctic=fehér
3. **Console command-ok:** `MapGen.ShowClimateZones`, `MapGen.ShowTemperatureGradient`

### Automatizált Tesztek (MVP 4)

**Test_GetClimateZoneAt.cpp:**
- Helyes klímazóna visszaadása
- Bounds ellenőrzés (invalid index → default)

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `RunTests.bat` futtatása – teljes climate zone pipeline tesztek zöldek legyenek
2. **[Manuális]** PIE-ben futtasd a `MapGen.ShowClimateZones` console command-ot
3. **[Manuális]** Ellenőrizd vizuálisan: klímazóna eloszlás realisztikus, észak hidegebb (Subarctic/Continental), dél melegebb (Tropical/Temperate)

### Elfogadási Kritériumok (MVP 4)

- [ ] GetClimateZoneAt() működik
- [ ] Bounds checking helyes
- [ ] Debug vizualizáció elérhető
- [ ] Automatizált tesztek zöldek
- [ ] Realisztikus klímazóna eloszlás

---

## Átfogó Tesztelési Követelmények

### Code Coverage Követelmények

**KÖTELEZŐ:** A tesztek code coverage-ének **90% feletti** kell lennie.

### Use Case Lefedettség

| Use Case ID       | Use Case Neve                            | Implementáló Metódus          | Teszt Osztály                    |
|-------------------|------------------------------------------|-------------------------------|----------------------------------|
| UC-MG-02.5        | Klímazónák meghatározása                 | CalculateClimateZones()       | Test_CalculateClimateZones       |
| UC-MG-02.5-A      | Klímazóna típus meghatározása            | DetermineClimateZone()        | Test_ClimateZone_Logic           |
| UC-MG-02.5-B      | Évszak alapú hőmérséklet számítás        | CalculateSeasonalTemperature()| Test_SeasonalTemperature         |
| UC-MG-02.5-C      | Hőmérséklet lekérdezése pozíció alapján  | GetTemperatureAt()            | Test_GetTemperatureAt            |

### Code Coverage Céleloszlás

| Komponens/Modul                  | Minimum Coverage | Cél Coverage | Prioritás |
|----------------------------------|------------------|--------------|-----------|
| CalculateClimateZones()          | 95%              | 100%         | Kritikus  |
| DetermineClimateZone()           | 100%             | 100%         | Kritikus  |
| CalculateSeasonalTemperature()   | 100%             | 100%         | Magas     |
| GetTemperatureAt()               | 100%             | 100%         | Magas     |
| GetClimateZoneAt()               | 100%             | 100%         | Közepes   |
| **Teljes modul**                 | **90%**          | **96%**      | Kritikus  |

## Kockázatok és Kockázatcsökkentés

| Kockázat                              | Valószínűség | Hatás   | Csökkentési Stratégia                              |
|---------------------------------------|--------------|---------|----------------------------------------------------|
| Évszak számítás túl komplex           | Alacsony     | Közepes | Egyszerűsített modell, lineáris interpoláció       |
| BiomeMap adatok hiánya                | Alacsony     | Alacsony | Alapértelmezett értékek használata                |
| Teljesítmény probléma runtime lekérd. | Alacsony     | Alacsony | Cache használata, grid-based lookup               |

## Elfogadási Kritériumok (Teljes Modul)

- [ ] **MVP 1 teljesítve:** DetermineClimateZone() működik
- [ ] **MVP 2 teljesítve:** CalculateClimateZones() ClimateZoneMap-et generál
- [ ] **MVP 3 teljesítve:** Évszakmodell és GetTemperatureAt() működik
- [ ] **MVP 4 teljesítve:** GetClimateZoneAt() és debug vizualizáció kész
- [ ] Összes automatizált teszt zöld

## Következő Lépések

1. **ResourceDistributor** - Klímazóna alapú erőforrás szűrés
2. **WorldGenerator** - ClimateZoneManager integráció a pipeline-ba
3. **Weather System** (jövőbeli) - Klímazóna alapú időjárás generálás
