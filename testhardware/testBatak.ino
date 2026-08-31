// /**********************************************************************
//  * AMZ BATAK PRO
//  *
//  * ================================================================
//  * TEST COMPLET DE CABLAGE
//  * ================================================================
//  *
//  * CARTE :
//  * Arduino MEGA 2560
//  * + module ESP8266 WiFi intégré
//  *
//  * IMPORTANT :
//  * Le WiFi / ESP8266 n'est PAS utilisé dans ce programme de test.
//  *
//  * ================================================================
//  * HARDWARE
//  * ================================================================
//  *
//  * 12 boutons
//  * 12 LEDs
//  * 1 buzzer
//  * 1 récepteur IR
//  * 6 modules MAX7219 8x8 en chaîne
//  *
//  * ================================================================
//  * FONCTIONNEMENT
//  * ================================================================
//  *
//  * AU DEMARRAGE :
//  *
//  * - toutes les LEDs sont éteintes
//  * - MAX7219 affiche TEST
//  * - mode par défaut = CLASSIC
//  *
//  *
//  * APPUI SUR UN BOUTON :
//  *
//  * - détection du bouton
//  * - LED correspondante ON
//  * - buzzer bip
//  * - MAX7219 affiche B01 ... B12
//  * - LED reste allumée 1 seconde
//  * - LED s'éteint
//  * - retour à l'affichage du mode
//  *
//  *
//  * TELECOMMANDE IR :
//  *
//  * Touche 1   -> MODE CLASSIC
//  * Touche 2   -> MODE FITNESS
//  * Touche PLAY -> GO
//  *
//  *
//  * ================================================================
//  * LIBRAIRIES
//  * ================================================================
//  *
//  * IRremote
//  * MD_Parola
//  * MD_MAX72xx
//  * SPI
//  *
//  **********************************************************************/

// #include <Arduino.h>
// #include <SPI.h>
// #define DECODE_NEC              // Define the protocol (NEC)
// #include <IRremote.h>

// #include <MD_Parola.h>
// #include <MD_MAX72xx.h>


// // ====================================================================
// // 1. CONFIGURATION DES 12 BOUTONS
// // ====================================================================
// //
// // Les boutons sont connectés entre le GPIO et GND.
// //
// // Nous utilisons INPUT_PULLUP.
// //
// // Donc :
// //
// // bouton relâché = HIGH
// // bouton appuyé  = LOW
// //
// // ====================================================================

// const uint8_t BTN_PIN[12] = {

//   22,  // Bouton 1
//   23,  // Bouton 2
//   24,  // Bouton 3
//   25,  // Bouton 4
//   26,  // Bouton 5
//   27,  // Bouton 6
//   28,  // Bouton 7
//   29,  // Bouton 8
//   30,  // Bouton 9
//   31,  // Bouton 10
//   32,  // Bouton 11
//   33   // Bouton 12
// };


// // ====================================================================
// // 2. CONFIGURATION DES 12 LEDs
// // ====================================================================
// //
// // Chaque LED doit avoir une résistance de limitation.
// //
// // GPIO HIGH = LED ON
// // GPIO LOW  = LED OFF
// //
// // ====================================================================

// const uint8_t LED_PIN[12] = {

//   34,  // LED 1
//   35,  // LED 2
//   36,  // LED 3
//   37,  // LED 4
//   38,  // LED 5
//   39,  // LED 6
//   40,  // LED 7
//   41,  // LED 8
//   42,  // LED 9
//   43,  // LED 10
//   44,  // LED 11
//   45   // LED 12
// };


// // ====================================================================
// // 3. BUZZER
// // ====================================================================

// const uint8_t BUZZER_PIN = 46;


// // ====================================================================
// // 4. RECEPTEUR IR
// // ====================================================================

// const uint8_t IR_RECEIVE_PIN = 47;


// // ====================================================================
// // 5. MAX7219
// // ====================================================================
// //
// // Mega 2560 :
// //
// // D51 = MOSI / DIN
// // D52 = SCK  / CLK
// // D53 = SS   / CS
// //
// // ====================================================================

// #define HARDWARE_TYPE MD_MAX72XX::FC16_HW

// const uint8_t MAX7219_CS = 53;

// const uint8_t MAX7219_MODULES = 6;

// MD_Parola display(
//   HARDWARE_TYPE,
//   MAX7219_CS,
//   MAX7219_MODULES);


