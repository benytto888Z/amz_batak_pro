// /**********************************************************************
//  *  AMZ BATAK PRO v2 - PONT ESP8266 (bartack_esp_bridge)
//  *  Cible : ESP8266 integre a l'Arduino MEGA WiFi R3
//  *  Version consolidee sur la base du TEST DE REFERENCE valide
//  *  (test_esp.ino / test_mega.ino / test_flutter.dart, sept. 2026).
//  *
//  *  ROLE :
//  *   - SoftAP Wi-Fi "AMZ-BATAK" (192.168.4.1) - parametres du test valide
//  *   - Serveur WebSocket port 81 (lib WebSockets de Markus Sattler,
//  *     meme signature d'evenement que le test valide)
//  *   - PONT TRANSPARENT : relaie tout message texte tel quel :
//  *       trame WebSocket -> une ligne Serial (vers Mega, Serial3)
//  *       une ligne Serial -> broadcast WebSocket (vers l'app)
//  *     => compatible CSV du test ("LED,5,1"/"BTN,6,1") ET JSON de la V2.
//  *
//  *  LECONS DU TEST DE REFERENCE integrees ici :
//  *   1. Le chainon critique Serial->broadcast (§12 du resume) est
//  *      implemente et NE PEUT PAS etre court-circuite par un parsing.
//  *   2. AUCUN print de diagnostic sur Serial : dans le test, les logs
//  *      ESP ("FLUTTER -> ESP : ...", "WIFI CLIENTS : ...") partaient
//  *      sur la MEME ligne serie que les commandes, donc arrivaient
//  *      dans le Mega comme du bruit. Ici la ligne serie ne transporte
//  *      QUE les messages utiles + 2 messages de service.
//  *   3. softAPConfig explicite, WIFI_NONE_SLEEP, heartbeat WS :
//  *      repris du test car valides sur votre materiel.
//  *
//  *  Messages de service (vers le Mega uniquement) :
//  *    {"t":"espReady","ip":"192.168.4.1"}
//  *    {"t":"espClient","connected":true,"n":1}
//  *    {"t":"espClient","connected":false,"n":0}
//  *
//  *  ============================ FLASHAGE ============================
//  *  1. DIP switches : 5=ON 6=ON 7=ON, reste OFF (selecteur RXD/TXD :
//  *     cote ESP8266 si present)
//  *  2. Carte : "Generic ESP8266 Module", Flash 4MB (FS:none)
//  *  3. Bibliotheque : "WebSockets" par Markus Sattler
//  *  4. Televerser puis DIP en mode JEU : selon votre carte, liaison
//  *     ESP<->Serial3 du Mega (configuration validee par votre test).
//  **********************************************************************/

// #include <Arduino.h>
// #include <ESP8266WiFi.h>
// #include <Hash.h>
// #include <WebSocketsServer.h>

// /* =================== Wi-Fi (parametres du test valide) ============= */
// static const char *WIFI_SSID     = "AMZ-BATAK";
// static const char *WIFI_PASSWORD = "AMZ123456";

// static const IPAddress AP_IP(192, 168, 4, 1);
// static const IPAddress AP_GATEWAY(192, 168, 4, 1);
// static const IPAddress AP_SUBNET(255, 255, 255, 0);

// static const uint8_t AP_CHANNEL     = 6;
// static const uint8_t AP_MAX_CLIENTS = 4;

// /* =================== WebSocket ===================================== */
// WebSocketsServer webSocket(81);
// void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload,
//                     size_t length);

// /* =================== Liaison serie vers le Mega ==================== */
// static const uint32_t LINK_BAUD = 115200;
// static const uint16_t LINE_MAX  = 512;

// char     lineBuf[LINE_MAX];
// uint16_t lineLen = 0;
// uint8_t  clientCount = 0;

// /* =================== Messages de service =========================== */
// void sendEspReady() {
//   String msg = String("{\"t\":\"espReady\",\"ip\":\"") +
//                WiFi.softAPIP().toString() + "\"}";
//   Serial.println(msg);                        // vers le Mega
// }

// void sendEspClient(bool connected) {
//   String msg = String("{\"t\":\"espClient\",\"connected\":") +
//                (connected ? "true" : "false") +
//                ",\"n\":" + clientCount + "}";
//   Serial.println(msg);                        // vers le Mega
// }

// /* =================== Evenements WebSocket ========================== */
// // Signature IDENTIQUE au test de reference (WStype_t valide)
// void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload,
//                     size_t length) {
//   switch (type) {

//     case WStype_CONNECTED:
//       clientCount++;
//       sendEspClient(true);
//       break;

//     case WStype_DISCONNECTED:
//       if (clientCount > 0) clientCount--;
//       sendEspClient(false);
//       break;

//     case WStype_TEXT:
//       // App -> Mega : relais transparent (CSV ou JSON), 1 trame = 1 ligne
//       if (length > 0 && length < LINE_MAX) {
//         Serial.write(payload, length);
//         Serial.write('\n');
//       }
//       break;

//     default:
//       break;
//   }
// }

// /* =================== SETUP ========================================= */
// void setup() {
//   Serial.begin(LINK_BAUD);                    // liaison vers le Mega
//   Serial.setRxBufferSize(1024);
//   delay(500);

//   // --- Wi-Fi : sequence du test valide ---
//   WiFi.persistent(false);
//   WiFi.disconnect(true);
//   WiFi.softAPdisconnect(true);
//   delay(100);

//   WiFi.mode(WIFI_AP);
//   WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
//   WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, AP_CHANNEL, false, AP_MAX_CLIENTS);
//   WiFi.setSleepMode(WIFI_NONE_SLEEP);         // stabilite (test valide)

//   // --- WebSocket ---
//   webSocket.onEvent(webSocketEvent);
//   webSocket.begin();
//   webSocket.enableHeartbeat(15000, 3000, 2);  // detecte les clients morts

//   delay(100);
//   sendEspReady();                             // informe le Mega
// }

// /* =================== BOUCLE ======================================== */
// void loop() {
//   webSocket.loop();

//   // Mega -> App : LE chainon critique (§12 du resume de test).
//   // Assemble les lignes venues du Serial et les diffuse en WebSocket.
//   while (Serial.available()) {
//     char c = (char)Serial.read();

//     if (c == '\n' || c == '\r') {
//       if (lineLen > 0) {
//         lineBuf[lineLen] = '\0';
//         webSocket.broadcastTXT(lineBuf, lineLen);
//         lineLen = 0;
//       }
//     } else if (lineLen < LINE_MAX - 1) {
//       lineBuf[lineLen++] = c;
//     } else {
//       lineLen = 0;                            // ligne trop longue : jetee
//     }
//   }

//   yield();
// }
