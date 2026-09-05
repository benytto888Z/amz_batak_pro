/**********************************************************************
 *  AMZ BATAK PRO v2 - FIRMWARE MEGA (bartack_pro.ino)
 *  Carte : Arduino MEGA WiFi R3 (ATmega2560 + ESP8266 pont Wi-Fi)
 *
 *  ETAPE 3 : firmware modulaire + protocole JSON complet.
 *   config.h        broches, constantes, etat global, stats
 *   display.h       6x MAX7219 (1 chaine, 2 zones, rotation 180)
 *   sounds.h        DFPlayer Mini (Serial1)
 *   records.h       TOP 3 x 4 modes (EEPROM)
 *   net_link.h      protocole JSON app<->mur via Serial3 (pont ESP)
 *   game_classic.h  challenge CLASSIC (V1) + telemetrie
 *   game_fitness.h  challenge FITNESS pilote par l'app (rounds work/rest)
 *
 *  MENU (telecommande IR, inchange V1) :
 *   1=CLASSIC  2=FITNESS  3=30s  6=60s  OK=Start
 *   - CLASSIC : jeu autonome complet (aucune app requise).
 *   - FITNESS + app connectee : l'app configure et demarre le challenge
 *     (etats APP_WAIT -> APP_READY -> countdown -> play).
 *   - FITNESS sans app : fallback = manche type CLASSIC (V1).
 *
 *  Un client WebSocket peut aussi observer une partie CLASSIC :
 *  hit/miss/tick/gameOver sont diffuses des qu'une app a dit hello.
 *
 *  Bibliotheques : IRremote (v4), MD_Parola, MD_MAX72XX,
 *                  DFRobotDFPlayerMini, ArduinoJson (v6/v7)
 **********************************************************************/

#include <IRremote.h>
#include "config.h"
#include "display.h"
#include "sounds.h"
#include "records.h"
#include "net_link.h"
#include "game_classic.h"
#include "game_fitness.h"

/* =================== ETATS APP (fitness pilote) ==================== */
// Sous-etats de ST_IDLE quand gameType==FITNESS et que l'app pilote :
enum AppSubState { APP_NONE, APP_WAIT, APP_READY };
AppSubState appState = APP_NONE;

const char MENU_HELP[] = "1=Classic 2=Fitness 3=30s 6=60s OK=Start";

/* =================== MENU (repos) ================================== */
void enterIdle() {
  state = ST_IDLE;
  appState = APP_NONE;
  allLedsOff();
  char b[40];
  snprintf(b, sizeof(b), "BARTACK %s-%us", typeName(), modeSeconds);
  scrollScoreZone(b);
  scrollTimeZone(MENU_HELP);
  if (netAppHello) netSendState("wait");
}

void enterAppWait() {
  state = ST_IDLE;
  appState = APP_WAIT;
  allLedsOff();
  scrollScoreZone("FITNESS - CONNECT APP");
  printTimeZone("AP");
  if (netAppHello) { appState = APP_READY; netSendState("ready");
                     scrollScoreZone("READY - APP OK"); }
  else               netSendState("wait");
}

/* =================== DEPART D'UNE MANCHE =========================== */
void startCountdown(bool fromApp) {
  state = ST_COUNTDOWN;
  countStep = 3;
  stateStartMs = millis();
  score = 0;
  statsReset();
  showScore(score);
  printTimeZone("03");
  playSound(SND_COUNT);
  if (netAppHello) { netSendState("countdown"); netSendCountdown(3); }
  Serial.println(F("Compte a rebours..."));
  (void)fromApp;
}

void startPlay() {
  playSound(SND_GO);
  if (netAppHello) netSendCountdown(0);          // 0 = GO !
  state = ST_PLAY;
  roundStartMs = millis();
  lastShownSec = -1;
  lastLit = -1;
  showScore(score);
  if (gameType == TYPE_FITNESS && fitnessProg.valid) fitnessBegin();
  startGap();
  gapStartMs = millis() - GAP_MS[0];
  if (netAppHello) netSendState("play");
}

/* =================== CALLBACKS RESEAU (net_link.h) ================= */
void onAppConnected() {
  Serial.println(F("[NET] App connectee"));
}

void onAppDisconnected() {
  Serial.println(F("[NET] App deconnectee"));
  // En pleine manche fitness pilotee : on termine proprement (Etape 1 §3.3)
  if (state == ST_PLAY && gameType == TYPE_FITNESS && fitnessProg.valid) {
    endRound();
  } else if (appState != APP_NONE) {
    enterAppWait();                              // retour a l'attente
  }
}

void onAppHello(JsonDocument &doc) {
  (void)doc;
  Serial.print(F("[NET] hello app, lang=")); Serial.println(netAppLang);
  if (appState == APP_WAIT) {
    appState = APP_READY;
    scrollScoreZone("READY - APP OK");
    netSendState("ready");
  }
}

void onAppConfigure() {
  Serial.print(F("[NET] configure: ")); Serial.print(fitnessProg.mode);
  Serial.print(F(" rounds=")); Serial.println(fitnessProg.nRounds);
  if (appState != APP_NONE || state == ST_IDLE) {
    char b[40];
    snprintf(b, sizeof(b), "PRG %s", fitnessProg.name);
    scrollScoreZone(b);
    printTimeZone("--");
    netSendState("ready");
  }
}

void onAppStart() {
  if (state != ST_IDLE) { netSendErr("busy", "manche en cours"); return; }
  if (gameType != TYPE_FITNESS) gameType = TYPE_FITNESS;
  if (!fitnessProg.valid) { netSendErr("noConfig", "configure d'abord"); return; }
  startCountdown(true);
}

