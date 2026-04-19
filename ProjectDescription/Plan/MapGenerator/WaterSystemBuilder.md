# WaterSystemBuilder - Fejlesztési Terv

## Áttekintés

A `UWaterSystemBuilder` felelős a folyók, tavak és tenger elhelyezéséért az Unreal Engine Water System plugin használatával. A heightmap gradiens alapján számítja ki a vízfolyások természetes útvonalát és Water Body actorokat hoz létre.

## Célkitűzések

- Realisztikus folyóhálózat generálása heightmap gradiens alapján
- Tavak elhelyezése természetes mélyedésekben
- Tenger/óceán generálása a térkép szélein
- Water Body actorok megfelelő konfigurálása (szélesség, mélység, spline)
- Bióm térkép frissítése vízközelségi információkkal
- Optimalizált teljesítmény és memória használat

## Függőségek

### Előfeltételek
- **MapGeneratorSettings** modul implementálva
- **HeightmapGenerator** modul implementálva
- **BiomeManager** modul implementálva (opcionális)
- `FWaterBodyDefinition` struktúra definiálva
- Water plugin engedélyezve a projektben

### Külső függőségek
- UE Water plugin (`Water` modul a Build.cs-ben)
- `AWaterBodyRiver`, `AWaterBodyLake`, `AWaterBodyOcean` osztályok

## MVP Fejlesztési Struktúra

## MVP 1: Water Plugin Setup és FWaterBodyDefinition Struktúra

### Cél
Water plugin engedélyezése, Build.cs konfiguráció, FWaterBodyDefinition struktúra.

### Implementációs Lépések

1. **Water plugin engedélyezése** – .uproject Plugins lista, Build.cs `"Water"` modul
2. **FWaterBodyDefinition struktúra** – EWaterBodyType (River/Lake/Ocean), SplinePoints, Width, Depth

### Automatizált Tesztek (MVP 1)

**Test_WaterPlugin.cpp** és **Test_WaterBodyDefinition.cpp:** Water modul elérhető, AWaterBody osztályok létrehozhatók, FWaterBodyDefinition fordul

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `BP_TestWaterDef` Blueprint létrehozása `FWaterBodyDefinition` típusú publikus változóval
2. **[AI]** `RunTests.bat` futtatása – `Priordium.MapGenerator.WaterSystemBuilder.*` tesztek zöldek legyenek
3. **[Manuális]** Ellenőrizd a .uproject fájlban, hogy a Water plugin engedélyezve van; ha nincs, engedélyezd
4. **[Manuális]** PIE-ben teszteld: manuális WaterBodyRiver létrehozás, spline szerkesztés, víz megjelenik

### Elfogadási Kritériumok (MVP 1)

- [ ] Water plugin engedélyezve
- [ ] FWaterBodyDefinition struktúra fordul
- [ ] AWaterBody osztályok használhatók
- [ ] Automatizált tesztek zöldek

---

## MVP 2: UWaterSystemBuilder Váz és Forráspontok

### Cél
UWaterSystemBuilder komponens váz és FindRiverSources() implementálása.

### Implementációs Lépések

1. **UWaterSystemBuilder header** – ActorComponent, WaterBodyDefinitions, SpawnedWaterBodies TArray, GetWaterBodies() getter
2. **FindRiverSources()** – Lokális maximumok keresése, RiverCount számú forrás, minimális távolság biztosítása

### Automatizált Tesztek (MVP 2)

**Test_FindRiverSources.cpp:** RiverCount számú forrás, Forráspontok magaspontokon, Determinizmus

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `BP_TestWaterSystem` Actor Blueprint létrehozása UWaterSystemBuilder és UHeightmapGenerator komponensekkel, BeginPlay → Print String "WaterBuilt" node-dal, spawolása a szintbe
2. **[AI]** `RunTests.bat` futtatása – river source detection tesztek zöldek legyenek
3. **[Manuális]** PIE-ben futtasd a `MapGen.ShowRiverSources` console command-ot
4. **[Manuális]** Ellenőrizd: forráspontok láthatók (piros gömbök), RiverCount beállítás hatása

### Elfogadási Kritériumok (MVP 2)

- [ ] UWaterSystemBuilder komponens fordul
- [ ] FindRiverSources() RiverCount számú forrást választ
- [ ] Automatizált tesztek zöldek

---

## MVP 3: Folyó Útvonal Nyomkövetés

