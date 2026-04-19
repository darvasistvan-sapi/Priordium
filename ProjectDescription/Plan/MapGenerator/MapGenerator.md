# MapGenerator - Fejlesztési Terv

## Áttekintés

Az `AMapGenerator` az egyetlen top-level Actor, amely a generálási pipeline-t vezérli. Felelős a komponensek helyes sorrendben történő futtatásáért, az adattovábbításért és a folyamat állapotának kezeléséért.

## Célkitűzések

- Teljes generálási pipeline orchestrálása
- Komponensek közötti adatáramlás biztosítása
- Generálási folyamat állapot- és hibakezelése
- Progress tracking és user feedback
- Blueprint és Editor integráció
- Fejlesztői és designer-barát interfész

## Függőségek

### Előfeltételek
- **MapGeneratorSettings** modul implementálva
- **HeightmapGenerator** modul implementálva
- **BiomeManager** modul implementálva
- **LandscapeBuilder** modul implementálva
- **WaterSystemBuilder** modul implementálva
- **ClimateZoneManager** modul implementálva
- **ResourceDistributor** modul implementálva

### Külső függőségek
- Minden előző modul függősége (Landscape, Water, GameplayTags)

## MVP Fejlesztési Struktúra

## MVP 1: AMapGenerator Váz és Komponensek Inicializálása

### Cél
AMapGenerator actor osztály vázának létrehozása és összes komponens inicializálása.

### Implementációs Lépések

1. **AMapGenerator header** – AActor alapú osztály, komponens pointer-ek, Delegate-ek, GeneratorSettings property

2. **Komponensek létrehozása konstruktorban**
```cpp
AMapGenerator::AMapGenerator()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    HeightmapGenerator  = CreateDefaultSubobject<UHeightmapGenerator>(TEXT("HeightmapGenerator"));
    BiomeManager        = CreateDefaultSubobject<UBiomeManager>(TEXT("BiomeManager"));
    LandscapeBuilder    = CreateDefaultSubobject<ULandscapeBuilder>(TEXT("LandscapeBuilder"));
    WaterSystemBuilder  = CreateDefaultSubobject<UWaterSystemBuilder>(TEXT("WaterSystemBuilder"));
    ClimateZoneManager  = CreateDefaultSubobject<UClimateZoneManager>(TEXT("ClimateZoneManager"));
    ResourceDistributor = CreateDefaultSubobject<UResourceDistributor>(TEXT("ResourceDistributor"));
}
```

### Automatizált Tesztek (MVP 1)

**Test_MapGenerator_Structure.cpp:** Komponensek != nullptr, GetGenerationProgress() = 0.0 kezdetben

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `DA_MapGeneratorSettings` Data Asset létrehozása a `/Game/MapGenerator/` mappában
2. **[AI]** `AMapGenerator` actor spawolása a szintbe
3. **[AI]** `RunTests.bat` futtatása – `Priordium.MapGenerator.MapGenerator.*` tesztek zöldek legyenek
4. **[Manuális]** Details Panelen állítsd be a GeneratorSettings property-t a `DA_MapGeneratorSettings` assetre

### Elfogadási Kritériumok (MVP 1)

- [ ] AMapGenerator osztály fordul
- [ ] Komponensek létrejönnek konstruktorban
- [ ] Automatizált tesztek zöldek
- [ ] Level-ben elhelyezhető

---

## MVP 2: Generálási Pipeline Definiálása

### Cél
ExecuteGenerationStep() metódus implementálása a pipeline lépéseinek helyes sorrendben futtatásához.

### Implementációs Lépések

```cpp
bool AMapGenerator::ExecuteGenerationStep(int32 StepIndex)
{
    switch (StepIndex)
    {
        case 0: return HeightmapGenerator->Generate(GeneratorSettings);
        case 1: return BiomeManager->AssignBiomes(HeightmapGenerator->GetHeightmapData(), GeneratorSettings);
        case 2: return ClimateZoneManager->CalculateClimateZones(BiomeManager->GetBiomeMap(), GeneratorSettings);
        case 3: return WaterSystemBuilder->BuildWaterBodies(HeightmapGenerator->GetHeightmapData(), BiomeManager->GetBiomeMap(), GeneratorSettings);
        case 4: return LandscapeBuilder->BuildLandscape(HeightmapGenerator->GetHeightmapData(), BiomeManager->GetBiomeMap(), GeneratorSettings);
        case 5: return ResourceDistributor->DistributeResources(GeneratorSettings, BiomeManager, ClimateZoneManager, WaterSystemBuilder);
        default: return false;
    }
}
```

### Automatizált Tesztek (MVP 2)