void onAppStop() {
  if (state == ST_PLAY || state == ST_COUNTDOWN) {
    Serial.println(F("[NET] stop demande par l'app"));
    endRound();
  }
}

void onAppQuit() {
  Serial.println(F("[NET] quit de l'app"));
  netAppHello = false;
  if (state == ST_IDLE) enterIdle();
}

/* =================== IR ============================================ */
void handleIR() {
  if (!IrReceiver.decode()) return;
  uint8_t cmd = IrReceiver.decodedIRData.command;
  bool repeat = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;
  IrReceiver.resume();
  if (repeat) return;
  if (state != ST_IDLE) return;

  if (cmd == IR_KEY_1) {
    gameType = TYPE_CLASSIC; playSound(SND_KEY); enterIdle();
    Serial.println(F("Type : CLASSIC"));
  } else if (cmd == IR_KEY_2) {
    gameType = TYPE_FITNESS; playSound(SND_KEY);
    Serial.println(F("Type : FITNESS"));
    enterAppWait();                              // attend l'app
  } else if (cmd == IR_KEY_3) {
    modeSeconds = 30; playSound(SND_KEY);
    if (appState == APP_NONE) enterIdle();
    Serial.println(F("Duree : 30 s"));
  } else if (cmd == IR_KEY_6) {
    modeSeconds = 60; playSound(SND_KEY);
    if (appState == APP_NONE) enterIdle();
    Serial.println(F("Duree : 60 s"));
  } else if (cmd == IR_KEY_OK) {
    if (appState != APP_NONE && !fitnessProg.valid) {
      // FITNESS en attente d'app : OK sans config -> fallback V1
      Serial.println(F("FITNESS sans app : fallback CLASSIC"));
    }
    // ---- TOP 3 du mode pendant 5 s ----
    state = ST_TOP3;
    stateStartMs = millis();
    top3Shown = -1;
    playSound(SND_KEY);
    printTimeZone("T3");
    Serial.print(F("TOP 3 du mode ")); Serial.print(typeName());
    Serial.print('-'); Serial.print(modeSeconds); Serial.println('s');
  }
}

/* =================== SETUP ========================================= */
void setup() {
  Serial.begin(115200);

  for (uint8_t i = 0; i < 12; i++) {
    pinMode(BTN_PIN[i], INPUT_PULLUP);
    pinMode(LED_PIN[i], OUTPUT);
    digitalWrite(LED_PIN[i], LOW);
  }

  soundsInit();
  displayInit();
  netInit();
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);

  randomSeed(analogRead(A0));
  loadTop3();

  Serial.print(F("AMZ BATAK PRO v")); Serial.print(FW_VERSION);
  Serial.println(F(" pret. 1=Classic 2=Fitness 3=30s 6=60s OK=Start"));
  enterIdle();
}

/* =================== BOUCLE PRINCIPALE ============================= */
void loop() {
  handleIR();
  netPoll();
  disp.displayAnimate();

  // timeout app en manche pilotee (Etape 1 §3.3)
  if (state == ST_PLAY && gameType == TYPE_FITNESS &&
      fitnessProg.valid && netAppTimedOut()) {
    Serial.println(F("[NET] timeout app -> fin de manche"));
    netAppConnected = false; netAppHello = false;
    endRound();
  }

  switch (state) {

    /* ---------- menu / attente app ---------- */
    case ST_IDLE:
      if (disp.getZoneStatus(ZONE_SCORE)) disp.displayReset(ZONE_SCORE);
      if (disp.getZoneStatus(ZONE_TIME))  disp.displayReset(ZONE_TIME);
      break;

    /* ---------- TOP 3 pendant 5 s ---------- */
    case ST_TOP3: {
      unsigned long now = millis();
      int8_t rankToShow = (now - stateStartMs) / (TOP3_SHOW_MS / 3);
      if (rankToShow > 2) rankToShow = 2;
      if (rankToShow != top3Shown) {
        top3Shown = rankToShow;
        char b[16];
        snprintf(b, sizeof(b), "%d.%04u", rankToShow + 1,
                 top3[modeIndex()][rankToShow]);
        printScoreZone(b);
      }
      if (now - stateStartMs >= TOP3_SHOW_MS) startCountdown(false);
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
          if (netAppHello) netSendCountdown(countStep);
        } else {
          startPlay();
        }
      }
      break;

    /* ---------- la manche : CLASSIC ou FITNESS ---------- */
    case ST_PLAY:
      if (gameType == TYPE_CLASSIC) playClassicStep();
      else                          playFitnessStep();
      break;

    /* ---------- resultat 15 s puis retour menu ---------- */
    case ST_GAMEOVER: {
      unsigned long now = millis();

      if (finalRank >= 0) {
        if (disp.getZoneStatus(ZONE_SCORE)) disp.displayReset(ZONE_SCORE);
        if (now - lastFlashMs >= 500) {
          lastFlashMs = now;
          flashOn = !flashOn;
          printTimeZone(flashOn ? "HI" : "  ");
        }
      } else {
        if (now - lastFlashMs >= 500) {
          lastFlashMs = now;
          flashOn = !flashOn;
          if (flashOn) showScore(score);
          else         printScoreZone("    ");
        }
      }

      if (now - stateStartMs >= GAMEOVER_SHOW_MS) {
        Serial.println(F("Retour au menu."));
        fitnessProg.valid = false;               // nouveau configure requis
        if (netAppHello && gameType == TYPE_FITNESS) enterAppWait();
        else enterIdle();
      }
      break;
    }
  }
}