### Cél
TraceRiverPath() implementálása gradiens-alapú útvonal nyomkövetéssel.

### Implementációs Lépések

1. **TraceRiverPath()** – Forrásponttól legmeredekebb lejt irányba haladás, tengerszint alatt megállás, SplinePoints feltöltése
2. **GenerateRiver()** – FWaterBodyDefinition kitöltése

### Automatizált Tesztek (MVP 3)

**Test_RiverPath.cpp:** Folyó tengerszint felé halad, Minden pont a HeightMap-en van, Nem végtelen ciklus

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `RunTests.bat` futtatása – river path tesztek zöldek legyenek
2. **[Manuális]** PIE-ben futtasd a `MapGen.ShowRiverPaths` console command-ot
3. **[Manuális]** Ellenőrizd vizuálisan: folyók hegytől tengerfelé haladnak (kék vonalak), természetes útvonalak, RiverCount darab folyó látható

### Elfogadási Kritériumok (MVP 3)

- [ ] TraceRiverPath() helyes útvonalat generál
- [ ] Folyók tengerszint alatti ponton leállnak
- [ ] Automatizált tesztek zöldek

---

## MVP 4: Tó Generálás

### Cél
FindLakeLocations() és GenerateLake() implementálása lokális minimumok alapján.

### Implementációs Lépések

1. **FindLakeLocations()** – Lokális minimumok keresése (minden szomszéd magasabb), LakeCount számú tó, minimális távolság
2. **CalculateLakeContour()** – Vízszint meghatározás, körülhatárolás
3. **GenerateLake()** – FWaterBodyDefinition kitöltése

### Automatizált Tesztek (MVP 4)

**Test_LakeGeneration.cpp:** LakeCount számú tó, Tavak lokális minimumokban, Kontúr pontok körülzártak

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `RunTests.bat` futtatása – lake detection tesztek zöldek legyenek
2. **[Manuális]** PIE-ben futtasd a `MapGen.ShowLakes` console command-ot
3. **[Manuális]** Ellenőrizd vizuálisan: LakeCount darab tó látható (zöld gömbök), tavak mélyedésekben vannak, kontúrok reálisak

### Elfogadási Kritériumok (MVP 4)

- [ ] FindLakeLocations() lokális minimumokat talál
- [ ] LakeCount számú tó definíció generálva
- [ ] Automatizált tesztek zöldek

---

## MVP 5: Óceán Generálás

### Cél
GenerateOcean() implementálása a térkép szélei mentén.

### Implementációs Lépések

1. **GenerateOcean()** – Térkép szélei mentén négyzet spline kontúr, SeaLevel alapú mélység
2. **BuildWaterBodies() bővítése** – GenerateOcean() hívása

### Automatizált Tesztek (MVP 5)

**Test_GenerateOcean.cpp:** Óceán definíció létrejön, SplinePoints a térkép szélein, WaterType == Ocean

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `RunTests.bat` futtatása – ocean boundary tesztek zöldek legyenek
2. **[Manuális]** PIE-ben futtasd a `MapGen.ShowOcean` console command-ot
3. **[Manuális]** Ellenőrizd vizuálisan: óceán kontúr látható (cyan vonal), térkép széleit lefedi

### Elfogadási Kritériumok (MVP 5)

- [ ] GenerateOcean() óceán definíciót hoz létre
- [ ] SplinePoints a térkép szélein
- [ ] Automatizált tesztek zöldek

---

## MVP 6: Water Body Actor Spawn és Query Funkciók

### Cél
WaterBodyDefinitions alapján AWaterBody actorok létrehozása és IsWaterAt(), GetNearestWaterDistance() query funkciók.

### Implementációs Lépések

1. **SpawnRiver(), SpawnLake(), SpawnOcean()** – AWaterBody actorok spawn, Spline konfiguráció
2. **BuildWaterBodies() finalizálás** – Minden definíció spawn-olása, SpawnedWaterBodies feltöltése
3. **IsWaterAt()** és **GetNearestWaterDistance()** query metódusok

### Automatizált Tesztek (MVP 6)

**Test_SpawnWaterBodies.cpp** és **Test_WaterQuery.cpp:** AWaterBody actorok létrejönnek, Query funkciók helyes értékeket adnak

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `RunTests.bat` futtatása – water body creation tesztek zöldek legyenek
2. **[Manuális]** PIE-ben ellenőrizd: folyók megjelennek blue víz shader-rel, tavak és óceán megjelennek
3. **[Manuális]** World Outliner-ben ellenőrizd: AWaterBodyRiver, AWaterBodyLake, AWaterBodyOcean actorok léteznek
4. **[Manuális]** Teszteld a víz interakciót: Player karakter vízbe lépés

