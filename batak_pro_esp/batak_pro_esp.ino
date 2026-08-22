/**********************************************************************
 *  BATAK PRO - Jeu de reflexes (12 boutons LED)  --  VERSION ESP32
 *  Carte    : ESP32 DevKit V1 30 broches (WROOM)
 *             (compatible aussi 38 broches : memes GPIO utilises)
 *  Afficheur: 6x MAX7219 8x8 (FC-16) EN UNE SEULE CHAINE, 2 zones :
 *             - zone SCORE = 4 modules (gauche)
 *             - zone TEMPS = 2 modules (droite, cote ESP32)
 *  E/S      : PCF8574 (I2C) -> LEDs 1 a 8 (actives a l'etat BAS)
 *             GPIO directs  -> LEDs 9 a 12, 12 boutons, IR, buzzer
 *  Memoire  : meilleurs scores 30s/60s en NVS (Preferences)
 *
 *  JEU :
 *   - Repos : touche 1 = mode 30 s, touche 2 = mode 60 s.
 *             Le score affiche = record du mode choisi.
 *   - PLAY  : compte a rebours 3-2-1 puis la manche demarre.
 *   - Un bouton s'allume au hasard. Touche -> +1 point -> suivant.
 *     Trop lent -> la lumiere change toute seule.
 *   - Vitesse : mode 30 s -> +vite toutes les 10 s
 *               mode 60 s -> +vite toutes les 20 s
 *   - Fin : melodie, score clignote 15 s, record sauvegarde, retour repos.
 *
 *  Bibliotheques (gestionnaire de bibliotheques) :
 *   - IRremote (v4.x)  - MD_Parola  - MD_MAX72XX
 **********************************************************************/

#include <IRremote.hpp>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <Wire.h>
#include <Preferences.h>

/* =================== BROCHAGE (ESP32 38 broches) =================== */
// 12 boutons : contact vers GND.
// Boutons 1-9 : pull-up interne. Boutons 10-12 (GPIO 34/36/39) :
// broches "entree seule" -> resistance externe 10k vers 3.3V OBLIGATOIRE.
const uint8_t BTN_PIN[12] = {13, 14, 15, 16, 19, 25, 26, 27, 32, 34, 36, 39};

// LEDs 1-8  -> PCF8574 P0..P7 (actives LOW : anode->3V3 via 150R, cathode->Pn)
// LEDs 9-12 -> GPIO directs (GPIO -> 220R -> LED -> GND)
const uint8_t LED_GPIO[4] = {2, 4, 12, 17};   // LEDs 9,10,11,12

const uint8_t IR_RECEIVE_PIN = 35;  // VS1838B OUT (entree seule : OK)
const uint8_t BUZZER_PIN     = 33;  // buzzer passif (+)

// I2C du PCF8574
const uint8_t I2C_SDA = 21, I2C_SCL = 22;

// Chaine UNIQUE de 6 MAX7219 : DIN=23 (MOSI), CLK=18 (SCK), CS=5
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
const uint8_t DISP_CS = 5;
const uint8_t NUM_MODULES = 6;
MD_Parola disp(HARDWARE_TYPE, DISP_CS, NUM_MODULES);
// Zones : module 0 = celui branche a l'ESP32 (extremite DROITE)
const uint8_t ZONE_TIME  = 0;   // modules 0-1 (droite)  -> temps
const uint8_t ZONE_SCORE = 1;   // modules 2-5 (gauche)  -> score

/* =================== CODES IR (NEC) ================================ */
// Telecommande 21 touches "kit MP3". Sinon : croquis ir_code_finder.
const uint8_t IR_KEY_MODE30 = 0x0C;   // touche "1"  -> mode 30 s
const uint8_t IR_KEY_MODE60 = 0x18;   // touche "2"  -> mode 60 s
const uint8_t IR_KEY_START  = 0x43;   // touche "PLAY" -> demarrer

/* =================== REGLAGES JEU ================================== */
const uint16_t LIGHT_ON_MS[3] = {1500, 1000, 650}; // duree lumiere / phase
const uint16_t GAP_MS[3]      = {300, 220, 140};   // pause noire / phase
const uint16_t GAMEOVER_SHOW_MS = 15000;           // resultat 15 s
const uint8_t  DISPLAY_BRIGHT   = 6;               // 0..15

/* =================== ETAT ========================================== */
enum GameState { ST_IDLE, ST_COUNTDOWN, ST_PLAY, ST_GAMEOVER };
GameState state = ST_IDLE;

Preferences prefs;
uint8_t  modeSeconds = 30;
uint16_t score = 0, best30 = 0, best60 = 0;
bool     newRecord = false;

unsigned long roundStartMs = 0, lightOnMs = 0, gapStartMs = 0;
unsigned long stateStartMs = 0, lastFlashMs = 0;
int8_t  litIndex = -1, lastLit = -1, countStep = 0;
bool    inGap = false, flashOn = true;
int     lastShownSec = -1;

