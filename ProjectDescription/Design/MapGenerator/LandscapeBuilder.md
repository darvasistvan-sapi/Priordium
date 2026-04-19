# ULandscapeBuilder - Landscape Építés

## Áttekintés

A `ULandscapeBuilder` felelős az UE Landscape rendszer használatával a tényleges terep actor létrehozásáért. A heightmap adatot és a bióm térképet Landscape magassági adattá és material réteg súlyokká konvertálja.

## Fájlok

| Fájl   | Útvonal                                                        |
|--------|----------------------------------------------------------------|
| Header | `Source/Priordium/MapGenerator/Public/LandscapeBuilder.h`      |
| Source | `Source/Priordium/MapGenerator/Private/LandscapeBuilder.cpp`   |

## Felelősségek

- `ALandscapeProxy` actor létrehozása a levelben
- Heightmap adat konvertálása Landscape `uint16` formátumra
- Landscape material rétegek (Layer Info) beállítása a biómok alapján
- Bióm blend súlyok alkalmazása a Landscape paint layer-ekre
- Landscape komponensek és szekciók helyes konfigurálása

## Landscape Struktúra

| Paraméter          | Érték                                   | Leírás                                    |
|--------------------|------------------------------------------|-----------------------------------------|
| Komponens méret    | 63×63                                    | Egy Landscape komponens quad-jainak száma |
| Szekciók/komponens | 2×2                                      | Szekciók száma komponensenként            |
| Teljes felbontás   | A Settings-ből (`LandscapeResolution`) | A heightmap pontjainak száma élenként     |

## Material Rétegek

Minden `EBiomeType`-hoz egy Landscape Material Layer tartozik:

| Bióm       | Layer név           | Anyag jellemző                |
|------------|---------------------|-------------------------------|
| `Plains`   | `Layer_Grass`       | Fű textúra                    |
| `Forest`   | `Layer_ForestFloor` | Erdői talaj (levelek, moha)   |
| `Hills`    | `Layer_DryGrass`    | Száraz fű                     |
| `Mountain` | `Layer_Rock`        | Szikla textúra                |
| `Beach`    | `Layer_Sand`        | Homok                         |
| `Swamp`    | `Layer_Mud`         | Sár, mocsári talaj            |
| `River`    | `Layer_RiverBank`   | Folyóparti kavics             |
| `Lake`     | `Layer_LakeShore`   | Tóparti föld                  |

## Algoritmus

### 1. Heightmap konvertálás

```
A float [0.0, 1.0] heightmap-et uint16 [0, 65535] formátumra konvertálja.
A Landscape középértéke 32768, tehát:

landscapeHeight = (uint16)(normalizedHeight * MaxHeight / LandscapeScaleZ * 32768 + 32768)
```

### 2. Landscape létrehozása

- `ALandscapeProxy::Import()` hívás a heightmap adattal
- Landscape transform beállítása (pozíció, skála)
- Material hozzárendelése

### 3. Paint Layer alkalmazás

- Minden cellára a `FBiomeCell::BiomeBlendWeights` alapján a layer súlyok beállítása
- A Landscape `LandscapeEditDataInterface` API-n keresztül

## Publikus metódusok

| Metódus                                                                                                          | Visszatérés        | Leírás                                |
|------------------------------------------------------------------------------------------------------------------|--------------------|---------------------------------------|
| `BuildLandscape(const TArray<float>& Heightmap, const TArray<FBiomeCell>& BiomeMap, UMapGeneratorSettings* Settings)` | `bool`             | Létrehozza a Landscape actort         |
| `GetGeneratedLandscape()`                                                                                        | `ALandscapeProxy*` | Vissza­ja a létrehozott Landscape-et |
| `ApplyMaterialLayers(const TArray<FBiomeCell>& BiomeMap)`                                                       | `bool`             | Alkalmazza a material réteg súlyokat  |

## Függőségek

- A Landscape module-t hozzá kell adni a Build.cs-hez: `"Landscape"`
- Szükséges egy alap Landscape Material asset, amely tartalmazza az összes Layer blend-et
