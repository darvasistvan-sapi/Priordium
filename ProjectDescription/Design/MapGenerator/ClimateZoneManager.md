# UClimateZoneManager - Klímazóna Kezelés

## Áttekintés

A `UClimateZoneManager` felelős a térkép klímazónáinak meghatározásáért. A klímazónák befolyásolják az időjárási mintákat, az elérhető erőforrásokat és a törzsek életfeltételeit.

## Fájlok

| Fájl   | Útvonal                                                          |
|--------|----------------------------------------------------------------|
| Header | `Source/Priordium/MapGenerator/Public/ClimateZoneManager.h`    |
| Source | `Source/Priordium/MapGenerator/Private/ClimateZoneManager.cpp` |

## Felelősségek

- Klímazónák kiszámítása a bióm térkép és földrajzi helyzet alapján
- Alap hőmérsékleti és csapadéki tartományok definiálása zónánként
- Az évszakok hatásának modellezése (a klímazóna határozza meg az évszakok intenzitását)
- Runtime lekérdezési interfész biztosítása az időjárás és AI rendszerek számára

## EClimateZone Enum

| Érték         | Leírás                          | Alap hőmérséklet tartomány | Évszak intenzitás                 |
|---------------|---------------------------------|----------------------------|-----------------------------------|
| `Tropical`    | Trópusi (meleg, csapadékos)     | 25-35 °C                   | Enyhe (kis hőingás)               |
| `Temperate`   | Mérsékelt öv                    | 5-25 °C                    | Közepes (4 évszak)                |
| `Continental` | Kontinentális (nagy hőingás)    | -10 – 30 °C                | Erős (kemény tél, meleg nyár)     |
| `Subarctic`   | Szubarktikus (hideg)            | -20 – 10 °C                | Nagyon erős (hosszú tél)          |

## Algoritmus

### Klímazóna meghatározás

A klímazóna a bióm cellákon lévő hőmérséklet és csapadék értékekből származik:

```
if temperature > 0.75 AND moisture > 0.5:  -> Tropical
if temperature > 0.4 AND temperature <= 0.75: -> Temperate
if temperature > 0.2 AND temperature <= 0.4:  -> Continental
if temperature <= 0.2:                        -> Subarctic
```

A magasság is korrigál: hegycsúcsok hidegebb klímazónába kerülhetnek, mint a környezetük.

### Évszak hatás modell

Minden klímazónához tartozik egy évszak-ciklus leírás:

| Klímazóna    | Tavasz hossza | Nyár hossza | Ősz hossza | Tél hossza | Téli hőmérséklet módosító |
|--------------|---------------|--------------|------------|-----------|--------------------------|
| Tropical     | 25%           | 50%          | 25%        | 0%        | -2 °C                   |
| Temperate    | 25%           | 25%          | 25%        | 25%       | -15 °C                  |
| Continental  | 20%           | 25%          | 20%        | 35%       | -25 °C                  |
| Subarctic    | 15%           | 20%          | 15%        | 50%       | -35 °C                  |

## Publikus metódusok

| Metódus                                                                                         | Visszatérés                  | Leírás                                        |
|-------------------------------------------------------------------------------------------------|----------------------------------|-------------------------------------------------|
| `CalculateClimateZones(const TArray<FBiomeCell>& BiomeMap, UMapGeneratorSettings* Settings)` | `bool`                           | Klímazónák kiszámítása                      |
| `GetClimateZoneAt(int32 X, int32 Y)`                                                            | `EClimateZone`                   | Klímazóna egy adott ponton                     |
| `GetTemperatureAt(FVector2D WorldPos, float TimeOfYear)`                                        | `float`                          | Hőmérséklet egy adott ponton és időpontban     |
| `GetClimateData()`                                                                               | `const TArray<EClimateZone>&`    | A teljes klímazóna térkép                   |