char scoreBuf[8], timeBuf[8];        // tampons statiques pour Parola

/* =================== PCF8574 ======================================= */
uint8_t pcfAddr = 0;                 // detecte au demarrage
uint8_t pcfMask = 0xFF;              // 0xFF = toutes LEDs eteintes

void pcfWrite(uint8_t v) {
  if (!pcfAddr) return;
  Wire.beginTransmission(pcfAddr);
  Wire.write(v);
  Wire.endTransmission();
}

void pcfDetect() {
  const uint8_t tries[] = {0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
                           0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F};
  for (uint8_t a : tries) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { pcfAddr = a; break; }
  }
  if (pcfAddr) {
    Serial.printf("PCF8574 trouve a l'adresse 0x%02X\n", pcfAddr);
    pcfWrite(0xFF);                  // tout eteint
  } else {
    Serial.println("ATTENTION : PCF8574 introuvable (LEDs 1-8 inactives)");
  }
}

/* =================== LEDs =========================================== */
void setLed(int8_t idx, bool on) {
  if (idx < 0) return;
  if (idx < 8) {                                  // via PCF8574, actif LOW
    if (on) pcfMask &= ~(1 << idx); else pcfMask |= (1 << idx);
    pcfWrite(pcfMask);
  } else {                                        // GPIO direct, actif HIGH
    digitalWrite(LED_GPIO[idx - 8], on ? HIGH : LOW);
  }
}

void allLedsOff() {
  pcfMask = 0xFF; pcfWrite(pcfMask);
  for (uint8_t i = 0; i < 4; i++) digitalWrite(LED_GPIO[i], LOW);
}

