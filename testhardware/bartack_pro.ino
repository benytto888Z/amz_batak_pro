/**********************************************************************
 *  BATAK PRO - VERSION 1 (12 boutons LED)
 *  Carte    : Arduino MEGA WiFi R3 (ATmega2560 + ESP8266 integre)
 *             -> DIP switches en mode "Mega seul" : 3=ON 4=ON, reste OFF
 *             -> L'ESP8266 n'est PAS utilise dans la V1 (reserve V2)
 *  Afficheur: 6x MAX7219 8x8 (FC-16) EN UNE SEULE CHAINE, 2 zones :
 *             - zone SCORE = 4 modules (gauche)  -> score 4 digits "0078"
 *             - zone TEMPS = 2 modules (droite)  -> temps 2 digits "08"
 *  Entrees  : telecommande IR (NEC) broche 47 + 12 boutons arcade
 *  Sons     : DFPlayer Mini (carte SD, fichiers MP3) sur Serial1
 *  Memoire  : TOP 3 scores de chacun des 4 modes en EEPROM
 *
 *  MENU (repos) :
 *   - Zone score (gauche) : nom du jeu selectionne qui defile
 *       "BARTACK CLASSIC-30s" / "BARTACK FITNESS-30s"
 *       "BARTACK CLASSIC-60s" / "BARTACK FITNESS-60s"
 *   - Zone temps (droite) : aide qui defile
 *       "1=Classic 2=Fitness 3=30s 6=60s OK=Start"
 *   - Touche 1 = type CLASSIC     Touche 2 = type FITNESS
 *     Touche 3 = duree 30 s       Touche 6 = duree 60 s
 *     Touche OK/PLAY = demarrer
 *
 *  DEROULEMENT :
 *   - OK -> les 3 meilleurs scores du mode choisi s'affichent 5 s
 *     (1er, 2e, 3e), puis compte a rebours 3-2-1 (MP3) -> GO !
 *   - CLASSIC : un bouton s'allume au hasard. Touche -> +1 point ->
 *     suivant. Trop lent -> la lumiere change toute seule.
 *     Vitesse : 30 s -> +vite toutes les 10 s ; 60 s -> toutes les 20 s.
 *   - FITNESS : *** BRECHE V2 *** (voir playFitnessStep) - provisoirement
 *     identique a CLASSIC, sera code etape par etape avec l'app Flutter.
 *   - Fin : si le joueur entre dans le TOP 3 -> message de felicitations
 *     anime (defilement) + son "record" ; sinon score clignotant + fanfare.
 *     Affichage 15 s puis retour au menu. TOP 3 sauvegarde en EEPROM.
 *
 *  CARTE SD du DFPlayer : dossier /mp3, fichiers :
 *   0001.mp3 bip touche | 0002.mp3 "3,2,1" (~3 s) | 0003.mp3 "GO !"
 *   0004.mp3 frappe (court !) | 0005.mp3 fanfare | 0006.mp3 "Nouveau record !"
 *
 *  Bibliotheques : IRremote (v4), MD_Parola, MD_MAX72XX, DFRobotDFPlayerMini
 **********************************************************************/

#include <IRremote.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <EEPROM.h>
#include "DFRobotDFPlayerMini.h"

/* =================== BROCHAGE (MEGA WiFi R3) ======================= */
// 12 boutons : contact vers GND (INPUT_PULLUP, aucune resistance)
const uint8_t BTN_PIN[12] = {22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33};

// 12 LEDs de boutons : broche -> 220R -> LED -> GND (ou transistor si 12V)
const uint8_t LED_PIN[12] = {34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45};

const uint8_t IR_RECEIVE_PIN = 47;   // VS1838B / TSOP38238 OUT

// DFPlayer Mini sur Serial1 : TX1=18 -> (1k) -> RX DFPlayer ; RX1=19 <- TX
// (Serial3 = 14/15 reste LIBRE : liaison interne ESP8266 pour la V2)
#define DFP_SERIAL Serial1

// 6x MAX7219 EN UNE SEULE CHAINE : DIN=51, CLK=52, CS=53
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
const uint8_t DISP_CS      = 53;
const uint8_t NUM_MODULES  = 6;
MD_Parola disp(HARDWARE_TYPE, DISP_CS, NUM_MODULES);
const uint8_t ZONE_TIME  = 0;        // -> temps/menu (modules 4-5)
const uint8_t ZONE_SCORE = 1;        // -> score/nom du jeu (modules 0-3)

