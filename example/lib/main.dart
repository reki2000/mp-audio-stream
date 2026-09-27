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
  static const sampleRate = 11025;

  late final AudioStream audioStream;

  AudioStreamStat stat = AudioStreamStat.empty();

  bool _isPlaying = false;

  /// whether to remove DC offset from the wave before pushing
  bool _dcCut = false;

  // DC-cut filter state, kept across pushes
  double _dcXPrev = 0, _dcYPrev = 0;

  @override
  void initState() {
    super.initState();
    audioStream = getAudioStream();
    audioStream.init(
        sampleRate: sampleRate,
        channels: 1,
        bufferMilliSec: 1000,
        waitingBufferMilliSec: 100);
  }

  @override
  void dispose() {
    audioStream.uninit();
    super.dispose();
  }

  static Float32List _synthSineWave(
      double freq, int sampleRate, Duration duration) {
    final length = duration.inMilliseconds * sampleRate ~/ 1000;
    final sineWave = List.generate(length,
        (i) => math.sin(2 * math.pi * ((i * freq) % sampleRate) / sampleRate));

    return Float32List.fromList(sineWave);
  }

  /// Removes DC offset in place with a one-pole high-pass filter (cutoff ~35Hz):
  /// y = x - x_prev + R * y_prev
  void _applyDcCut(Float32List wave) {
    final r = math.exp(-2 * math.pi * 35 / sampleRate);
    for (int i = 0; i < wave.length; i++) {
      final x = wave[i];
      final y = x - _dcXPrev + r * _dcYPrev;
      _dcXPrev = x;
      _dcYPrev = y;
      wave[i] = y;
    }
  }

  void _onPressed() async {
    setState(() => _isPlaying = true);
    _dcXPrev = 0;
    _dcYPrev = 0;

    // for web, calling `resume()` from user-action is needed
    audioStream.resume();

    const noteDuration = Duration(seconds: 1);
    const pushFreq = 60; // Hz

    for (double noteFreq in [261.626, 293.665, 329.628]) {
      final wave = _synthSineWave(noteFreq, sampleRate, noteDuration);
      if (_dcCut) {
        _applyDcCut(wave);
      }

      // push wave data to audio stream in specified interval (pushFreq)
      const step = sampleRate ~/ pushFreq;
      for (int pos = 0; pos < wave.length; pos += step) {
        audioStream.push(wave.sublist(pos, math.min(wave.length, pos + step)));

        setState(() => stat = audioStream.stat());
        await Future.delayed(noteDuration ~/ pushFreq);
      }
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
            Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                const Text("DC cut"),
                Switch(
                    value: _dcCut,
                    onChanged: (v) => setState(() => _dcCut = v)),
              ],
            ),
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
