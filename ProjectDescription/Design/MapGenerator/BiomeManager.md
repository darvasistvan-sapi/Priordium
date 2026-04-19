# UBiomeManager - Bióm Kezelés

## Áttekintés

A `UBiomeManager` felelős a térkép minden cellájához egy bióm típus hozzárendeléséért. A bióm meghatározza a terep vizuális megjelenését (Landscape anyag réteg), a rajta megjelenő növényzetet és az elérhető erőforrásokat.

## Fájlok

| Fájl   | Útvonal                                                    |
|--------|------------------------------------------------------------|
| Header | `Source/Priordium/MapGenerator/Public/BiomeManager.h`    |
| Source | `Source/Priordium/MapGenerator/Private/BiomeManager.cpp` |

## Felelősségek

- Biómok meghatározása a magasság, hőmérséklet és csapadék alapján
- Bióm térkép előállítása (2D grid, ahol minden cella egy `FBiomeCell`)
- Biómok közötti átmenetek simítása
- Lokális csapadék- és hőmérséklettérkép generálása a bióm-meghatározáshoz

## EBiomeType Enum

| Érték      | Leírás                      | Jellemző magasság                | Jellemző növényzet           |
|------------|-----------------------------|---------------------------------|------------------------------|
| `Ocean`    | Tenger (tengerszint alatt)  | < SeaLevel                      | Nincs                        |
| `Beach`    | Tengerpart                  | SeaLevel ± kis sáv              | Homok, nád                   |
| `Plains`   | Síkság                      | Alacsony-közepes                | Fű, bokrok                   |
| `Forest`   | Erdő                        | Alacsony-közepes, magas csapadék | Fák, bokrok, gomba           |
| `Hills`    | Dombvidék                   | Közepes                         | Fű, szorányos fák            |
| `Mountain` | Hegy                        | Magas                           | Kő, sziklák, kevés növényzet |
| `River`    | Folyópart                   | Változó (folyó mentén)          | Nád, fűz, agyag              |
| `Lake`     | Tópart                      | Változó (tó mentén)             | Nád, halak                   |
| `Swamp`    | Mocsár                      | Alacsony, magas csapadék        | Mocsári növényzet            |

## FBiomeCell Struktúra

| Mező                | Típus                     | Leírás                                                            |
|---------------------|---------------------------|-------------------------------------------------------------------|
| `BiomeType`         | `EBiomeType`              | A cella bióm típusa                                               |
| `Height`            | `float`                   | Normalizált magasság (0.0-1.0)                                    |
| `Temperature`       | `float`                   | Lokális hőmérséklet (0.0-1.0, ahol 0=hideg, 1=meleg)             |
| `Moisture`          | `float`                   | Lokális csapadék/nedvesség (0.0-1.0)                              |
| `BiomeBlendWeights` | `TMap<EBiomeType, float>` | Szomszédos biómokkal való átmeneti súlyok (material blending)     |

## Algoritmus

### 1. Hőmérséklet térkép

- Alap hőmérséklet: a térkép "szélességi foka" alapján (Y tengely mentén, észak hidegebb)
- Magasság korrekció: magasabb pontok hidegebbek
- Zaj: kis mértékű Perlin zaj a változatosságért

```
temperature = baseLatitudeTemp - heightPenalty + noise
```

### 2. Csapadék térkép

- Perlin noise alapú alap csapadék
- Tengerközelség növeli a csapadékot
- Hegyek mögötti terület kevesebb csapadékot kap (esőárnyék)

### 3. Bióm meghatározás

Minden cellára a bióm a magasság, hőmérséklet és csapadék kombinációja alapján:

```
if height < seaLevel:         -> Ocean
if height < seaLevel + beach:  -> Beach
if height > mountainThreshold: -> Mountain
if height > hillThreshold:     -> Hills
if moisture > forestThreshold: -> Forest
if moisture < swampThreshold AND height < lowThreshold: -> Swamp
else:                          -> Plains
```

### 4. Átmenetek simítása

- A bióm határok mentén `BiomeBlendWeights` számítása a szomszédos cellák alapján
- Ez a Landscape material rétegek közötti lágy átmenetet biztosítja

## Publikus metódusok

| Metódus                                                                          | Visszatérés                 | Leírás                               |
|----------------------------------------------------------------------------------|-----------------------------|--------------------------------------|
| `AssignBiomes(const TArray<float>& Heightmap, UMapGeneratorSettings* Settings)` | `bool`                      | Bióm térkép generálása               |
| `GetBiomeMap()`                                                                  | `const TArray<FBiomeCell>&` | Vissza­ja a teljes bióm térképet     |
| `GetBiomeAt(int32 X, int32 Y)`                                                   | `const FBiomeCell&`         | Egy pont bióm adata                  |
| `GetBiomeTypeAt(int32 X, int32 Y)`                                               | `EBiomeType`                | Egy pont bióm típusa                 |
