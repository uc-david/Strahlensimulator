// Strahlensimulator
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Zielhardware: ATmega32U4 (Arduino Leonardo / Micro), 16 MHz
//
// Analogeingang A0 (0.34 V .. 1.6 V) wird logarithmisch auf eine
// Pulsrate von 1 .. 8500 Pulse/s an Pin 13 abgebildet.
// 1 Puls/s = 1 uSv/h, 8500 Pulse/s = 9.9 mSv/h
// (Werte empirisch am 6150AD2 ermittelt, Zusammenhang nicht linear)
//
// Jeder Puls ist exakt 20 us lang (Sondensimulation fuer 6150AD2).
// Die Flanken erzeugt die Timer4-Hardware ueber OC4A (= Pin 13),
// dadurch hat die Interrupt-Latenz keinen Einfluss auf die Pulsbreite.
//
// Kalibrierung: Beim Start gelten immer die Standardwerte 70 / 327.
// Taster an Pin 10: aktuellen Wert als Nullpunkt uebernehmen
// Taster an Pin 12: aktuellen Wert als Endwert uebernehmen
// (Die Taster wirken bis zum naechsten Reset und schreiben zusaetzlich
//  ins EEPROM, die EEPROM-Werte werden beim Start aber nicht verwendet.)

#include <Arduino.h>
#include <EEPROM.h>
#include <util/atomic.h>

//#define DEBUG               // Ausgabe der Werte ueber Serial (oder env:leonardo_debug)

const int takt = 13;          // Pin 13 = PC7 = OC4A (Leonardo/Micro)
const int tasterNullPin = 10;
const int tasterEndPin = 12;
const int sensorPin = A0;

const int EE_ADR_NULL = 0;
const int EE_ADR_END = 4;

const int NULLWERT_DEFAULT = 70;    // 1 uSv/h   = 0.34 V
const int ENDWERT_DEFAULT = 327;    // 9.9 mSv/h = 1.6 V
const int MIN_SPANNE = 20;          // minimaler Abstand Null-/Endwert in Digit

const float PULS_MAX = 8500.0;      // Pulse/s bei Endwert
const uint16_t TICKS_PRO_S = 50000; // ISR alle 20 us

const unsigned long ENTPRELLZEIT = 500; // ms

int nullwert_in, endwert_in;

const int numReadings = 20;
int readings[numReadings];          // die letzten Messwerte von A0
int readIndex = 0;
long total = 0;
float average = 0;

volatile uint16_t rate = 1;         // Pulse pro Sekunde, wird von der ISR gelesen


// Taster mit Entprellung, liefert true genau einmal pro Druck
struct Taster {
  int pin;
  bool zustand;          // entprellter Zustand (true = gedrueckt)
  bool letzterRoh;
  unsigned long zeit;
};

Taster tasterNull = { tasterNullPin, false, false, 0 };
Taster tasterEnd = { tasterEndPin, false, false, 0 };

bool gedrueckt(Taster &t)
{
  bool roh = !digitalRead(t.pin);
  if (roh != t.letzterRoh) {
    t.letzterRoh = roh;
    t.zeit = millis();
  }
  if ((millis() - t.zeit) >= ENTPRELLZEIT && roh != t.zustand) {
    t.zustand = roh;
    return roh;          // nur die Flanke "losgelassen -> gedrueckt"
  }
  return false;
}


bool kalibrierungGueltig(int nullwert, int endwert)
{
  return nullwert >= 0 && endwert <= 1023 && (endwert - nullwert) >= MIN_SPANNE;
}


