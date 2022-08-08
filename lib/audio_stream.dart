library audio_stream;

import 'dart:typed_data';

import 'src/audio_stream_mastream.dart'
    if (dart.library.html) 'src/audio_stream_web.dart';

abstract class AudioStream {
  Future<int> init();
  Future<void> uninit();
  Future<void> push(Float32List buf);
}

AudioStream getAudioStream() => AudioStreamImpl();
