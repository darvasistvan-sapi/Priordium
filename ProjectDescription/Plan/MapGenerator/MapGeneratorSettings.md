# MapGeneratorSettings - Fejlesztési Terv

## Áttekintés

A `UMapGeneratorSettings` egy központi konfigurációs Data Asset, amely a térkép generálás összes paraméterét tárolja. Ez az első komponens, amelyet implementálni kell, mert minden más modul függősége.

## Célkitűzések

- Központosított konfiguráció biztosítása a térkép generáláshoz
- Editor-barát beállítási felület létrehozása kategóriákkal és tooltip-ekkel
- Determinisztikus generálás támogatása seed értékkel
- Könnyen bővíthető struktúra biztosítása új paraméterek hozzáadásához

## Függőségek

### Előfeltételek
- Unreal Engine 5.x projekt
- MapGenerator modul létrehozása a Source könyvtárban

### Külső függőségek
- Nincs (ez az alapvető komponens)

## MVP Fejlesztési Struktúra

A fejlesztés 3 MVP-ben valósul meg, mindegyik után automatizált tesztekkel és editor ellenőrzéssel.

---

## MVP 1: Modul Alapstruktúra és Alapvető Enumok

### Cél
Működő MapGenerator modul alapvető típusdefiníciókkal, amely fordul és tesztelhető.

### Implementációs Lépések

1. **Fájlstruktúra létrehozása**
   - `Source/Priordium/MapGenerator/Public/` könyvtár
   - `Source/Priordium/MapGenerator/Private/` könyvtár
   - `MapGenerator.Build.cs` fájl létrehozása

2. **Alapvető enumok definiálása**
   - `EBiomeType` enum létrehozása `MapGeneratorTypes.h`-ban
   - `EClimateZone` enum létrehozása
   - UENUM makrók alkalmazása

**Kód példa:**
```cpp
UENUM(BlueprintType)
enum class EBiomeType : uint8
{
    Ocean      UMETA(DisplayName = "Ocean"),
    Beach      UMETA(DisplayName = "Beach"),
    Plains     UMETA(DisplayName = "Plains"),
    Forest     UMETA(DisplayName = "Forest"),
    Hills      UMETA(DisplayName = "Hills"),
    Mountain   UMETA(DisplayName = "Mountain"),
    River      UMETA(DisplayName = "River"),
    Lake       UMETA(DisplayName = "Lake"),
    Swamp      UMETA(DisplayName = "Swamp")
};
```

### Automatizált Tesztek (MVP 1)

**Test_MapGeneratorModule_Compiles.cpp:**
- Modul sikeresen betöltődik
- MapGeneratorTypes.h includeable
- Enumok definiálva vannak

**Test_BiomeType.cpp:**
- EBiomeType minden értéke elérhető
- Blueprint-ben látható
- Reflection működik

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `BP_TestEnums` Blueprint létrehozása Actor szülőosztállyal, `EBiomeType` és `EClimateZone` típusú publikus változókkal a "Map Generator Tests" kategóriában
2. **[AI]** `RunTests.bat` futtatása – az összes `Priordium.MapGenerator.*` teszt zöld legyen
3. **[Manuális]** Content Browser → C++ Classes → Priordium56 → MapGenerator mappa látható
4. **[Manuális]** Output Log → Priordium56 modul betöltve

### Elfogadási Kritériumok (MVP 1)

- [ ] MapGenerator modul sikeresen fordul
- [ ] Compiler warning nincs
- [ ] EBiomeType és EClimateZone enumok Blueprint-ben elérhetők
- [ ] Automatizált tesztek sikeresen futnak (zöld)
- [ ] Editor újraindítás után modul betöltődik

---

## MVP 2: Konfigurációs Struktúrák

### Cél
Heightmap és Resource konfigurációs struktúrák, amelyek Blueprint-ben használhatók.

### Implementációs Lépések

1. **FHeightmapConfig struktúra**
   - Frequency, Amplitude, Octaves paraméterek
   - USTRUCT makró, BlueprintType
   - Dokumentációs kommentek

