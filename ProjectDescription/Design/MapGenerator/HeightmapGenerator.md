# UHeightmapGenerator — Magasságtérkép Generálás

## Áttekintés

A `UHeightmapGenerator` felelős a térkép magassági adatainak procedurális előállításáért. Fractal Brownian Motion (FBM) algoritmust használ, amely több zajréteget összegez természetes megjelenésű terep kialakítása érdekében. Opcionálisan hidraulikus erózió szimulációt is futtat a valósabb folyóvölgyek és lejtők érdekében.

## Fájlok

| Fájl   | Útvonal |
|--------|---------|
| Header | `Source/Priordium/MapGenerator/Public/UHeightmapGenerator.h` |
| Source | `Source/Priordium/MapGenerator/Private/UHeightmapGenerator.cpp` |

---

## FHeightmapConfig — Zajgenerálás paraméterei

### `Frequency` — float, alapérték: 0.01

Meghatározza, hogy milyen nagyok az alapvető terep-formák. A zaj "hullámhossza" fordítottan arányos a frekvenciával.

- **Alacsony érték (pl. 0.002–0.005)**: nagy léptékű, sima formák — kontinentális hegyvonulatok, hosszú völgyek. Kisebb térképen (pl. 513 felbontás) ez szinte sík, egyetlen óriási domb lehet.
- **Közepes érték (pl. 0.01–0.03)**: közepes terep-formák — jól látható dombok és völgyek, kellemes játéktér-méret
- **Magas érték (pl. 0.05–0.1)**: apró, sűrű dudorok — kaotikus, érdes terep, nincs nagy összefüggő síkság

> **Praktikus tanács**: 8000 felbontású térképen a 0.05-ös frekvencia jó eredményt ad. 513-as felbontáson ugyanez a frekvencia zsúfolt, apró dombokat eredményez.

---

### `Octaves` — int32, alapérték: 6

A zajrétegek száma. Minden egymást követő réteg (`octave`) kétszer akkora frekvenciájú és feleakkora amplitúdójú az előzőnél (alapértelmezett Lacunarity és Persistence esetén). Az összes réteg összege adja a végső magassági értéket.

- **1 octave**: teljesen sima, egyetlen hullámzó felület — nincs részlet
- **2–3 octave**: nagy formák + némi közép-léptékű tagoltság
- **6 octave (alapértelmezett)**: nagy hegyek + közepes dombok + kisebb érdességek — természetesnek ható terep
- **8–10 octave**: nagyon részletes, sziklás textúra — de lassabb generálás, és kis térképen nehezen látható különbség a 6-oshoz képest

> **Fontos**: az Octaves növelése exponenciálisan növeli a generálási időt nagy felbontáson.

---

### `Amplitude` — float, alapérték: 1.0

Az első zajréteg amplitúdója, vagyis az alap magassági skála. A végső heightmap értékek normalizálásra kerülnek [0.0–1.0] tartományba, ezért ez a paraméter inkább az egyes rétegek egymáshoz viszonyított arányát befolyásolja.

- **1.0**: normál arányok — a legtöbb esetben ezt érdemes változatlanul hagyni
- Az Amplitude módosítása csak akkor van értelme, ha a Persistence-szel együtt finomhangolod a rétegek súlyát

---

### `Lacunarity` — float, alapérték: 2.0

Meghatározza, hogy az egyes oktávok frekvenciája mennyivel nő az előzőhöz képest.

- **1.0**: minden réteg ugyanolyan frekvenciájú — nincs léptékváltás, monoton eredmény
- **2.0 (alapértelmezett)**: minden réteg kétszer akkora frekvenciájú — természetes megjelenés, a legtöbb esetben ideális
- **3.0+**: az egymást követő rétegek nagyon gyorsan válnak aprókká — érdes, "csipkés" textúra

---

### `Persistence` — float, alapérték: 0.5

Meghatározza, hogy az egyes oktávok amplitúdója mennyivel csökken az előzőhöz képest (0.0–1.0 között).

- **Alacsony érték (pl. 0.2–0.3)**: a kis részlet-rétegek gyorsan elhalnak — sima, lágyan gömbölyű domborzat, kevés érdességgel. Ideális síkságok és enyhe dombok esetén.
- **0.5 (alapértelmezett)**: minden réteg feleakkora súlyú az előzőnél — természetes, kiegyensúlyozott megjelenés
- **Magas érték (pl. 0.7–0.9)**: a kis rétegek is erősen hatnak — érdes, csipkés, hegyes terep, sok apró részlettel

> **Összefoglalva**: az Octaves meghatározza *hány* léptéked van, a Persistence meghatározza *mennyire látszanak* a finomabb léptékek.

---

## Hidraulikus erózió paraméterek

### `bEnableErosion` — bool, alapérték: false

Ha be van kapcsolva, a FBM generálás után egy részecskebasú erózió-szimulációt futtat. Ez természetesebb, erodált völgyeket és lejtőket eredményez — folyómedrek, törmelékkúpok, lekerekített hegycsúcsok.

**Figyelem**: nagy felbontáson (513+) és magas ErosionIterations értéknél a generálás akár percekig is tarthat.

---

### `ErosionIterations` — int32, alapérték: 50000

Az erózió-szimulációban elhelyezett esőcseppek száma. Minden csepp a lejtők mentén folyik le, talajt erodálva és lerakva.

- **Alacsony érték (pl. 5000–10000)**: enyhe erózió, alig látható hatás — gyors futás
- **50000 (alapértelmezett)**: mérsékelt erózió, jól látható völgyek és lejtők
- **Magas érték (pl. 100000–200000)**: erősen erodált terep, mély kanyonok és széles deltaszakaszok — de nagyon lassú

---

## Az algoritmus lépései

### 1. FBM zajgenerálás

Minden `(x, y)` cellára:

```
height = 0
freq = Frequency
amp = Amplitude

for i in range(Octaves):
    height += PerlinNoise(x * freq + seedOffset, y * freq + seedOffset) * amp
    freq *= Lacunarity
    amp  *= Persistence

height = normalize(height)  // [0.0, 1.0] tartományba
```

### 2. Kontinens maszk (ha bUseContinentMask = true)

A térkép széleihez közel a magasság csökken:

```
distFromEdge = min(x, y, width-x, height-y) / (size * EdgeFalloffDistance)
mask = smoothstep(0, 1, distFromEdge)
finalHeight = height * mask
```

### 3. Hidraulikus erózió (ha bEnableErosion = true)

Részecske-alapú szimuláció: esőcseppek véletlenszerűen kerülnek a térképre, a lejtőkön folynak le, talajt erodálnak és raknak le.

---

## Kimenet

- `TArray<float>` — `Resolution × Resolution` méretű tömb, soronként tárolva
- Értékek `[0.0, 1.0]` tartományban normalizálva
- `0.0` = legmélyebb pont, `1.0` = legmagasabb csúcs
- A tényleges fizikai magasságot a landscape `QuadSize` és Z-scale szorzata határozza meg
