
A Flutter plugin for multi-platform, simple audio stream playback with real-time generated audio data streams

## Features

- Continuously plays buffered audio data at 44.1kHz until the buffer is empty
- Supported format: float32 single-channel
- Suppots all flutter platforms: Android, iOS, macOS, Linux, Windows, and Web platforms
  - Web platform implementaion relies on WebAudio and `AudioWorkletProcessor`
  - Other platforms utilize [miniaudio](https://github.com/mackron/miniaudio.git), an outstandig multi-platform audio library

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

For more API Documents, visit [pub.dev](https://pub.dev/packages/mp_audio_stream).
