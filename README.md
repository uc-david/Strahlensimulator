# Strahlensimulator für das 6150AD2

Ein Arduino Leonardo (oder Micro) simuliert die externe Sonde eines Dosisleistungsmessgeräts 6150AD2. Eine Steuerspannung am Analogeingang wird in Zählimpulse umgerechnet. Das Messgerät zeigt diese Impulse als Dosisleistung an, ohne dass eine radioaktive Quelle nötig ist. Gedacht ist das für Ausbildung und Übungen.

| Steuerspannung A0 | Pulse/s an Pin 13 | Anzeige 6150AD2 (ca.) |
|---|---|---|
| ≤ 0,34 V | 1 | 1 µSv/h |
| 0,66 V | 10 | 10 µSv/h |
| 0,98 V | 100 | 100 µSv/h |
| 1,30 V | 1 000 | 1 mSv/h |
| ≥ 1,60 V | 8 500 | 9,9 mSv/h |

Die Kennlinie ist **logarithmisch**: Jede Dekade der Dosisleistung entspricht etwa 0,32 V. Die Eckwerte (0,34 V ≙ 1 µSv/h, 1,6 V ≙ 9,9 mSv/h) wurden empirisch am 6150AD2 ermittelt.

## Hardware

- Arduino Leonardo oder Arduino Micro (ATmega32U4, 16 MHz)
- Andere Boards (z. B. Uno, Nano) funktionieren **nicht**, weil das Programm Timer4 des ATmega32U4 direkt verwendet.

| Pin | Funktion |
|---|---|
| A0 | Steuerspannung 0 … 5 V (genutzt wird 0,34 … 1,6 V) |
| 13 | Impulsausgang zur Sondenbuchse des 6150AD2, 5-V-Pulse, 20 µs, aktiv HIGH |
| 10 | Taster gegen GND: aktuellen Wert als Nullpunkt übernehmen (optional) |
| 12 | Taster gegen GND: aktuellen Wert als Endwert übernehmen (optional) |
| GND | gemeinsame Masse mit Steuerspannung und Messgerät |

Die Taster brauchen keine externen Widerstände, die internen Pull-ups sind aktiv.

## Anschluss an das 6150AD2

Der Arduino wird **nicht direkt** an das Messgerät angeschlossen. Dazwischen sitzt die Platine der Automess-Simulationssonde **6150AD-20** (Leiterplatte 700.15.01/2). Sie wird über das normale Sondenkabel (6 Adern + Schirm, 1,25 m) mit dem 6150AD2 verbunden und meldet sich dort als externe Sonde an. Der Arduino ersetzt den Pulsgenerator, der laut Automess an diese Platine angeschlossen wird.

```
Arduino Pin 13 ──── Pulseingang der Simulationssonden-Platine (Ader braun)
Arduino GND    ──── Masse (Ader schwarz / Kabelschirm)
                    Simulationssonden-Platine ════ Sondenkabel ════ 6150AD2
```

| Ader | Funktion |
|---|---|
| braun | Zählimpulse |
| schwarz | Masse, kein eigener Pin, über den Kabelschirm |
| grün | 4,75 V Versorgung aus dem 6150AD2 für die Sondenplatine |
| weiß, gelb | Erkennung der Sonde durch das 6150AD2 |
| übrige 2 Pins | Sonderfunktionen des 6150AD2, **dürfen nicht angeschlossen werden** |

**Pulsvorgabe von Automess:** 10 … 30 µs lang, Pegel 0 V / 4,75 V (5 V ± 10 %). Dieser Simulator liefert 20 µs. In der Automess-Skizze ruht die Leitung auf HIGH und der Puls geht nach LOW; der Arduino gibt dagegen LOW in Ruhe und HIGH-Pulse aus. Am 6150AD2 funktioniert das in der Praxis.

### Sondencodierung

Auf der Simulationssonden-Platine wird der Sondentyp mit Lötbrücken der Wertigkeit 1, 2, 4, 8, 16, 32 eingestellt. Der Code muss zur Seriennummer des Messgeräts passen:

| Code | gesetzte Brücken | Messgerät |
|---|---|---|
| 20 | 4 + 16 | 6150AD1 sowie 6150AD2 mit Seriennummer **unter 65602** |
| 23 | 1 + 2 + 4 + 16 | 6150AD2 mit Seriennummer **ab 65602** |

### Anzeige abhängig von der Pulsrate

Herstellerangabe von Automess (17.08.1989) für das 6150AD2:

