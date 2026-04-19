# UResourceDistributor - Erőforrás Elosztás

## Áttekintés

A `UResourceDistributor` felelős az összes természeti erőforrás (állatok, növények, ásványi anyagok) elhelyezéséért a térképen. A bióm térkép, a klímazóna adat és a víz közelsége alapján határozza meg, hová milyen erőforrások kerüljenek.

## Fájlok

| Fájl   | Útvonal                                                         |
|--------|----------------------------------------------------------------|
| Header | `Source/Priordium/MapGenerator/Public/ResourceDistributor.h`    |
| Source | `Source/Priordium/MapGenerator/Private/ResourceDistributor.cpp` |

## Felelősségek

- Az erőforrások elhelyezése a `FResourceSpawnRule`-ok alapján
- A bióm, klíma és vízközelség szűrők alkalmazása
- Az erőforrás sűrűség számítása és a véletlenszerű elhelyezés
- Az elhelyezett erőforrás actorok nyilvántartása
- PCG (Procedural Content Generation) integráció a növényzet elhelyezéséhez

## FResourceSpawnRule Struktúra

| Mező                  | Típus                       | Leírás                                                                     |
|----------------------|-----------------------------|-----------------------------------------------------------------------------|
| `ResourceTag`        | `FGameplayTag`              | Az erőforrás azonosító Gameplay Tag-je (pl. `Resource.Animal.Deer`)        |
| `ActorClass`         | `TSoftClassPtr<AActor>`     | Az elhelyezendő actor osztálya                                              |
| `AllowedBiomes`      | `TArray<EBiomeType>`        | Melyik biómokban jelenhet meg                                             |
| `AllowedClimateZones`| `TArray<EClimateZone>`      | Melyik klímazónákban jelenhet meg                                         |
| `MinHeight`          | `float`                     | Minimális magasság (normalizált, 0.0-1.0)                                   |
| `MaxHeight`          | `float`                     | Maximális magasság (normalizált, 0.0-1.0)                                   |
| `MaxWaterDistance`   | `float`                     | Maximális távolság víztől (-1 = nincs korlát)                             |
| `MinWaterDistance`   | `float`                     | Minimális távolság víztől (0 = lehet vízparton)                           |
| `Density`            | `float`                     | Előfordulási sűrűség (darab / 10000 m²)                                   |
| `ClusterSize`        | `int32`                     | Csoportos elhelyezés mérete (1 = egyenként)                                |
| `ClusterRadius`      | `float`                     | Csoportos elhelyezés sugara                                              |

## Algoritmus

### 1. Elhelyezési terület meghatározás

Minden `FResourceSpawnRule`-ra:

```
1. A teljes bióm térkép szűrése az AllowedBiomes alapján
2. A klímazóna szűrés alkalmazása (AllowedClimateZones)
3. Magassági szűrés (MinHeight, MaxHeight)
4. Víztávolság szűrés (MinWaterDistance, MaxWaterDistance)
5. A fennmaradó cellák = érvényes elhelyezési terület
```

### 2. Mennyiség számítás

```
totalValidArea = érvényes cellák száma * cellaMéret²
resourceCount = totalValidArea * Density * GlobalResourceDensity / 10000m²
```

### 3. Pozíció kiválasztás

```
for i in range(resourceCount):
    if ClusterSize > 1:
        // Klaszter közepének véletlenszerű kiválasztása
        centerCell = random érvényes cella
        for j in range(ClusterSize):
            offset = random pont a ClusterRadius-on belül
            spawnPos = centerCell + offset
            if spawnPos érvényes:
                SpawnActor(ActorClass, spawnPos)
    else:
        // Egyedi elhelyezés
        cell = random érvényes cella
        SpawnActor(ActorClass, cell)
```

### 4. PCG integráció (növényzet)

A fák, bokrok és fű elhelyezéséhez a PCG Framework-öt használja:
- Biómonként egy PCG Graph asset, amely a növényzeti szabályokat tartalmazza
- A `UResourceDistributor` futtatja a PCG Graph-okat a megfelelő területeken
- A PCG Graph-ok a bióm és klíma adatot attribútumként kapják

## Erőforrás-bióm hozzárendelés (a Resources.md alapján)

| Erőforrás kategória      | Biómok                            | Példák                            |
|---------------------------|-----------------------------------|---------------------------------------|
| Vadászható állatok       | Forest, Plains, Hills             | Nyúl, őz, szarvas, vaddisznó       |
| Ragadozók               | Forest, Mountain                  | Medve, farkas                         |
| Halak                   | River, Lake (víztávolság < 0)     | Pisztráng, ponty, harcsa             |
| Gyümölcsök, bogyók      | Forest, Plains                    | Alma, szeder, málna                  |
| Ehető növények           | Forest, Plains, Swamp             | Gomba, ehető gyökerek               |
| Faanyag                 | Forest                            | Tölgy, fenyő, nyír                  |
| Kő, kavics              | Hills, Mountain, River            | Kovakő, mészkő, gránit             |
| Fémércek                | Mountain                          | Rézérc, ónérc, vasérc              |
| Agyag                   | River, Lake, Swamp                | Agyag                                 |
| Nád, gyékény           | River, Lake, Swamp                | Nád                                   |
| Gyógynövények           | Forest, Plains                    | Különféle gyógynövények            |
| Só                     | Mountain, Lake                    | Sóbánya, sós tó                   |

## Publikus metódusok

| Metódus                                                                                                                                                                       | Visszatérés       | Leírás                                          |
|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-------------------|-------------------------------------------------|
| `DistributeResources(const TArray<FBiomeCell>& BiomeMap, const TArray<EClimateZone>& ClimateData, const TArray<AWaterBody*>& WaterBodies, UMapGeneratorSettings* Settings)` | `bool`            | Az összes erőforrás elhelyezése                 |
| `GetResourcesInArea(FBox2D Area, FGameplayTag FilterTag)`                                                                                                                    | `TArray<AActor*>` | Erőforrások lekérdezése egy területen belül     |
| `GetResourceCount()`                                                                                                                                                          | `int32`           | Az összes elhelyezett erőforrás száma           |
| `ClearResources()`                                                                                                                                                            | `void`            | Az összes elhelyezett erőforrás törlése         |

## Függőségek

- `GameplayTags` modul a Build.cs-ben
- PCG plugin engedélyezése a .uproject-ben
- A Resource actor Blueprintek előzetes létrehozása szükséges