**Test_PipelineSteps.cpp:** Minden step hívja a megfelelő komponenst, hibás index → false

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `RunTests.bat` futtatása – single step pipeline tesztek zöldek legyenek
2. **[Manuális]** PIE-ben futtasd a `MapGen.ExecuteSingleStep 0` console command-ot
3. **[Manuális]** Output Log-ban ellenőrizd: "Heightmap generation completed", "Biome assignment completed" log üzenetek

### Elfogadási Kritériumok (MVP 2)

- [ ] ExecuteGenerationStep() helyes sorrendben futtat
- [ ] Adattovábbítás komponensek között működik
- [ ] Automatizált tesztek zöldek
- [ ] Lépések egyesével futtathatók

---

## MVP 3: GenerateWorld() Orchestration és Hibakezelés

### Cél
GenerateWorld() teljes pipeline futtatása, progress tracking, delegate broadcast és hibakezelés.

### Implementációs Lépések

```cpp
void AMapGenerator::GenerateWorld()
{
    if (bIsGenerating || !GeneratorSettings) { OnGenerationError(TEXT("Invalid state")); return; }
    bIsGenerating = true;
    const int32 TotalSteps = 6;
    for (int32 Step = 0; Step < TotalSteps; ++Step)
    {
        CurrentProgress = (float)Step / (float)TotalSteps;
        if (!ExecuteGenerationStep(Step)) { bIsGenerating = false; return; }
    }
    CurrentProgress = 1.0f;
    bIsGenerating = false;
    OnGenerationSuccess();
}
```

### Automatizált Tesztek (MVP 3)

**Test_GenerateWorld.cpp:** Teljes pipeline lefut, Delegate-ek broadcast-olva, Null Settings → OnGenerationFailed

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `BP_TestMapGenPipeline` Blueprint létrehozása, BeginPlay event és Print String node-ok hozzáadása az OnGenerationCompleted és OnGenerationFailed callback-ekhez, spawolása a szintbe
2. **[AI]** `RunTests.bat` futtatása – pipeline callback tesztek zöldek legyenek
3. **[Manuális]** PIE-ben ellenőrizd: Landscape, Water, Resources megjelennek, "GenerationCompleted" kiíródik
4. **[Manuális]** Null Settings esetén "GenerationFailed" callback kiíródik

### Elfogadási Kritériumok (MVP 3)

- [ ] GenerateWorld() teljes pipeline-t futtat
- [ ] Hibakezelés működik
- [ ] Delegate-ek broadcast-olva
- [ ] Automatizált tesztek zöldek

---

## MVP 4: ClearGeneratedWorld() és BeginPlay Auto-Generate

### Cél
ClearGeneratedWorld() és BeginPlay() auto-generate funkció.

### Implementációs Lépések

```cpp
void AMapGenerator::ClearGeneratedWorld()
{
    ResourceDistributor->ClearResources();
    for (AWaterBody* W : WaterSystemBuilder->GetWaterBodies()) if (W) W->Destroy();
    if (ALandscapeProxy* L = LandscapeBuilder->GetGeneratedLandscape()) L->Destroy();
    bIsGenerating = false; CurrentStep = 0; CurrentProgress = 0.0f;
}

void AMapGenerator::BeginPlay()
{
    Super::BeginPlay();
    if (bAutoGenerateOnBeginPlay) GenerateWorld();
}
```

### Automatizált Tesztek (MVP 4)

**Test_ClearGeneratedWorld.cpp** és **Test_AutoGenerate.cpp**

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `RunTests.bat` futtatása – auto-generate tesztek zöldek legyenek
2. **[Manuális]** AMapGenerator actor Details Panelén állítsd be: bAutoGenerateOnBeginPlay = true
3. **[Manuális]** PIE-ben ellenőrizd: automatikus generálás elindul, világ létrejön
4. **[Manuális]** ClearGeneratedWorld() hívással ellenőrizd, hogy a világ eltűnik

### Elfogadási Kritériumok (MVP 4)

- [ ] ClearGeneratedWorld() törli az összes generált actort
- [ ] BeginPlay auto-generate működik
- [ ] Automatizált tesztek zöldek

---

## MVP 5: Blueprint Integráció és Validáció

### Cél
Blueprint-barát interfész, Editor Utility Widget integráció, ValidateSettings().

### Implementációs Lépések

1. **WBP_MapGeneratorUtility** – Generate/Clear gombok, Progress Bar, Step label
2. **ValidateSettings()** – Settings teljességi és értéktartomány ellenőrzés

### Automatizált Tesztek (MVP 5)

