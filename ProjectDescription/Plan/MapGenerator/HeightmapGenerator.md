# HeightmapGenerator - Fejlesztési Terv

## Áttekintés

A `UHeightmapGenerator` felelős a procedurális magasságtérkép generálásáért többrétegű Perlin/Simplex zaj algoritmussal. Ez az első aktív generáló komponens a pipeline-ban, amely a terep alapját képezi.

## Célkitűzések

- Természetes megjelenésű terep generálása fraktál zaj algoritmussal
- Kontinens maszk implementálása a térképszéli partszakaszok kialakításához
- Opcionális hidraulikus erózió szimuláció az élethű völgyek létrehozásához
- Determinisztikus generálás biztosítása seed alapján
- Optimalizált teljesítmény nagy felbontású térképekhez

## Függőségek

### Előfeltételek
- **MapGeneratorSettings** modul implementálva
- `FHeightmapConfig` struktúra definiálva

### Külső függőségek
- SimplexNoise vagy FastNoise library (C++ implementáció)
- UE CoreUObject, Engine modulok

## MVP Fejlesztési Struktúra

A fejlesztés 5 MVP-ben valósul meg, mindegyik után automatizált tesztekkel és editor ellenőrzéssel.

---

## MVP 1: Noise Library Integráció és Wrapper

### Cél
Működő noise library integráció, amely determinisztikus 2D zajt generál.

### Implementációs Lépések

1. **FastNoise library hozzáadása**
   - ThirdParty/FastNoise mappa létrehozása
   - FastNoise forrás fájlok másolása
   - License fájl hozzáadása

2. **Build.cs frissítése**
```csharp
PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "../../ThirdParty/FastNoise"));
PublicDefinitions.Add("WITH_FASTNOISE=1");
```

3. **NoiseWrapper osztály létrehozása**
   - SimplexNoise2D static függvény
   - Seed kezelés
   - UE típusokhoz konverzió (FVector2D → float koordináták)

**Kód példa:**
```cpp
class NoiseWrapper
{
public:
    static float SimplexNoise2D(float X, float Y);
    static void SetSeed(int32 Seed);
};
```

### Automatizált Tesztek (MVP 1)

**Test_NoiseWrapper.cpp:**
- SimplexNoise2D visszaad értéket [-1, 1] tartományban
- Ugyanaz a bemenet ugyanazt az outputot adja (determinizmus)
- Seed változtatása megváltoztatja az outputot
- Nagy X,Y értékek nem crashelnek

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `RunTests.bat` futtatása – `Priordium.MapGenerator.HeightmapGenerator.*` tesztek zöldek legyenek
2. **[Manuális]** Az Editor konzolban futtasd: `MapGen.TestNoise`
3. **[Manuális]** Output Log-ban ellenőrizd: Noise érték -1 és 1 között van
4. **[Manuális]** Többszöri futtatással ellenőrizd a determinizmust (ugyanaz a seed → ugyanaz az érték)

### Elfogadási Kritériumok (MVP 1)

- [ ] FastNoise library sikeresen fordul
- [ ] NoiseWrapper::SimplexNoise2D meghívható
- [ ] Automatizált tesztek átmennek
- [ ] Console command működik
- [ ] Compiler warning nincs

---

## MVP 2: HeightmapGenerator Komponens Alapja

### Cél
Működő UHeightmapGenerator komponens, amely tárol heightmap adatot és lekérdezhető.

### Implementációs Lépések

1. **UHeightmapGenerator osztály váz**
   - UActorComponent származtatás
   - `TArray<float> HeightmapData` privát mező
   - `FIntPoint Resolution` privát mező
   - Public interfész: GetHeightmapData(), GetHeightAt(X, Y)

2. **Inicializáció metódus**
```cpp
void UHeightmapGenerator::InitializeHeightmap(int32 SizeX, int32 SizeY)
{
    Resolution = FIntPoint(SizeX, SizeY);
    HeightmapData.SetNum(SizeX * SizeY);
    
    // Fill with default 0.5 height
    for (int32 i = 0; i < HeightmapData.Num(); ++i)
    {
        HeightmapData[i] = 0.5f;
    }
}
```

3. **GetHeightAt implementálás**
   - Bounds checking
   - Index számítás: `Index = Y * Resolution.X + X`
   - Érték visszaadása

### Automatizált Tesztek (MVP 2)

**Test_HeightmapGenerator_Storage.cpp:**
- Komponens létrehozható
- InitializeHeightmap meghívása után HeightmapData helyes méretű
- GetHeightAt bounds checking működik (érvénytelen X,Y → 0.0f return)
- GetHeightAt helyes értéket ad vissza érvényes koordinátákra

