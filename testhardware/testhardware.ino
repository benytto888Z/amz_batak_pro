// /**********************************************************************
//  * AMZ BATAK PRO
//  * TEST DE CABLAGE HARDWARE
//  *
//  * ESP32 DevKit V1 - 30 broches
//  *
//  * TEST :
//  *   - 12 boutons
//  *   - 12 LEDs
//  *   - PCF8574 avec Adafruit_PCF8574
//  *   - Buzzer
//  *   - Télécommande IR
//  *   - 6 x MAX7219
//  *
//  * FONCTIONNEMENT :
//  *
//  * Bouton 1  -> LED 1 ON pendant 1 seconde + bip
//  * Bouton 2  -> LED 2 ON pendant 1 seconde + bip
//  * ...
//  * Bouton 12 -> LED 12 ON pendant 1 seconde + bip
//  *
//  * IR :
//  *   1    -> MODE CLASSIC
//  *   2    -> MODE FITNESS
//  *   PLAY -> GO / validation
//  *
//  * MAX7219 :
//  *   affiche TEST
//  *   puis CLASSIC / FITNESS
//  *   puis B01 ... B12 lors d'un appui
//  *
//  **********************************************************************/

// #include <Arduino.h>
// #include <Wire.h>
// #include <Adafruit_PCF8574.h>

// #include <IRremote.hpp>

// #include <MD_Parola.h>
// #include <MD_MAX72xx.h>
// #include <SPI.h>


// // ================================================================
// // 1. BROCHAGE DES 12 BOUTONS
// // ================================================================
// //
// // Bouton -> GPIO -> GND
// //
// // Pour GPIO 34 / 36 / 39 :
// // résistance externe 10 kΩ vers 3.3V obligatoire.
// //

// const uint8_t BTN_PIN[12] = {
//   13, 14, 15, 16,
//   19, 25, 26, 27,
//   32, 34, 36, 39
// };


// // ================================================================
// // 2. LEDs 9 à 12
// // ================================================================
// //
// // LEDs 1 à 8 = PCF8574
// // LEDs 9 à 12 = GPIO directs
// //

// const uint8_t LED_GPIO[4] = {
//   2, 4, 12, 17
// };


// // ================================================================
// // 3. PCF8574
// // ================================================================

// Adafruit_PCF8574 pcf;

// #define PCF8574_ADDR 0x20

// const uint8_t I2C_SDA = 21;
// const uint8_t I2C_SCL = 22;


// // ================================================================
// // 4. BUZZER
// // ================================================================

// const uint8_t BUZZER_PIN = 33;


// // ================================================================
// // 5. IR
// // ================================================================

// const uint8_t IR_RECEIVE_PIN = 35;


// // Codes télécommande
// //
// // 1    = CLASSIC
// // 2    = FITNESS
// // PLAY = GO
// //

// const uint8_t IR_KEY_CLASSIC = 0x0C;
// const uint8_t IR_KEY_FITNESS = 0x18;
// const uint8_t IR_KEY_PLAY    = 0x43;


// // ================================================================
// // 6. MAX7219
// // ================================================================

// #define HARDWARE_TYPE MD_MAX72XX::FC16_HW

// const uint8_t DISP_CS = 5;

// const uint8_t NUM_MODULES = 6;

// MD_Parola disp(
//   HARDWARE_TYPE,
//   DISP_CS,
//   NUM_MODULES
// );


// // ================================================================
// // 7. MODE
// // ================================================================

// enum TestMode {
//   MODE_CLASSIC,
//   MODE_FITNESS
// };

// TestMode currentMode = MODE_CLASSIC;


// // ================================================================
// // 8. GESTION LED ACTIVE
// // ================================================================

// int8_t activeLed = -1;

// unsigned long ledStartTime = 0;

// const unsigned long LED_ON_TIME = 1000;


// // ================================================================
// // 9. ANTI-REBOND BOUTONS
// // ================================================================

