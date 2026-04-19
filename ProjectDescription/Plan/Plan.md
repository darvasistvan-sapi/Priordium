# Priordium - Fejlesztési terv

A játék Unreal Engine 5.6-ban készül, AI asszisztenssel. A projektet kizárólag az AI asszisztens módosítja, kizárólag akkor módosít a projekten, amikor az AI asszisztens azt kéri.
A legenerált kód meg kell feleljen a SOLID programozási elveknek. Minden osztály, struktúra és enum külön fileban kell legyen definiálva.
A fejlesztés apró szakaszokban valósul meg. A fejlesztést úgy kell a lehető legtöbb részre bontani, hogy minden rész lefejlesztése után automatizált teszteket lehessen készíteni és a megírt funkciókból össze lehessen állítani egy MVP-t amit leellenőrizhet. Minden rész fejlesztése után automatizált tesztek fejlesztése következik amik tesztelik a rész funkcionalitását, a teszteknek 90% feletti kell legyen a code coverage és teljesen le kell fedjék a use case diagramokat, ezután pedig egy MVP összeállítása amit átnézhet az editorban.
A dokumentáció filejaiban a táblázatok oszlopai egyenesek kell legyenek, tehát a | karakterek előtt annyi karakter kell legyen a sorban mint a fölötte levő előtt, az mmd diagramok pedig mind külön fileban kell legyenek. Minden modul design részében szerepelnie kell egy use-case diagramnak és egy osztály diagramnak.

---

## Technológiai eszköztár

Az alábbi eszközök kiválasztásánál a fő szempont, hogy az AI asszisztens hatékonyan tudja őket kezelni (Blueprint-szerkesztés, Data Asset-ek, Data Table-ök, Gameplay Tag-ek, C++ generálás, AI viselkedésfák, UMG widgetek).

### Alaparchitektúra

| Terület                    | Eszköz                                | Indoklás                                                                                                                                                                                                                                                      |
|----------------------------|---------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Programozási nyelv**     | **Blueprint + C++**                   | A C++ az alap rendszerekhez (pl. GameMode, GameInstance), a Blueprint a tartalom-közeli logikához (törzstagok, szakmák, AI).                                                                    |
| **Keretrendszer**          | **Gameplay Ability System (GAS)**     | A törzstagok szükségletei (éhség, szomjúság, morál, hőmérséklet), a szakmák képességei és a technológia-effektek mind GAS-on keresztül kezelhetők.   |
| **Input rendszer**         | **Enhanced Input System**             | UE 5.6 alapértelmezett input rendszer.                                                                                                   |

### Adatvezérelt tartalom

| Terület              | Eszköz                            | Indoklás                                                                                                                                                                                     |
|----------------------|-----------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Játékadatok**      | **Data Table + Data Asset**       | A szakmák, nyersanyagok, állatok, növények, technológiák és receptek mind adatvezérelten tárolhatók.      |
| **Struktúrák**       | **Blueprint Struct (UStruct)**    | Minden játékadat-típushoz (F_ProfessionData, F_ResourceData, F_TechnologyData, stb.) saját struktúra.                        |
| **Címkézés**         | **Gameplay Tag-ek**               | A szakmák, erőforrás-típusok, szükségletek, technológiák és törzs-tulajdonságok hierarchikus címkézéséhez.                                         |
| **Felsorolások**     | **Blueprint Enum**                | Állapotok, kategóriák és típusok meghatározásához (évszakok, terептípusok, szükséglet-szintek).                                                |

### AI és viselkedés

| Terület         | Eszköz                                         | Indoklás                                                                                                                                                                                                                                  |
|-----------------|------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Törzstag AI** | **Behavior Tree + Blackboard**                 | A törzstagok napi tevékenységei (vadászat, gyűjtögetés, kézműveskedés, pihenés) viselkedésfákkal vezérelhetők. |
| **Törzs AI**    | **Blueprint + Gameplay Tag lekérdezések**      | A törzs szintű döntéshozatal (feladatkiosztás, technológia-fejlesztés, diplomácia) magasabb szintű Blueprint logikával, az erőforrások és szükségletek Gameplay Tag/Attribute alapú lekérdezésével.                                      |
| **Érzékelés**   | **AI Perception System**                       | Beépített UE rendszer, amely kezeli a törzstagok látását, hallását és érzékelését (vadállatok észlelése, nyersanyagok megtalálása).                                                                                                      |

### Térkép és világ

| Terület         | Eszköz                                                   | Indoklás                                                                                                                                                                  |
|-----------------|----------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Terep**       | **Landscape System**                                     | UE beépített tereprendszer (hegyek, síkságok, folyópartok).                                                                                                               |
| **Növényzet**   | **Foliage System + PCG (Procedural Content Generation)** | A PCG Framework az UE 5.6 beépített rendszere, amely szabályalapú procedurális elhelyezést biztosít (erdők, bokrok, fű, nyersanyag-lelőhelyek).                          |
| **Vízrendszer** | **Water System plugin**                                  | UE beépített plugin tengerekhez, folyókhoz és tavakhoz.                                                                                                                   |
| **Időjárás**    | **Blueprint + Niagara**                                  | 

### Felhasználói felület

| Terület         | Eszköz                           | Indoklás                                                                                                                                                                                      |
|-----------------|----------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **UI rendszer** | **UMG (Unreal Motion Graphics)** | A törzs kezelő felület, erőforrás kijelző, technológia fa és minimap mind UMG widgetekkel valósítható meg. |