### Elfogadási Kritériumok (MVP 6)

- [ ] AWaterBodyRiver, AWaterBodyLake, AWaterBodyOcean actorok spawn-olva
- [ ] IsWaterAt() és GetNearestWaterDistance() működnek
- [ ] Automatizált tesztek zöldek

---

## Átfogó Tesztelési Követelmények

### Use Case Lefedettség

| Use Case ID       | Use Case Neve                            | Implementáló Metódus          | Teszt Osztály                    |
|-------------------|------------------------------------------|-------------------------------|----------------------------------|
| UC-MG-02.4        | Vízrendszer létrehozása                  | BuildWaterBodies()            | Test_SpawnWaterBodies            |
| UC-MG-02.4-A      | Folyó forráspontok meghatározása         | FindRiverSources()            | Test_FindRiverSources            |
| UC-MG-02.4-B      | Folyó útvonal nyomkövetés               | TraceRiverPath()              | Test_RiverPath                   |
| UC-MG-02.4-C      | Tó generálás                             | FindLakeLocations()           | Test_LakeGeneration              |
| UC-MG-02.4-D      | Óceán generálás                          | GenerateOcean()               | Test_GenerateOcean               |
| UC-MG-02.4-E      | Water Body actor spawn                   | SpawnRiver/Lake/Ocean()       | Test_SpawnWaterBodies            |

### Code Coverage Céleloszlás

| Komponens/Modul                  | Minimum Coverage | Cél Coverage | Prioritás |
|----------------------------------|------------------|--------------|-----------|
| BuildWaterBodies()               | 95%              | 100%         | Kritikus  |
| FindRiverSources()               | 100%             | 100%         | Kritikus  |
| TraceRiverPath()                 | 100%             | 100%         | Kritikus  |
| FindLakeLocations()              | 100%             | 100%         | Magas     |
| GenerateOcean()                  | 100%             | 100%         | Magas     |
| SpawnRiver/Lake/Ocean()          | 95%              | 100%         | Magas     |
| IsWaterAt()                      | 100%             | 100%         | Közepes   |
| GetNearestWaterDistance()        | 100%             | 100%         | Közepes   |
| **Teljes modul**                 | **90%**          | **96%**      | Kritikus  |

## Kockázatok és Kockázatcsökkentés

| Kockázat                              | Valószínűség | Hatás   | Csökkentési Stratégia                                      |
|---------------------------------------|--------------|---------|------------------------------------------------------------|
| Water plugin API változás             | Alacsony     | Magas   | UE verzió dokumentáció konzultálása                       |
| Folyók nem természetesek              | Közepes      | Közepes | Erózió szimuláció, gradiens smoothing                     |
| Teljesítmény probléma sok folyónál    | Közepes      | Közepes | Folyók számának limitálása, LOD használata                |
| Landscape-Water integráció problémák  | Közepes      | Közepes | Water plugin dokumentáció, példák tanulmányozása          |

## Elfogadási Kritériumok (Teljes Modul)

- [ ] **MVP 1 teljesítve:** Water plugin engedélyezve, FWaterBodyDefinition kész
- [ ] **MVP 2 teljesítve:** UWaterSystemBuilder váz és forráspontok kiválasztása működik
- [ ] **MVP 3 teljesítve:** Folyó útvonal nyomkövetés gradiens alapján
- [ ] **MVP 4 teljesítve:** Tó generálás lokális minimumokon
- [ ] **MVP 5 teljesítve:** Óceán generálás térkép széleinél
- [ ] **MVP 6 teljesítve:** Water Body actorok spawn-olva, query funkciók működnek
- [ ] BuildWaterBodies() metódus teljes pipeline-t lefuttatja
- [ ] IsWaterAt() és GetNearestWaterDistance() működnek
- [ ] Összes automatizált teszt zöld

## Következő Lépések

1. **LandscapeBuilder** – Landscape és Water integráció
2. **ResourceDistributor** – Vízközelség figyelembevétele az erőforrás elosztásban
3. **BiomeManager frissítés** – River/Lake biómok hozzáadása a vízpartokra
