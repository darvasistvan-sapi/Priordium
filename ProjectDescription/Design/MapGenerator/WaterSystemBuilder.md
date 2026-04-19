# UWaterSystemBuilder - Vízrendszer Építés

## Áttekintés

A `UWaterSystemBuilder` felelős a folyók, tavak és tenger elhelyezéséért az UE Water System plugin segítségével. A heightmap adatból kiszámítja a természetes vízelvezetési útvonalakat és ezek mentén Water Body actorokat hoz létre.

## Fájlok

| Fájl   | Útvonal                                                          |
|--------|------------------------------------------------------------------|
| Header | `Source/Priordium/MapGenerator/Public/WaterSystemBuilder.h`      |
| Source | `Source/Priordium/MapGenerator/Private/WaterSystemBuilder.cpp`   |

## Felelősségek

- Folyók útvonalának kiszámítása a heightmap gradiense alapján
- `AWaterBodyRiver` actorok létrehozása spline pontokkal
- `AWaterBodyLake` actorok létrehozása mélyedések helyén
- `AWaterBodyOcean` actor létrehozása a térkép szélén
- A víztestek hatásának visszajelzése a bióm térképre (River, Lake biómok)

## FWaterBodyDefinition Struktúra

| Mező           | Típus             | Leírás                       |
|----------------|-------------------|------------------------------|
| `WaterType`    | `EWaterBodyType`  | Típus: River, Lake, Ocean   |
| `SplinePoints` | `TArray<FVector>` | A víztest spline pontjai    |
| `Width`        | `float`           | A víztest szélessége         |
| `Depth`        | `float`           | A víztest mélysége          |

## Algoritmus

### 1. Folyó generálás

**Forráspontok kiválasztása:**
- Magas pontok keresése a heightmap-en (hegyek)
- A `RiverCount` számú legmagasabb pont kiválasztása (egymástól távol)

**Útvonal számítás (water flow simulation):**
```
Minden forrásponttól:
1. A jelenlegi cella szomszédjai közül a legalacsonyabb irányba lépés
2. A lépés ismétlése amíg:
   a) Eléri a tengerszintet (tenger/tó)
   b) Egy már meglévő folyóba torkollik
   c) Elakad (lokális minimum - itt tó keletkezik)
3. A bejárt pontok a folyó spline pontjai
```

**Szélesség számítás:**
- A folyó forrásánál keskeny (pl. 200 cm)
- Lefelé haladva egyre szélesedik, ahogy más ágak csatlakoznak
- A torkolatnál a legszélesebb

### 2. Tó generálás

- A heightmap lokális minimumainak keresése (mélyedések, ahová víz gyűlik)
- A `LakeCount` számú legnagyobb mélyedés kiválasztása
- A tó területe a mélyedés méretétől függ
- Spline pontok a tó partjának kontúrja mentén

### 3. Tenger generálás (ha `bGenerateOcean` igaz)

- Egyetlen `AWaterBodyOcean` actor
- A tengerszint (`SeaLevel`) alatti területek automatikusan víz alatt
- A térkép szélei felé a kontinens maszk miatt természetes partszakaszok

## Publikus metódusok

| Metódus                                                                                                                       | Visszatérés                        | Leírás                                     |
|-----------------------------------------------------------------------------------------------------------------------------------|-----------------------------------|-----------------------------------------------|
| `BuildWaterBodies(const TArray<float>& Heightmap, const TArray<FBiomeCell>& BiomeMap, UMapGeneratorSettings* Settings)`          | `bool`                            | Az összes víztest létrehozása                |
| `GetWaterBodies()`                                                                                                                | `const TArray<AWaterBody*>&`      | Vissza­ja a létrehozott Water Body actorokat |
| `GetRiverPaths()`                                                                                                                 | `const TArray<FWaterBodyDefinition>&` | Vissza­ja a folyó definíciókat            |
| `IsWaterAt(FVector2D WorldPos)`                                                                                                   | `bool`                            | Van-e víz az adott pozíción                  |
| `GetNearestWaterDistance(FVector2D WorldPos)`                                                                                     | `float`                           | A legközelebbi víz távolsága                 |

## Függőségek

- A Water plugin-t engedélyezni kell a .uproject fájlban
- Build.cs-ben: `"Water"`