void setup(void)
{
  pinMode(tasterNullPin, INPUT_PULLUP);
  pinMode(tasterEndPin, INPUT_PULLUP);
  pinMode(takt, OUTPUT);
  digitalWrite(takt, 0);

  Serial.begin(115200);

  // Kalibrierung: fest auf die Standardwerte
  // (EEPROM wird gelesen, die Werte werden aber nicht uebernommen)
  EEPROM.get(EE_ADR_NULL, nullwert_in);
  EEPROM.get(EE_ADR_END, endwert_in);
  //if (!kalibrierungGueltig(nullwert_in, endwert_in) ) {
    nullwert_in = NULLWERT_DEFAULT;
    endwert_in = ENDWERT_DEFAULT;
  //}

  // Mittelwert mit aktuellem Messwert vorbelegen (kein Hochlaufen von 0)
  int start = analogRead(sensorPin);
  for (int i = 0; i < numReadings; i++) {
    readings[i] = start;
  }
  total = (long)start * numReadings;
  average = start;

  // Timer4 synchron zum Systemtakt (PLLTM = 0)
  PLLFRQ &= (1<<PINMUX) | (1<<PLLUSB) | (0<<PLLTM1) | (0<<PLLTM0) | (1<<PDIV3) | (1<<PDIV2) | (1<<PDIV1) | (1<<PDIV0);

  // Normal Mode, Prescaler 64 -> 250 kHz (4 us pro Tick)
  // OC4A: "Clear on Compare Match" -> Pin 13 bleibt LOW bis ein Puls faellig ist
  TCCR4A = (1<<COM4A1);
  TCCR4B = (1<<CS42) | (1<<CS41) | (1<<CS40);
  TCCR4C = 0;
  TCCR4D = 0;
  TCCR4E = 0;

  // TOP = OCR4C = 4 -> Zyklus von 5 Ticks = 20 us
  // Compare Match A einmal pro Zyklus (bei TCNT4 = 2)
  TC4H = 0;
  OCR4C = 4;
  TC4H = 0;
  OCR4A = 2;
  TC4H = 0;
  TCNT4 = 0;

  TIMSK4 = (1<<OCIE4A);
  sei();
}


void loop(void)
{
  // Kalibrierung per Taster, gespeichert wird der Mittelwert
  if (gedrueckt(tasterNull)) {
    int neu = (int)(average + 0.5);
    if (kalibrierungGueltig(neu, endwert_in)) {
      nullwert_in = neu;
      EEPROM.put(EE_ADR_NULL, nullwert_in);
    }
  }

  if (gedrueckt(tasterEnd)) {
    int neu = (int)(average + 0.5);
    if (kalibrierungGueltig(nullwert_in, neu)) {
      endwert_in = neu;
      EEPROM.put(EE_ADR_END, endwert_in);
    }
  }

  // gleitender Mittelwert ueber numReadings Messungen
  total = total - readings[readIndex];
  readings[readIndex] = analogRead(sensorPin);
  total = total + readings[readIndex];
  readIndex = readIndex + 1;
  if (readIndex >= numReadings) {
    readIndex = 0;
  }
  average = (float)total / numReadings;

  // Ausgangssignal berechnen ==========================================
  // anteil = 0 .. 1 zwischen Null- und Endwert
  // Pulsrate = PULS_MAX ^ anteil  ->  1 .. 8500 Pulse/s (logarithmisch)
  float wert = average;
  if (wert < nullwert_in) wert = nullwert_in;
  if (wert > endwert_in) wert = endwert_in;
  float anteil = (wert - nullwert_in) / (float)(endwert_in - nullwert_in);

  float pulse = pow(PULS_MAX, anteil);
  uint16_t r = (uint16_t)(pulse + 0.5);
  if (r < 1) r = 1;

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    rate = r;
  }

#ifdef DEBUG
  static unsigned long letzteAusgabe = 0;
  if (millis() - letzteAusgabe >= 500) {
    letzteAusgabe = millis();
    Serial.print("Null: ");    Serial.print(nullwert_in);
    Serial.print("  End: ");   Serial.print(endwert_in);
    Serial.print("  avg: ");   Serial.print(average);
    Serial.print("  Pulse/s: "); Serial.println(r);
  }
#endif
}


// Timer4 Compare Match A alle 20 us (50 kHz)
// Die Hardware hat die Flanke an Pin 13 bereits geschaltet. Hier wird nur
// festgelegt, was beim naechsten Compare Match (in genau 20 us) passiert:
//   Puls faellig -> Pin setzen, sonst -> Pin loeschen
// Ein Puls ist damit genau ein Zyklus = 20 us lang, unabhaengig von der
// Interrupt-Latenz (die ISR muss nur innerhalb von 20 us fertig sein).
// Phasen-Akkumulator: pro Sekunde werden genau 'rate' Pulse erzeugt.
ISR(TIMER4_COMPA_vect)
{
  static uint16_t acc = 0;

  acc += rate;                 // max. 49999 + 8500 < 65535, kein Ueberlauf
  if (acc >= TICKS_PRO_S) {
    acc -= TICKS_PRO_S;
    TCCR4A = (1<<COM4A1) | (1<<COM4A0);   // naechster Match: Pin 13 HIGH
  } else {
    TCCR4A = (1<<COM4A1);                 // naechster Match: Pin 13 LOW
  }
}