// bool lastButtonState[12];

// bool buttonPressed[12];

// unsigned long lastDebounceTime[12];

// const unsigned long DEBOUNCE_TIME = 50;


// // ================================================================
// // 10. LED PCF8574
// // ================================================================
// //
// // Avec ton câblage actif LOW :
// //
// // PCF = LOW  -> LED ON
// // PCF = HIGH -> LED OFF
// //

// void setLed(uint8_t index, bool state)
// {
//   if (index >= 12)
//     return;


//   // ------------------------------------------------------------
//   // LEDs 1 à 8 : PCF8574
//   // ------------------------------------------------------------

//   if (index < 8)
//   {
//     pcf.digitalWrite(
//       index,
//       state ? LOW : HIGH
//     );
//   }


//   // ------------------------------------------------------------
//   // LEDs 9 à 12 : GPIO directs
//   // ------------------------------------------------------------

//   else
//   {
//     digitalWrite(
//       LED_GPIO[index - 8],
//       state ? HIGH : LOW
//     );
//   }
// }


// // ================================================================
// // 11. ÉTEINDRE LES 12 LEDs
// // ================================================================

// void allLedsOff()
// {
//   // LEDs 1 à 8
//   for (uint8_t i = 0; i < 8; i++)
//   {
//     pcf.digitalWrite(
//       i,
//       HIGH
//     );
//   }


//   // LEDs 9 à 12
//   for (uint8_t i = 0; i < 4; i++)
//   {
//     digitalWrite(
//       LED_GPIO[i],
//       LOW
//     );
//   }
// }


// // ================================================================
// // 12. BUZZER
// // ================================================================

// void beepButton()
// {
//   tone(
//     BUZZER_PIN,
//     2200,
//     80
//   );
// }


// void beepMode()
// {
//   tone(
//     BUZZER_PIN,
//     1500,
//     100
//   );
// }


// void beepGo()
// {
//   tone(
//     BUZZER_PIN,
//     2000,
//     200
//   );
// }


// // ================================================================
// // 13. MAX7219
// // ================================================================

// void displayText(const char *text)
// {
//   disp.displayClear();

//   disp.displayZoneText(
//     0,
//     text,
//     PA_CENTER,
//     0,
//     0,
//     PA_PRINT,
//     PA_NO_EFFECT
//   );
// }


// // ================================================================
// // 14. AFFICHAGE BOUTON
// // ================================================================

// void displayButton(uint8_t index)
// {
//   char buffer[8];

//   snprintf(
//     buffer,
//     sizeof(buffer),
//     "B%02u",
//     index + 1
//   );

//   displayText(buffer);
// }


// // ================================================================
// // 15. AFFICHAGE MODE
// // ================================================================

// void displayCurrentMode()
// {
//   if (currentMode == MODE_CLASSIC)
//   {
//     displayText("CLASSIC");
//   }
//   else
//   {
//     displayText("FITNESS");
//   }
// }


// // ================================================================
// // 16. APPUI BOUTON
// // ================================================================

// void activateButton(uint8_t index)
// {
//   Serial.print(">>> BOUTON ");
//   Serial.print(index + 1);
//   Serial.println(" APPUYE");


//   // sécurité
//   allLedsOff();


//   // affichage
//   displayButton(index);


//   // bip
//   beepButton();


//   // LED correspondante
//   setLed(
//     index,
//     true
//   );


//   // mémorisation
//   activeLed = index;

//   ledStartTime = millis();
// }


// // ================================================================
// // 17. TIMER LED
// // ================================================================

// void updateLedTimer()
// {
//   if (activeLed < 0)
//     return;


//   if (
//     millis() - ledStartTime
//     >= LED_ON_TIME
//   )
//   {
//     setLed(
//       activeLed,
//       false
//     );


//     Serial.print("LED ");
//     Serial.print(activeLed + 1);
//     Serial.println(" OFF");


