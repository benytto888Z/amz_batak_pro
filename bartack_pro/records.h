/**********************************************************************
 *  records.h - AMZ BATAK PRO v2 (Mega)
 *  TOP 3 de chacun des 4 modes en EEPROM.
 *  Modes : 0=CLASSIC-30  1=CLASSIC-60  2=FITNESS-30  3=FITNESS-60
 **********************************************************************/
#pragma once
#include <EEPROM.h>
#include "config.h"

const int     EE_MAGIC_ADDR = 0;
const uint8_t EE_MAGIC      = 0x43;  // changer pour effacer les scores
const int     EE_TOP3_ADDR  = 2;     // 4 modes x 3 x uint16 = 24 octets

uint16_t top3[4][3];

uint8_t modeIndex() {
  return (uint8_t)gameType * 2 + (modeSeconds == 60 ? 1 : 0);
}

const char *typeName() {
  return (gameType == TYPE_FITNESS) ? "FITNESS" : "CLASSIC";
}

void loadTop3() {
  if (EEPROM.read(EE_MAGIC_ADDR) != EE_MAGIC) {
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