const uint8_t  DISPLAY_BRIGHT = 6;   // 0..15
const uint16_t SCROLL_SPEED   = 50;  // ms/pixel (defilement)

/* =================== SONS (pistes SD) ============================== */
DFRobotDFPlayerMini dfp;
bool dfpOk = false;

const uint8_t    SND_HIT = 1;
const uint8_t SND_COUNT   = 2;
const uint8_t SND_GO = 3;
const uint8_t    SND_KEY  = 4;
const uint8_t SND_SUCCESS = 5;
const uint8_t SND_RECORD  = 6;
const uint8_t DFP_VOLUME  = 25;      // 0..30

void playSound(uint8_t track) {
  if (dfpOk) dfp.play(track);
 // dfp.pause();
}

/* =================== CODES IR (NEC) ================================ */
// Codes releves sur VOTRE telecommande (via ir_code_finder).
const uint8_t IR_KEY_1  = 69;        // type CLASSIC
const uint8_t IR_KEY_2  = 70;        // type FITNESS
const uint8_t IR_KEY_3  = 71;        // duree 30 s
const uint8_t IR_KEY_6  = 67;        // duree 60 s
const uint8_t IR_KEY_OK = 28;        // demarrer (touche OK/PLAY)

/* =================== REGLAGES JEU ================================== */
const uint16_t LIGHT_ON_MS[3] = {1500, 1000, 650}; // duree lumiere / phase
const uint16_t GAP_MS[3]      = {300, 220, 140};   // pause noire / phase
const uint16_t TOP3_SHOW_MS     = 5000;            // top 3 pendant 5 s
const uint16_t GAMEOVER_SHOW_MS = 15000;           // resultat 15 s

/* =================== EEPROM : TOP 3 x 4 modes ====================== */
// Modes : 0=CLASSIC-30  1=CLASSIC-60  2=FITNESS-30  3=FITNESS-60
const int     EE_MAGIC_ADDR = 0;
const uint8_t EE_MAGIC      = 0x43;  // changer pour effacer les scores
const int     EE_TOP3_ADDR  = 2;     // 4 modes x 3 x uint16 = 24 octets

uint16_t top3[4][3];                 // [mode][rang 0..2]

/* =================== ETAT ========================================== */
enum GameState { ST_IDLE, ST_TOP3, ST_COUNTDOWN, ST_PLAY, ST_GAMEOVER };
enum GameType  { TYPE_CLASSIC = 0, TYPE_FITNESS = 1 };

GameState state    = ST_IDLE;
GameType  gameType = TYPE_CLASSIC;   // le "gamechallenge" selectionne
uint8_t   modeSeconds = 30;          // 30 ou 60
uint16_t  score = 0;
int8_t    finalRank = -1;            // 0..2 si TOP 3, sinon -1

unsigned long roundStartMs = 0, lightOnMs = 0, gapStartMs = 0;
unsigned long stateStartMs = 0, lastFlashMs = 0;
int8_t  litIndex = -1, lastLit = -1, countStep = 0;
bool    inGap = false, flashOn = true;
int     lastShownSec = -1;
int8_t  top3Shown = -1;              // rang en cours d'affichage (0..2)

// Tampons statiques : MD_Parola garde un pointeur sur le texte !
char scoreText[40], timeText[48];

const char MENU_HELP[] = "1=Classic 2=Fitness 3=30s 6=60s OK=Start";

/* =================== AIDES ========================================= */
uint8_t modeIndex() {                // 0..3
  return (uint8_t)gameType * 2 + (modeSeconds == 60 ? 1 : 0);
}

const char *typeName() {
  return (gameType == TYPE_FITNESS) ? "FITNESS" : "CLASSIC";
}

void allLedsOff() {
  for (uint8_t i = 0; i < 12; i++) digitalWrite(LED_PIN[i], LOW);
}

