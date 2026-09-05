/**********************************************************************
 *  sounds.h - AMZ BATAK PRO v2 (Mega)
 *  DFPlayer Mini sur Serial1. Pistes SD dans /mp3 :
 *   0001 bip touche | 0002 "3,2,1" | 0003 "GO !" | 0004 frappe (court)
 *   0005 fanfare fin | 0006 "Nouveau record !"
 **********************************************************************/
#pragma once
#include "DFRobotDFPlayerMini.h"
#include "config.h"

DFRobotDFPlayerMini dfp;
bool dfpOk = false;

const uint8_t SND_KEY     = 1;
const uint8_t SND_COUNT   = 2;
const uint8_t SND_GO      = 3;
const uint8_t SND_HIT     = 4;
const uint8_t SND_SUCCESS = 5;
const uint8_t SND_RECORD  = 6;
const uint8_t DFP_VOLUME  = 24;      // 0..30

void soundsInit() {
  DFP_SERIAL.begin(9600);
  if (dfp.begin(DFP_SERIAL, /*isACK=*/true, /*doReset=*/true)) {
    dfpOk = true;
    dfp.volume(DFP_VOLUME);
    Serial.println(F("DFPlayer OK"));
  } else {
    Serial.println(F("ATTENTION : DFPlayer introuvable (jeu silencieux)"));
  }
}

void playSound(uint8_t track) {
  if (dfpOk) dfp.play(track);
}
