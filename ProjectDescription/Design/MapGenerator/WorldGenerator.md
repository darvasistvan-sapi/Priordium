# AWorldGenerator - Orchestrator Actor

## Áttekintés

Az `AWorldGenerator` az egyetlen top-level Actor, amelyet a generálás indításakor a levelbe kell helyezni (vagy kódból spawnolni). Felelős a generálási pipeline lépéseinek helyes sorrendben történő végrehajtásáért és az egyes alrendszerek közötti adattovábbításért.

## Fájlok

| Fájl   | Útvonal                                                    |
|--------|----------------------------------------------------------|
| Header | `Source/Priordium/MapGenerator/Public/WorldGenerator.h`    |
| Source | `Source/Priordium/MapGenerator/Private/WorldGenerator.cpp` |

## Felelősségek

- A generálási pipeline elindítása és vezérlése
- Az alkomponensek létrehozása és összekötése
- Az adatok (heightmap, bióm térkép, víz adat) továbbítása a lépések között
- Generálási folyamat állapotának nyilvántartása (progress tracking)
- Hiba esetén a generálás leállítása és a hiba jelzése

## Komponensek

Az `AWorldGenerator` a következő `UActorComponent`-eket tartalmazza (a konstruktorban létrehozva):

| Komponens             | Típus                    | Leírás                    |
|-----------------------|---------------------------|---------------------------|
| `HeightmapGenerator`  | `UHeightmapGenerator*`    | Magasságtérkép generálás      |
| `BiomeManager`        | `UBiomeManager*`          | Bióm kiosztás                 |
| `LandscapeBuilder`    | `ULandscapeBuilder*`      | UE Landscape létrehozás       |
| `WaterSystemBuilder`  | `UWaterSystemBuilder*`    | Víz actorok létrehozása         |
| `ClimateZoneManager`  | `UClimateZoneManager*`    | Klímazóna számítás            |
| `ResourceDistributor` | `UResourceDistributor*`   | Erőforrás elhelyezés           |

## Tulajdonságok

| Tulajdonság                | Típus                    | Leírás                                                  |
|----------------------------|--------------------------|----------------------------------------------------------|
| `GeneratorSettings`        | `UMapGeneratorSettings*` | Referencia a konfigurációs Data Assetre                   |
| `bAutoGenerateOnBeginPlay` | `bool`                   | Automatikusan induljon-e a generálás BeginPlay-nél       |
| `CurrentStep`              | `int32`                  | Az aktuális generálási lépés indexe                       |
| `bIsGenerating`            | `bool`                   | Folyamatban van-e éppen generálás                         |

## Publikus metódusok

| Metódus                  | Visszatérés | Leírás                                             |
|---------------------------|---------|----------------------------------------------------|
| `GenerateWorld()`         | `void`  | Elindítja a teljes generálási pipeline-t             |
| `GetGenerationProgress()` | `float` | Vissza­ja a generálás előrehaladását (0.0-1.0)      |
| `IsGenerating()`          | `bool`  | Fut-e éppen generálás                               |
| `ClearGeneratedWorld()`   | `void`  | Törli az összes generált elemet a levelből            |

## Generálási Pipeline

```
GenerateWorld()
│
├─ 1. HeightmapGenerator->Generate(Settings)
│     └─ Kimenet: TArray<float> HeightmapData
│
├─ 2. BiomeManager->AssignBiomes(HeightmapData, Settings)
│     └─ Kimenet: TArray<FBiomeCell> BiomeMap
│
├─ 3. LandscapeBuilder->BuildLandscape(HeightmapData, BiomeMap, Settings)
│     └─ Kimenet: ALandscapeProxy* GeneratedLandscape
│
├─ 4. WaterSystemBuilder->BuildWaterBodies(HeightmapData, BiomeMap, Settings)
│     └─ Kimenet: TArray<AWaterBody*> WaterBodies
│
├─ 5. ClimateZoneManager->CalculateClimateZones(BiomeMap, Settings)
│     └─ Kimenet: TArray<EClimateZone> ClimateData
│
└─ 6. ResourceDistributor->DistributeResources(BiomeMap, ClimateData, WaterBodies, Settings)
      └─ Kimenet: Elhelyezett resource actorok
```

## Hibakezelés

- Minden lépés bool-t ad vissza (sikerült-e)
- Hiba esetén a pipeline leáll és `OnGenerationFailed` delegate-et hív
- Sikerüs befejezés esetén `OnGenerationCompleted` delegate-et hív

## Delegate-ek

| Delegate                    | Paraméter              | Leírás                                |
|-----------------------------|------------------------|---------------------------------------|
| `OnGenerationCompleted`     | -                      | A generálás sikeresen befejeződött     |
| `OnGenerationFailed`        | `FString ErrorMessage` | A generálás hiba miatt leállt         |
| `OnGenerationStepCompleted` | `int32 StepIndex`      | Egy generálási lépés befejeződött     |