2. **FResourceSpawnRule struktúra előkészítése**
   - Alapvető mezők GameplayTag nélkül (később bővítjük)
   - AllowedBiomes TArray
   - Density, ClusterSize paraméterek

**Kód példa:**
```cpp
USTRUCT(BlueprintType)
struct FHeightmapConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heightmap")
    float Frequency = 0.01f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heightmap", meta = (ClampMin = "1", ClampMax = "10"))
    int32 Octaves = 6;

    // ... további mezők
};
```

### Automatizált Tesztek (MVP 2)

**Test_HeightmapConfig.cpp:**
- Struktúra default értékei helyesek
- Reflection működik
- Szerializáció/deszerializáció
- Blueprint változó létrehozható

**Test_ResourceSpawnRule.cpp:**
- Struktúra példányosítható
- TArray mezők működnek
- Alapértelmezett értékek érvényesek

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `BP_TestStructs` Blueprint létrehozása Actor szülőosztállyal, `FHeightmapConfig` és `FResourceSpawnRule` típusú publikus változókkal a "Map Generator Tests" kategóriában
2. **[AI]** `RunTests.bat` futtatása – `Priordium.MapGenerator.HeightmapConfig.*` és `Priordium.MapGenerator.ResourceSpawnRule.*` tesztek mind zöldek legyenek
3. **[Manuális]** `BP_TestStructs` megnyitása az editorban: a Details panelen az `FHeightmapConfig` és `FResourceSpawnRule` mezők szerkeszthetők legyenek

### Elfogadási Kritériumok (MVP 2)

- [ ] FHeightmapConfig és FResourceSpawnRule Blueprint-ben használhatók
- [ ] Struktúrák mezői szerkeszthetők az editorban
- [ ] Automatizált tesztek átmennek
- [ ] Szerializáció működik (mentés/betöltés)
- [ ] Compiler warning nincs

---

## MVP 3: MapGeneratorSettings Data Asset

### Cél
Teljes konfigurációs Data Asset, amely az editorban szerkeszthető és tesztelhető.

### Implementációs Lépések

1. **UMapGeneratorSettings osztály implementálása**
   - UDataAsset származtatás
   - Általános beállítások (Seed, MapSize, Resolution)
   - FHeightmapConfig típusú mező
   - Bióm paraméterek
   - Víz paraméterek
   - Erőforrás paraméterek (FResourceSpawnRule tömb)

2. **UPROPERTY kategórizálás**
   - "General" kategória
   - "Heightmap" kategória
   - "Biomes" kategória
   - "Water" kategória
   - "Resources" kategória
   - Tooltip-ek minden mezőhöz

3. **Validáció implementálása** (opcionális)
   - MapSizeX/Y > 0 ellenőrzés
   - Resolution 2^n + 1 formátum
   - SeaLevel [0, 1] tartományban

**Kód példa:**
```cpp
UCLASS(BlueprintType)
class PRIORDIUM_API UMapGeneratorSettings : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "General", meta = (ToolTip = "Random seed for deterministic generation"))
    int32 Seed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heightmap")
    FHeightmapConfig HeightmapConfig;

    // ... további mezők
};
```

### Automatizált Tesztek (MVP 3)

**Test_MapGeneratorSettings.cpp:**
- Data Asset létrehozható programmatikusan
- Alapértelmezett értékek helyesek
- Mentés/betöltés működik
- Validáció helyes (ha implementálva)

**Test_Settings_Serialization.cpp:**
- Asset mentése .uasset fájlba
- Betöltés után értékek megmaradnak
- Deep copy működik

### MVP Ellenőrzés (Editor)

> Az AI asszisztens az alábbi lépéseket automatikusan végrehajtja az unrealMCP-n keresztül.

