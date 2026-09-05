// ====================================================================
// AMZ BATAK PRO — TESTEUR DU PROTOCOLE JSON (Étape 3)
//
// Base : le test de référence validé (GetX + web_socket_channel,
// reconnexion 2 s, await ws.ready). Adapté au protocole JSON du
// firmware bartack_pro 2.0-e3 :
//
//   ENVOI  : hello, ping, configure, start, stop, quit
//   RECU   : hello, state, countdown, tick, hit, miss, gameOver,
//            pong, err  (+ compatibilité CSV BTN,n,v de l'ancien test)
//
// Écran : bandeau de connexion, boutons protocole, tableau de bord
// (état/score/temps/round/hpm), miroir des 12 cibles (flash vert = hit,
// rouge = miss), rapport gameOver, console de log.
// ====================================================================

import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:get/get.dart';
import 'package:web_socket_channel/web_socket_channel.dart';

// ====================================================================
// CONTROLLER
// ====================================================================

class BatakController extends GetxController {
  // ---------------- ESP8266 ----------------
  static const String espIp = '192.168.4.1';
  static const int espPort = 81;

  // ---------------- WebSocket ----------------
  WebSocketChannel? channel;
  StreamSubscription? subscription;

  // ---------------- État connexion ----------------
  final RxBool connected = false.obs;
  Timer? reconnectTimer;
  bool manuallyClosed = false;

  // ---------------- État du jeu (reçu du mur) ----------------
  final RxString wallState = '—'.obs; // wait/ready/countdown/play/rest/gameover
  final RxInt countdown = (-1).obs; // 3,2,1,0 ; -1 = inactif
  final RxInt score = 0.obs;
  final RxInt remainSec = 0.obs;
  final RxInt round = 0.obs;
  final RxString phase = ''.obs;
  final RxInt hpm = 0.obs;
  final RxString fwVersion = ''.obs;
  final RxInt lastReactMs = 0.obs;
  final Rxn<Map<String, dynamic>> gameOver = Rxn<Map<String, dynamic>>();

  // ---------------- Miroir des 12 cibles ----------------
  // 0 = éteint, 1 = flash HIT (vert), 2 = flash MISS (rouge)
  final RxList<int> pads = List<int>.filled(12, 0).obs;
  final List<Timer?> padTimers = List<Timer?>.filled(12, null);

  // ---------------- Console ----------------
  final RxList<String> log = <String>[].obs;
  int pingSeq = 0;

  void addLog(String prefix, String msg) {
    final now = DateTime.now();
    final ts = '${now.hour.toString().padLeft(2, '0')}:'
        '${now.minute.toString().padLeft(2, '0')}:'
        '${now.second.toString().padLeft(2, '0')}.'
        '${(now.millisecond ~/ 10).toString().padLeft(2, '0')}';
    log.insert(0, '$ts $prefix $msg');
    if (log.length > 150) log.removeRange(150, log.length);
  }

  // ================================================================
  // CONNECT (base du test de référence)
  // ================================================================
  Future<void> connect() async {
    if (connected.value) return;
    addLog('··', 'Tentative connexion ws://$espIp:$espPort ...');

    try {
      manuallyClosed = false;
      await subscription?.cancel();
      await channel?.sink.close();

      final ws = WebSocketChannel.connect(Uri.parse('ws://$espIp:$espPort'));
      channel = ws;
      await ws.ready;

      connected.value = true;
      addLog('··', 'CONNECTÉ');

      subscription = ws.stream.listen(
            (dynamic message) => handleMessage(message.toString()),
        onDone: () {
          addLog('··', 'FERMÉ');
          connected.value = false;
          scheduleReconnect();
        },
        onError: (Object error, StackTrace stack) {
          addLog('!!', 'ERREUR : $error');
          connected.value = false;
          scheduleReconnect();
        },
      );

      // hello automatique à la connexion (contrat Étape 1)
      sendHello();
    } catch (e) {
      connected.value = false;
      addLog('!!', 'ERREUR : $e');
      scheduleReconnect();
    }
  }

  void scheduleReconnect() {
    if (manuallyClosed) return;
    if (reconnectTimer != null && reconnectTimer!.isActive) return;
    reconnectTimer = Timer(const Duration(seconds: 2), connect);
  }

  Future<void> disconnect() async {
    manuallyClosed = true;
    reconnectTimer?.cancel();
    await subscription?.cancel();
    try {
      await channel?.sink.close();
    } catch (_) {}
    connected.value = false;
  }

