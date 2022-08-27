import 'dart:async';
import 'dart:math' as math;
import 'dart:typed_data';

import 'package:flutter/material.dart';

import 'package:mp_audio_stream/mp_audio_stream.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({Key? key}) : super(key: key);

  // This widget is the root of your application.
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Audio Stream Demo',
      theme: ThemeData(
        primarySwatch: Colors.blue,
      ),
      home: const MyHomePage(title: 'Audio Stream Demo'),
    );
  }
}

class MyHomePage extends StatefulWidget {
  const MyHomePage({Key? key, required this.title}) : super(key: key);

  final String title;

  @override
  State<MyHomePage> createState() => _MyHomePageState();
}

class _MyHomePageState extends State<MyHomePage> {
  late final AudioStream audioStream;
  AudioStreamStat stat = AudioStreamStat.empty();

  @override
  void initState() {
    super.initState();
    audioStream = getAudioStream();
    audioStream.init(bufferMilliSec: 1000, waitingBufferMilliSec: 100);
  }

  @override
  void dispose() {
    audioStream.uninit();
    super.dispose();
  }

  static Float32List synthSineWave(double freq, int rate, Duration duration) {
    final length = duration.inMilliseconds * rate ~/ 1000;

    final sineWave = List.generate(length, (i) {
      final amp = math.sin(2 * math.pi * ((i * freq) % rate) / rate);

      const reduceSamples = 100;
      double reduce;
      if (i > length - reduceSamples) {
        reduce = (length - i - 1) / reduceSamples;
      } else if (i < reduceSamples) {
        reduce = i / reduceSamples;
      } else {
        reduce = 1.0;
      }

      return amp * reduce;
    });

    return Float32List.fromList(sineWave);
  }

  Future<void> play(double freq, Duration duration) async {
    const hz = 60;
    const rate = 44100;
    const step = rate ~/ hz;

    final wave = synthSineWave(freq, rate, duration);
    int pos = 0;

    final completer = Completer();

    // push pieces of generated wave to audio stream
    Timer.periodic(const Duration(milliseconds: 1000 ~/ hz), (t) {
      audioStream.push(wave.sublist(pos, pos + step));

      setState(() => stat = audioStream.stat());

      pos += step;
      if (pos >= wave.length) {
        t.cancel();
        completer.complete();
      }
    });

    await completer.future;
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(widget.title),
      ),
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: <Widget>[
            Text("full: ${stat.full} exhaust:${stat.exhaust}"),
            ElevatedButton(
                onPressed: () async {
                  // for web, calling `resume()` from user-action is needed
                  audioStream.resume();
                  for (double freq in [261.626, 293.665, 329.628]) {
                    await play(freq, const Duration(seconds: 1));
                  }
                },
                child: const Text(
                  'generate sine wave',
                ))
          ],
        ),
      ),
    );
  }
}