**Test_HeightmapGenerator_Lifecycle.cpp:**
- Komponens Actor-hoz adható
- BeginPlay után is működik
- Többszöri inicializálás nem crashel

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `BP_TestHeightmapGenerator` Actor Blueprint létrehozása, BeginPlay event hozzáadása, Print String node-ok a logikához
2. **[AI]** `BP_TestHeightmapGenerator` spawolása a szintbe
3. **[AI]** `RunTests.bat` futtatása – tesztek zöldek legyenek
4. **[Manuális]** PIE-ben ellenőrizd: "Height: X.XX" kiírás megjelenik a képernyőn, Output Log-ban nincs hiba

### Elfogadási Kritériumok (MVP 2)

- [ ] UHeightmapGenerator komponens létrehozható
- [ ] InitializeHeightmap működik
- [ ] GetHeightAt helyes értéket ad
- [ ] Automatizált tesztek zöldek
- [ ] Blueprint-ben használható
- [ ] PIE-ban működik

---

## MVP 3: FBM Noise Generálás

### Cél
Fractal Brownian Motion alapú heightmap generálás, amely természetes terep mintát ad.

### Implementációs Lépések

1. **GenerateNoiseValue metódus**
   - FBM algoritmus implementálása
   - Oktáv iteráció
   - Frequency, Amplitude, Persistence, Lacunarity paraméterek
   - Seed offset

2. **Generate metódus implementálása**
```cpp
bool UHeightmapGenerator::Generate(UMapGeneratorSettings* Settings)
{
    if (!Settings) return false;
    
    const int32 Res = Settings->LandscapeResolution;
    InitializeHeightmap(Res, Res);
    
    const FHeightmapConfig& Config = Settings->HeightmapConfig;
    
    for (int32 Y = 0; Y < Resolution.Y; ++Y)
    {
        for (int32 X = 0; X < Resolution.X; ++X)
        {
            float Height = GenerateNoiseValue(X, Y, Config, Settings->Seed);
            int32 Index = Y * Resolution.X + X;
            HeightmapData[Index] = Height;
        }
    }
    
    return true;
}
```

3. **Normalizálás biztosítása**
   - Output [0, 1] tartományban

### Automatizált Tesztek (MVP 3)

**Test_FBM_Generation.cpp:**
- Generate() metódus sikeres (return true)
- Heightmap minden értéke [0, 1] tartományban
- Ugyanaz a Seed ugyanazt a heightmap-et adja
- Különböző Seed eltérő heightmap-et ad
- Octaves paraméter változtatása befolyásolja az eredményt

**Test_FBM_Performance.cpp:**
- 1009x1009 felbontás generálása < 5 másodperc alatt
- Memória használat elfogadható

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `DA_TestMapSettings` Data Asset létrehozása (Seed=42, LandscapeResolution=257, HeightmapConfig.Octaves=6)
2. **[AI]** `BP_TestHeightmapGen` Blueprint létrehozása BeginPlay → Print String "Generation Success" node-dal, spawolása a szintbe
3. **[AI]** `RunTests.bat` futtatása – tesztek zöldek legyenek
4. **[Manuális]** PIE-ben ellenőrizd: "Generation Success" log megjelenik, Height érték 0 és 1 között, nincs warning/error

### Elfogadási Kritériumok (MVP 3)

- [ ] Generate() metódus működik
- [ ] FBM algoritmus helyes zajt generál
- [ ] Determinizmus biztosított (seed)
- [ ] Normalizálás [0, 1] tartományra
- [ ] Automatizált tesztek zöldek
- [ ] Blueprint-ben tesztelhető
- [ ] Teljesítmény célok teljesülnek

---

## MVP 4: Kontinens Maszk

### Cél
Térkép szélén fokozatos magasság-csökkenés, partszakaszok kialakítása.

### Implementációs Lépések

1. **ApplyContinentMask metódus**
   - Távolságszámítás a térképszéltől
   - Smoothstep falloff
   - Maszk alkalmazása heightmap-re

2. **Generate metódus bővítése**
```cpp
bool UHeightmapGenerator::Generate(UMapGeneratorSettings* Settings)
{
    // ... FBM generálás (MVP 3) ...
    
    ApplyContinentMask(Settings);
    
    return true;
}
```

3. **Paraméterezhetőség**
   - EdgeFalloffDistance beállítható (pl. MapSizeX %-a)

### Automatizált Tesztek (MVP 4)

**Test_ContinentMask.cpp:**
- Térkép sarki értékek (0,0) közel 0-hoz
- Térkép középpontja nem módosul jelentősen
- Smooth átmenet a széltől a középig
- Maszk alkalmazása után értékek továbbra is [0, 1] tartományban

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `RunTests.bat` futtatása – `Priordium.MapGenerator.HeightmapGenerator.EdgeFalloff.*` tesztek zöldek legyenek
2. **[Manuális]** DA_TestMapSettings-ben állítsd be: Seed=42, LandscapeResolution=257
3. **[Manuális]** PIE-ben futtasd a `MapGen.CheckEdge` console command-ot
4. **[Manuális]** Output Log-ban ellenőrizd: széli pontok magassága ~0, középső pontok magasabbak

### Elfogadási Kritériumok (MVP 4)

