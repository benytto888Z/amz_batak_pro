import 'dart:async';

import 'package:flutter/material.dart';
import 'package:get/get.dart';
import 'package:web_socket_channel/web_socket_channel.dart';


// ====================================================================
// CONTROLLER
// ====================================================================


class BatakController extends GetxController {

  // ================================================================
  // ESP8266
  // ================================================================

  static const String espIp =
      '192.168.4.1';

  static const int espPort =
  81;


  // ================================================================
  // WEBSOCKET
  // ================================================================

  WebSocketChannel? channel;

  StreamSubscription? subscription;


  // ================================================================
  // ETAT
  // ================================================================

  final RxBool connected =
      false.obs;


  final RxList<bool> buttons =
      List<bool>.filled(
        12,
        false,
      ).obs;


  // ================================================================
  // CONTROLE RECONNEXION
  // ================================================================

  Timer? reconnectTimer;

  bool manuallyClosed =
  false;


  // ================================================================
  // CONNECT
  // ================================================================

  Future<void> connect() async {

    if (connected.value)
    {
      return;
    }


    debugPrint(
        'Tentative connexion WebSocket...'
    );


    try {

      manuallyClosed =
      false;


      // ------------------------------------------------------------
      // Nettoyage ancienne connexion
      // ------------------------------------------------------------

      await subscription?.cancel();

      await channel?.sink.close();


      // ------------------------------------------------------------
      // Nouvelle connexion
      // ------------------------------------------------------------

      final ws =
      WebSocketChannel.connect(
        Uri.parse(
          'ws://$espIp:$espPort',
        ),
      );


      channel =
          ws;


      // ------------------------------------------------------------
      // ATTENDRE LA CONNEXION
      // ------------------------------------------------------------

      await ws.ready;


      connected.value =
      true;


      debugPrint(
          'WebSocket CONNECTED'
      );


      // ------------------------------------------------------------
      // Ecouter les messages
      // ------------------------------------------------------------

      subscription =
          ws.stream.listen(

                (dynamic message) {

              debugPrint(
                  'ESP -> FLUTTER : $message'
              );


              handleMessage(
                message.toString(),
              );
            },


            onDone: () {

              debugPrint(
                  'WebSocket CLOSED'
              );


              connected.value =
              false;


              scheduleReconnect();
            },


            onError: (
                Object error,
                StackTrace stack,
                ) {

              debugPrint(
                  'WebSocket ERROR : $error'
              );


              connected.value =
              false;


              scheduleReconnect();
            },
          );

    }

    catch (e) {

      connected.value =
      false;


      debugPrint(
          'WebSocket ERROR : $e'
      );


      scheduleReconnect();
    }
  }


  // ================================================================
  // RECONNEXION
  // ================================================================

  void scheduleReconnect() {

    if (manuallyClosed)
    {
      return;
    }


    if (
    reconnectTimer != null &&
        reconnectTimer!.isActive
    )
    {
      return;
    }


    reconnectTimer =
        Timer(
          const Duration(
            seconds: 2,
          ),

              () {

            connect();

          },
        );
  }


  // ================================================================
  // DECONNECT
  // ================================================================

  Future<void> disconnect() async {

    manuallyClosed =
    true;


    reconnectTimer?.cancel();


    await subscription?.cancel();


    try {

      await channel?.sink.close();

    }

    catch (_) {}


    connected.value =
    false;
  }


  // ================================================================
  // COMMAND LED
  // ================================================================

  void setLed(
      int index,
      bool state,
      ) {

    if (!connected.value)
    {
      debugPrint(
          'Impossible : ESP non connecté'
      );

      return;
    }


    final int button =
        index + 1;


    final String command =
        'LED,$button,${state ? 1 : 0}';


    debugPrint(
        'FLUTTER -> ESP : $command'
    );


    channel!.sink.add(
      command,
    );
  }


  // ================================================================
  // TOGGLE
  // ================================================================

  void toggleFromFlutter(
      int index,
      ) {

    final bool newState =
    !buttons[index];

    setLed(
      index,
      newState,
    );
  }


  // ================================================================
  // RECEPTION
  // ================================================================

  void handleMessage(
      String message,
      ) {

    final parts =
    message.split(',');


    if (
    parts.length < 3
    )
    {
      return;
    }


    final String type =
    parts[0];


    final int? number =
    int.tryParse(
      parts[1],
    );


    final int? value =
    int.tryParse(
      parts[2],
    );


    if (
    number == null ||
        value == null
    )
    {
      return;
    }


    final int index =
        number - 1;


    if (
    index < 0 ||
        index >= 12
    )
    {
      return;
    }


    if (
    type == 'BTN' ||
        type == 'LED'
    )
    {
      buttons[index] =
          value == 1;
    }
  }


  // ================================================================
  // INIT
  // ================================================================

  @override
  void onInit() {

    super.onInit();


    connect();
  }


  // ================================================================
  // CLOSE
  // ================================================================