// // ====================================================================
// // 6. CODES TELECOMMANDE IR
// // ====================================================================
// //
// // Codes repris du programme Batak précédent.
// //
// // 1    = 0x0C
// // 2    = 0x18
// // PLAY = 0x43
// //
// // ====================================================================

// const uint8_t IR_KEY_CLASSIC = 0x45;

// const uint8_t IR_KEY_FITNESS = 0x46;

// const uint8_t IR_KEY_PLAY = 0x1C;


// // ====================================================================
// // 7. MODES
// // ====================================================================

// enum GameMode {
//   MODE_CLASSIC,

//   MODE_FITNESS
// };


// GameMode currentMode = MODE_CLASSIC;


// // ====================================================================
// // 8. GESTION LED TEMPORAIRE
// // ====================================================================
// //
// // Lorsqu'un bouton est appuyé :
// //
// // LED ON
// // ↓
// // 1000 ms
// // ↓
// // LED OFF
// //
// // ====================================================================

// int8_t activeLed = -1;

// unsigned long activeLedStart = 0;

// const unsigned long LED_DURATION = 1000;


// // ====================================================================
// // 9. ANTI-REBOND
// // ====================================================================

// bool lastButtonState[12];

// bool buttonPressed[12];

// unsigned long lastDebounceTime[12];

// const unsigned long DEBOUNCE_TIME = 50;


// // ====================================================================
// // 10. AFFICHAGE MAX7219
// // ====================================================================

// void showText(const char *text) {
//   display.displayClear();

//   display.displayZoneText(
//     0,
//     text,
//     PA_CENTER,
//     0,
//     0,
//     PA_PRINT,
//     PA_NO_EFFECT);
// }


// // ====================================================================
// // 11. AFFICHER LE MODE ACTUEL
// // ====================================================================

// void showCurrentMode() {
//   if (currentMode == MODE_CLASSIC) {
//     showText("CLASSIC");
//   } else {
//     showText("FITNESS");
//   }
// }


// // ====================================================================
// // 12. AFFICHER LE NUMERO DU BOUTON
// // ====================================================================
// //
// // B01
// // B02
// // ...
// // B12
// //
// // ====================================================================

// void showButtonNumber(uint8_t buttonIndex) {
//   char buffer[8];

//   snprintf(
//     buffer,
//     sizeof(buffer),
//     "B%02u",
//     buttonIndex + 1);

//   showText(buffer);
// }


// // ====================================================================
// // 13. ETEINDRE TOUTES LES LEDs
// // ====================================================================

// void allLedsOff() {
//   for (uint8_t i = 0; i < 12; i++) {
//     digitalWrite(
//       LED_PIN[i],
//       LOW);
//   }
// }


// // ====================================================================
// // 14. ALLUMER UNE LED
// // ====================================================================

// void setLed(
//   uint8_t index,
//   bool state) {
//   if (index >= 12)
//     return;

//   digitalWrite(
//     LED_PIN[index],
//     state ? HIGH : LOW);
// }


// // ====================================================================
// // 15. BIP BOUTON
// // ====================================================================

// void beepButton() {
//   tone(
//     BUZZER_PIN,
//     2200,
//     80);
// }


// // ====================================================================
// // 16. BIP SELECTION MODE
// // ====================================================================

// void beepMode() {
//   tone(
//     BUZZER_PIN,
//     1500,
//     100);
// }


// // ====================================================================
// // 17. BIP GO
// // ====================================================================

// void beepGo() {
//   tone(
//     BUZZER_PIN,
//     2000,
//     200);
// }


// // ====================================================================
// // 18. ACTIVATION D'UN BOUTON
// // ====================================================================

// void activateButton(
//   uint8_t index) {
//   Serial.print(
//     "BOUTON ");

//   Serial.print(
//     index + 1);

//   Serial.println(
//     " APPUYE");


//   // --------------------------------------------------------------
//   // Sécurité :
//   // toutes les LEDs OFF
//   // --------------------------------------------------------------

//   allLedsOff();


//   // --------------------------------------------------------------
//   // Affichage
//   // --------------------------------------------------------------

//   showButtonNumber(index);


//   // --------------------------------------------------------------
//   // Buzzer
//   // --------------------------------------------------------------

//   beepButton();


//   // --------------------------------------------------------------
//   // LED correspondante ON
//   // --------------------------------------------------------------

