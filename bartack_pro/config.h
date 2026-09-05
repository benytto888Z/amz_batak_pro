/**********************************************************************
 *  config.h - AMZ BATAK PRO v2 (Mega)
 *  Broches, constantes, structures et etat global partages.
 **********************************************************************/
#pragma once
#include <Arduino.h>

/* =================== BROCHAGE (MEGA WiFi R3) ======================= */
// 12 boutons : contact vers GND (INPUT_PULLUP)
const uint8_t BTN_PIN[12] = {22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33};
// 12 LEDs de boutons
const uint8_t LED_PIN[12] = {34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45};

const uint8_t IR_RECEIVE_PIN = 47;   // VS1838B / TSOP38238 OUT

// DFPlayer Mini sur Serial1 (TX1=18 ->1k-> RX ; RX1=19 <- TX)
#define DFP_SERIAL Serial1
// Liaison ESP8266 (pont Wi-Fi) sur Serial3 - VALIDEE par le test de ref.
#define NET_SERIAL  Serial3
const uint32_t NET_BAUD = 115200;

/* =================== CODES IR (NEC) - votre telecommande =========== */
const uint8_t IR_KEY_1  = 69;        // type CLASSIC
const uint8_t IR_KEY_2  = 70;        // type FITNESS
const uint8_t IR_KEY_3  = 71;        // duree 30 s
const uint8_t IR_KEY_4  = 65;        // duree 30 s
const uint8_t IR_KEY_5  = 66;        // duree 30 s
const uint8_t IR_KEY_6  = 67;        // duree 60 s
const uint8_t IR_KEY_OK = 28;        // demarrer

/* =================== REGLAGES JEU CLASSIC ========================== */
const uint16_t LIGHT_ON_MS[3] = {1500, 1000, 650};
const uint16_t GAP_MS[3]      = {300, 220, 140};
const uint16_t TOP3_SHOW_MS     = 5000;
const uint16_t GAMEOVER_SHOW_MS = 15000;

/* =================== VERSION FIRMWARE ============================== */
const char FW_VERSION[] = "2.0-e3";

/* =================== ETATS & TYPES ================================= */
enum GameState { ST_IDLE, ST_TOP3, ST_COUNTDOWN, ST_PLAY, ST_GAMEOVER };
enum GameType  { TYPE_CLASSIC = 0, TYPE_FITNESS = 1 };

/* ---- programme fitness recu par "configure" (amendement A1) ---- */
struct RoundCfg {
  uint16_t workSec, restSec, lightOnMs, gapMs;
  uint8_t  accel;
};
const uint8_t MAX_ROUNDS = 10;
struct FitnessProgram {
  char     mode[16];                 // quickReaction|speed30|...|hiit
  char     name[24];
  uint8_t  nRounds;
  RoundCfg rounds[MAX_ROUNDS];
  uint8_t  simultaneous;
  bool     valid;
};

/* =================== ETAT GLOBAL DU JEU ============================ */
GameState state    = ST_IDLE;
GameType  gameType = TYPE_CLASSIC;
uint8_t   modeSeconds = 30;
uint16_t  score = 0;
int8_t    finalRank = -1;

unsigned long roundStartMs = 0, lightOnMs = 0, gapStartMs = 0;
unsigned long stateStartMs = 0, lastFlashMs = 0;
int8_t  litIndex = -1, lastLit = -1, countStep = 0;
bool    inGap = false, flashOn = true;
int     lastShownSec = -1;
int8_t  top3Shown = -1;

FitnessProgram fitnessProg = {"", "", 0, {}, 1, false};

/* ---- statistiques de manche (amendements A3/A4) ---- */
struct RoundStats {
  uint16_t hits, misses;
  uint32_t sumReact, sumReactSq;
  uint16_t minReact, maxReact;
  uint16_t thirdHits[3];
  uint32_t thirdSumReact[3];
  uint8_t  hitsPerSec[10];           // fenetre glissante 10 s -> hpm
};
RoundStats rs;

void statsReset() {
  memset(&rs, 0, sizeof(rs));
  rs.minReact = 0xFFFF;
}

uint8_t currentThird() {             // 0..2 selon l'avancement de la manche
  unsigned long total = (unsigned long)modeSeconds * 1000UL;
  unsigned long elapsed = millis() - roundStartMs;
  uint8_t t = (uint8_t)((elapsed * 3UL) / total);
  return (t > 2) ? 2 : t;
}

void statsAddHit(uint16_t reactMs) {
  rs.hits++;
  rs.sumReact   += reactMs;
  rs.sumReactSq += (uint32_t)reactMs * reactMs;
  if (reactMs < rs.minReact) rs.minReact = reactMs;
  if (reactMs > rs.maxReact) rs.maxReact = reactMs;
  uint8_t th = currentThird();
  rs.thirdHits[th]++;
  rs.thirdSumReact[th] += reactMs;
  uint16_t sec = (millis() - roundStartMs) / 1000UL;
  rs.hitsPerSec[sec % 10]++;
}

uint16_t statsHpm() {                // frappes/minute (fenetre 10 s)
  uint16_t s = 0;
  for (uint8_t i = 0; i < 10; i++) s += rs.hitsPerSec[i];
  return s * 6;
}
