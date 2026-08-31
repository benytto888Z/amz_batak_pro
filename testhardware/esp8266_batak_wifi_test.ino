// #include <Arduino.h>
// #include <ESP8266WiFi.h>
// #include <Hash.h>
// #include <WebSocketsServer.h>


// // ============================================================
// // AMZ BATAK PRO
// // ESP8266 WIFI BRIDGE
// //
// // FLUTTER <-> ESP8266 <-> MEGA
// // ============================================================


// // ============================================================
// // WIFI
// // ============================================================

// static const char* WIFI_SSID =
//   "AMZ-BATAK";

// static const char* WIFI_PASSWORD =
//   "AMZ123456";


// // ============================================================
// // IP
// // ============================================================

// static const IPAddress AP_IP(
//   192, 168, 4, 1
// );

// static const IPAddress AP_GATEWAY(
//   192, 168, 4, 1
// );

// static const IPAddress AP_SUBNET(
//   255, 255, 255, 0
// );


// // ============================================================
// // WEBSOCKET
// // ============================================================

// WebSocketsServer webSocket(81);


// // IMPORTANT :
// // On conserve EXACTEMENT le type qui fonctionne
// // dans ton ancien programme.

// void webSocketEvent(
//   uint8_t num,
//   WStype_t type,
//   uint8_t * payload,
//   size_t length
// );


// // ============================================================
// // BUFFER MEGA
// // ============================================================

// String megaBuffer = "";


// // ============================================================
// // WEBSOCKET SEND
// // ============================================================

// inline bool wsSend(
//   uint8_t num,
//   String msg
// )
// {
//   return webSocket.sendTXT(
//     num,
//     msg
//   );
// }


// inline bool wsSend(
//   uint8_t num,
//   const char* s
// )
// {
//   String msg = s;

//   return webSocket.sendTXT(
//     num,
//     msg
//   );
// }


// inline bool wsBroadcast(
//   String msg
// )
// {
//   return webSocket.broadcastTXT(
//     msg
//   );
// }


// // ============================================================
// // FLUTTER -> ESP -> MEGA
// // ============================================================

// static void handleCommand(
//   uint8_t num,
//   const String& cmd
// )
// {
//   // ----------------------------------------------------------
//   // PING
//   // ----------------------------------------------------------

//   if (
//     cmd == "PING"
//   )
//   {
//     wsSend(
//       num,
//       "PONG"
//     );

//     return;
//   }


//   // ----------------------------------------------------------
//   // STATUS
//   // ----------------------------------------------------------

//   if (
//     cmd == "STATUS"
//   )
//   {
//     String rep =
//       "STATUS,CLIENTS=";

//     rep +=
//       webSocket.connectedClients(
//         false
//       );

//     rep +=
//       ",UPTIME=";

//     rep +=
//       millis() / 1000UL;


//     wsSend(
//       num,
//       rep
//     );

//     return;
//   }


//   // ----------------------------------------------------------
//   // LED
//   // ----------------------------------------------------------

//   if (
//     cmd.startsWith(
//       "LED,"
//     )
//   )
//   {
//     Serial.print(
//       cmd
//     );

//     Serial.print(
//       '\n'
//     );


//     Serial.print(
//       "FLUTTER -> MEGA : "
//     );

//     Serial.println(
//       cmd
//     );


//     wsSend(
//       num,
//       "ESP,LED_SENT"
//     );

//     return;
//   }


//   // ----------------------------------------------------------
//   // AUTRES COMMANDES
//   // ----------------------------------------------------------

//   wsSend(
//     num,
//     "ESP,OK"
//   );
// }


// // ============================================================
// // WEBSOCKET EVENT
// // ============================================================

// void webSocketEvent(
//   uint8_t num,
//   WStype_t type,
//   uint8_t * payload,
//   size_t length
// )
// {
//   switch (
//     type
//   )
//   {

//     // ========================================================
//     // CLIENT CONNECTE
//     // ========================================================

//     case WStype_CONNECTED:
//     {
//       IPAddress ip =
//         webSocket.remoteIP(
//           num
//         );


//       Serial.println();

//       Serial.println(
//         "================================"
//       );

//       Serial.println(
//         "FLUTTER CONNECTE"
//       );


//       Serial.print(
//         "CLIENT : "
//       );

//       Serial.println(
//         num
//       );


//       Serial.print(
//         "IP CLIENT : "
//       );

//       Serial.println(
//         ip
//       );


//       Serial.println(
//         "================================"
//       );


//       wsSend(
//         num,
//         "STATUS,CONNECTED"
//       );


//       break;
//     }


//     // ========================================================
//     // CLIENT DECONNECTE
//     // ========================================================

//     case WStype_DISCONNECTED:

//       Serial.print(
//         "FLUTTER DECONNECTE - CLIENT : "
//       );

//       Serial.println(
//         num
//       );

//       break;


//     // ========================================================
//     // MESSAGE FLUTTER
//     // ========================================================

//     case WStype_TEXT:
//     {
//       String msg;

//       msg.reserve(
//         length + 1
//       );


//       for (
//         size_t i = 0;
//         i < length;
//         i++
//       )
//       {
//         msg +=
//           (char)payload[i];
//       }


//       msg.trim();


