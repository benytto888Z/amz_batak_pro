/**********************************************************************
 *  net_link.h - AMZ BATAK PRO v2 (Mega)
 *  Protocole JSON app <-> mur via Serial3 <-> pont ESP8266.
 *  Reference : ETAPE_01_ARCHITECTURE_PROTOCOLE.md + amendements A1-A4.
 *
 *  RECOIT (app -> mur)   : hello, configure, start, stop, quit, ping
 *  RECOIT (pont ESP)     : espReady, espClient
 *  EMET  (mur -> app)    : hello, state, countdown, tick, hit, miss,
 *                          gameOver, pong, err
 *
 *  1 ligne (\n) = 1 message JSON. Trames < 512 octets.
 **********************************************************************/
#pragma once
#include <ArduinoJson.h>
#include "config.h"
#include "records.h"

/* =================== ETAT RESEAU =================================== */
bool netEspReady    = false;         // pont demarre (espReady recu)
bool netAppConnected = false;        // client WebSocket present
bool netAppHello     = false;        // l'app a envoye son hello
char netAppLang[6]   = "fr";
unsigned long netLastRxMs = 0;       // pour le timeout 10 s (Etape 1 §3.3)
const unsigned long NET_TIMEOUT_MS = 10000;

static char netLine[512];
static uint16_t netLineLen = 0;

/* =================== EMISSION ====================================== */
void netSendRaw(const char *s) { NET_SERIAL.println(s); }

void netSendJson(JsonDocument &doc) {
  serializeJson(doc, NET_SERIAL);
  NET_SERIAL.write('\n');
}

void netSendState(const char *s) {
  StaticJsonDocument<64> d;
  d["t"] = "state"; d["s"] = s;
  netSendJson(d);
}

void netSendCountdown(int8_t n) {
  StaticJsonDocument<48> d;
  d["t"] = "countdown"; d["n"] = n;
  netSendJson(d);
}

void netSendTick(uint8_t round, const char *phase, int remainSec) {
  StaticJsonDocument<128> d;                     // amendement A3
  d["t"] = "tick"; d["round"] = round; d["phase"] = phase;
  d["remainSec"] = remainSec; d["score"] = score;
  d["hpm"] = statsHpm();
  netSendJson(d);
}

void netSendHit(uint8_t btn, uint16_t reactMs) {
  StaticJsonDocument<128> d;
  d["t"] = "hit"; d["btn"] = btn + 1; d["reactMs"] = reactMs;
  d["score"] = score; d["ms"] = millis();
  netSendJson(d);
}

void netSendMiss(uint8_t btn) {
  StaticJsonDocument<96> d;
  d["t"] = "miss"; d["btn"] = btn + 1; d["ms"] = millis();
  netSendJson(d);
}

void netSendHello() {
  StaticJsonDocument<384> d;
  d["t"] = "hello"; d["fw"] = FW_VERSION; d["buttons"] = 12;
  JsonObject t3 = d.createNestedObject("top3");
  JsonArray c30 = t3.createNestedArray("classic30");
  JsonArray c60 = t3.createNestedArray("classic60");
  JsonArray f30 = t3.createNestedArray("fitness30");
  JsonArray f60 = t3.createNestedArray("fitness60");
  for (uint8_t i = 0; i < 3; i++) {
    c30.add(top3[0][i]); c60.add(top3[1][i]);
    f30.add(top3[2][i]); f60.add(top3[3][i]);
  }
  netSendJson(d);
}

void netSendGameOver(uint16_t durationSec) {
  StaticJsonDocument<512> d;                     // amendement A4
  d["t"] = "gameOver";
  d["score"] = score; d["hits"] = rs.hits; d["misses"] = rs.misses;
  uint16_t avg = rs.hits ? rs.sumReact / rs.hits : 0;
  d["avgReactMs"]   = avg;
  d["bestReactMs"]  = (rs.minReact == 0xFFFF) ? 0 : rs.minReact;
  d["worstReactMs"] = rs.maxReact;
  // ecart-type (consistency) : sqrt(E[x2] - E[x]2)
  uint16_t sd = 0;
  if (rs.hits > 1) {
    uint32_t mean2 = (uint32_t)avg * avg;
    uint32_t ex2   = rs.sumReactSq / rs.hits;
    sd = (ex2 > mean2) ? (uint16_t)sqrt((float)(ex2 - mean2)) : 0;
  }
  d["sdReactMs"] = sd;
  d["rank"] = finalRank + 1;                     // 0 = pas dans le TOP 3
  JsonArray t3 = d.createNestedArray("top3");
  for (uint8_t i = 0; i < 3; i++) t3.add(top3[modeIndex()][i]);
  JsonObject th = d.createNestedObject("thirds");
  JsonArray thh = th.createNestedArray("hits");
  JsonArray tha = th.createNestedArray("avgReactMs");
  for (uint8_t i = 0; i < 3; i++) {
    thh.add(rs.thirdHits[i]);
    tha.add(rs.thirdHits[i] ? rs.thirdSumReact[i] / rs.thirdHits[i] : 0);
  }
  d["durationSec"] = durationSec;
  netSendJson(d);
}