  @override
  void onClose() {

    disconnect();


    super.onClose();
  }
}


// ====================================================================
// MAIN
// ====================================================================

void main() {

  runApp(
    const BatakApp(),
  );
}


// ====================================================================
// APPLICATION
// ====================================================================

class BatakApp extends StatelessWidget {

  const BatakApp({
    super.key,
  });


  @override
  Widget build(
      BuildContext context,
      ) {

    return GetMaterialApp(

      debugShowCheckedModeBanner:
      false,

      title:
      'AMZ Batak Pro',

      theme:
      ThemeData.dark(),

      home:
      const BatakTestScreen(),
    );
  }
}


// ====================================================================
// SCREEN
// ====================================================================

class BatakTestScreen
    extends StatelessWidget {

  const BatakTestScreen({
    super.key,
  });


  @override
  Widget build(
      BuildContext context,
      ) {

    final controller =
    Get.put(
      BatakController(),
    );


    return Scaffold(

      backgroundColor:
      const Color(0xFF101114),


      appBar:
      AppBar(

        title:
        const Text(
          'AMZ BATAK PRO',
        ),

        actions: [

          Obx(
                () => Padding(
              padding:
              const EdgeInsets.only(
                right: 20,
              ),

              child:
              Center(
                child:
                Row(
                  children: [

                    Icon(
                      Icons.circle,

                      size: 12,

                      color:
                      controller
                          .connected
                          .value
                          ? Colors.green
                          : Colors.red,
                    ),

                    const SizedBox(
                      width: 8,
                    ),

                    Text(
                      controller
                          .connected
                          .value
                          ? 'CONNECTED'
                          : 'DISCONNECTED',
                    ),
                  ],
                ),
              ),
            ),
          ),
        ],
      ),


      body:
      SafeArea(

        child:
        LayoutBuilder(

          builder:
              (
              context,
              constraints,
              ) {

            final double size =
            constraints.maxWidth <
                constraints.maxHeight
                ? constraints.maxWidth
                : constraints.maxHeight;


            return Center(

              child:
              ConstrainedBox(

                constraints:
                const BoxConstraints(
                  maxWidth: 1000,
                ),

                child:
                Padding(

                  padding:
                  const EdgeInsets.all(
                    24,
                  ),

                  child:
                  GridView.builder(

                    shrinkWrap:
                    true,

                    physics:
                    const BouncingScrollPhysics(),

                    itemCount:
                    12,

                    gridDelegate:
                    SliverGridDelegateWithFixedCrossAxisCount(

                      crossAxisCount:
                      constraints.maxWidth >=
                          800
                          ? 4
                          : constraints.maxWidth >=
                          500
                          ? 3
                          : 2,

                      crossAxisSpacing:
                      20,

                      mainAxisSpacing:
                      20,

                      childAspectRatio:
                      1,
                    ),

                    itemBuilder:
                        (
                        context,
                        index,
                        ) {

                      return Obx(
                            () {

                          final bool active =
                          controller
                              .buttons[index];


                          return GestureDetector(

                            onTap:
                                () {

                              controller
                                  .toggleFromFlutter(
                                index,
                              );
                            },


                            child:
                            AnimatedContainer(

                              duration:
                              const Duration(
                                milliseconds: 180,
                              ),

                              decoration:
                              BoxDecoration(

                                shape:
                                BoxShape.circle,

                                color:
                                active
                                    ? Colors.green
                                    : const Color(
                                  0xFF292C32,
                                ),

                                border:
                                Border.all(

                                  color:
                                  active
                                      ? Colors.white
                                      : Colors.grey.shade700,

                                  width:
                                  active
                                      ? 4
                                      : 2,
                                ),

                                boxShadow:
                                active
                                    ? [
                                  BoxShadow(
                                    color:
                                    Colors.green.withOpacity(
                                      0.5,
                                    ),
                                    blurRadius:
                                    25,
                                    spreadRadius:
                                    5,
                                  ),
                                ]
                                    : [],
                              ),


                              child:
                              Center(

                                child:
                                Column(

                                  mainAxisAlignment:
                                  MainAxisAlignment.center,

                                  children: [

                                    Text(
                                      'B${(index + 1).toString().padLeft(2, '0')}',

                                      style:
                                      const TextStyle(

                                        fontSize:
                                        28,

                                        fontWeight:
                                        FontWeight.bold,
                                      ),
                                    ),


                                    const SizedBox(
                                      height: 8,
                                    ),


                                    Text(
                                      active
                                          ? 'ON'
                                          : 'OFF',

                                      style:
                                      TextStyle(

                                        fontSize:
                                        14,

                                        color:
                                        active
                                            ? Colors.white
                                            : Colors.grey,
                                      ),
                                    ),
                                  ],
                                ),
                              ),
                            ),
                          );
                        },
                      );
                    },
                  ),
                ),
              ),
            );
          },
        ),
      ),
    );
  }
}