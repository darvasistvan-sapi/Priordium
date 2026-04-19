# UMapGeneratorSettings — Konfigurációs Data Asset

## Áttekintés

A `UMapGeneratorSettings` egy `UDataAsset` leszármazott, amely az összes térkép-generálási paramétert egyetlen helyen tárolja. A Content Browserből létrehozható (*Miscellaneous → Data Asset → MapGeneratorSettings*), és az `AMapGenerator` actor Details Panelén rendelhető hozzá. A paraméterek megváltoztatásához nem szükséges kódot módosítani.

## Fájlok

| Fájl   | Útvonal |
|--------|---------|
| Header | `Source/Priordium/MapGenerator/Public/UMapGeneratorSettings.h` |

---

## Általános paraméterek

### `Seed` — int32, alapérték: 0

A véletlenszám-generátor magja. Azonos seed azonos térképet eredményez bármikor, bármilyen gépen — ez teszi a generálást determinisztikussá.

- **0**: minden generálásnál véletlenszerű, eltérő térkép keletkezik
- **0 ≠ érték** (pl. 42): mindig ugyanaz a térkép — hasznos teszteléshez és megosztáshoz

---

### `MapSizeX` / `MapSizeY` — int32, alapérték: 512

Jelenleg csak validációra szolgál (> 0 ellenőrzés). A tényleges térkép méretet a `Resolution` és a `QuadSize` szorzata határozza meg. Jövőbeli tile-alapú rendszer esetén ezek a tile-ok számát fogják jelölni.

---

### `Resolution` — int32, alapérték: 513

A heightmap felbontása: `Resolution × Resolution` cellából áll a térkép. Ez a landscape vertex-ek száma is egyben.

- **Alacsony érték (pl. 65, 129)**: kevés részlet, simább terep, gyorsabb generálás — teszteléshez ideális
- **Magas érték (pl. 513, 1009)**: részletes, tagolt terep, lassabb generálás
- **Fontos**: a landscape generálás szempontjából a `(Resolution - 1)` értéknek a 255, 127, 63, 31, 15, 7 vagy 3 valamelyikével oszthatónak kell lennie a hatékony komponens-elrendezéshez. A 2ⁿ+1 formátumú értékek (pl. 513, 257) technikailag működnek, de a kód automatikusan a legmegfelelőbb komponensméretet választja.

---

### `QuadSize` — int32, alapérték: 100 (cm)

A szomszédos landscape vertex-ek közötti távolság centiméterben. Ez határozza meg a térkép fizikai méretét.

- **Alacsony érték (pl. 100 cm = 1 m/vertex)**: kis méretű, részletes táj — beltéri vagy kis léptékű jelenetek
- **Magas érték (pl. 1000 cm = 10 m/vertex)**: hatalmas nyílt világ — egy 513×513 felbontású térkép 512×1000 cm = ~5 km széles lesz

> **Példa**: Resolution=513, QuadSize=1000 → térkép mérete: 512 × 10 m = **5,12 km × 5,12 km**

---

## Heightmap paraméterek

### `HeightmapConfig` — FHeightmapConfig struktúra

A zajgenerálás részletes paraméterei. Lásd: [HeightmapGenerator.md](HeightmapGenerator.md).

---

## Kontinens maszk

### `bUseContinentMask` — bool, alapérték: true

Ha be van kapcsolva, a térkép szélei felé a magasság fokozatosan csökken a tengerszint alá. Ez azt eredményezi, hogy a térkép szigetszerű kontinenst alkot — a széleken tenger, a közepén szárazföld.

- **true**: természetes partvidékek, a térkép szélein víz
- **false**: a terep a szélekig egyformán folytatódik, nem lesz természetes partvonal

---

### `EdgeFalloffDistance` — float, alapérték: 0.3

A kontinens maszk lecsengési távolsága a térkép szélétől, a térképméret arányában (0.0–0.5).

- **Alacsony érték (pl. 0.1)**: csak a térkép legkülső 10%-a simul le — nagy, kiterjedt szárazföld, kis tengerpart
- **Magas érték (pl. 0.45)**: szinte az egész térkép lecseng — kis, izolált sziget sok vízzel körülvéve

---

## Bióm paraméterek

### `ClimateZone` — EClimateZone, alapérték: Temperate

Az egész térképre vonatkozó éghajlati zóna, amely meghatározza a biómok eloszlását.

- **Tropical**: meleg, párás — dzsungel, mocsár dominál
- **Temperate**: mérsékelt — vegyes erdők, síkságok
- **Continental**: szárazföldi — füves puszták, tűlevelű erdők
- **Subarctic**: hideg — tundra, ritkás növényzet

---

### `SeaLevel` — float, alapérték: 0.3

A tengerszint magassága normalizált [0.0–1.0] skálán. Az ennél alacsonyabb heightmap értékű cellák víznek számítanak.

- **Alacsony érték (pl. 0.1)**: kevés víz, főleg szárazföld
- **Magas érték (pl. 0.6)**: sok víz, csak a legmagasabb területek maradnak szárazon — archipelágó jelleg

---

### `MountainThreshold` — float, alapérték: 0.75

Az a normalizált magasság, amely felett egy cella Hegy biómnak számít.

- **Alacsony érték (pl. 0.5)**: már a közepes magasságú területek is hegyesnek számítanak — sok hegy
- **Magas érték (pl. 0.9)**: csak a legmagasabb csúcsok hegyek — ritka, drámai hegycsúcsok

---

## Víz paraméterek

### `bGenerateRivers` / `MaxRiverCount` — bool / int32, alapérték: true / 5

Vezérli a folyók generálását. A folyók a legmagasabb pontokból indulnak és a tengerszint felé folynak le.

- **MaxRiverCount = 0**: nincsenek folyók
- **MaxRiverCount nagy értéke**: sok folyó — ezek a magasabb területekből indulnak, így zsúfolt folyórendszer alakulhat ki

---

### `bGenerateLakes` / `MaxLakeCount` — bool / int32, alapérték: true / 10

Vezérli a tavak generálását. A tavak lokális mélypontokban (medencékben) helyezkednek el.

- **MaxLakeCount = 0**: nincsenek tavak
- **MaxLakeCount nagy értéke**: sok tó — sok lokális mélypontot kell találni, ami ritka lehet lapos terepen

---

## Erőforrás paraméterek

### `ResourceSpawnRules` — TArray\<FResourceSpawnRule\>

Az egyes erőforrástípusok elhelyezési szabályait tartalmazó tömb. Minden bejegyzés egy adott erőforráshoz (pl. fa, kő, érc) tartozó szabályrendszert ír le: melyik biómban, milyen magasságban, mekkora sűrűséggel és klaszter-mérettel jelenjen meg.

Üres tömb esetén nem kerülnek erőforrások a térképre — ez nem hiba, csak üres világ.

Részletes leírás: [ResourceDistributor.md](ResourceDistributor.md).
