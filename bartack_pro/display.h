/**********************************************************************
 *  display.h - AMZ BATAK PRO v2 (Mega)
 *  6x MAX7219 en UNE chaine, 2 zones MD_Parola.
 *  Config materielle VALIDEE : SCORE=modules 0-3, TEMPS=modules 4-5,
 *  rotation 180 (PA_FLIP_UD + PA_FLIP_LR), defilement PA_SCROLL_RIGHT.
 **********************************************************************/
#pragma once
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include "config.h"

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
const uint8_t DISP_CS     = 53;
const uint8_t NUM_MODULES = 6;
MD_Parola disp(HARDWARE_TYPE, DISP_CS, NUM_MODULES);
const uint8_t ZONE_TIME  = 0;
const uint8_t ZONE_SCORE = 1;

const uint8_t  DISPLAY_BRIGHT = 6;
const uint16_t SCROLL_SPEED   = 60;

// Tampons statiques : MD_Parola garde un pointeur sur le texte !
char scoreText[40], timeText[48];

void displayInit() {
  disp.begin(2);
  disp.setZone(ZONE_SCORE, 0, 3);    // 4 modules (gauche)
  disp.setZone(ZONE_TIME,  4, 5);    // 2 modules (droite)
  disp.setZoneEffect(ZONE_SCORE, true, PA_FLIP_UD);
  disp.setZoneEffect(ZONE_SCORE, true, PA_FLIP_LR);
  disp.setZoneEffect(ZONE_TIME,  true, PA_FLIP_UD);
  disp.setZoneEffect(ZONE_TIME,  true, PA_FLIP_LR);
  disp.setIntensity(DISPLAY_BRIGHT);
  disp.displayClear();
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

/* ---- LEDs ---- */
void allLedsOff() {
  for (uint8_t i = 0; i < 12; i++) digitalWrite(LED_PIN[i], LOW);
}