1. **[AI]** `DA_DefaultMapSettings` Data Asset létrehozása a `/Game/MapGenerator/` mappában `UMapGeneratorSettings` típussal
2. **[AI]** `RunTests.bat` futtatása – az összes `Priordium.MapGenerator.*` teszt zöld legyen
3. **[Manuális]** `DA_DefaultMapSettings` megnyitása az editorban: General, Heightmap, Biomes, Water, Resources kategóriák láthatók, mezők szerkeszthetők
4. **[Manuális]** Seed = 12345, HeightmapConfig.Octaves = 8 beállítása → mentés → editor újraindítás → értékek megmaradtak

### Elfogadási Kritériumok (MVP 3)

- [ ] UMapGeneratorSettings Data Asset létrehozható az editorban
- [ ] Minden property szerkeszthető a Details panelban
- [ ] Kategóriák logikusan szervezettek
- [ ] Tooltip-ek informatívak
- [ ] Asset mentés/betöltés működik
- [ ] Automatizált tesztek zöldek
- [ ] Compiler warning nincs
- [ ] Legalább egy "DA_DefaultMapSettings" asset létezik a Content-ben

---

## Átfogó Tesztelési Követelmények

### Code Coverage Követelmények

**KÖTELEZŐ:** A tesztek code coverage-ének **90% feletti** kell lennie.

**Mérési módszer:**
```bash
RunUAT BuildCookRun -project=Priordium.uproject -test=MapGeneratorSettings -CodeCoverage
```

### Use Case Lefedettség

A MapGeneratorSettings modul az alábbi use case-t implementálja:

| Use Case ID       | Use Case Neve                            | Implementáló Komponens        | Teszt Osztály                    |
|-------------------|------------------------------------------|-------------------------------|----------------------------------|
| UC-MG-01          | Térkép generálás konfigurálása           | UMapGeneratorSettings         | Test_MapGeneratorSettings        |
| UC-MG-01-A        | Enum típusok használata                  | EBiomeType, EClimateZone      | Test_BiomeType, Test_ClimateZone |
| UC-MG-01-B        | Konfigurációs struktúrák használata      | FHeightmapConfig, stb.        | Test_HeightmapConfig             |
| UC-MG-01-C        | Data Asset mentése/betöltése             | UMapGeneratorSettings         | Test_Settings_Serialization      |

### Elfogadási Kritérium Teszteléshez

- [ ] **Code Coverage minimum 90% elérve**
- [ ] **UC-MG-01 use case teljesen tesztekkel lefedve**
- [ ] Minden MVP teszt suite zöld
- [ ] Coverage report generálva és elmentve

## Kockázatok és Kockázatcsökkentés

| Kockázat                            | Valószínűség | Hatás  | Csökkentési Stratégia                                          |
|-------------------------------------|--------------|--------|----------------------------------------------------------------|
| GameplayTag függőség hiányzik       | Közepes      | Közepes | GameplayTags plugin engedélyezése a projekt inicializáláskor  |
| Szerializációs problémák            | Alacsony     | Magas   | Alapos tesztelés mentés/betöltés után                         |
| Editor crash property szerkesztéskor | Alacsony    | Magas   | UPROPERTY meta specifier-ek helyes használata                 |

## Elfogadási Kritériumok (Teljes Modul)

**MVP 1 kritériumai:**
- [ ] MapGenerator modul sikeresen fordul
- [ ] Enumok Blueprint-ben elérhetők
- [ ] Automatizált tesztek átmennek

**MVP 2 kritériumai:**
- [ ] Konfigurációs struktúrák használhatók
- [ ] Blueprint-ben szerkeszthetők
- [ ] Szerializáció működik

**MVP 3 kritériumai:**
- [ ] UMapGeneratorSettings Data Asset létrehozható
- [ ] Minden tulajdonság szerkeszthető
- [ ] Asset mentés/betöltés működik
- [ ] Legalább egy példa asset létezik

**Általános:**
- [ ] Nincs compiler warning
- [ ] Kód követi az UE coding standards-et
- [ ] Minden MVP automatizált tesztjei zöldek

## Következő Lépések

A MapGeneratorSettings implementálása után a következő modul fejleszthető MVP-alapon:
1. **HeightmapGenerator** - A magasságtérkép generálás (5 MVP-re bontva)
