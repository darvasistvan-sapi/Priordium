# ResourceDistributor - Fejlesztési Terv

## Áttekintés

A `UResourceDistributor` felelős az összes természeti erőforrás (állatok, növények, ásványi anyagok) elhelyezéséért a térképen. A bióm térkép, klímazóna adat és víz közelsége alapján határozza meg az erőforrás elhelyezési pozíciókat.

## Célkitűzések

- Erőforrások realisztikus elhelyezése bióm és klíma alapján
- FResourceSpawnRule rendszer implementálása
- Szűrési logika (bióm, klíma, magasság, víztávolság)
- Sűrűség és klaszter alapú elhelyezés
- Spawn-olt actorok nyilvántartása és lekérdezhetősége
- Optimalizált teljesítmény

## Függőségek

### Előfeltételek
- **MapGeneratorSettings** modul implementálva
- **BiomeManager** modul implementálva
- **ClimateZoneManager** modul implementálva
- **WaterSystemBuilder** modul implementálva
- `FResourceSpawnRule` struktúra definiálva
- GameplayTags plugin engedélyezve

### Külső függőségek
- UE GameplayTags modul
- PCG Framework (opcionális, növényzethez)
- Resource actor Blueprint-ek vagy C++ osztályok

## MVP Fejlesztési Struktúra

## MVP 1: GameplayTags Setup és FResourceSpawnRule Struktúra

### Cél
GameplayTags plugin engedélyezése és FResourceSpawnRule struktúra létrehozása.

### Implementációs Lépések

1. **Build.cs** – GameplayTags modul hozzáadása
2. **FResourceSpawnRule struktúra** – ResourceTag, ActorClass, AllowedBiomes, AllowedClimateZones, MinHeight/MaxHeight, Density, ClusterSize, ClusterRadius
3. **Alapvető GameplayTag-ek** – Resource.Animal.Deer, Resource.Plant.Tree, Resource.Mineral.Iron stb.

### Automatizált Tesztek (MVP 1)

**Test_GameplayTags.cpp** és **Test_ResourceSpawnRule.cpp:** GameplayTags modul elérhető, FResourceSpawnRule létrehozható, TArray és TSoftClassPtr műveletek

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `BP_TestResourceRule` Blueprint létrehozása `FResourceSpawnRule` típusú publikus változóval
2. **[AI]** `RunTests.bat` futtatása – `Priordium.MapGenerator.ResourceDistributor.*` tesztek zöldek legyenek
3. **[Manuális]** Hozd létre a GameplayTag-eket a Project Settings-ben: Resource.Animal.Deer, Resource.Plant.Tree, Resource.Mineral.Iron, Resource.Plant.Berry, Resource.Mineral.Stone, Resource.Animal.Wolf
4. **[Manuális]** BP_TestResourceRule Details Panelén ellenőrizd, hogy az FResourceSpawnRule mezők szerkeszthetők

### Elfogadási Kritériumok (MVP 1)

- [ ] GameplayTags plugin engedélyezve
- [ ] FResourceSpawnRule struktúra fordul
- [ ] GameplayTag-ek definiálva
- [ ] Automatizált tesztek zöldek

---

## MVP 2: UResourceDistributor Váz és Szűrési Logika

### Cél
UResourceDistributor komponens váz és CheckFilters() szűrési logika implementálása.

### Implementációs Lépések

1. **UResourceDistributor header** – ActorComponent, SpawnedResources TArray, Publikus interfész
2. **CheckFilters() implementálás** – Bióm, klíma, magasság szűrők
3. **GetBiomeFilterResult()** és **GetClimateFilterResult()** helper-ek

### Automatizált Tesztek (MVP 2)

**Test_CheckFilters.cpp:** Megfelelő bióm/klíma → true, Nem megfelelő → false, Üres AllowedBiomes → minden bióm elfogadott

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `BP_TestResourceDistributor` Actor Blueprint létrehozása UResourceDistributor komponenssel, spawolása a szintbe
2. **[AI]** `RunTests.bat` futtatása – resource filter tesztek zöldek legyenek
3. **[Manuális]** PIE-ben futtasd a `MapGen.TestResourceFilters` console command-ot, ellenőrizd a szűrési logikát az Output Log-ban

