# LandscapeBuilder - Fejlesztési Terv

## Áttekintés

A `ULandscapeBuilder` felelős az Unreal Engine Landscape rendszer használatával a tényleges terepactor létrehozásáért. A heightmap adatot és a bióm térképet Landscape magassági adattá és material réteg súlyokká konvertálja.

## Célkitűzések

- Landscape actor létrehozása és konfigurálása a generált heightmap alapján
- Heightmap konvertálás UE Landscape formátumra (uint16)
- Material rétegek (Layer Info) beállítása biómok alapján
- Bióm blend súlyok alkalmazása a paint layer-ekre
- Landscape komponensek és szekciók helyes konfigurálása
- Optimalizált teljesítmény és memória használat

## Függőségek

### Előfeltételek
- **MapGeneratorSettings** modul implementálva
- **HeightmapGenerator** modul implementálva
- **BiomeManager** modul implementálva
- Landscape plugin engedélyezve (core UE feature)

### Külső függőségek
- UE Landscape modul (`Landscape` a Build.cs-ben)
- `ALandscapeProxy`, `ULandscapeInfo` osztályok
- `LandscapeEditorUtils` Edit/PIE különböző kezeléshez
- Landscape Material asset a bióm rétegekkel

## MVP Fejlesztési Struktúra

## MVP 1: Landscape Plugin Konfiguráció és ULandscapeBuilder Váz

### Cél
Landscape modul engedélyezése, Build.cs konfiguráció, valamint ULandscapeBuilder komponens vázának létrehozása.

### Implementációs Lépések

1. **Build.cs módosítása**
```csharp
PublicDependencyModuleNames.AddRange(new string[] { 
    "Core", "CoreUObject", "Engine", "Landscape"
});
if (Target.bBuildEditor)
{
    PrivateDependencyModuleNames.Add("LandscapeEditor");
}
```

2. **ULandscapeBuilder header** – ActorComponent, GeneratedLandscape pointer, LayerInfoAssets TMap, GetGeneratedLandscape() getter

### Automatizált Tesztek (MVP 1)

**Test_LandscapePlugin.cpp** és **Test_LandscapeBuilder_Structure.cpp:**
- Landscape modul elérhető
- ALandscapeProxy példányosítható
- ULandscapeBuilder komponens létrehozható

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `BP_TestLandscapeBuilder` Actor Blueprint létrehozása ULandscapeBuilder komponenssel, spawolása a szintbe
2. **[AI]** `RunTests.bat` futtatása – `Priordium.MapGenerator.LandscapeBuilder.*` tesztek zöldek legyenek
3. **[Manuális]** PIE-ben ellenőrizd a Landscape plugin elérhetőségét (nincs error az Output Log-ban)

### Elfogadási Kritériumok (MVP 1)

- [ ] Landscape modul engedélyezve Build.cs-ben
- [ ] Projekt fordul
- [ ] ULandscapeBuilder osztály fordul
- [ ] ALandscapeProxy használható
- [ ] Automatizált tesztek zöldek
- [ ] Manuális Landscape létrehozható

---

## MVP 2: Heightmap Konvertálás uint16-ra

### Cél
ConvertHeightmapToUint16() metódus implementálása float [0,1] tartományból uint16 [0, 65535] tartományba konvertáláshoz.

### Implementációs Lépések

```cpp
TArray<uint16> ULandscapeBuilder::ConvertHeightmapToUint16(const TArray<float>& Heightmap, UMapGeneratorSettings* Settings)
{
    TArray<uint16> Uint16Heightmap;
    Uint16Heightmap.SetNum(Heightmap.Num());
    for (int32 i = 0; i < Heightmap.Num(); ++i)
    {
        float H = FMath::Clamp(Heightmap[i], 0.0f, 1.0f);
        Uint16Heightmap[i] = static_cast<uint16>(H * 65535.0f);
    }
    return Uint16Heightmap;
}
```

### Automatizált Tesztek (MVP 2)