- [ ] Kontinens maszk működik
- [ ] Szélek magassága ~0
- [ ] Középső terület kevésbé érintett
- [ ] Smooth átmenet
- [ ] Automatizált tesztek zöldek
- [ ] Vizuálisan ellenőrizhető

---

## MVP 5: Hidraulikus Erózió (Opcionális Feature)

### Cél
Realisztikus völgyek és folyómedrek képzése particle-based eróziós szimulációval.

### Implementációs Lépések

1. **ApplyHydraulicErosion metódus**
   - Particle (esőcsepp) szimuláció
   - Gradiens követés
   - Sediment felvétel és lerakás

2. **Generate metódus kiegészítése**
```cpp
if (Settings->HeightmapConfig.bEnableErosion)
{
    ApplyHydraulicErosion(Settings->HeightmapConfig);
}
```

3. **Optimalizálás**
   - Iterációszám limitálása
   - Early exit feltételek
   - Teljesítmény mérés

### Automatizált Tesztek (MVP 5)

**Test_HydraulicErosion.cpp:**
- Erózióval generált heightmap értékei [0, 1] tartományban
- Iterációszám növelése nem crashel
- Teljesítmény: 1009x1009 + 50k iteráció < 10 másodperc

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `RunTests.bat` futtatása – `Priordium.MapGenerator.HeightmapGenerator.Erosion.*` tesztek zöldek legyenek
2. **[Manuális]** DA_TestMapSettings-ben állítsd be: bEnableErosion=true, ErosionIterations=5000
3. **[Manuális]** PIE-ben ellenőrizd: völgyek láthatók, természetesebb terep mintázat, folyómedreket emlékeztető formák
4. **[Manuális]** Hasonlítsd össze bEnableErosion=false verzióval (ugyanaz a seed)

### Elfogadási Kritériumok (MVP 5)

- [ ] Hidraulikus erózió implementálva
- [ ] bEnableErosion flag működik
- [ ] Völgyek és medrképződmények láthatók
- [ ] Automatizált tesztek zöldek
- [ ] Teljesítmény elfogadható
- [ ] Vizuálisan természetesebb terep

---

## Átfogó Tesztelési Követelmények

### Code Coverage Követelmények

**KÖTELEZŐ:** A tesztek code coverage-ének **90% feletti** kell lennie.

### Use Case Lefedettség

| Use Case ID       | Use Case Neve                            | Implementáló Metódus          | Teszt Osztály                    |
|-------------------|------------------------------------------|-------------------------------|----------------------------------|
| UC-MG-02.1        | Magasságtérkép generálása                | Generate()                    | Test_FBM_Generation              |
| UC-MG-02.1-A      | Noise library használata                 | NoiseWrapper::SimplexNoise2D  | Test_NoiseWrapper                |
| UC-MG-02.1-B      | FBM algoritmus futtatása                 | GenerateNoiseValue()          | Test_FBM_Generation              |
| UC-MG-02.1-C      | Kontinens maszk alkalmazása              | ApplyContinentMask()          | Test_ContinentMask               |
| UC-MG-02.1-D      | Hidraulikus erózió (opcionális)          | ApplyHydraulicErosion()       | Test_HydraulicErosion            |

### Code Coverage Céleloszlás Komponensenként

| Komponens/Modul                  | Minimum Coverage | Cél Coverage | Prioritás |
|----------------------------------|------------------|--------------|-----------|
| NoiseWrapper                     | 100%             | 100%         | Kritikus  |
| Generate()                       | 95%              | 100%         | Kritikus  |
| GenerateNoiseValue() (FBM)       | 100%             | 100%         | Kritikus  |
| ApplyContinentMask()             | 100%             | 100%         | Kritikus  |
| ApplyHydraulicErosion()          | 85%              | 95%          | Közepes   |
| InitializeHeightmap()            | 100%             | 100%         | Magas     |
| GetHeightAt()                    | 100%             | 100%         | Magas     |
| **Teljes modul**                 | **90%**          | **96%**      | Kritikus  |

## Elfogadási Kritériumok (Teljes Modul)

- [ ] **MVP 1 teljesítve:** Noise library integráció működik
- [ ] **MVP 2 teljesítve:** HeightmapGenerator komponens alapstruktúrája kész
- [ ] **MVP 3 teljesítve:** FBM generálás működik, determinisztikus
- [ ] **MVP 4 teljesítve:** Kontinens maszk alkalmazza a falloff-ot
- [ ] **MVP 5 teljesítve (opcionális):** Hidraulikus erózió működik
- [ ] Heightmap mindig [0, 1] tartományban normalizált
- [ ] Összes automatizált teszt zöld

## Következő Lépések

A HeightmapGenerator implementálása után a következő modulok fejleszthetők:
1. **BiomeManager** - Használja a heightmap adatot a biómok meghatározásához
2. **WaterSystemBuilder** - Használja a heightmap gradienst a folyók útvonalához
3. **LandscapeBuilder** - Konvertálja a heightmap-et UE Landscape formátumra