/* ---- affichage fixe ---- */
void printScoreZone(const char *t) {
  strncpy(scoreText, t, sizeof(scoreText) - 1);
  scoreText[sizeof(scoreText) - 1] = 0;
  disp.displayZoneText(ZONE_SCORE, scoreText, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
}
void printTimeZone(const char *t) {
  strncpy(timeText, t, sizeof(timeText) - 1);
  timeText[sizeof(timeText) - 1] = 0;
  disp.displayZoneText(ZONE_TIME, timeText, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
}
void showScore(uint16_t v) {
  char b[8]; snprintf(b, sizeof(b), "%04u", v);   // 4 digits : "0078"
  printScoreZone(b);
}
void showTimeSec(int sec) {
  char b[6]; snprintf(b, sizeof(b), "%02d", sec); // 2 digits : "08"
  printTimeZone(b);
}

/* ---- affichage defilant ---- */
void scrollScoreZone(const char *t) {
  strncpy(scoreText, t, sizeof(scoreText) - 1);
  scoreText[sizeof(scoreText) - 1] = 0;
  disp.displayZoneText(ZONE_SCORE, scoreText, PA_RIGHT, SCROLL_SPEED, 0,
                       PA_SCROLL_RIGHT, PA_SCROLL_RIGHT);
}
void scrollTimeZone(const char *t) {
  strncpy(timeText, t, sizeof(timeText) - 1);
  timeText[sizeof(timeText) - 1] = 0;
  disp.displayZoneText(ZONE_TIME, timeText, PA_RIGHT, SCROLL_SPEED, 0,
                       PA_SCROLL_RIGHT, PA_SCROLL_RIGHT);
}

/* =================== TOP 3 (EEPROM) ================================ */
void loadTop3() {
  if (EEPROM.read(EE_MAGIC_ADDR) != EE_MAGIC) {   // 1er demarrage
    memset(top3, 0, sizeof(top3));
    EEPROM.put(EE_TOP3_ADDR, top3);
    EEPROM.write(EE_MAGIC_ADDR, EE_MAGIC);
  } else {
    EEPROM.get(EE_TOP3_ADDR, top3);
  }
}

void saveTop3() { EEPROM.put(EE_TOP3_ADDR, top3); }

// Insere le score dans le TOP 3 du mode courant.
// Retourne le rang obtenu (0=1er, 1=2e, 2=3e) ou -1.
int8_t insertScore(uint16_t s) {
  uint8_t m = modeIndex();
  for (uint8_t i = 0; i < 3; i++) {
    if (s > top3[m][i]) {
      for (uint8_t j = 2; j > i; j--) top3[m][j] = top3[m][j - 1];
      top3[m][i] = s;
      saveTop3();
      return i;
    }
  }
  return -1;
}

/* =================== LOGIQUE JEU =================================== */
// Phase de vitesse : 0,1,2 (30s -> toutes les 10s, 60s -> toutes les 20s)
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
  digitalWrite(LED_PIN[litIndex], HIGH);
  lightOnMs = millis();
  inGap = false;
}

void startGap() {
  if (litIndex >= 0) digitalWrite(LED_PIN[litIndex], LOW);
  litIndex = -1;
  inGap = true;
  gapStartMs = millis();
}

/* ---- fin de manche (commune aux deux challenges) ---- */
void endRound() {
  allLedsOff();
  finalRank = insertScore(score);            // TOP 3 ? (sauve EEPROM)
  state = ST_GAMEOVER;
  stateStartMs = millis();
  lastFlashMs  = millis();
  flashOn = true;

  if (finalRank >= 0) {                      // *** FELICITATIONS ***
    playSound(SND_RECORD);
    char b[40];
    snprintf(b, sizeof(b), "BRAVO ! TOP %d - %u PTS", finalRank + 1, score);
    scrollScoreZone(b);                      // message anime
    printTimeZone("HI");
  } else {
    playSound(SND_SUCCESS);
    showScore(score);
    printTimeZone("00");
  }
  Serial.print(F("Fin. Score = ")); Serial.println(score);
  if (finalRank >= 0) {
    Serial.print(F("*** TOP ")); Serial.print(finalRank + 1);
    Serial.println(F(" - sauvegarde ***"));
  }
}

/* ==================================================================
 *  CHALLENGE CLASSIC (version 1) - un pas de boucle de la manche
 * ================================================================== */
void playClassicStep() {
  unsigned long now = millis();
  long remainMs = (long)modeSeconds * 1000L - (long)(now - roundStartMs);

  int remainSec = (remainMs > 0) ? (remainMs + 999) / 1000 : 0;
  if (remainSec != lastShownSec) {
    lastShownSec = remainSec;
    showTimeSec(remainSec);
  }

  if (remainMs <= 0) {                       // manche terminee
    endRound();
    return;
  }

  uint8_t ph = currentPhase();

  if (inGap) {
    if (now - gapStartMs >= GAP_MS[ph]) pickNewLight();
  } else {
    if (digitalRead(BTN_PIN[litIndex]) == LOW) {     // touche !
      score++;
      playSound(SND_HIT);
      showScore(score);
      startGap();
    } else if (now - lightOnMs >= LIGHT_ON_MS[ph]) { // trop lent
      startGap();
    }
  }
}

/* ==================================================================
 *  CHALLENGE FITNESS (version 2) - *** BRECHE : A CODER ***
 *  Sera developpe etape par etape avec l'app Flutter (Wi-Fi ESP8266,
 *  work/rest, courbes de vitesse, telemetrie temps de reaction...).
 *  PROVISOIRE : joue la meme manche que CLASSIC pour que le mode
 *  reste utilisable en attendant la V2.
 * ================================================================== */
void playFitnessStep() {
  // TODO V2 :
  //  - recevoir la config du challenge depuis l'app (Serial3/ESP8266)
  //  - intervalles work/rest, cibles multiples, duree libre
  //  - envoyer hit/miss + temps de reaction a l'app en direct
  playClassicStep();                         // provisoire (V1)
}

/* =================== MENU (repos) ================================== */
void enterIdle() {
  state = ST_IDLE;
  allLedsOff();
  // Gauche : nom du jeu selectionne, qui defile
  char b[40];
  snprintf(b, sizeof(b), "BARTACK %s-%us", typeName(), modeSeconds);
  scrollScoreZone(b);
  // Droite : aide des touches, qui defile
  scrollTimeZone(MENU_HELP);
}

/* =================== SETUP ========================================= */
void setup() {
  Serial.begin(115200);              // moniteur serie (USB)

  for (uint8_t i = 0; i < 12; i++) {
    pinMode(BTN_PIN[i], INPUT_PULLUP);
    pinMode(LED_PIN[i], OUTPUT);
    digitalWrite(LED_PIN[i], LOW);
  }

  // --- DFPlayer Mini ---
  DFP_SERIAL.begin(9600);
  if (dfp.begin(DFP_SERIAL, /*isACK=*/true, /*doReset=*/true)) {
    dfpOk = true;
    dfp.volume(DFP_VOLUME);
    Serial.println(F("DFPlayer OK"));
  } else {
    Serial.println(F("ATTENTION : DFPlayer introuvable (jeu silencieux)"));
  }

         /* playSound(SND_COUNT);        // piste "3, 2, 1" (~3 s)
        Serial.println(F("Compte a rebours..."));
        delay(3000);*/

  // --- Afficheur : 1 chaine, 2 zones (config materielle validee) ---
  disp.begin(2);
  disp.setZone(ZONE_SCORE, 0, 3);    // 4 modules (gauche)
  disp.setZone(ZONE_TIME,  4, 5);    // 2 modules (droite)

  // Modules montes a l'envers -> rotation 180 des deux zones
  disp.setZoneEffect(ZONE_SCORE, true, PA_FLIP_UD);
  disp.setZoneEffect(ZONE_SCORE, true, PA_FLIP_LR);
  disp.setZoneEffect(ZONE_TIME,  true, PA_FLIP_UD);
  disp.setZoneEffect(ZONE_TIME,  true, PA_FLIP_LR);
  disp.setIntensity(DISPLAY_BRIGHT);
  disp.displayClear();

  // --- IR ---
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);

  randomSeed(analogRead(A0));        // laisser A0 en l'air
  loadTop3();

  Serial.println(F("BATAK V1 pret. 1=Classic 2=Fitness 3=30s 6=60s OK=Start"));
  enterIdle();
}

