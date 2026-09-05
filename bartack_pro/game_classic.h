/**********************************************************************
 *  game_classic.h - AMZ BATAK PRO v2 (Mega)
 *  CHALLENGE CLASSIC (logique V1 validee) + telemetrie reseau :
 *  si une app est connectee, les hit/miss/tick partent aussi vers elle.
 **********************************************************************/
#pragma once
#include "config.h"
#include "display.h"
#include "sounds.h"
#include "records.h"
#include "net_link.h"

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

/* ---- fin de manche (commune) ---- */
void endRound() {
  allLedsOff();
  finalRank = insertScore(score);
  uint16_t durationSec = (millis() - roundStartMs) / 1000UL;
  state = ST_GAMEOVER;
  stateStartMs = millis();
  lastFlashMs  = millis();
  flashOn = true;

  if (finalRank >= 0) {
    playSound(SND_RECORD);
    char b[40];
    snprintf(b, sizeof(b), "BRAVO ! TOP %d - %u PTS", finalRank + 1, score);
    scrollScoreZone(b);
    printTimeZone("HI");
  } else {
    playSound(SND_SUCCESS);
    showScore(score);
    printTimeZone("00");
  }

  if (netAppHello) {                             // telemetrie -> app
    netSendState("gameover");
    netSendGameOver(durationSec);
  }

  Serial.print(F("Fin. Score = ")); Serial.println(score);
  if (finalRank >= 0) {
    Serial.print(F("*** TOP ")); Serial.print(finalRank + 1);
    Serial.println(F(" - sauvegarde ***"));
  }
}

/* ---- un pas de boucle de la manche CLASSIC ---- */
void playClassicStep() {
  unsigned long now = millis();
  long remainMs = (long)modeSeconds * 1000L - (long)(now - roundStartMs);

  int remainSec = (remainMs > 0) ? (remainMs + 999) / 1000 : 0;
  if (remainSec != lastShownSec) {
    lastShownSec = remainSec;
    showTimeSec(remainSec);
    // vider la case de la fenetre hpm pour la nouvelle seconde
    uint16_t sec = (now - roundStartMs) / 1000UL;
    rs.hitsPerSec[sec % 10] = 0;
    if (netAppHello) netSendTick(1, "work", remainSec);
  }

  if (remainMs <= 0) { endRound(); return; }

  uint8_t ph = currentPhase();

  if (inGap) {
    if (now - gapStartMs >= GAP_MS[ph]) pickNewLight();
  } else {
    if (digitalRead(BTN_PIN[litIndex]) == LOW) {         // touche !
      uint16_t react = (uint16_t)(now - lightOnMs);
      score++;
      statsAddHit(react);
      playSound(SND_HIT);
      showScore(score);
      if (netAppHello) netSendHit(litIndex, react);
      startGap();
    } else if (now - lightOnMs >= LIGHT_ON_MS[ph]) {     // trop lent
      rs.misses++;
      if (netAppHello) netSendMiss(litIndex);
      startGap();
    }
  }
}
