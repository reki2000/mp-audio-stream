import 'dart:async';
import 'dart:js' as js;
import 'dart:html' as html;
import 'dart:typed_data';

import 'package:flutter/services.dart';

import '../mp_audio_stream.dart';

Future<String> _jsText() async =>
    await rootBundle.loadString('packages/mp_audio_stream/js/audio_stream.js');

/// Contol class for AudioStream on web platform. Use `getAudioStream()` to get its instance.
class AudioStreamImpl extends AudioStream {
  final _stream = Completer<js.JsObject>();

  AudioStreamImpl() {
    (() async {
      final scriptTag = html.ScriptElement()..text = await _jsText();
      html.document.head?.children.add(scriptTag);
      _stream.complete(js.context['AudioStream']);
    })();
  }

  void call(String method, List args) {
    _stream.future.then((s) => s.callMethod(method, args));
  }

  @override
  void resume() {
    call('resume', []);
  }

  @override
  int push(Float32List buf) {
    // TODO: returns the result for calling `push`
    call('push', [buf]);
    return 0;
  }

  @override
  int init(
      {int bufferMilliSec = 3000,
      int waitingBufferMilliSec = 100,
      int channels = 1,
      int sampleRate = 44100}) {
    call('init', [
      bufferMilliSec * sampleRate / 1000,
      waitingBufferMilliSec * sampleRate / 1000,
      channels,
      sampleRate
    ]);
    return 0;
  }

  @override
  void uninit() {
    call('uninit', []);
  }

  @override
  AudioStreamStat stat() {
    return AudioStreamStat.empty();
  }

  @override
  void resetStat() {}
}
