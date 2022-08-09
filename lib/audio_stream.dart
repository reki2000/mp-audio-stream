/// A multi-platform audio stream output library for real-time generated wave data
library audio_stream;

import 'dart:typed_data';

import 'src/audio_stream_mastream.dart'
    if (dart.library.html) 'src/audio_stream_web.dart';

/// Contol class for AudioStream. Use `getAudioStream()` to get its instance.
abstract class AudioStream {
  /// Initializes an audio stream and starts to play. Returns 0 then scucess.
  /// Calling more than once makes a new AudioStream, the previous device will be `uninit`ed.
  int init(
      {int bufferMilliSec = 3000,
      int waitingBufferMilliSec = 100,
      int channels = 1,
      int sampleRate = 44100});

  /// Release current audio stream.
  void uninit();

  /// Resumes audio stream.
  /// For web platform, you should call this from some user-action to activate `AudioContext`.
  /// Ignored on platforms other than web, but recommended to call this to keep multi-platform ready.
  void resume();

  /// Pushes wave data (float32, -1.0 to 1.0) into audio stream. When buffer is full, the input is ignored.
  int push(Float32List buf);
}

/// Returns an `AudioStream` instance for running platform (web/others)
AudioStream getAudioStream() => AudioStreamImpl();