/* =================== AFFICHAGE ====================================== */
void showScore(uint16_t v) {
  snprintf(scoreBuf, sizeof(scoreBuf), "%4u", v);
  disp.displayZoneText(ZONE_SCORE, scoreBuf, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
}
void showScoreText(const char *t) {
  snprintf(scoreBuf, sizeof(scoreBuf), "%s", t);
  disp.displayZoneText(ZONE_SCORE, scoreBuf, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
}
void showTime(int sec) {
  snprintf(timeBuf, sizeof(timeBuf), "%2d", sec);
  disp.displayZoneText(ZONE_TIME, timeBuf, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
}
void showTimeText(const char *t) {
  snprintf(timeBuf, sizeof(timeBuf), "%s", t);
  disp.displayZoneText(ZONE_TIME, timeBuf, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
}

/* =================== RECORDS (NVS) ================================== */
uint16_t &bestForMode() { return (modeSeconds == 30) ? best30 : best60; }

void saveBest() {
  if (modeSeconds == 30) prefs.putUShort("best30", best30);
  else                   prefs.putUShort("best60", best60);
}

void loadBests() {
  best30 = prefs.getUShort("best30", 0);
  best60 = prefs.getUShort("best60", 0);
}

/* =================== LOGIQUE JEU ==================================== */
uint8_t currentPhase() {
  unsigned long elapsed = (millis() - roundStartMs) / 1000UL;
  uint8_t interval = (modeSeconds == 30) ? 10 : 20;
  uint8_t p = elapsed / interval;
  return (p > 2) ? 2 : p;
}

void pickNewLight() {
  int8_t n;
  do { n = random(12); } while (n == lastLit);   // jamais 2x le meme
  litIndex = n;  lastLit = n;
  setLed(litIndex, true);
  lightOnMs = millis();
  inGap = false;
}

void startGap() {
  if (litIndex >= 0) setLed(litIndex, false);
  litIndex = -1;
  inGap = true;
  gapStartMs = millis();
}

/* =================== SONS =========================================== */
void beepHit()   { tone(BUZZER_PIN, 2200, 40); }
void beepCount() { tone(BUZZER_PIN, 1000, 120); }
void beepGo()    { tone(BUZZER_PIN, 1800, 300); }
void beepKey()   { tone(BUZZER_PIN, 1400, 60); }

void successMelody() {                    // fanfare ~1.2 s (bloquante)
  const uint16_t f[] = {1047, 1319, 1568, 2093, 1568, 2093};
  const uint16_t d[] = {130, 130, 130, 260, 130, 420};
  for (uint8_t i = 0; i < 6; i++) { tone(BUZZER_PIN, f[i], d[i]); delay(d[i] + 25); }
  noTone(BUZZER_PIN);
}

/* =================== ECRAN DE REPOS ================================= */
void enterIdle() {
  state = ST_IDLE;
  allLedsOff();
  showScore(bestForMode());       // gauche : record du mode
  showTime(modeSeconds);          // droite : 30 ou 60
}

/* =================== SETUP ========================================== */
void setup() {
  Serial.begin(115200);

  for (uint8_t i = 0; i < 12; i++) {
    // GPIO >= 34 : entree seule, pas de pull-up interne (10k externe !)
    pinMode(BTN_PIN[i], (BTN_PIN[i] >= 34) ? INPUT : INPUT_PULLUP);
  }
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(LED_GPIO[i], OUTPUT);
    digitalWrite(LED_GPIO[i], LOW);
  }
  pinMode(BUZZER_PIN, OUTPUT);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  pcfDetect();

  disp.begin(2);                                 // 2 zones
  disp.setZone(ZONE_TIME,  0, 1);                // 2 modules cote ESP32
  disp.setZone(ZONE_SCORE, 2, 5);                // 4 modules suivants
  disp.setIntensity(DISPLAY_BRIGHT);
  disp.displayClear();

  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);

  randomSeed(esp_random());
  prefs.begin("batak", false);
  loadBests();

  Serial.println("BATAK PRO (ESP32) pret. 1=30s  2=60s  PLAY=demarrer");
  enterIdle();
}

/* =================== IR ============================================= */
void handleIR() {
  if (!IrReceiver.decode()) return;
  uint8_t cmd = IrReceiver.decodedIRData.command;
  bool repeat = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;
  IrReceiver.resume();
  if (repeat) return;

  if (state == ST_IDLE) {
    if (cmd == IR_KEY_MODE30) {
      modeSeconds = 30; beepKey(); enterIdle();
      Serial.println("Mode : 30 s");
    } else if (cmd == IR_KEY_MODE60) {
      modeSeconds = 60; beepKey(); enterIdle();
      Serial.println("Mode : 60 s");
    } else if (cmd == IR_KEY_START) {
      state = ST_COUNTDOWN;
      countStep = 3;
      stateStartMs = millis();
      score = 0;
      showScore(score);
      showTimeText(" 3");
      beepCount();
      Serial.println("Compte a rebours...");
    }
  }
}

/* =================== BOUCLE PRINCIPALE ============================== */
void loop() {
  handleIR();
  disp.displayAnimate();          // rafraichit les 2 zones

  switch (state) {

    case ST_IDLE:
      break;

    /* ---------- 3..2..1..GO ---------- */
    case ST_COUNTDOWN:
      if (millis() - stateStartMs >= 1000) {
        stateStartMs = millis();
        countStep--;
        if (countStep > 0) {
          char b[4]; snprintf(b, sizeof(b), "%2d", countStep);
          showTimeText(b);
          beepCount();
        } else {
          beepGo();
          state = ST_PLAY;
          roundStartMs = millis();
          lastShownSec = -1;
          lastLit = -1;
          showScore(score);
          startGap();
          gapStartMs = millis() - GAP_MS[0];     // 1re lumiere immediate
        }
      }
      break;

    /* ---------- la manche ---------- */
    case ST_PLAY: {
      unsigned long now = millis();
      long remainMs = (long)modeSeconds * 1000L - (long)(now - roundStartMs);

      int remainSec = (remainMs > 0) ? (remainMs + 999) / 1000 : 0;
      if (remainSec != lastShownSec) {
        lastShownSec = remainSec;
        showTime(remainSec);
      }

      if (remainMs <= 0) {                       // manche terminee
        allLedsOff();
        showTimeText(" 0");
        newRecord = false;
        if (score > bestForMode()) {
          bestForMode() = score;
          saveBest();
          newRecord = true;
        }
        successMelody();
        state = ST_GAMEOVER;
        stateStartMs = millis();
        lastFlashMs  = millis();
        flashOn = true;
        showScore(score);
        Serial.printf("Fin de manche. Score = %u\n", score);
        if (newRecord) Serial.println("*** NOUVEAU RECORD sauvegarde ***");
        break;
      }

      uint8_t ph = currentPhase();

      if (inGap) {
        if (now - gapStartMs >= GAP_MS[ph]) pickNewLight();
      } else {
        if (digitalRead(BTN_PIN[litIndex]) == LOW) {   // touche !
          score++;
          beepHit();
          showScore(score);
          startGap();
        } else if (now - lightOnMs >= LIGHT_ON_MS[ph]) { // trop lent
          startGap();
        }
      }
      break;
    }

    /* ---------- resultat 15 s puis reset ---------- */
    case ST_GAMEOVER: {
      unsigned long now = millis();

      if (now - lastFlashMs >= 500) {            // score clignotant
        lastFlashMs = now;
        flashOn = !flashOn;
        if (flashOn) {
          showScore(score);
          if (newRecord) showTimeText("HI");
        } else {
          showScoreText("    ");
          if (newRecord) showTimeText("  ");
        }
      }

      if (now - stateStartMs >= GAMEOVER_SHOW_MS) {
        Serial.println("Retour au repos.");
        enterIdle();
      }
      break;
    }
  }
}