**Test_ValidateSettings.cpp:** Invalid settings → false, Valid settings → true

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `RunTests.bat` futtatása – generation progress tesztek zöldek legyenek
2. **[Manuális]** Hozd létre a `WBP_MapGeneratorUtility` Editor Utility Widget-et: "Generate World" gomb, "Clear World" gomb, Progress Bar (GetGenerationProgress() binding), Current Step label
3. **[Manuális]** PIE-ben teszteld: gomb kattintás → generálás elindul, progress bar frissül, completion jelzés megjelenik

### Elfogadási Kritériumok (MVP 5)

- [ ] Blueprint integráció működik
- [ ] Editor Utility Widget használható
- [ ] ValidateSettings() működik
- [ ] Automatizált tesztek zöldek

---

## Átfogó Tesztelési Követelmények

### Use Case Lefedettség

| Use Case ID       | Use Case Neve                            | Implementáló Metódus          | Teszt Osztály                  |
|-------------------|------------------------------------------|-------------------------------|--------------------------------|
| UC-MG-01          | Térkép generálás konfigurálása           | SetGeneratorSettings()        | Test_MapGenerator_Structure    |
| UC-MG-02          | Teljes világ generálása                  | GenerateWorld()               | Test_GenerateWorld             |
| UC-MG-02.1        | Magasságtérkép generálása                | ExecuteGenerationStep(0)      | Test_PipelineSteps             |
| UC-MG-02.2        | Biómok kiosztása                         | ExecuteGenerationStep(1)      | Test_PipelineSteps             |
| UC-MG-02.3        | Landscape építése                        | ExecuteGenerationStep(4)      | Test_PipelineSteps             |
| UC-MG-02.4        | Vízrendszer létrehozása                  | ExecuteGenerationStep(3)      | Test_PipelineSteps             |
| UC-MG-02.5        | Klímazónák meghatározása                 | ExecuteGenerationStep(2)      | Test_PipelineSteps             |
| UC-MG-02.6        | Erőforrások elhelyezése                  | ExecuteGenerationStep(5)      | Test_PipelineSteps             |
| UC-MG-03          | Generált világ törlése                   | ClearGeneratedWorld()         | Test_ClearGeneratedWorld       |
| UC-MG-04          | Generálás előrehaladásának lekérdezése   | GetGenerationProgress()       | Test_GenerateWorld             |

### Code Coverage Céleloszlás

| Komponens/Modul                  | Minimum Coverage | Cél Coverage | Prioritás |
|----------------------------------|------------------|--------------|-----------|
| Konstruktor                      | 100%             | 100%         | Kritikus  |
| GenerateWorld()                  | 95%              | 100%         | Kritikus  |
| ExecuteGenerationStep()          | 100%             | 100%         | Kritikus  |
| ClearGeneratedWorld()            | 100%             | 100%         | Kritikus  |
| ValidateSettings()               | 90%              | 100%         | Magas     |
| BeginPlay()                      | 100%             | 100%         | Magas     |
| **Teljes modul**                 | **90%**          | **95%**      | Kritikus  |

## Kockázatok és Kockázatcsökkentés

| Kockázat                                  | Valószínűség | Hatás    | Csökkentési Stratégia                                              |
|-------------------------------------------|--------------|----------|--------------------------------------------------------------------|
| Pipeline lépések közötti adatátvitel hibák | Közepes     | Magas    | Részletes logging, validáció lépések között                       |
| Komponens inicializálás hiba              | Alacsony     | Kritikus | Nullptr check-ek, ValidateSettings() hívása elején               |
| Teljesítmény probléma nagy térképeknél    | Közepes      | Magas    | Progress tracking, batch spawn optimalizálás                      |
| Editor crash generálás közben             | Alacsony     | Kritikus | Hibakezelés minden ExecuteGenerationStep() lépésben               |

## Elfogadási Kritériumok (Teljes Modul)

- [ ] AMapGenerator actor létrehozható és placeable
- [ ] Minden komponens (6 db) inicializálva konstruktorban
- [ ] GenerateWorld() sikeresen futtatja a teljes 6-lépéses pipeline-t
- [ ] Adattovábbítás működik minden komponens között
- [ ] Delegate-ek működnek (Completed, Failed, StepCompleted)
- [ ] ClearGeneratedWorld() törli az összes generált actort
- [ ] bAutoGenerateOnBeginPlay működik PIE-ben
- [ ] Összes automatizált teszt zöld

## Következő Lépések

1. **Teljes MapGenerator rendszer integráció** – mind a 9 modul összehangolt működésének tesztelése
2. **Save/Load rendszer** – generált világok mentése/betöltése
3. **Editor tooling** – Custom editor panel bővítése