| Pulse/s | Anzeige | | Pulse/s | Anzeige |
|---|---|---|---|---|
| 1 | 0,41 µSv/h | | 3 000 | 2,24 mSv/h |
| 3 | 1,63 µSv/h | | 4 000 | 3,20 mSv/h |
| 10 | 6,00 µSv/h | | 5 000 | 4,30 mSv/h |
| 30 | 18,5 µSv/h | | 6 000 | 5,63 mSv/h |
| 100 | 62,3 µSv/h | | 7 000 | 7,12 mSv/h |
| 300 | 189 µSv/h | | 8 000 | 9,00 mSv/h |
| 1 000 | 655 µSv/h | | 8 250 | 9,50 mSv/h |
| 2 000 | 1,40 mSv/h | | 8 500 | Überlauf |

Bei kleinen Raten zeigt das Gerät wegen des Hintergrundabzugs weniger an, bei großen Raten wegen der Totzeitkorrektur mehr, als ein linearer Zusammenhang ergäbe.

## Bauen und Hochladen

Voraussetzung ist [PlatformIO](https://platformio.org/) (Kommandozeile oder die VS-Code-Erweiterung).

```bash
pio run                          # bauen (Leonardo)
pio run -e leonardo -t upload    # auf einen Leonardo hochladen
pio run -e micro -t upload       # auf einen Micro hochladen
pio device monitor               # serieller Monitor, 115200 Baud
```

| Umgebung | Zweck |
|---|---|
| `leonardo` | Standard |
| `micro` | Arduino Micro |
| `leonardo_debug` | wie `leonardo`, gibt alle 500 ms Null-/Endwert, Mittelwert und Pulsrate seriell aus |

In VS Code wählst du die Umgebung unten in der Statusleiste aus.

## Kalibrierung

Beim Start gelten immer die Standardwerte aus [src/main.cpp](src/main.cpp):

```cpp
const int NULLWERT_DEFAULT = 70;    // 1 uSv/h   = 0.34 V
const int ENDWERT_DEFAULT = 327;    // 9.9 mSv/h = 1.6 V
```

Die Werte sind ADC-Digits (5 V Referenz, 1 Digit ≈ 4,9 mV). Dauerhaft ändern lassen sie sich nur hier im Code.

Mit den Tastern an Pin 10 und 12 lassen sich Null- und Endwert im laufenden Betrieb auf die gerade anliegende Spannung setzen (Taster 500 ms halten). Diese Werte gelten nur bis zum nächsten Reset. Sie werden zwar ins EEPROM geschrieben, beim Start aber nicht wieder geladen. Werte, bei denen der Endwert nicht mindestens 20 Digit über dem Nullwert liegt, werden ignoriert.

## Funktionsweise (Kurzfassung)

1. Die Spannung an A0 wird gemessen und über 20 Messungen gemittelt.
2. Der Mittelwert wird zwischen Null- und Endwert auf einen Anteil von 0 … 1 umgerechnet.
3. Die Pulsrate ist `8500 ^ Anteil`, also 1 … 8500 Pulse/s.
4. Timer4 erzeugt die Pulse in Hardware: jeder Puls ist exakt 20 µs lang, die mittlere Rate stimmt genau.

Details zu Timer, Pulserzeugung und Genauigkeit stehen in [docs/technik.md](docs/technik.md).

## Hinweise

- Das 6150AD2 braucht Pulse von 20 µs Länge, damit es die Zerfälle einer externen Sonde erkennt. Die Pulsbreite darf deshalb nicht verändert werden.
- Laut Automess-Tabelle bedeuten 8 500 Pulse/s bereits Überlauf und 1 Puls/s nur 0,41 µSv/h. Die Eckwerte der Kennlinie oben (1 µSv/h bzw. 9,9 mSv/h) wurden dagegen empirisch mit der Version von 2019 ermittelt, die am Endwert nur 8 333 Pulse/s erreichte. Am Endwert kann diese Version daher Überlauf anzeigen; mit `PULS_MAX = 8333.0` in [src/main.cpp](src/main.cpp) verhält sie sich dort wie die alte Version.
- Bei hohen Pulsraten zeigt diese Version etwas mehr an als die ursprüngliche Version von 2019 (bis ca. 14 % zwischen 1 und 9,9 mSv/h). Die alte Version konnte dort nur in Stufen zählen. Siehe [docs/technik.md](docs/technik.md#unterschied-zur-version-von-2019).
- Der Simulator ersetzt keine Kalibrierung des Messgeräts mit einer echten Quelle.

## Lizenz

[GNU General Public License v3.0](LICENSE). Nutzung, Änderung und kommerzielle Verwendung sind erlaubt. Wer die Software oder ein Gerät damit weitergibt, muss den Quellcode einschließlich aller Änderungen ebenfalls unter der GPL-3.0 offenlegen.