### Elfogadási Kritériumok (MVP 2)

- [ ] UResourceDistributor komponens fordul
- [ ] CheckFilters() helyes szűrési eredményt ad
- [ ] Automatizált tesztek zöldek

---

## MVP 3: Víztávolság Számítás és Elhelyezési Területek

### Cél
CheckWaterDistanceFilter() és CalculatePlacementAreas() implementálása.

### Implementációs Lépések

1. **CheckWaterDistanceFilter()** – MinWaterDistance és MaxWaterDistance ellenőrzés
2. **CalculatePlacementAreas()** – CheckFilters() + víztávolság szűrő, valid területek listája
3. **WaterDistanceMap cache** (teljesítmény optimalizálás)

### Automatizált Tesztek (MVP 3)

**Test_WaterDistanceFilter.cpp** és **Test_PlacementAreas.cpp**

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `RunTests.bat` futtatása – placement area tesztek zöldek legyenek
2. **[Manuális]** PIE-ben futtasd a `MapGen.ShowPlacementAreas ResourceTag` console command-ot
3. **[Manuális]** Ellenőrizd vizuálisan a valid placement területeket (zöld gömbök), Output Log-ban a területek számát

### Elfogadási Kritériumok (MVP 3)

- [ ] CheckWaterDistanceFilter() működik
- [ ] CalculatePlacementAreas() szűrt listát ad
- [ ] Automatizált tesztek zöldek

---

## MVP 4: Egyedi Erőforrás Spawn

### Cél
SpawnResource() actor spawn implementálása és SpawnedResources nyilvántartás.

### Implementációs Lépések

1. **SpawnResource()** – TSoftClassPtr betöltés, Actor spawn, SpawnedResources hozzáadás
2. **CalculateResourceCount()** – Density alapú mennyiség számítás

### Automatizált Tesztek (MVP 4)

**Test_SpawnResource.cpp:** Actor létrejön, SpawnedResources bővül, Pozíció helyes, nullptr ActorClass → nullptr return

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `RunTests.bat` futtatása – resource distribution tesztek zöldek legyenek
2. **[Manuális]** PIE-ben futtasd a DistributeResources()-t (Deer, Forest bióm, Density=0.1)
3. **[Manuális]** World Outliner-ben ellenőrizd, hogy Deer actorok megjelentek

### Elfogadási Kritériumok (MVP 4)

- [ ] SpawnResource() actort hoz létre
- [ ] SpawnedResources nyilvántartás működik
- [ ] Automatizált tesztek zöldek

---

## MVP 5: Klaszter Spawn

### Cél
SpawnCluster() implementálása ClusterSize és ClusterRadius paraméterekkel.

### Implementációs Lépések

1. **SpawnCluster()** – ClusterSize darab actor, ClusterRadius-on belüli random pozíciókkal
2. **DistributeResources() bővítése** – SpawnCluster() használata ClusterSize > 1 esetén

### Automatizált Tesztek (MVP 5)

**Test_SpawnCluster.cpp:** ClusterSize darab actor jön létre, Mind ClusterRadius-on belül, Random offset alkalmazva

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `RunTests.bat` futtatása – cluster distribution tesztek zöldek legyenek
2. **[Manuális]** PIE-ben futtasd: DistributeResources() (Tree, Forest, ClusterSize=5, ClusterRadius=1000)
3. **[Manuális]** Ellenőrizd vizuálisan: fa klaszterek láthatók a debug gömbökkel

### Elfogadási Kritériumok (MVP 5)

- [ ] SpawnCluster() ClusterSize darab actort hoz létre
- [ ] Actorok ClusterRadius-on belül
- [ ] Automatizált tesztek zöldek

---

## MVP 6: DistributeResources() Orchestration és Query Funkciók

### Cél
DistributeResources() teljes pipeline és GetResourcesInArea(), ClearResources() query funkciók.

### Implementációs Lépések

1. **DistributeResources()** – ResourceSpawnRules iterálás, CalculatePlacementAreas(), SpawnCluster()
2. **GetResourcesInArea()** – Box query, GameplayTag szűrés
3. **ClearResources()** – SpawnedResources Destroy() és törlés

### Automatizált Tesztek (MVP 6)

