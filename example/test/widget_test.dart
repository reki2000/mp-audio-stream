import 'dart:typed_data';

import 'package:audio_stream_example/main.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mp_audio_stream/mp_audio_stream.dart';

class FakeAudioStream implements AudioStream {
  final fadeSettings = <bool>[];
  int releaseCount = 0;

  @override
  int init(
      {int bufferMilliSec = 3000,
      int waitingBufferMilliSec = 100,
      int channels = 1,
      int sampleRate = 44100,
      bool fadeOnExhaust = false}) {
    fadeSettings.add(fadeOnExhaust);
    return 0;
  }

  @override
  void uninit() => releaseCount++;

  @override
  void resume() {}

  @override
  int push(Float32List buf) => 0;

  @override
  AudioStreamStat stat() => AudioStreamStat.empty();

  @override
  void resetStat() {}
}

void main() {
  testWidgets('initializes the stream, applies fade settings and releases it',
      (tester) async {
    final audio = FakeAudioStream();
    await tester.pumpWidget(MyApp(audioStream: audio));

    expect(find.text('Audio Stream Demo'), findsOneWidget);
    expect(find.text('generate sine wave'), findsOneWidget);
    expect(audio.fadeSettings, [false]);

    await tester.tap(find.byType(Switch));
    await tester.pump();
    expect(audio.fadeSettings, [false, true]);

    await tester.pumpWidget(const SizedBox.shrink());
    expect(audio.releaseCount, 1);
  });
}
