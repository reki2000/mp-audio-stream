
A simple multi-platform audio stream player for raw audio data streams.

Helps playing real-time synthesized audio data.

## Features

- Plays buffered audio data in 44.1kHz continuously until the buffer becomes empty.

## Getting started

```
flutter pub add audio_stream
```

## Usage

```dart
import 'dart:math' as math;
import 'dart:async';
import 'dart:typed_data';

import 'package:audio_stream/audio_stream.dart';

void main() async {
  final audioStream = getAudioStream();

  // attach the audio device and start playing
  audioStream.init();

  // generate 440Hz sine wave and push it into stream
  const freq = 440;
  const rate = 44100;
  final sineWave = List.generate(
      rate * 1, (i) => math.sin(2 * math.pi * ((i * freq) % rate) / rate));
  audioStream.push(Float32List.fromList(sineWave));

  await Future.delayed(const Duration(seconds: 2));

  // stop playing and detatch the audio device
  audioStream.uninit();
}
```

## Additional information

Except web platform, this plugin is a thin wrapper of [miniaudio](http://github.com//miniaudio), a great great multi-platform audio library. 