**Test_HeightmapConversion.cpp:** Float 0.0→0, Float 1.0→65535, Float 0.5→~32767, clamping működik

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `RunTests.bat` futtatása – heightmap conversion tesztek zöldek legyenek
2. **[Manuális]** PIE-ben futtasd a `MapGen.TestHeightmapConversion` console command-ot
3. **[Manuális]** Output Log-ban ellenőrizd a konvertált értékeket (0–65535 tartomány)

### Elfogadási Kritériumok (MVP 2)

- [ ] ConvertHeightmapToUint16() működik
- [ ] Konverzió helyes (0.0→0, 1.0→65535)
- [ ] Clamping működik
- [ ] Automatizált tesztek zöldek

---

## MVP 3: Layer Info és Material Előkészítés

### Cél
ULandscapeLayerInfoObject asset-ek létrehozása biómonként és Landscape Material előkészítése.

### Implementációs Lépések

1. Layer Info Object-ek manuális létrehozása: LI_Ocean, LI_Beach, LI_Plains, LI_Forest, LI_Mountain, LI_Swamp, LI_Hills
2. Landscape Material (M_Landscape) létrehozása Layer Blend node-okkal
3. LoadLayerInfoAssets() metódus implementálás

### Automatizált Tesztek (MVP 3)

**Test_LayerInfo_Loading.cpp:** LayerInfoAssets nem üres, LoadLayerInfoAssets() betölti az object-eket

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `RunTests.bat` futtatása – layer info tesztek zöldek legyenek
2. **[Manuális]** Hozd létre a Layer Info Object-eket a Content Browser-ben: LI_Grass, LI_ForestFloor, LI_DryGrass, LI_Rock, LI_Sand, LI_Mud, LI_RiverBank, LI_LakeShore
3. **[Manuális]** Hozz létre egy Landscape Material-t (M_Landscape) Layer Blend node-okkal
4. **[Manuális]** PIE-ben ellenőrizd az Output Log-ban a LoadLayerInfoAssets() log üzeneteket

### Elfogadási Kritériumok (MVP 3)

- [ ] Layer Info Object-ek léteznek minden biómhoz
- [ ] Landscape Material létezik
- [ ] LoadLayerInfoAssets() működik
- [ ] Automatizált tesztek zöldek

---

## MVP 4: Landscape Létrehozása (Import)

### Cél
Landscape actor spawn-olása és heightmap import FLandscapeImportLayerInfo API-val.

### Implementációs Lépések

1. CreateLandscape() – ALandscapeProxy spawn, FTransform, Material beállítás
2. ImportHeightmap() – FLandscapeImportLayerInfo, komponens konfiguráció
3. ConfigureLandscapeTransform() – QuadSize skála, középre pozicionálás
4. BuildLandscape() – a fentiek összefűzése

### Automatizált Tesztek (MVP 4)

**Test_LandscapeCreation.cpp:** BuildLandscape() után GeneratedLandscape != nullptr, Landscape actor létezik a világban, Transform helyes

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `RunTests.bat` futtatása – landscape build tesztek zöldek legyenek
2. **[Manuális]** PIE-ben futtasd: HeightmapGenerator.Generate() → BuildLandscape()
3. **[Manuális]** World Outliner-ben ellenőrizd, hogy a Landscape actor létezik
4. **[Manuális]** Ellenőrizd vizuálisan: hegyek és völgyek megfelelő magasságban jelennek meg

### Elfogadási Kritériumok (MVP 4)

- [ ] CreateLandscape() Landscape actort hoz létre
- [ ] ImportHeightmap() heightmap adatokat importál
- [ ] Landscape megjelenik a világban
- [ ] Transform helyes (skála, pozíció)
- [ ] Automatizált tesztek zöldek

---

## MVP 5: Material Layer-ek Alkalmazása (Paint)

### Cél
ApplyMaterialLayers() implementálása BiomeBlendWeights alapján weight map-ek festésével.

### Implementációs Lépések

1. CalculateWeightMaps() – BiomeBlendWeights → TArray<uint8> [0, 255]
2. ApplyMaterialLayers() – LandscapeEditorUtils, weight painting
3. BuildLandscape() finalizálás – ApplyMaterialLayers() hívása

### Automatizált Tesztek (MVP 5)