void netSendErr(const char *code, const char *msg) {
  StaticJsonDocument<128> d;
  d["t"] = "err"; d["code"] = code; d["msg"] = msg;
  netSendJson(d);
}

/* =================== PARSING DU "configure" (A1) =================== */
bool parseConfigure(JsonDocument &doc) {
  const char *mode = doc["mode"] | "";
  if (!mode[0]) { netSendErr("badConfig", "mode manquant"); return false; }

  FitnessProgram p = {};
  strlcpy(p.mode, mode, sizeof(p.mode));
  strlcpy(p.name, doc["program"]["name"] | "Custom", sizeof(p.name));
  p.simultaneous = doc["program"]["simultaneous"] | 1;

  JsonArray rounds = doc["program"]["rounds"];
  if (rounds.isNull() || rounds.size() == 0) {
    netSendErr("badConfig", "rounds vide");
    return false;
  }
  p.nRounds = 0;
  for (JsonObject r : rounds) {
    if (p.nRounds >= MAX_ROUNDS) break;
    RoundCfg &rc = p.rounds[p.nRounds];
    rc.workSec   = r["workSec"]   | 30;
    rc.restSec   = r["restSec"]   | 0;
    rc.lightOnMs = constrain((int)(r["lightOnMs"] | 1500), 300, 5000);
    rc.gapMs     = constrain((int)(r["gapMs"]     | 300),  50, 2000);
    rc.accel     = r["accel"] | 0;
    p.nRounds++;
  }
  p.valid = true;
  fitnessProg = p;
  return true;
}

/* =================== CALLBACKS (definies dans le .ino) ============= */
void onAppConnected();
void onAppDisconnected();
void onAppHello(JsonDocument &doc);
void onAppConfigure();
void onAppStart();
void onAppStop();
void onAppQuit();

/* =================== TRAITEMENT D'UN MESSAGE ======================= */
void netProcessLine(char *line) {
  StaticJsonDocument<768> doc;
  DeserializationError e = deserializeJson(doc, line);
  if (e) { netSendErr("badJson", e.c_str()); return; }

  const char *t = doc["t"] | "";
  netLastRxMs = millis();

  /* ---- messages de service du pont ESP ---- */
  if (!strcmp(t, "espReady")) {
    netEspReady = true;
    Serial.println(F("[NET] Pont ESP pret"));
    return;
  }
  if (!strcmp(t, "espClient")) {
    bool c = doc["connected"] | false;
    int  n = doc["n"] | 0;
    netAppConnected = (n > 0);
    if (c) { onAppConnected(); }
    else if (!netAppConnected) { netAppHello = false; onAppDisconnected(); }
    return;
  }

  /* ---- messages de l'app ---- */
  if (!strcmp(t, "hello")) {
    netAppHello = true;
    strlcpy(netAppLang, doc["lang"] | "fr", sizeof(netAppLang));
    netSendHello();                              // reponse contractuelle
    onAppHello(doc);
    return;
  }
  if (!strcmp(t, "ping")) {
    StaticJsonDocument<48> d;
    d["t"] = "pong"; d["seq"] = doc["seq"] | 0;
    netSendJson(d);
    return;
  }
  if (!strcmp(t, "configure")) {
    if (parseConfigure(doc)) onAppConfigure();
    return;
  }
  if (!strcmp(t, "start")) { onAppStart(); return; }
  if (!strcmp(t, "stop"))  { onAppStop();  return; }
  if (!strcmp(t, "quit"))  { onAppQuit();  return; }
  /* messages inconnus : ignores silencieusement (Etape 1 §3.3) */
}

/* =================== LECTURE SERIAL3 =============================== */
void netPoll() {
  while (NET_SERIAL.available()) {
    char c = (char)NET_SERIAL.read();
    if (c == '\n' || c == '\r') {
      if (netLineLen > 0) {
        netLine[netLineLen] = '\0';
        netProcessLine(netLine);
        netLineLen = 0;
      }
    } else if (netLineLen < sizeof(netLine) - 1) {
      netLine[netLineLen++] = c;
    } else {
      netLineLen = 0;                            // ligne trop longue
    }
  }
}

void netInit() {
  NET_SERIAL.begin(NET_BAUD);
  netLastRxMs = millis();
}

// Timeout de l'app (Etape 1 §3.3) : vrai si plus rien recu depuis 10 s
bool netAppTimedOut() {
  return netAppConnected && (millis() - netLastRxMs > NET_TIMEOUT_MS);
}