  // ================================================================
  // ENVOI
  // ================================================================
  void sendRaw(String s) {
    if (!connected.value) {
      addLog('!!', 'Impossible : non connecté');
      return;
    }
    channel!.sink.add(s);
    addLog('->', s);
  }

  void sendJson(Map<String, dynamic> m) => sendRaw(jsonEncode(m));

  void sendHello() => sendJson({
    't': 'hello',
    'app': 'batak_ws_tester',
    'ver': '1.0',
    'lang': 'fr',
  });

  void sendPing() => sendJson({'t': 'ping', 'seq': ++pingSeq});

  void sendConfigure() => sendJson({
    't': 'configure',
    'mode': 'hiit',
    'program': {
      'name': 'HIIT Test',
      'rounds': [
        {
          'workSec': 20,
          'restSec': 10,
          'lightOnMs': 1200,
          'gapMs': 250,
          'accel': 1
        },
        {
          'workSec': 20,
          'restSec': 0,
          'lightOnMs': 900,
          'gapMs': 200,
          'accel': 1
        },
      ],
      'simultaneous': 1,
      'missMode': 'skip',
    },
  });

  void sendStart() {
    gameOver.value = null;
    sendJson({'t': 'start'});
  }

  void sendStop() => sendJson({'t': 'stop'});
  void sendQuit() => sendJson({'t': 'quit'});

  // ================================================================
  // RÉCEPTION
  // ================================================================
  void flashPad(int index, int kind) {
    if (index < 0 || index >= 12) return;
    pads[index] = kind;
    padTimers[index]?.cancel();
    padTimers[index] = Timer(const Duration(milliseconds: 350), () {
      pads[index] = 0;
    });
  }

  void handleMessage(String message) {
    addLog('<-', message);

    // ---- compatibilité CSV de l'ancien test (BTN,n,v / LED,n,v) ----
    if (!message.startsWith('{')) {
      final parts = message.split(',');
      if (parts.length >= 3 && (parts[0] == 'BTN' || parts[0] == 'LED')) {
        final n = int.tryParse(parts[1]);
        final v = int.tryParse(parts[2]);
        if (n != null && v != null) flashPad(n - 1, v == 1 ? 1 : 0);
      }
      return;
    }

    // ---- JSON ----
    Map<String, dynamic> m;
    try {
      m = jsonDecode(message) as Map<String, dynamic>;
    } catch (_) {
      return;
    }

    switch (m['t']) {
      case 'hello':
        fwVersion.value = (m['fw'] ?? '?').toString();
        break;

      case 'state':
        wallState.value = (m['s'] ?? '—').toString();
        if (wallState.value != 'countdown') countdown.value = -1;
        break;

      case 'countdown':
        countdown.value = (m['n'] ?? -1) as int;
        break;

      case 'tick':
        round.value = (m['round'] ?? 0) as int;
        phase.value = (m['phase'] ?? '').toString();
        remainSec.value = (m['remainSec'] ?? 0) as int;
        score.value = (m['score'] ?? 0) as int;
        hpm.value = (m['hpm'] ?? 0) as int;
        break;

      case 'hit':
        score.value = (m['score'] ?? score.value) as int;
        lastReactMs.value = (m['reactMs'] ?? 0) as int;
        flashPad(((m['btn'] ?? 0) as int) - 1, 1);
        break;

      case 'miss':
        flashPad(((m['btn'] ?? 0) as int) - 1, 2);
        break;

      case 'gameOver':
        gameOver.value = m;
        break;

      case 'pong':
      case 'err':
        break; // visibles dans la console
    }
  }

  // ================================================================
  @override
  void onInit() {
    super.onInit();
    connect();
  }

  @override
  void onClose() {
    for (final t in padTimers) {
      t?.cancel();
    }
    disconnect();
    super.onClose();
  }
}

// ====================================================================
// MAIN / APP
// ====================================================================

void main() {
  runApp(const BatakApp());
}

class BatakApp extends StatelessWidget {
  const BatakApp({super.key});

  @override
  Widget build(BuildContext context) {
    return GetMaterialApp(
      debugShowCheckedModeBanner: false,
      title: 'AMZ Batak Tester',
      theme: ThemeData.dark(useMaterial3: true).copyWith(
        scaffoldBackgroundColor: const Color(0xFF101114),
      ),
      home: const TesterScreen(),
    );
  }
}

// ====================================================================
// SCREEN
// ====================================================================

class TesterScreen extends StatelessWidget {
  const TesterScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final c = Get.put(BatakController());

