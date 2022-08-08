import 'dart:js_util';
import 'dart:typed_data';

import 'package:js/js.dart';

import '../audio_stream.dart';

@JS('resumeAudioContext')
external Object resumeAudioContext();

@JS('pushWaveData')
external Object pushWaveData(Float32List buf);

class AudioStreamImpl extends AudioStream {
  @override
  Future<int> init() async {
    await promiseToFuture(resumeAudioContext());
    return 0;
  }

  @override
  Future<void> uninit() async {}

  @override
  Future<void> push(Float32List input) async {
    pushWaveData(input);
  }
}
