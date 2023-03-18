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

  bool _isPlaying = false;

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

  static Float32List _synthSineWave(double freq, int rate, Duration duration) {
    final length = duration.inMilliseconds * rate ~/ 1000;

    final fadeSamples = math.min(100, length ~/ 2);

    final sineWave = List.generate(length, (i) {
      final amp = math.sin(2 * math.pi * ((i * freq) % rate) / rate);

      // apply fade in/out to avoid click noise
      final volume = (i > length - fadeSamples)
          ? (length - i - 1) / fadeSamples
          : (i < fadeSamples)
              ? i / fadeSamples
              : 1.0;

      return amp * volume;
    });

    return Float32List.fromList(sineWave);
  }

  Future<void> _playNote(double freq, Duration duration) async {
    const hz = 60;
    const rate = 44100;
    const step = rate ~/ hz;

    final wave = _synthSineWave(freq, rate, duration);

    final completer = Completer();

    // divides given wave data into pieces by specified frequency(hz),
    // then pushes them to the audio stream
    int pos = 0;
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

  void _onPressed() async {
    setState(() => _isPlaying = true);

    // for web, calling `resume()` from user-action is needed
    audioStream.resume();

    for (double freq in [261.626, 293.665, 329.628]) {
      await _playNote(freq, const Duration(seconds: 1));
    }

    setState(() => _isPlaying = false);
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
                onPressed: _isPlaying ? null : _onPressed,
                child: const Text(
                  'generate sine wave',
                ))
          ],
        ),
      ),
    );
  }
}