**Test_MaterialLayers.cpp:** Weight map-ek [0, 255] tartományban, minden cellához súlyok összege ~255

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `RunTests.bat` futtatása – paint layer tesztek zöldek legyenek
2. **[Manuális]** PIE-ben futtasd a teljes pipeline-t: HeightmapGenerator → BiomeManager → BuildLandscape
3. **[Manuális]** Landscape Mode-ban ellenőrizd: Layer-ek láthatók (Ocean, Beach, Plains, Forest, stb.), bióm határok mentén smooth átmenetek

### Elfogadási Kritériumok (MVP 5)

- [ ] CalculateWeightMaps() weight map-eket generál
- [ ] ApplyMaterialLayers() weight map-eket alkalmazza
- [ ] Material rétegek láthatók a Landscape-en
- [ ] Bióm átmenetek simák
- [ ] Automatizált tesztek zöldek

---

## Átfogó Tesztelési Követelmények

### Use Case Lefedettség

| Use Case ID       | Use Case Neve                            | Implementáló Metódus          | Teszt Osztály                    |
|-------------------|------------------------------------------|-------------------------------|----------------------------------|
| UC-MG-02.3        | Landscape építése                        | BuildLandscape()              | Test_LandscapeCreation           |
| UC-MG-02.3-A      | Heightmap konvertálás uint16-ra          | ConvertHeightmapToUint16()    | Test_HeightmapConversion         |
| UC-MG-02.3-B      | Landscape actor létrehozása              | CreateLandscape()             | Test_LandscapeCreation           |
| UC-MG-02.3-C      | Heightmap importálása                    | ImportHeightmap()             | Test_LandscapeCreation           |
| UC-MG-02.3-D      | Material rétegek alkalmazása             | ApplyMaterialLayers()         | Test_MaterialLayers              |
| UC-MG-02.3-E      | Transform konfiguráció                   | ConfigureLandscapeTransform() | Test_LandscapeTransform          |

### Code Coverage Céleloszlás

| Komponens/Modul                  | Minimum Coverage | Cél Coverage | Prioritás |
|----------------------------------|------------------|--------------|-----------|
| BuildLandscape()                 | 95%              | 100%         | Kritikus  |
| ConvertHeightmapToUint16()       | 100%             | 100%         | Kritikus  |
| CreateLandscape()                | 95%              | 100%         | Kritikus  |
| ApplyMaterialLayers()            | 90%              | 100%         | Magas     |
| CalculateWeightMaps()            | 100%             | 100%         | Magas     |
| ConfigureLandscapeTransform()    | 100%             | 100%         | Magas     |
| **Teljes modul**                 | **90%**          | **96%**      | Kritikus  |

## Kockázatok és Kockázatcsökkentés

| Kockázat                                   | Valószínűség | Hatás | Csökkentési Stratégia                                      |
|--------------------------------------------|--------------|-------|------------------------------------------------------------|
| Landscape API változás UE verziók között   | Közepes      | Magas  | UE verzió specifikus dokumentáció tanulmányozása           |
| Weight map alkalmazás bonyolult            | Közepes      | Magas  | LandscapeEditorUtils használata, step-by-step debug       |
| Landscape import hibák                     | Közepes      | Magas  | Részletes hibakezelés, paraméter validálás                 |
| Material performance probléma              | Alacsony     | Közepes| Material optimization, LOD használata                     |

## Elfogadási Kritériumok (Teljes Modul)

- [ ] **MVP 1 teljesítve:** Landscape plugin engedélyezve, ULandscapeBuilder váz kész
- [ ] **MVP 2 teljesítve:** Heightmap konvertálás uint16-ra működik
- [ ] **MVP 3 teljesítve:** Layer Info Object-ek és Material előkészítve
- [ ] **MVP 4 teljesítve:** Landscape létrehozása és heightmap import működik
- [ ] **MVP 5 teljesítve:** Material layer-ek alkalmazva, bióm átmenetek simák
- [ ] Összes automatizált teszt zöld

## Következő Lépések

1. **WorldGenerator** – orchestrálja a teljes pipeline-t
2. **WaterSystemBuilder integráció** – Landscape és Water Body együttműködés
3. **ResourceDistributor** – Landscape felületére erőforrás elhelyezés