//   setLed(
//     index,
//     true);


//   // --------------------------------------------------------------
//   // Mémorisation
//   // --------------------------------------------------------------

//   activeLed = index;

//   activeLedStart = millis();
// }


// // ====================================================================
// // 19. GESTION DU TIMER LED
// // ====================================================================

// void updateActiveLed() {
//   if (activeLed < 0)
//     return;


//   if (
//     millis() - activeLedStart
//     >= LED_DURATION) {
//     setLed(
//       activeLed,
//       false);


//     Serial.print(
//       "LED ");

//     Serial.print(
//       activeLed + 1);

//     Serial.println(
//       " OFF");


//     activeLed = -1;


//     showCurrentMode();
//   }
// }


// // ====================================================================
// // 20. LECTURE DES BOUTONS
// // ====================================================================

// void handleButtons() {
//   for (uint8_t i = 0; i < 12; i++) {
//     bool reading =
//       digitalRead(
//         BTN_PIN[i]);


//     // --------------------------------------------------------------
//     // changement d'état
//     // --------------------------------------------------------------

//     if (
//       reading != lastButtonState[i]) {
//       lastDebounceTime[i] =
//         millis();

//       lastButtonState[i] =
//         reading;
//     }


//     // --------------------------------------------------------------
//     // signal stable
//     // --------------------------------------------------------------

//     if (
//       millis() - lastDebounceTime[i]
//       > DEBOUNCE_TIME) {
//       // ------------------------------------------------------------
//       // APPUI
//       // ------------------------------------------------------------

//       if (
//         reading == LOW && !buttonPressed[i]) {
//         buttonPressed[i] = true;

//         activateButton(i);
//       }


//       // ------------------------------------------------------------
//       // RELÂCHEMENT
//       // ------------------------------------------------------------

//       if (
//         reading == HIGH) {
//         buttonPressed[i] = false;
//       }
//     }
//   }
// }


// // ====================================================================
// // 21. TELECOMMANDE IR
// // ====================================================================
// int ircmd;

// void handleIR() {
//   if (
//     !IrReceiver.decode()) {
//     return;
//   }


//   // --------------------------------------------------------------
//   // commande reçue
//   // --------------------------------------------------------------

//   uint8_t command =
//     IrReceiver.decodedIRData.command;


//   // --------------------------------------------------------------
//   // répétition
//   // --------------------------------------------------------------

//   bool repeat =
//     IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;


//   // --------------------------------------------------------------
//   // préparer la prochaine réception
//   // --------------------------------------------------------------

//   IrReceiver.resume();


//   // --------------------------------------------------------------
//   // ignorer les répétitions
//   // --------------------------------------------------------------

//   if (repeat) {
//     return;
//   }


//   // --------------------------------------------------------------
//   // DEBUG SERIAL
//   // --------------------------------------------------------------

//   Serial.print(
//     "IR COMMAND = 0x");

//   Serial.println(
//     command,
//     HEX);


//   // ================================================================
//   // TOUCHE 1 = CLASSIC
//   // ================================================================

//   if (
//     command == IR_KEY_CLASSIC) {
//     currentMode =
//       MODE_CLASSIC;


//     // LEDs OFF
//     allLedsOff();


//     // affichage
//     showText(
//       "CLASSIC");


//     // bip
//     beepMode();


//     Serial.println(
//       "MODE CLASSIC");
//   }


//   // ================================================================
//   // TOUCHE 2 = FITNESS
//   // ================================================================

//   else if (
//     command == IR_KEY_FITNESS) {
//     currentMode =
//       MODE_FITNESS;


//     // LEDs OFF
//     allLedsOff();


//     // affichage
//     showText(
//       "FITNESS");


//     // bip
//     beepMode();


//     Serial.println(
//       "MODE FITNESS");
//   }


//   // ================================================================
//   // PLAY = GO
//   // ================================================================

//   else if (
//     command == IR_KEY_PLAY) {
//     Serial.println(
//       "IR PLAY / GO");


//     showText(
//       "GO");


//     beepGo();


//     delay(500);


//     showCurrentMode();
//   }
// }

