USBImager
=========

<img src="https://gitlab.com/bztsrc/usbimager/raw/master/src/misc/icon32.png">
Az [USBImager](https://bztsrc.gitlab.io/usbimager) egy igen igen faék egyszerűségű ablakos alkalmazás, amivel
tömörített lemezképeket lehet USB meghajtókra írni és lementeni. Elérhető Windows, MaxOSX és Linux rendszereken. A felülete
annyira egyszerű, amennyire csak lehetséges, teljesen salang mentes.

| Platform     | Felület      | Leírás                       |
|--------------|--------------|------------------------------|
| Windows      | [GDI](https://gitlab.com/bztsrc/usbimager/raw/binaries/usbimager_1.0.7-i686-win-gdi.zip)<br>[GDI wo](https://gitlab.com/bztsrc/usbimager/raw/binaries/usbimager_1.0.7_wo-i686-win-gdi.zip) | natív interfész<br>egyszerűsített, csak író felület |
| MacOSX       | [Cocoa](https://gitlab.com/bztsrc/usbimager/raw/binaries/usbimager_1.0.7-intel-macosx-cocoa.zip)<br>[Cocoa wo](https://gitlab.com/bztsrc/usbimager/raw/binaries/usbimager_1.0.7_wo-intel-macosx-cocoa.zip) | natív interfész<br>egyszerűsített, csak író felület |
| Ubuntu LTS   | [GTK+](https://gitlab.com/bztsrc/usbimager/raw/binaries/usbimager_1.0.7-amd64.deb)<br>[GTK+ wo](https://gitlab.com/bztsrc/usbimager/raw/binaries/usbimager_1.0.7_wo-amd64.deb) | ua. mint a Linux PC GTK verzió udisks2-vel, csak .deb formátumban<br>egyszerűsített, csak író felület |
| RaspiOS      | [GTK+](https://gitlab.com/bztsrc/usbimager/raw/binaries/usbimager_1.0.7-armhf.deb)<br>[GTK+ wo](https://gitlab.com/bztsrc/usbimager/raw/binaries/usbimager_1.0.7_wo-armhf.deb) | ua. mint a Raspberry Pi GTK verzió udisks2-vel, csak .deb formátumban<br>egyszerűsített, csak író felület |
| Arch/Manjaro | [GTK+](https://aur.archlinux.org/packages/usbimager/) | ua. mint a Linux PC GTK verzió udisks2-vel, csak AUR csomagban |
| Linux PC     | [X11](https://gitlab.com/bztsrc/usbimager/raw/binaries/usbimager_1.0.7-x86_64-linux-x11.zip)<br>[GTK+](https://gitlab.com/bztsrc/usbimager/raw/binaries/usbimager_1.0.7-x86_64-linux-gtk.zip)  | javalott<br>kompatíbilitás (van egy kis biztonsági kockázat a nyers lemezelérések engedélyezésekor udisks2 nélkül) |
| Raspberry Pi | [X11](https://gitlab.com/bztsrc/usbimager/raw/binaries/usbimager_1.0.7-armv7l-linux-x11.zip)<br>[X11](https://gitlab.com/bztsrc/usbimager/raw/binaries/usbimager_1.0.7-aarch64-linux-x11.zip) | natív interfész, AArch32 (armv7l)<br>natív interfész, AArch64 (arm64) |

Képernyőképek
-------------

<img src="https://gitlab.com/bztsrc/usbimager/raw/master/usbimager.png">

Telepítés
---------

1. töltsd le a megfelelő `usbimager_*.zip` csomagolt fájlt a [repó](https://gitlab.com/bztsrc/usbimager/tree/binaries)ból a gépedhez (kevesebb, mint 192 Kilobájt mind)
2. csomagold ki: `C:\Program Files` (Windows), `/Applications` (MacOSX) vagy `/usr` (Linux) mappába
3. Használd!

A csomagban lévő futtathatót egyből használhatod, nem kell telepíteni, és a többi fájl is csak azért van, hogy beillessze az asztali
környezetedbe (ikonok és hasonlók). Automatikusan érzékeli az operációs rendszeredben beállított nyelvet, és ha talál hozzá szótárat, akkor
a saját nyelveden köszönt (természetesen tud magyarul).

Ubuntu LTS és Raspbian rendszeren letöltheted a deb csomagot is, amit aztán a
```
sudo dpkg -i usbimager_*.deb
```
paranccsal telepíthetsz.

Támogasd a fejlesztést adománnyal
---------------------------------

Ha tetszik, vagy hasznosnak találod, szívesen fogadok akármekkora adományt:<br>
<a href="bitcoin:3EsdxN1ZsX5JkLgk3uR4ybHLDX5i687dkx"><img src="https://gitlab.com/bztsrc/usbimager/raw/master/donate.png"><br>BTC 3EsdxN1ZsX5JkLgk3uR4ybHLDX5i687dkx</a>

Fícsörök
--------

- Nyílt Forráskódú és MIT licenszű
- Szállítható futtatható, nem kell telepíteni, csak csomagold ki
- Kicsi. Nagyon kicsi, csupán pár kilobájt, mégsincs függősége
- Az etch*r-el ellentétben nem kell aggódnod a privát szférád vagy a reklámok miatt, garantáltan GDPR kompatíbilis
- Minimalista, többnyelvű, natív interfész minden platformon
- Igyekszik hülyebiztos lenni, és nem engedi véletlenül felülírni a rendszerlemezed
- Szinkronizáltan ír, azaz minden adat garantáltan a lemezen lesz, amikorra a csík a végére ér
- Képes ellenőrizni az írást visszaolvasással és az eredeti lemezképpel való összevetéssel
- Képes nyers lemezképeket olvasni: .img, .bin, .raw, .iso, .dd, stb.
- Képes futási időben kitömöríteni: .gz, .bz2, .xz, .zst
- Képes csomagolt fájlokat kitömöríteni: .zip (PKZIP és ZIP64), .zzz (ZZZip), .tar, .cpio, .pax (*)
- Képes lemezképeket készíteni, nyers és ZStandard tömörített formátumban
- Képes mikrokontrollerek számára soros vonalon leküldeni a lemezképeket
- 17 nyelvre lefordítva

(* - a több fájlt is tartalmazó csomagolt fájlok esetén a csomagolt fájl legelső fájlját használja bemenetnek)

Összehasonlítás
---------------

| Leírás                          | balenaEtcher  | WIN32 Disk Imager | USBImager |
|---------------------------------|---------------|-------------------|-----------|
| Többplatformos                  | ✔             | ✗                 | ✔         |
| Minimum Windows                 | Win 7         | Win XP            | Win XP    |
| Minimum MacOSX (1)              | ?             | ✗                 | 10.13     |
| Elérhető Raspbian-on            | ✗             | ✗                 | ✔         |
| Program mérete (2)              | 130 Mb        | ✗                 | 300 Kb    |
| Függőségek                      | sok, ~300 Mb  | Qt, ~8 Mb         | ✗ nincs   |
| Kémkedés- és reklámmentes       | ✗             | ✔                 | ✔         |
| Natív interfész                 | ✗             | ✗                 | ✔         |
| Garantált kiírás (3)            | ✗             | ✗                 | ✔         |
| Kiírt adatok ellenőrzése        | ✔             | ✗                 | ✔         |
| Tömörített lemezképek           | ✔             | ✗                 | ✔         |
| Nyers kiírási idő (4)           | 23:16         | 23:28             | 24:05     |
| Tömörített kiírás (4)           | 01:12:51      | ✗                 | 30:47     |

(1) - a mellékelt bináris 10.14-en lett fordítva (mert nekem az van), de visszaigazolták, hogy a forrás 10.13 alatt is gond nélkül lefordul.

(2) - a szállítható futtatható mérete Windowson. A WIN32 Disk Imagerhez nem tudtam letölteni előre lefordított hivatalos csomagokat, csak forrást.

(3) - USBImager csak nem-bufferelt IO utasításokat használ, hogy a fizikális lemezreírás biztos legyen

(4) - a méréseket @CaptainMidnight végezte Windows 10 Pro alatt egy SanDisk Ulta 32GB A1 kártyával. A nyers lemezkép mérete 31,166,976 Kb volt, míg a bzip2 tömörítetté 1,887,044 Kb. WIN32 Disk Imager nem kezel tömörített lemezképeket, így a végeredménye nem volt bebootolható.

Használat
---------

Ha nem tudod írni a céleszközt (folyton "hozzáférés megtagadva" hibaüzenetet kapsz), akkor:


__Windows__: jobbklikk az usbimager.exe-n és használd a "Futtatás rendszergazdaként" opciót.

__MacOSX__: 10.14 és afölött: menj a rendszerbeállításokhoz "System Preferences", aztán "Security & Privacy" és "Privacy". Add hozzá az USBImager-t a
"Full Disk Access" listához. Alternatívaként indíthatod Terminálból a *sudo /Applications/USBImager.app/Contents/MacOS/usbimager* paranccsal (10.13 alatt csak ez utóbbi működik).

__Linux__: ez valószínűleg nem fordulhat elő, mivel az USBImager setgid bittel érkezik. Ha mégsem, akkor a *sudo chgrp disk usbimager && sudo chmod g+s usbimager*
parancs beállítja. Alternatívaként add hozzá a felhasználódat a "disk" csoportokhoz (az "ls -la /dev|grep -e ^b" parancs kiírja, melyik csoportban vannak az oprendszered
alatt a lemezeszközök). __Elvileg nincs szükség__ a *sudo /usr/bin/usbimager*-re, csak győzödj meg róla, hogy a felhasználódnak van írási
hozzáférése az eszközökhöz, ez a Legalacsonyabb Privilégium Elve (Principle of Least Privilege).

### Interfész

1. sor: lemezkép fájl
2. sor: műveletek, írás és olvasás ebben a sorrendben
3. sor: eszköz kiválasztás
4. sor: opciók: írás ellenőrzése, kimenet tömörítése és buffer méret rendre

Az X11 esetén mindent a nulláról írtam meg, hogy elkerüljem a függőségeket. A kattintás és a billentyűnavigáció a megszokott: <kbd>Tab</kbd>
és <kbd>Shift</kbd> + <kbd>Tab</kbd> váltogat a mezők között, <kbd>Enter</kbd> a kiválasztás. Plusz a fájl tallózásakor a <kbd>Bal nyíl</kbd>
/ <kbd>BackSpace</kbd> (törlés) feljebb lép egy könyvtárral, a <kbd>Jobbra nyíl</kbd> / <kbd>Enter</kbd> pedig beljebb megy (vagy kiválaszt, ha
az aktuális elem nem könyvtár). A sorrendezést a <kbd>Shift</kbd> + <kbd>Fel nyíl</kbd> / <kbd>Le nyíl</kbd> kombinációkkal tudod változtatni.
A "Legutóbb használt" fájlok listája szintén támogatott (a freedesktop.org féle [Desktop Bookmarks](https://freedesktop.org/wiki/Specifications/desktop-bookmark-spec/)
szabvány alapján).

### Lemezkép kiírása eszközre

1. kattints a "..." gombra az első sorban és válassz lemezkép fájlt
2. kattints a harmadik sorra és válassz eszközt
3. kattints a második sor első gombjára (Kiír)

Ennél a műveletnél a fájl formátuma és a tömörítése automatikusan detektálásra kerül. Kérlek vedd figyelembe, hogy a hátralévő idő becsült.
Bizonyos tömörített fájlok nem tárolják a kicsomagolt méretet, ezeknél a státuszban "x MiB ezidáig" szerepel. A hátralévő idejük nem lesz pontos,
csak egy közelítés a becslésre a tömörített pozíció / tömörített méret arányában (magyarán a mértékegysége sacc/kb).

Ha az "Ellenőrzés" be van pipálva, akkor minden kiírt blokkot visszaolvas, és összehasonlít az eredeti lemezképpel.

Az utolsó opció, a legördülő állítja, hogy mekkora legyen a buffer. Ekkora adagokban fogja a lemezképet kezeli. Vedd figyelembe, hogy a
tényleges memóriaigény ennek háromszorosa, mivel van egy buffer a tömörített adatoknak, egy a kicsomagolt adatoknak, és egy az ellenőrzésre
visszaolvasott adatoknak.

### Lemezkép készítése eszközről

1. kattints a harmadik sorra és válassz eszközt
2. kattints a második sor második gombjára (Beolvas)
3. a lemezkép az Asztalodon fog létrejönni, a fájlnév pedig megjelenik az első sorban

A generált lemezkép neve "usbimager-(dátum)T(idő).dd" lesz, a pontos időből számítva. Ha a "Tömörítés" be volt pipálva, akkor a fájlnév
végére egy ".zst" kiterszejtést biggyeszt, és a lemezkép tartalma ZStandard tömörített lesz. Ennek sokkal jobb a tömörítési aránya, mint
a gzipé. Nyers lemezképek esetén a hátralévő idő pontos, tömörítés esetén nagyban ingadozik a tömörítés műveletigényétől, ami meg az
adatok függvénye, ezért csak egy becslés.

Megjegyzés: Linuxon ha nincs ~/Desktop (Asztal), akkor a ~/Downloads (Letöltések) mappát használja. Ha az sincs, akkor a lemezkép a
home mappába lesz lementve. A többi platformon mindig van Asztal, ha mégse találná, akkor az aktuális könyvtárba ment. Minden platformon
érvényes, ha egy létező könyvtár meg van adva a parancssorban, akkor azt használja a lemezképek lementéséhez.

### Haladó funkciók

| Kapcsoló            | Leírás                    |
|---------------------|---------------------------|
| -v/-vv              | Részletes kimenet         |
| -Lxx                | Nyelvkód kikényszerítés   |
| -1..9               | Buffer méret beállítása   |
| -a                  | Minden meghajtó listázása |
| -s\[baud]/-S\[baud] | Soros portok használata   |
| --version           | Kiírja a verziót          |
| (könyvtár)          | Az első nem-kapcsoló a mentési könyvtár |

Windows felhasználóknak: jobb-klikk az usbimager.exe-n, majd választd a "Parancsikon létrehozása" menüt. Aztán jobb-klikk az újonnan
létrejött ".lnk" fájlra, és válaszd a "Tulajdonságok" menüt. A "Parancsikon" fülön, a "Cél" mezőben tudod hozzáadni a kapcsolókat.
Ugyancsak itt, a "Biztonság" fülön be lehet állítani, hogy rendszergazdaként futtassa, ha problémáid lennének a direkt lemezhozzáférésekkel.

A legelső paraméter, ami nem kapcsoló (nem '-'-el kezdődik) a lemezképek lementési könyvtáraként értelmeződik.

A '-a' kapcsoló minden eszközt listáz, még a rendszerlemezeket és a különösen nagyokat is. Ezzel használhatatlanná lehet tenni a gépet, óvatosan.

A kapcsolókat külön-külön (pl. "usbimager -v -s -2") vagy egyben ("usbimager -2vs") is megadhatod, a sorrend nem számít. Azon kapcsolók
közül, amik ugyanazt állítják, csak a legutolsót veszi figyelembe (pl "-124" ugyanaz, mint a "-4").

A '-v' és '-vv' kapcsolók szószátyárrá teszik az USBImager-t, és mindenféle részletes infókat fog kiírni a konzolra. Ez utóbbi a szabvány
kimenet (stdout) Linux és MacOSX alatt (szóval terminálból használd), míg Windowson egy külön ablakot nyit az üzeneteknek.

A '-Lxx' kapcsoló utolsó két karaktere "en", "es", "de", "fr" stb. lehet. Ez a kapcsoló felülbírája a detektált nyelvet, és a megadott
szótárat használja. Ha nincs ilyen szótár, akkor angol nyelvre vált.

A szám kapcsolók a buffer méretét állítják a kettő hatványa Megabájtra (0 = 1M, 1 = 2M, 2 = 4M, 3 = 8M, 4 = 16M, ... 9 = 512M). Ha nincs
megadva, a buffer méret alapértelmezetten 1 Megabájt.

Ha az USBImager-t '-s' (kisbetű) kapcsolóval indítod, akkor a soros portra is engedi küldeni a lemezképeket. Ehhez szükséges, hogy a
felhasználó az "uucp" illetve a "dialout" csoport tagja legyen (disztribúciónként eltérő, használd a "ls -la /dev|grep tty" parancsot).
Ez esetben a kliensen:
1. tetszőleges ideig várakozni kell az első bájtra, majd lementeni azt a bufferbe
2. ezután a többi bájtot időtúllépéssel (mondjuk 250 mszekkel vagy 500 mszekkel) kell olvasni, és lerakni azokat is a bufferbe
3. ha az időtúllépés bekövetkezik, a lemezkép megérkezett.

A '-S' (nagybetű) kapcsoló hasonló, de ekkor az USBImager a [raspbootin](https://github.com/bztsrc/raspi3-tutorial/tree/master/14_raspbootin64)
kézfogást fogja alkalmazni a soros vonalon:
1. USBImager várakozik a kliensre
2. a kliens három bájtot küld, '\003\003\003' (3-szor <kbd>Ctrl</kbd>+<kbd>C</kbd>)
3. USBImager leküldi a lemezkép méretét, 4 bájt kicsi-elöl (little-endian) formátumban (méret = 4.bájt * 16777216 + 3.bájt * 65536 + 2.bájt * 256 + 1.bájt)
4. a kliens két bájttal válaszol, vagy 'OK' vagy 'SE' (size error, méret hiba)
5. ha a válasz OK volt, akkor az USBImager leküldi a lemezképet, méret bájtnyit
6. amikor a kliens fogadta a méretedik bájtot, a lemezkép megérkezett.

Mindkét esetben a soros port 115200 baud, 8 adatbit, nincs paritás, 1 stopbit módra kerül felkonfigurálásra. A soros vonali átvitelek esetében
az USBImager nem tömöríti ki a lemezképet, hogy csökkentse az átviteli időt, így a kicsomagolást a kliensen kell elvégezni. Ha egy egyszerű
rendszerbetöltőre vágysz, ami kompatíbilis az USBImager-el, akkor javalom az [Image Receiver](https://gitlab.com/bztsrc/imgrecv)-t
(elérhető RPi1, 2, 3, 4 és IBM PC BIOS gépekre). Használható vészhelyzeti induló lemezképek küldésére
[BOOTBOOT](https://gitlab.com/bztsrc/bootboot) kompatíbilis rendszerbetöltők számára.

Ha más baud-ot szeretnél, csak add meg a kapcsoló után, pl. "-s57600" vagy "-S230400". Lehetséges értékek:
57600, 115200, 230400, 460800, 500000, 576000, 921600, 1000000, 1152000, 1500000, 2000000, 2500000, 3000000, 3500000, 4000000

FIGYELEM: nem minden soros port kezeli az összes baud rátát. Ellenőrizd a kézikönyvében.

Fordítás
--------

Részletes leírás a [használati útmutató](https://gitlab.com/bztsrc/usbimager/-/raw/master/usbimager-manual.pdf) mellékletében.

Ismert bugok
------------

Nincs. Ha belefutnál egybe, használd kérlek az [issue tracker](https://gitlab.com/bztsrc/usbimager/issues)-t.

Szerzők
-------

- libui: Pietro Gagliardi
- bzip2: Julian R. Seward
- xz: Igor Pavlov és Lasse Collin
- zlib: Mark Adler
- zstd: FB, számos kontribútor (lásd http://www.zstd.net)
- zip kezelés: bzt (semmilyen PKWARE függvénykönyvtár vagy forrás nem lett felhasználva)
- usbimager: bzt

Hozzájárulások
--------------

Szeretnék köszönetet mondani a következő felhasználóknak: @mattmiller, @MisterEd, @the_scruss, @rpdom, @DarkElvenAngel, és különösen
@tvjon-nak, @CaptainMidnight-nak és @gitlabhack-nek amiért több különböző platformon és számos különböző eszközzel is letesztelték az USBImager-t.

Köszönet a fordítások ellenőrzéséért és javításáért: @mline-nak és @vordenken-nek (német), @epoch1970-nek és @JumpZero-nak (francia), @hansotten-nek és @zonstraal-nak (holland), @ller (orosz), @zaval (ukrán), @lmarmisa (spanyol), @otani (japán), @ngedizaydindogmus (török), @coltrane (portugál), @Matthaiks (lengyel).

További köszönet @munntjlx-nek és @lfomartins-nak, hogy lefordították az USBImager-t MacOS-en, és @tido- -nak az Ubuntu debért.

Legjobbakat,

bzt