//       Serial.print(
//         "FLUTTER -> ESP : "
//       );

//       Serial.println(
//         msg
//       );


//       handleCommand(
//         num,
//         msg
//       );


//       break;
//     }


//     // ========================================================
//     // BINAIRE
//     // ========================================================

//     case WStype_BIN:

//       Serial.printf(
//         "BIN RECU %u octets\n",
//         (unsigned)length
//       );

//       wsSend(
//         num,
//         "ESP,BIN_OK"
//       );

//       break;


//     // ========================================================
//     // ERREUR
//     // ========================================================

//     case WStype_ERROR:

//       Serial.printf(
//         "WS ERREUR CLIENT %u\n",
//         num
//       );

//       break;


//     default:

//       break;
//   }
// }


// // ============================================================
// // MEGA -> ESP -> FLUTTER
// // ============================================================

// void readMega()
// {
//   while (
//     Serial.available()
//   )
//   {
//     char c =
//       Serial.read();


//     // --------------------------------------------------------
//     // FIN DE LIGNE
//     // --------------------------------------------------------

//     if (
//       c == '\n'
//     )
//     {
//       megaBuffer.trim();


//       if (
//         megaBuffer.length() > 0
//       )
//       {
//         // ----------------------------------------------
//         // Diagnostic
//         // ----------------------------------------------

//         Serial.print(
//           "MEGA -> ESP : "
//         );

//         Serial.println(
//           megaBuffer
//         );


//         // ----------------------------------------------
//         // Envoi à Flutter
//         // ----------------------------------------------

//         wsBroadcast(
//           megaBuffer
//         );
//       }


//       megaBuffer =
//         "";
//     }


//     // --------------------------------------------------------
//     // Retour chariot
//     // --------------------------------------------------------

//     else if (
//       c == '\r'
//     )
//     {
//       // Ignorer
//     }


//     // --------------------------------------------------------
//     // CARACTERE NORMAL
//     // --------------------------------------------------------

//     else
//     {
//       megaBuffer +=
//         c;


//       // Protection mémoire

//       if (
//         megaBuffer.length() > 100
//       )
//       {
//         megaBuffer =
//           "";
//       }
//     }
//   }
// }


// // ============================================================
// // SETUP
// // ============================================================

// void setup()
// {
//   Serial.begin(
//     115200
//   );


//   delay(
//     1000
//   );


//   Serial.println();

//   Serial.println(
//     "=========================================="
//   );

//   Serial.println(
//     "AMZ BATAK PRO - ESP8266"
//   );

//   Serial.println(
//     "FLUTTER <-> ESP8266 <-> MEGA"
//   );

//   Serial.println(
//     "=========================================="
//   );


//   // ==========================================================
//   // WIFI
//   // ==========================================================

//   WiFi.persistent(
//     false
//   );


//   WiFi.disconnect(
//     true
//   );


//   WiFi.softAPdisconnect(
//     true
//   );


//   delay(
//     100
//   );


//   WiFi.mode(
//     WIFI_AP
//   );


//   WiFi.softAPConfig(
//     AP_IP,
//     AP_GATEWAY,
//     AP_SUBNET
//   );


//   WiFi.softAP(
//     WIFI_SSID,
//     WIFI_PASSWORD,
//     6,
//     false,
//     4
//   );


//   WiFi.setSleepMode(
//     WIFI_NONE_SLEEP
//   );


//   Serial.print(
//     "IP : "
//   );

//   Serial.println(
//     WiFi.softAPIP()
//   );


//   // ==========================================================
//   // WEBSOCKET
//   // ==========================================================

//   webSocket.onEvent(
//     webSocketEvent
//   );


//   webSocket.begin();


//   webSocket.enableHeartbeat(
//     15000,
//     3000,
//     2
//   );


//   Serial.println();

//   Serial.println(
//     "WIFI ACCESS POINT READY"
//   );

//   Serial.println(
//     "SSID : AMZ-BATAK"
//   );

//   Serial.println(
//     "PASSWORD : AMZ123456"
//   );

//   Serial.println(
//     "WEBSOCKET SERVER READY"
//   );

//   Serial.println(
//     "PORT : 81"
//   );

//   Serial.println(
//     "URL : ws://192.168.4.1:81"
//   );


//   Serial.println(
//     "=========================================="
//   );
// }


// // ============================================================
// // LOOP
// // ============================================================

// void loop()
// {
//   // ----------------------------------------------------------
//   // WebSocket
//   // ----------------------------------------------------------

//   webSocket.loop();


//   // ----------------------------------------------------------
//   // Mega -> ESP -> Flutter
//   // ----------------------------------------------------------

//   readMega();


//   // ----------------------------------------------------------
//   // Diagnostic
//   // ----------------------------------------------------------

//   static uint32_t lastCheck =
//     0;


//   if (
//     millis() - lastCheck >= 3000
//   )
//   {
//     lastCheck =
//       millis();


//     Serial.printf(
//       "WIFI CLIENTS : %u | "
//       "WS CLIENTS : %u | "
//       "HEAP : %u\n",

//       WiFi.softAPgetStationNum(),

//       webSocket.connectedClients(
//         false
//       ),

//       ESP.getFreeHeap()
//     );
//   }


//   yield();
// }