//     activeLed = -1;


//     displayCurrentMode();
//   }
// }


// // ================================================================
// // 18. TEST BOUTONS
// // ================================================================

// void handleButtons()
// {
//   for (uint8_t i = 0; i < 12; i++)
//   {
//     bool reading =
//       digitalRead(
//         BTN_PIN[i]
//       );


//     // ----------------------------------------------------------
//     // changement de niveau
//     // ----------------------------------------------------------

//     if (
//       reading !=
//       lastButtonState[i]
//     )
//     {
//       lastDebounceTime[i] =
//         millis();

//       lastButtonState[i] =
//         reading;
//     }


//     // ----------------------------------------------------------
//     // signal stable
//     // ----------------------------------------------------------

//     if (
//       millis() - lastDebounceTime[i]
//       > DEBOUNCE_TIME
//     )
//     {

//       // --------------------------------------------------------
//       // APPUI
//       // --------------------------------------------------------

//       if (
//         reading == LOW &&
//         !buttonPressed[i]
//       )
//       {
//         buttonPressed[i] = true;

//         activateButton(i);
//       }


//       // --------------------------------------------------------
//       // RELÂCHEMENT
//       // --------------------------------------------------------

//       if (
//         reading == HIGH
//       )
//       {
//         buttonPressed[i] = false;
//       }
//     }
//   }
// }


// // ================================================================
// // 19. TELECOMMANDE IR
// // ================================================================

// void handleIR()
// {
//   if (!IrReceiver.decode())
//     return;


//   uint8_t command =
//     IrReceiver.decodedIRData.command;


//   bool repeat =
//     IrReceiver.decodedIRData.flags &
//     IRDATA_FLAGS_IS_REPEAT;


//   IrReceiver.resume();


//   // ignorer répétitions
//   if (repeat)
//     return;


//   Serial.print(
//     "IR = 0x"
//   );

//   Serial.println(
//     command,
//     HEX
//   );


//   // ============================================================
//   // MODE CLASSIC
//   // ============================================================

//   if (
//     command ==
//     IR_KEY_CLASSIC
//   )
//   {
//     currentMode =
//       MODE_CLASSIC;


//     allLedsOff();


//     displayText(
//       "CLASSIC"
//     );


//     beepMode();


//     Serial.println(
//       "MODE CLASSIC SELECTIONNE"
//     );
//   }


//   // ============================================================
//   // MODE FITNESS
//   // ============================================================

//   else if (
//     command ==
//     IR_KEY_FITNESS
//   )
//   {
//     currentMode =
//       MODE_FITNESS;


//     allLedsOff();


//     displayText(
//       "FITNESS"
//     );


//     beepMode();


//     Serial.println(
//       "MODE FITNESS SELECTIONNE"
//     );
//   }


//   // ============================================================
//   // PLAY
//   // ============================================================

//   else if (
//     command ==
//     IR_KEY_PLAY
//   )
//   {
//     Serial.println(
//       "IR PLAY"
//     );


//     displayText(
//       "GO"
//     );


//     beepGo();


//     delay(500);


//     displayCurrentMode();
//   }
// }


// // ================================================================
// // 20. SETUP
// // ================================================================

// void setup()
// {
//   Serial.begin(115200);

//   delay(1000);


//   Serial.println();
//   Serial.println(
//     "=========================================="
//   );

//   Serial.println(
//     " AMZ BATAK PRO"
//   );

//   Serial.println(
//     " TEST CABLAGE - ESP32 30 BROCHES"
//   );

//   Serial.println(
//     "=========================================="
//   );


//   // ============================================================
//   // BOUTONS
//   // ============================================================

//   for (
//     uint8_t i = 0;
//     i < 12;
//     i++
//   )
//   {
//     // GPIO 34 / 36 / 39
//     // pas de INPUT_PULLUP disponible

