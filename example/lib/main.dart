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

  /// whether to apply the DC-cut filter to NES-like (0..15) square waves
  bool _dcCut = true;

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

  /// Synthesizes a NES-like 4-bit square wave: each sample is 0..15 (0 = silence)
  static List<int> _synthNesSquareWave(
      double freq, int sampleRate, Duration duration,
      {int volume = 15}) {
    final length = duration.inMilliseconds * sampleRate ~/ 1000;
    return List.generate(
        length, (i) => (i * freq * 2 ~/ sampleRate).isEven ? volume : 0);
  }

  /// Converts 0..15 DAC values to float samples.
  /// Maps to 0.0..1.0 (silence = 0.0), then optionally removes DC offset
  /// with a one-pole high-pass filter: y = x - x_prev + R * y_prev
  static Float32List _dacToFloat(List<int> dac, bool dcCut) {
    // cutoff ~35Hz; R depends on the sample rate
    final r = math.exp(-2 * math.pi * 35 / sampleRate);
    double xPrev = 0, yPrev = 0;

    final out = Float32List(dac.length);
    for (int i = 0; i < dac.length; i++) {
      final x = dac[i] / 15.0;
      if (dcCut) {
        final y = x - xPrev + r * yPrev;
        xPrev = x;
        yPrev = y;
        out[i] = y;
      } else {
        out[i] = x;
      }
    }
    return out;
  }

  Future<void> _play(Float32List wave) async {
    setState(() => _isPlaying = true);

    // for web, calling `resume()` from user-action is needed
    audioStream.resume();

    const pushFreq = 60; // Hz

    // push wave data to audio stream in specified interval (pushFreq)
    const step = sampleRate ~/ pushFreq;
    for (int pos = 0; pos < wave.length; pos += step) {
      audioStream.push(wave.sublist(pos, math.min(wave.length, pos + step)));

      setState(() => stat = audioStream.stat());
      await Future.delayed(const Duration(seconds: 1) ~/ pushFreq);
    }

    setState(() => _isPlaying = false);
  }

  void _onPressed() {
    const noteDuration = Duration(seconds: 1);

    final wave = <double>[
      for (double noteFreq in [261.626, 293.665, 329.628])
        ..._synthSineWave(noteFreq, sampleRate, noteDuration)
    ];
    _play(Float32List.fromList(wave));
  }

  void _onPressedNes() {
    const noteDuration = Duration(milliseconds: 300);
    final rest = List.filled(200 * sampleRate ~/ 1000, 0); // 200ms silence

    // notes separated by silence, to make DC-offset pops audible
    final dac = <int>[
      ...rest,
      for (double noteFreq in [261.626, 293.665, 329.628, 349.228]) ...[
        ..._synthNesSquareWave(noteFreq, sampleRate, noteDuration),
        ...rest,
      ]
    ];
    _play(_dacToFloat(dac, _dcCut));
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
                )),
            const SizedBox(height: 32),
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
                onPressed: _isPlaying ? null : _onPressedNes,
                child: const Text(
                  'generate NES-like square wave (0..15)',
                ))
          ],
        ),
      ),
    );
  }
}