/* =================== IR ============================================ */
void handleIR() {
  if (!IrReceiver.decode()) return;
  uint8_t cmd = IrReceiver.decodedIRData.command;
  bool repeat = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;
  Serial.println(cmd);
  IrReceiver.resume();
  if (repeat) return;

  if (state != ST_IDLE) return;      // touches actives seulement au menu

  if (cmd == IR_KEY_1) {
    gameType = TYPE_CLASSIC; playSound(SND_KEY); enterIdle();
    Serial.println(F("Type : CLASSIC"));
  } else if (cmd == IR_KEY_2) {
    gameType = TYPE_FITNESS; playSound(SND_KEY); enterIdle();
    Serial.println(F("Type : FITNESS"));
  } else if (cmd == IR_KEY_3) {
    modeSeconds = 30; playSound(SND_KEY); enterIdle();
    Serial.println(F("Duree : 30 s"));
  } else if (cmd == IR_KEY_6) {
    modeSeconds = 60; playSound(SND_KEY); enterIdle();
    Serial.println(F("Duree : 60 s"));
  } else if (cmd == IR_KEY_OK) {
    // ---- afficher le TOP 3 du mode pendant 5 s ----
    state = ST_TOP3;
    stateStartMs = millis();
    top3Shown = -1;                  // forcera l'affichage du rang 1
    playSound(SND_KEY);
    printTimeZone("T3");
    Serial.print(F("TOP 3 du mode ")); Serial.print(typeName());
    Serial.print('-'); Serial.print(modeSeconds); Serial.println('s');
  }
}

