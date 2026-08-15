/// A multi-platform audio stream output library for real-time generated wave data
import 'dart:typed_data';

import 'src/mp_audio_stream_mastream.dart'
    if (dart.library.js_interop) 'src/mp_audio_stream_web.dart';

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

  /// Resumes or recovers the audio stream.
  /// On web, call this from a user action to activate `AudioContext`. On native
  /// platforms, call it after application resume or an output-device change.
  void resume();

  /// Pushes wave data (float32, -1.0 to 1.0) into audio stream. When buffer is full, the input is ignored.
  int push(Float32List buf);

  /// Returns statistics about buffer full/exhaust between the last reset and now
  AudioStreamStat stat();

  /// Resets all statistics as zero
  void resetStat();
}

/// Returns an `AudioStream` instance for running platform (web/others)
AudioStream getAudioStream() => AudioStreamImpl();

/// Statistics about buffer full/exhaust
class AudioStreamStat {
  final int full;
  final int exhaust;
  AudioStreamStat({required this.full, required this.exhaust});

  factory AudioStreamStat.empty() => AudioStreamStat(full: 0, exhaust: 0);
}
