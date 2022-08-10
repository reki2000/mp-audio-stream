
A Flutter plug-in for multi platform simple audio stream playback with real-time generated audio data streams

## Features

Currently only float32-mono format on Android, Linux, Windows and Web platforms.

- Plays buffered audio data in 44.1kHz continuously until the buffer becomes empty.
- Playing on Web platform is based on AudioContext and AudioWorkletProcessor.
- Playing on other platforms are based on [miniaudio](https://github.com/mackron/miniaudio.git), an amazing multi-platform audio library.

## Getting started

```
flutter pub add mp_audio_stream
```

## Usage

```dart
import 'dart:math' as math;
import 'dart:async';
import 'dart:typed_data';

import 'package:mp_audio_stream/mp_audio_stream.dart';

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
```