//     if (
//       BTN_PIN[i] == 34 ||
//       BTN_PIN[i] == 36 ||
//       BTN_PIN[i] == 39
//     )
//     {
//       pinMode(
//         BTN_PIN[i],
//         INPUT
//       );
//     }
//     else
//     {
//       pinMode(
//         BTN_PIN[i],
//         INPUT_PULLUP
//       );
//     }


//     lastButtonState[i] =
//       digitalRead(
//         BTN_PIN[i]
//       );


//     buttonPressed[i] =
//       false;


//     lastDebounceTime[i] =
//       millis();
//   }


//   // ============================================================
//   // LEDs 9-12
//   // ============================================================

//   for (
//     uint8_t i = 0;
//     i < 4;
//     i++
//   )
//   {
//     pinMode(
//       LED_GPIO[i],
//       OUTPUT
//     );


//     digitalWrite(
//       LED_GPIO[i],
//       LOW
//     );
//   }


//   // ============================================================
//   // BUZZER
//   // ============================================================

//   pinMode(
//     BUZZER_PIN,
//     OUTPUT
//   );


//   noTone(
//     BUZZER_PIN
//   );


//   // ============================================================
//   // I2C
//   // ============================================================

//   Wire.begin(
//     I2C_SDA,
//     I2C_SCL
//   );


//   // ============================================================
//   // PCF8574
//   // ============================================================

//   Serial.println(
//     "Initialisation PCF8574..."
//   );


//   if (
//     !pcf.begin(
//       PCF8574_ADDR,
//       &Wire
//     )
//   )
//   {
//     Serial.println(
//       "ERREUR : PCF8574 NON TROUVE !"
//     );


//     displayText(
//       "PCF ERR"
//     );


//     while (true)
//     {
//       delay(100);
//     }
//   }


//   Serial.print(
//     "PCF8574 OK - adresse 0x"
//   );


//   Serial.println(
//     PCF8574_ADDR,
//     HEX
//   );


//   // ------------------------------------------------------------
//   // P0 à P7 = LEDs
//   // ------------------------------------------------------------

//   for (
//     uint8_t i = 0;
//     i < 8;
//     i++
//   )
//   {
//     pcf.pinMode(
//       i,
//       OUTPUT
//     );


//     // LEDs OFF
//     // actif LOW

//     pcf.digitalWrite(
//       i,
//       LOW
//     );
//   }


//   // ============================================================
//   // MAX7219
//   // ============================================================

//   Serial.println(
//     "Initialisation MAX7219..."
//   );


//   disp.begin();


//   disp.setIntensity(
//     5
//   );


//   disp.displayClear();


//   displayText(
//     "TEST"
//   );


//   // ============================================================
//   // IR
//   // ============================================================

//   Serial.println(
//     "Initialisation IR..."
//   );


//   IrReceiver.begin(
//     IR_RECEIVE_PIN,
//     DISABLE_LED_FEEDBACK
//   );


//   // ============================================================
//   // LEDs OFF
//   // ============================================================

//   allLedsOff();


//   // ============================================================
//   // TEST DE DÉMARRAGE
//   // ============================================================

//   delay(1000);


//   displayCurrentMode();


//   Serial.println();
//   Serial.println(
//     "=========================================="
//   );

//   Serial.println(
//     " SYSTEME PRET"
//   );

//   Serial.println(
//     "=========================================="
//   );

//   Serial.println(
//     "Boutons : 1 -> 12"
//   );

//   Serial.println(
//     "IR 1    = CLASSIC"
//   );

//   Serial.println(
//     "IR 2    = FITNESS"
//   );

//   Serial.println(
//     "IR PLAY = GO"
//   );

//   Serial.println(
//     "=========================================="
//   );

//   Serial.println();
// }


// // ================================================================
// // 21. LOOP
// // ================================================================

// void loop()
// {
//   handleButtons();

//   handleIR();

//   updateLedTimer();

//   disp.displayAnimate();
// }