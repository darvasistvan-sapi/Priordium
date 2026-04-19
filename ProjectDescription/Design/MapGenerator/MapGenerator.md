# Térkép Generátor - Architektúra Terv

## Áttekintés

A térkép generátor egy moduláris, lépésről-lépésre működő rendszer, amely egyetlen nagy procedurális térképet állít elő. A generálás determinisztikus (seed-alapú), így ugyanaz a seed mindig ugyanazt a térképet adja. A rendszer a következő UE rendszerekre épít: **Landscape System**, **Water System**, **PCG Framework**.

## Architektúra Diagram

![Architektúra Diagram](MapGenerator-Architecture.mmd)

## Use Case Diagram

![Use Case Diagram](MapGenerator-UseCases.mmd)

## Generálási sorrend

| Lépés | Osztály                  | Feladat                          | Bemenet                               | Kimenet                          |
|-------|--------------------------|----------------------------------|---------------------------------------|----------------------------------|
| 1     | `UHeightmapGenerator`    | Magasságtérkép generálás         | Seed, térkép méret                    | 2D float tömb (heightmap)        |
| 2     | `UBiomeManager`          | Biómok kiosztása                 | Heightmap, seed                       | 2D bióm térkép                   |
| 3     | `ULandscapeBuilder`      | UE Landscape létrehozása         | Heightmap, bióm térkép                | ALandscapeProxy actor            |
| 4     | `UWaterSystemBuilder`    | Vizek elhelyezése                | Heightmap, bióm térkép                | Water Body actorok               |
| 5     | `UClimateZoneManager`    | Klímazónák meghatározása         | Bióm térkép, szélességi fok           | Klíma adat tömb                  |
| 6     | `UResourceDistributor`   | Erőforrások elhelyezése          | Bióm térkép, klíma adat, víz adat     | Elhelyezett resource actorok     |

## Osztályok és fájlok

| Osztály                   | Típus           | Header fájl                                      | Source fájl                                        |
|---------------------------|-----------------|--------------------------------------------------|----------------------------------------------------|
| `UMapGeneratorSettings`   | UDataAsset      | `MapGenerator/Public/MapGeneratorSettings.h`     | `MapGenerator/Private/MapGeneratorSettings.cpp`    |
| `AWorldGenerator`         | AActor          | `MapGenerator/Public/WorldGenerator.h`           | `MapGenerator/Private/WorldGenerator.cpp`          |
| `UHeightmapGenerator`     | UActorComponent | `MapGenerator/Public/HeightmapGenerator.h`       | `MapGenerator/Private/HeightmapGenerator.cpp`      |
| `UBiomeManager`           | UActorComponent | `MapGenerator/Public/BiomeManager.h`             | `MapGenerator/Private/BiomeManager.cpp`            |
| `ULandscapeBuilder`       | UActorComponent | `MapGenerator/Public/LandscapeBuilder.h`         | `MapGenerator/Private/LandscapeBuilder.cpp`        |
| `UWaterSystemBuilder`     | UActorComponent | `MapGenerator/Public/WaterSystemBuilder.h`       | `MapGenerator/Private/WaterSystemBuilder.cpp`      |
| `UResourceDistributor`    | UActorComponent | `MapGenerator/Public/ResourceDistributor.h`      | `MapGenerator/Private/ResourceDistributor.cpp`     |
| `UClimateZoneManager`     | UActorComponent | `MapGenerator/Public/ClimateZoneManager.h`       | `MapGenerator/Private/ClimateZoneManager.cpp`      |

> **Megjegyzés**: A `MapGenerator/` mappa a `Source/Priordium/` alatt helyezkedik el.

## Támogató struktúrák és enumok

| Név                     | Típus  | Leírás                                                                                       |
|-------------------------|--------|----------------------------------------------------------------------------------------------|
| `EBiomeType`            | Enum   | Bióm típusok (Erdő, Síkság, Hegy, Folyópart, Tengerpart, Sivatag, Tundra)                   |
| `EClimateZone`          | Enum   | Klímazónák (Trópusi, Mérsékelt, Kontinentális, Szubarktikus)                                 |
| `FBiomeCell`            | Struct | Egy cella bióm adata (típus, hőmérséklet, csapadék, tengerszint feletti magasság)            |
| `FResourceSpawnRule`    | Struct | Erőforrás elhelyezési szabály (bióm szűrők, sűrűség, min/max magasság, víztávolság)         |
| `FHeightmapConfig`      | Struct | Magasságtérkép konfigurációs paraméterek (oktávok, frekvencia, amplitúdó, lakunaritás)      |
| `FWaterBodyDefinition`  | Struct | Víztestre vonatkozó definíció (típus, pontok, szélesség, mélység)                            |

## Alfunkció dokumentumok

Minden alfunkciónak külön leíró dokumentum tartozik:

- **[MapGeneratorSettings](MapGeneratorSettings.md)** - Konfigurációs Data Asset
- **[WorldGenerator](WorldGenerator.md)** - Orchestrator / fő vezérlő
- **[HeightmapGenerator](HeightmapGenerator.md)** - Magasságtérkép generálás
- **[BiomeManager](BiomeManager.md)** - Bióm kezelés
- **[LandscapeBuilder](LandscapeBuilder.md)** - Landscape építés
- **[WaterSystemBuilder](WaterSystemBuilder.md)** - Vízrendszer építés
- **[ClimateZoneManager](ClimateZoneManager.md)** - Klímazóna kezelés
- **[ResourceDistributor](ResourceDistributor.md)** - Erőforrás elosztás
