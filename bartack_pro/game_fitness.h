/**********************************************************************
 *  game_fitness.h - AMZ BATAK PRO v2 (Mega)
 *  CHALLENGE FITNESS pilote par l'app (programme work/rest par rounds).
 *
 *  ETAPE 3 : moteur de base fonctionnel -
 *   - execute les rounds du "configure" (work/rest, lightOnMs, gapMs,
 *     accel lineaire dans le round)
 *   - telemetrie complete (hit/miss/tick avec round et phase)
 *  ETAPE 4 completera : cibles simultanees (simultaneous>1), modes
 *  speciaux (sequence/memory...), sons dedies par phase.
 *
 *  Sans app (mode FITNESS choisi a la telecommande, personne de
 *  connecte) : fallback = manche CLASSIC (comportement V1 conserve).
 **********************************************************************/
#pragma once
#include "config.h"
#include "display.h"
#include "sounds.h"
#include "records.h"
#include "net_link.h"
#include "game_classic.h"          // reutilise pickNewLight/startGap/endRound

/* ---- etat du programme fitness en cours ---- */
uint8_t  fitRound = 0;             // round courant (0-based)
bool     fitInRest = false;
unsigned long fitPhaseStartMs = 0;

void fitnessBegin() {
  fitRound = 0;
  fitInRest = false;
  fitPhaseStartMs = millis();
  roundStartMs = millis();
  // duree totale (pour les stats par tiers)
  uint32_t total = 0;
  for (uint8_t i = 0; i < fitnessProg.nRounds; i++)
    total += fitnessProg.rounds[i].workSec + fitnessProg.rounds[i].restSec;
  modeSeconds = (total > 0 && total < 3600) ? total : 60;
}

/* vitesse courante du round (accel lineaire : x1.0 -> x0.6) */
uint16_t fitLightOnMs() {
  RoundCfg &r = fitnessProg.rounds[fitRound];
  if (!r.accel) return r.lightOnMs;
  unsigned long el = millis() - fitPhaseStartMs;
  unsigned long tot = (unsigned long)r.workSec * 1000UL;
  if (tot == 0) return r.lightOnMs;
  float f = 1.0f - 0.4f * ((float)el / (float)tot);   // 1.0 -> 0.6
  return (uint16_t)(r.lightOnMs * f);
}

/* ---- un pas de boucle de la manche FITNESS ---- */
void playFitnessStep() {
  // Pas de programme valide (pas d'app) -> comportement V1
  if (!fitnessProg.valid) { playClassicStep(); return; }

  unsigned long now = millis();
  RoundCfg &r = fitnessProg.rounds[fitRound];
  unsigned long phaseDur =
      (fitInRest ? r.restSec : r.workSec) * 1000UL;
  long remainMs = (long)phaseDur - (long)(now - fitPhaseStartMs);

  int remainSec = (remainMs > 0) ? (remainMs + 999) / 1000 : 0;
  if (remainSec != lastShownSec) {
    lastShownSec = remainSec;
    showTimeSec(remainSec);
    uint16_t sec = (now - roundStartMs) / 1000UL;
    rs.hitsPerSec[sec % 10] = 0;
    if (netAppHello)
      netSendTick(fitRound + 1, fitInRest ? "rest" : "work", remainSec);
  }

  /* ---- fin de la phase courante ---- */
  if (remainMs <= 0) {
    if (!fitInRest && r.restSec > 0) {           // work -> rest
      fitInRest = true;
      fitPhaseStartMs = now;
      allLedsOff();
      litIndex = -1; inGap = true; gapStartMs = now;
      char b[20]; snprintf(b, sizeof(b), "REST %d", fitRound + 1);
      printScoreZone(b);
      if (netAppHello) netSendState("rest");
      return;
    }
    // rest fini (ou pas de rest) -> round suivant ou fin
    fitRound++;
    if (fitRound >= fitnessProg.nRounds) {       // programme termine
      endRound();
      return;
    }
    fitInRest = false;
    fitPhaseStartMs = now;
    showScore(score);
    startGap();
    if (netAppHello) netSendState("play");
    return;
  }

  /* ---- phase repos : rien a jouer ---- */
  if (fitInRest) return;

  /* ---- phase travail : meme moteur que classic, params du round ---- */
  if (inGap) {
    if (now - gapStartMs >= r.gapMs) pickNewLight();
  } else {
    if (digitalRead(BTN_PIN[litIndex]) == LOW) {
      uint16_t react = (uint16_t)(now - lightOnMs);
      score++;
      statsAddHit(react);
      playSound(SND_HIT);
      showScore(score);
      if (netAppHello) netSendHit(litIndex, react);
      startGap();
    } else if (now - lightOnMs >= fitLightOnMs()) {
      rs.misses++;
      if (netAppHello) netSendMiss(litIndex);
      startGap();
    }
  }
}
