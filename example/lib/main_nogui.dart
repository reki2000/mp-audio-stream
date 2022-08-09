import 'dart:math' as math;
import 'dart:async';
import 'dart:typed_data';

import 'package:audio_stream/audio_stream.dart';

void main() async {
  final audioStream = getAudioStream();
  audioStream.init();

  const freq = 440;
  const rate = 44100;

  final sineWave = List.generate(
      rate * 1, (i) => math.sin(2 * math.pi * ((i * freq) % rate) / rate));
  audioStream.push(Float32List.fromList(sineWave));

  await Future.delayed(const Duration(seconds: 2));

  audioStream.uninit();
}