**Test_DistributeResources.cpp** és **Test_ResourceQuery.cpp:** Teljes pipeline, Seed determinizmus, Query funkciók

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `RunTests.bat` futtatása – full pipeline resource tesztek zöldek legyenek
2. **[Manuális]** PIE-ben futtasd a teljes pipeline-t: HeightmapGenerator → BiomeManager → ClimateZoneManager → WaterSystemBuilder → ResourceDistributor.DistributeResources()
3. **[Manuális]** World Outliner-ben ellenőrizd, hogy erőforrás actorok megjelentek
4. **[Manuális]** PIE-ben teszteld: GetResourcesInArea() → találat darabszám az Output Log-ban, ClearResources() → erőforrások eltűnnek

### Elfogadási Kritériumok (MVP 6)

- [ ] DistributeResources() teljes pipeline-t futtat
- [ ] GetResourcesInArea() és ClearResources() működnek
- [ ] Automatizált tesztek zöldek

---

## Átfogó Tesztelési Követelmények

### Use Case Lefedettség

| Use Case ID       | Use Case Neve                            | Implementáló Metódus          | Teszt Osztály                    |
|-------------------|------------------------------------------|-------------------------------|----------------------------------|
| UC-MG-02.6        | Erőforrások elhelyezése                  | DistributeResources()         | Test_DistributeResources         |
| UC-MG-02.6-A      | Szűrési logika                           | CheckFilters()                | Test_CheckFilters                |
| UC-MG-02.6-B      | Víztávolság szűrés                       | CheckWaterDistanceFilter()    | Test_WaterDistanceFilter         |
| UC-MG-02.6-C      | Elhelyezési területek                    | CalculatePlacementAreas()     | Test_PlacementAreas              |
| UC-MG-02.6-D      | Egyedi spawn                             | SpawnResource()               | Test_SpawnResource               |
| UC-MG-02.6-E      | Klaszter spawn                           | SpawnCluster()                | Test_SpawnCluster                |

### Code Coverage Céleloszlás

| Komponens/Modul                  | Minimum Coverage | Cél Coverage | Prioritás |
|----------------------------------|------------------|--------------|-----------|
| DistributeResources()            | 95%              | 100%         | Kritikus  |
| CheckFilters()                   | 100%             | 100%         | Kritikus  |
| CalculatePlacementAreas()        | 100%             | 100%         | Kritikus  |
| SpawnResource()                  | 95%              | 100%         | Magas     |
| SpawnCluster()                   | 100%             | 100%         | Magas     |
| GetResourcesInArea()             | 100%             | 100%         | Közepes   |
| ClearResources()                 | 100%             | 100%         | Közepes   |
| **Teljes modul**                 | **90%**          | **96%**      | Kritikus  |

## Kockázatok és Kockázatcsökkentés

| Kockázat                                      | Valószínűség | Hatás   | Csökkentési Stratégia                                        |
|-----------------------------------------------|--------------|---------|--------------------------------------------------------------|
| Teljesítmény probléma sok erőforrás spawn-nál | Közepes      | Magas   | Batch spawn, async loading, instanced static mesh            |
| Víztávolság számítás lassú                    | Közepes      | Közepes | Water distance map cache-elés előre                         |
| Resource Actor Blueprint-ek hiánya            | Közepes      | Magas   | Placeholder actorok használata fejlesztés közben            |

## Elfogadási Kritériumok (Teljes Modul)

- [ ] **MVP 1 teljesítve:** GameplayTags engedélyezve, FResourceSpawnRule kész
- [ ] **MVP 2 teljesítve:** Szűrési logika működik
- [ ] **MVP 3 teljesítve:** Víztávolság és elhelyezési területek működnek
- [ ] **MVP 4 teljesítve:** Egyedi spawn működik
- [ ] **MVP 5 teljesítve:** Klaszter spawn működik
- [ ] **MVP 6 teljesítve:** DistributeResources() és query funkciók működnek
- [ ] Összes automatizált teszt zöld

## Következő Lépések

1. **WorldGenerator** – ResourceDistributor integráció a pipeline-ba
2. **PCG integráció** (opcionális) – Növényzet generálás PCG Framework-kel
3. **Resource Manager** (jövőbeli) – Spawn-olt erőforrások lifecycle kezelése
