// /**********************************************************************
//  *  BARTACK PRO v2 - PONT ESP8266 (bartack_esp_bridge)
//  *  Cible : ESP8266 integre a l'Arduino MEGA WiFi R3
//  *
//  *  ROLE (voir ETAPE_01_ARCHITECTURE_PROTOCOLE.md §1 et §4) :
//  *   - Cree le Wi-Fi SoftAP "BARTACK_PRO" (192.168.4.1)
//  *   - Serveur WebSocket sur le port 81
//  *   - PONT TRANSPARENT : il NE COMPREND PAS le JSON, il relaie :
//  *       trame texte WebSocket  ->  une ligne sur Serial (vers le Mega)
//  *       une ligne sur Serial   ->  trame texte WebSocket (vers l'app)
//  *   - Genere seulement 3 messages de service (prefixe "esp") :
//  *       {"t":"espReady","ip":"192.168.4.1"}        au demarrage
//  *       {"t":"espClient","connected":true,"n":1}   connexion app
//  *       {"t":"espClient","connected":false,"n":0}  deconnexion app
//  *
//  *  LIAISON MEGA : le Serial de l'ESP8266 est relie au Serial3 du Mega
//  *  PAR LA CARTE (DIP switches) - aucun fil a ajouter.
//  *  Debit : 115200 bauds. 1 ligne (terminee par \n) = 1 message.
//  *
//  *  ============================ FLASHAGE ============================
//  *  1. DIP switches : 5=ON 6=ON 7=ON, tout le reste OFF
//  *     (petit selecteur RXD/TXD : cote ESP8266 si present)
//  *  2. IDE Arduino : installer le core ESP8266
//  *     (URL gestionnaire de cartes :
//  *      http://arduino.esp8266.com/stable/package_esp8266com_index.json)
//  *  3. Carte : "Generic ESP8266 Module"
//  *     Flash Size : "4MB (FS:none)"  |  Upload Speed : 115200
//  *  4. Bibliotheque : "WebSockets" par Markus Sattler (gestionnaire)
//  *  5. Televerser. Puis remettre les DIP en mode JEU v2 :
//  *     1=ON 2=ON 3=ON 4=ON, reste OFF.
//  *  Ce croquis n'est flashe qu'UNE FOIS : toute la logique de jeu
//  *  evolue cote Mega sans re-flasher l'ESP8266.
//  *  ==================================================================
//  *
//  *  Bibliotheques : ESP8266WiFi (core), WebSockets (Markus Sattler)
//  **********************************************************************/

// #include <ESP8266WiFi.h>
// #include <WebSocketsServer.h>

// /* =================== PARAMETRES Wi-Fi (Etape 1 §5) ================= */
// const char *AP_SSID     = "BARTACK_PRO";
// const char *AP_PASSWORD = "bartack2026";     // WPA2, 8 caracteres mini
// const uint8_t AP_CHANNEL     = 6;
// const uint8_t AP_MAX_CLIENTS = 2;            // 1 app + 1 debug

// const uint16_t WS_PORT = 81;



// /* =================== LIAISON SERIE VERS LE MEGA ==================== */
// const uint32_t LINK_BAUD    = 115200;
// const uint16_t LINE_MAX     = 512;           // trames < 512 octets (Etape 1)

// WebSocketsServer ws(WS_PORT);

// void onWsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);

// char     lineBuf[LINE_MAX];                  // ligne en cours (Mega -> app)
// uint16_t lineLen = 0;
// uint8_t  clientCount = 0;

// /* =================== MESSAGES DE SERVICE "esp" ===================== */
// void sendEspReady() {
//   String msg = String("{\"t\":\"espReady\",\"ip\":\"") +
//                WiFi.softAPIP().toString() + "\"}";
//   Serial.println(msg);                       // vers le Mega
// }

// void sendEspClient(bool connected) {
//   String msg = String("{\"t\":\"espClient\",\"connected\":") +
//                (connected ? "true" : "false") +
//                ",\"n\":" + clientCount + "}";
//   Serial.println(msg);                       // vers le Mega
// }

// /* =================== EVENEMENTS WEBSOCKET ========================== */
// void onWsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
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
//       // App -> Mega : relais transparent, 1 trame = 1 ligne
//       if (length > 0 && length < LINE_MAX) {
//         Serial.write(payload, length);
//         Serial.write('\n');
//       }
//       break;

//     default:
//       break;                                 // binaire/ping WS : ignores
//   }
// }

// /* =================== SETUP ========================================= */
// void setup() {
//   Serial.begin(LINK_BAUD);                   // liaison interne vers le Mega
//   Serial.setRxBufferSize(1024);              // rafales de tick/hit

//   WiFi.persistent(false);                    // ne pas user la flash
//   WiFi.mode(WIFI_AP);
//   WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, /*hidden=*/false,
//               AP_MAX_CLIENTS);

//   ws.begin();
//   ws.onEvent(onWsEvent);

//   delay(100);
//   sendEspReady();                            // informe le Mega
// }

// /* =================== BOUCLE ======================================== */
// void loop() {
//   ws.loop();

//   // Mega -> App : assemble les lignes venues du Serial
//   while (Serial.available()) {
//     char c = (char)Serial.read();

//     if (c == '\n' || c == '\r') {
//       if (lineLen > 0) {
//         lineBuf[lineLen] = '\0';
//         ws.broadcastTXT(lineBuf, lineLen);   // vers tous les clients WS
//         lineLen = 0;
//       }
//     } else if (lineLen < LINE_MAX - 1) {
//       lineBuf[lineLen++] = c;
//     } else {
//       lineLen = 0;                           // ligne trop longue : jetee
//     }
//   }
// }