/* =================== BOUCLE PRINCIPALE ============================= */
void loop() {
  handleIR();
  disp.displayAnimate();

  switch (state) {

    /* ---------- menu : defilement en boucle ---------- */
    case ST_IDLE:
      if (disp.getZoneStatus(ZONE_SCORE)) disp.displayReset(ZONE_SCORE);
      if (disp.getZoneStatus(ZONE_TIME))  disp.displayReset(ZONE_TIME);
      break;

    /* ---------- TOP 3 pendant 5 s ---------- */
    case ST_TOP3: {
      unsigned long now = millis();
      // 3 rangs affiches tour a tour (~1,66 s chacun)
      int8_t rankToShow = (now - stateStartMs) / (TOP3_SHOW_MS / 3);
      if (rankToShow > 2) rankToShow = 2;
      if (rankToShow != top3Shown) {
        top3Shown = rankToShow;
        char b[16];
        snprintf(b, sizeof(b), "%d.%04u", rankToShow + 1,
                 top3[modeIndex()][rankToShow]);
        printScoreZone(b);
      }
      if (now - stateStartMs >= TOP3_SHOW_MS) {
        // ---- compte a rebours ----
        state = ST_COUNTDOWN;
        countStep = 3;
        stateStartMs = now;
        score = 0;
        showScore(score);
        printTimeZone("03");
      
        playSound(SND_COUNT);        // piste "3, 2, 1" (~3 s)
        Serial.println(F("Compte a rebours..."));
        delay(3000);
      }
      break;
    }

    /* ---------- 3..2..1..GO ---------- */
    case ST_COUNTDOWN:
      if (millis() - stateStartMs >= 1000) {
        stateStartMs = millis();
        countStep--;
        if (countStep > 0) {
          char b[6]; snprintf(b, sizeof(b), "%02d", countStep);
          printTimeZone(b);
        } else {
          playSound(SND_GO);
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

    /* ---------- la manche : CLASSIC ou FITNESS ---------- */
    case ST_PLAY:
      if (gameType == TYPE_CLASSIC) {
        // ============ VERSION 1 : CHALLENGE CLASSIC ============
        playClassicStep();
      } else {
        // ============ VERSION 2 : CHALLENGE FITNESS ============
        // (breche : a coder etape par etape - voir playFitnessStep)
        playFitnessStep();
      }
      break;

    /* ---------- resultat 15 s puis retour menu ---------- */
    case ST_GAMEOVER: {
      unsigned long now = millis();

      if (finalRank >= 0) {
        // felicitations : defilement en boucle + "HI" clignotant
        if (disp.getZoneStatus(ZONE_SCORE)) disp.displayReset(ZONE_SCORE);
        if (now - lastFlashMs >= 500) {
          lastFlashMs = now;
          flashOn = !flashOn;
          printTimeZone(flashOn ? "HI" : "  ");
        }
      } else {
        // pas de record : score clignotant
        if (now - lastFlashMs >= 500) {
          lastFlashMs = now;
          flashOn = !flashOn;
          if (flashOn) showScore(score);
          else         printScoreZone("    ");
        }
      }

      if (now - stateStartMs >= GAMEOVER_SHOW_MS) {
        Serial.println(F("Retour au menu."));
        enterIdle();
      }
      break;
    }
  }
}
