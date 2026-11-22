# Technische Details

## Kennlinie

```
anteil = (A0_mittel - nullwert) / (endwert - nullwert)     begrenzt auf 0 … 1
pulse  = 8500 ^ anteil                                     1 … 8500 Pulse/s
```

Mit den Standardwerten 70 / 327 Digit gilt:

| A0 (Digit) | A0 (V) | Pulse/s |
|---|---|---|
| ≤ 70 | ≤ 0,34 | 1 |
| 135 | 0,66 | 10 |
| 201 | 0,98 | 100 |
| 266 | 1,30 | 1 000 |
| ≥ 327 | ≥ 1,60 | 8 500 |

Zwischen Null- und Endwert liegen 257 Digit für knapp 4 Dekaden, also rund 65 Digit pro Dekade. Ein Digit ändert die Pulsrate um etwa 3,6 %.

Der Mittelwert läuft über die letzten 20 Messungen. Da `loop()` sehr schnell läuft, glättet das nur wenige Millisekunden und dient vor allem gegen ADC-Rauschen.

## Pulserzeugung mit Timer4

Timer4 des ATmega32U4 läuft synchron zum Systemtakt:

| Einstellung | Wert |
|---|---|
| Takt | 16 MHz / 64 = 250 kHz, also 4 µs pro Tick |
| Betriebsart | Normal Mode, TOP = `OCR4C` = 4 |
| Zykluslänge | 5 Ticks = **20 µs** (50 kHz) |
| Compare Match A | bei `TCNT4` = 2, einmal pro Zyklus |
| Ausgang | OC4A = PC7 = Pin 13 |

Bei jedem Compare Match schaltet die **Hardware** den Ausgang Pin 13. Die Interrupt-Routine legt nur fest, was beim **nächsten** Compare Match passiert:

- Puls fällig: Ausgang beim nächsten Match setzen (`COM4A1:0 = 11`)
- sonst: Ausgang beim nächsten Match löschen (`COM4A1:0 = 10`)

Ein Puls dauert damit genau einen Zyklus, also 20 µs, quarzgenau. Wie spät die Interrupt-Routine startet (z. B. weil gerade der millis- oder USB-Interrupt läuft), spielt keine Rolle, solange sie innerhalb von 20 µs fertig ist.

```
Zyklus     |  20 µs  |  20 µs  |  20 µs  |  20 µs  |  20 µs  |  20 µs  |
Pin 13  ___|‾‾‾‾‾‾‾‾‾|_________________________________________|‾‾‾‾‾‾‾‾‾|___
           Puls                                                Puls
```

### Phasen-Akkumulator

Wann ein Puls fällig ist, bestimmt ein Akkumulator:

```cpp
acc += rate;               // alle 20 µs
if (acc >= 50000) {        // 50 000 Zyklen pro Sekunde
  acc -= 50000;
  // Puls
}
```

Pro Sekunde entstehen so genau `rate` Pulse. Die Abstände wechseln bei krummen Raten zwischen zwei benachbarten Werten (z. B. 5 und 6 Zyklen bei 8500 Pulse/s), der Mittelwert stimmt exakt.

Bei 8500 Pulse/s liegen mindestens 80 µs LOW zwischen zwei Pulsen. Der Akkumulator ist 16 Bit breit, das reicht bis 15 535 Pulse/s ohne Überlauf.

`rate` ist eine 16-Bit-Variable, die `loop()` schreibt und die Interrupt-Routine liest. Sie wird deshalb in einem `ATOMIC_BLOCK` geschrieben.

## Unterschied zur Version von 2019

Die ursprüngliche Version hat einen Puls ausgegeben, sobald `Zähler × Rate ≥ 50000` war. Der Pulsabstand war damit immer auf ganze 20-µs-Zyklen **aufgerundet**:

| berechnete Rate | Version 2019 | diese Version |
|---|---|---|
| bis 1 000 /s | fast gleich | gleich |
| 3 000 /s | 2 941 /s | 3 000 /s |
| 6 000 /s | 5 556 /s | 6 000 /s |
| 7 000 /s | 6 250 /s | 7 000 /s |
| 8 500 /s | 8 333 /s | 8 500 /s |

Die Eckwerte (9,9 mSv/h am Endwert) wurden mit der Version von 2019 ermittelt, also bei tatsächlich 8 333 Pulse/s. Soll der Endwert wieder genauso angezeigt werden wie früher, kann `PULS_MAX` in [src/main.cpp](../src/main.cpp) auf `8333.0` gesetzt werden.

Weitere Änderungen gegenüber 2019:

- Pulsflanken durch die Timer-Hardware statt `digitalWrite()` in der Interrupt-Routine (keine Schwankung der Pulsbreite mehr)
- kein Nachladen von `TCNT4` in der Interrupt-Routine (kein Driften des Takts)
- Taster entprellt, reagieren einmal pro Druck
- Plausibilitätsprüfung für Null- und Endwert (keine Division durch 0)
- Mittelwert startet mit dem aktuellen Messwert statt mit 0