    return Scaffold(
      appBar: AppBar(
        backgroundColor: const Color(0xFF16181D),
        title: const Text('AMZ BATAK — TESTEUR PROTOCOLE'),
        actions: [
          Obx(() => Padding(
            padding: const EdgeInsets.only(right: 20),
            child: Center(
              child: Row(children: [
                Icon(Icons.circle,
                    size: 12,
                    color: c.connected.value ? Colors.green : Colors.red),
                const SizedBox(width: 8),
                Text(c.connected.value
                    ? 'CONNECTÉ ${c.fwVersion.value.isNotEmpty ? "(fw ${c.fwVersion.value})" : ""}'
                    : 'DÉCONNECTÉ'),
              ]),
            ),
          )),
        ],
      ),
      body: SafeArea(
        child: LayoutBuilder(builder: (context, constraints) {
          final wide = constraints.maxWidth >= 900;
          final left = _leftColumn(c);
          final right = _console(c);
          return wide
              ? Row(children: [
            Expanded(flex: 3, child: left),
            SizedBox(width: 360, child: right),
          ])
              : ListView(children: [
            left,
            SizedBox(height: 300, child: right),
          ]);
        }),
      ),
    );
  }

  // ---------------- colonne principale ----------------
  Widget _leftColumn(BatakController c) {
    return SingleChildScrollView(
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          _protocolButtons(c),
          const SizedBox(height: 16),
          _dashboard(c),
          const SizedBox(height: 16),
          _padGrid(c),
          const SizedBox(height: 16),
          Obx(() =>
          c.gameOver.value != null ? _gameOverCard(c) : const SizedBox()),
        ],
      ),
    );
  }

  // ---------------- boutons protocole ----------------
  Widget _protocolButtons(BatakController c) {
    Widget btn(String label, VoidCallback onTap, {Color? color}) =>
        ElevatedButton(
          style: ElevatedButton.styleFrom(
            backgroundColor: color ?? const Color(0xFF23262D),
            foregroundColor: Colors.white,
            padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 14),
          ),
          onPressed: onTap,
          child: Text(label, style: const TextStyle(fontWeight: FontWeight.bold)),
        );

    return Wrap(
      spacing: 10,
      runSpacing: 10,
      children: [
        btn('HELLO', c.sendHello),
        btn('PING', c.sendPing),
        btn('CONFIGURE HIIT', c.sendConfigure, color: const Color(0xFF1C4587)),
        btn('START', c.sendStart, color: const Color(0xFF38761D)),
        btn('STOP', c.sendStop, color: const Color(0xFF990000)),
        btn('QUIT', c.sendQuit),
      ],
    );
  }

  // ---------------- tableau de bord ----------------
  Widget _dashboard(BatakController c) {
    Widget cell(String label, Widget value) => Expanded(
      child: Container(
        margin: const EdgeInsets.all(4),
        padding: const EdgeInsets.symmetric(vertical: 12),
        decoration: BoxDecoration(
          color: const Color(0xFF1A1D23),
          borderRadius: BorderRadius.circular(12),
        ),
        child: Column(children: [
          Text(label,
              style: const TextStyle(fontSize: 11, color: Colors.grey)),
          const SizedBox(height: 6),
          value,
        ]),
      ),
    );

    const vs = TextStyle(fontSize: 22, fontWeight: FontWeight.bold);

    return Obx(() {
      final cd = c.countdown.value;
      return Column(children: [
        if (cd >= 0)
          Padding(
            padding: const EdgeInsets.only(bottom: 8),
            child: Text(
              cd == 0 ? 'GO !' : '$cd',
              style: TextStyle(
                  fontSize: 64,
                  fontWeight: FontWeight.w900,
                  color: cd == 0 ? Colors.green : Colors.amber),
            ),
          ),
        Row(children: [
          cell('ÉTAT', Text(c.wallState.value.toUpperCase(), style: vs)),
          cell('SCORE',
              Text(c.score.value.toString().padLeft(4, '0'), style: vs)),
          cell('TEMPS',
              Text(c.remainSec.value.toString().padLeft(2, '0'), style: vs)),
        ]),
        Row(children: [
          cell(
              'ROUND / PHASE',
              Text(
                  c.round.value > 0
                      ? 'R${c.round.value} ${c.phase.value}'
                      : '—',
                  style: vs)),
          cell('FRAPPES/MIN', Text('${c.hpm.value}', style: vs)),
          cell('DERNIÈRE RÉACTION',
              Text('${c.lastReactMs.value} ms', style: vs)),
        ]),
      ]);
    });
  }

  // ---------------- miroir des 12 cibles ----------------
  // NB : un Obx PAR CELLULE (dans itemBuilder), jamais autour du
  // GridView.builder : les cellules sont construites paresseusement,
  // hors de la portee de tracage d'un Obx externe (erreur "improper
  // use of GetX"). Meme structure que le test de reference valide.
  Widget _padGrid(BatakController c) {
    return GridView.builder(
      shrinkWrap: true,
      physics: const NeverScrollableScrollPhysics(),
      itemCount: 12,
      gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
        crossAxisCount: 4,
        crossAxisSpacing: 12,
        mainAxisSpacing: 12,
        childAspectRatio: 1,
      ),
      itemBuilder: (context, index) {
        return Obx(() {
          final k = c.pads[index];
          final Color bg = k == 1
              ? Colors.green
              : k == 2
              ? Colors.red
              : const Color(0xFF292C32);
          return AnimatedContainer(
            duration: const Duration(milliseconds: 120),
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              color: bg,
              border: Border.all(
                  color: k != 0 ? Colors.white : Colors.grey.shade800,
                  width: k != 0 ? 3 : 2),
              boxShadow: k != 0
                  ? [
                BoxShadow(
                    color: bg.withOpacity(0.5),
                    blurRadius: 20,
                    spreadRadius: 4)
              ]
                  : [],
            ),
            child: Center(
              child: Text(
                'B${(index + 1).toString().padLeft(2, '0')}',
                style: const TextStyle(
                    fontSize: 18, fontWeight: FontWeight.bold),
              ),
            ),
          );
        });
      },
    );
  }

  // ---------------- rapport gameOver ----------------
  Widget _gameOverCard(BatakController c) {
    final m = c.gameOver.value!;
    String v(String k) => (m[k] ?? '—').toString();
    final thirds = m['thirds'] as Map<String, dynamic>?;

    Widget line(String label, String value) => Padding(
      padding: const EdgeInsets.symmetric(vertical: 2),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(label, style: const TextStyle(color: Colors.grey)),
          Text(value,
              style: const TextStyle(fontWeight: FontWeight.bold)),
        ],
      ),
    );

    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: const Color(0xFF14210F),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: Colors.green.shade900),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          const Text('WORKOUT COMPLETE',
              textAlign: TextAlign.center,
              style: TextStyle(
                  fontSize: 18,
                  fontWeight: FontWeight.w900,
                  color: Colors.green)),
          const SizedBox(height: 8),
          line('Score', v('score')),
          line('Hits / Misses', '${v('hits')} / ${v('misses')}'),
          line('Réaction moy / best / worst',
              '${v('avgReactMs')} / ${v('bestReactMs')} / ${v('worstReactMs')} ms'),
          line('Écart-type (consistency)', '${v('sdReactMs')} ms'),
          line('Rang TOP 3', v('rank') == '0' ? 'hors TOP 3' : 'TOP ${v('rank')}'),
          line('TOP 3 du mode', (m['top3'] ?? []).toString()),
          if (thirds != null)
            line('Tiers (hits)', (thirds['hits'] ?? []).toString()),
          if (thirds != null)
            line('Tiers (réaction moy)',
                (thirds['avgReactMs'] ?? []).toString()),
          line('Durée', '${v('durationSec')} s'),
        ],
      ),
    );
  }

  // ---------------- console ----------------
  Widget _console(BatakController c) {
    return Container(
      margin: const EdgeInsets.all(8),
      padding: const EdgeInsets.all(8),
      decoration: BoxDecoration(
        color: const Color(0xFF0B0D10),
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: const Color(0xFF23262D)),
      ),
      child: Obx(() {
        // copie de la RxList DANS le build de l'Obx : l'observable est
        // ainsi lu dans sa portee de tracage (et pas seulement dans les
        // itemBuilder paresseux du ListView).
        final items = c.log.toList();
        return ListView.builder(
          reverse: false,
          itemCount: items.length,
          itemBuilder: (context, i) {
            final l = items[i];
            Color col = Colors.grey;
            if (l.contains(' -> ')) col = Colors.amber;
            if (l.contains(' <- ')) col = Colors.lightGreen;
            if (l.contains(' !! ')) col = Colors.redAccent;
            return Text(l,
                style: TextStyle(
                    fontFamily: 'monospace', fontSize: 11.5, color: col));
          },
        );
      }),
    );
  }
}