// void checkIRcode() {
//   /*Serial.print("Raw = ");  
//   Serial.print(IrReceiver.decodedIRData.decodedRawData, HEX);  // Print raw data in HEX*/
//   Serial.print("   Command = ");
//   Serial.println(IrReceiver.decodedIRData.command);  // Print decoded command
//   ircmd = IrReceiver.decodedIRData.command;
//   IrReceiver.decodedIRData.command = 0;  // Reset command after processing
// }


// // ====================================================================
// // 22. SETUP
// // ====================================================================

// void setup() {
//   // ----------------------------------------------------------------
//   // SERIAL
//   // ----------------------------------------------------------------

//   Serial.begin(
//     115200);


//   delay(1000);


//   Serial.println();
//   Serial.println(
//     "================================================");

//   Serial.println(
//     "       AMZ BATAK PRO");

//   Serial.println(
//     "       TEST CABLAGE");

//   Serial.println(
//     "       ARDUINO MEGA 2560");

//   Serial.println(
//     "================================================");


//   // ----------------------------------------------------------------
//   // 12 BOUTONS
//   // ----------------------------------------------------------------

//   Serial.println(
//     "Initialisation boutons...");


//   for (
//     uint8_t i = 0;
//     i < 12;
//     i++) {
//     pinMode(
//       BTN_PIN[i],
//       INPUT_PULLUP);


//     lastButtonState[i] =
//       digitalRead(
//         BTN_PIN[i]);


//     buttonPressed[i] =
//       false;


//     lastDebounceTime[i] =
//       millis();
//   }


//   // ----------------------------------------------------------------
//   // 12 LEDs
//   // ----------------------------------------------------------------

//   Serial.println(
//     "Initialisation LEDs...");


//   for (
//     uint8_t i = 0;
//     i < 12;
//     i++) {
//     pinMode(
//       LED_PIN[i],
//       OUTPUT);


//     // IMPORTANT :
//     // toutes les LEDs OFF au démarrage

//     digitalWrite(
//       LED_PIN[i],
//       LOW);
//   }


//   // ----------------------------------------------------------------
//   // BUZZER
//   // ----------------------------------------------------------------

//   Serial.println(
//     "Initialisation buzzer...");


//   pinMode(
//     BUZZER_PIN,
//     OUTPUT);


//   noTone(
//     BUZZER_PIN);


//   // ----------------------------------------------------------------
//   // MAX7219
//   // ----------------------------------------------------------------

//   Serial.println(
//     "Initialisation MAX7219...");


//   display.begin();


//   display.setIntensity(
//     5);


//   display.displayClear();


//   showText(
//     "TEST");


//   // ----------------------------------------------------------------
//   // IR
//   // ----------------------------------------------------------------

//   Serial.println(
//     "Initialisation IR...");


//    //IrReceiver.begin(IR_RECEIVE_PIN,DISABLE_LED_FEEDBACK);
//    IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK, USE_DEFAULT_FEEDBACK_LED_PIN);  // Initialize IR receiver


//   // ----------------------------------------------------------------
//   // SÉCURITÉ
//   // ----------------------------------------------------------------

//   allLedsOff();


//   // ----------------------------------------------------------------
//   // AFFICHAGE INITIAL
//   // ----------------------------------------------------------------

//   delay(1000);


//   showCurrentMode();


//   // ----------------------------------------------------------------
//   // MESSAGE SERIAL
//   // ----------------------------------------------------------------

//   Serial.println();
//   Serial.println(
//     "================================================");

//   Serial.println(
//     "SYSTEME PRET");

//   Serial.println(
//     "================================================");

//   Serial.println(
//     "12 boutons disponibles");

//   Serial.println(
//     "12 LEDs disponibles");

//   Serial.println(
//     "IR 1    = CLASSIC");

//   Serial.println(
//     "IR 2    = FITNESS");

//   Serial.println(
//     "IR PLAY = GO");

//   Serial.println(
//     "================================================");

//   Serial.println();
// }


// // ====================================================================
// // 23. LOOP
// // ====================================================================

// void loop() {
//   // --------------------------------------------------------------
//   // Boutons
//   // --------------------------------------------------------------

//   handleButtons();


//   // --------------------------------------------------------------
//   // Télécommande IR
//   // --------------------------------------------------------------

//    handleIR();


//   // --------------------------------------------------------------
//   // Timer LED
//   // --------------------------------------------------------------

//   updateActiveLed();

//   // --------------------------------------------------------------
//   // MAX7219
//   // --------------------------------------------------------------

//   display.displayAnimate();
// }

