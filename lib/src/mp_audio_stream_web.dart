import 'dart:js' as js;
import 'dart:html' as html;
import 'dart:typed_data';

import '../mp_audio_stream.dart';

String _jsText() => r"""
(async () => {
  class AudioWorkletProcessor{}

  class Processor extends AudioWorkletProcessor {
    constructor() {
      super();
      this.maxBufferSize = 1024 * 128;
      this.keepBufferSize = 1024 * 2;
      this.buffer = [];
      this.port.onmessage = (event) => {
        if (this.buffer.length < this.maxBufferSize) {
          this.buffer.push(...event.data);
        }
      };
    }

    process(_, outputs, __) {
      const out = outputs[0][0];

      if (out.length + this.keepBufferSize < this.buffer.length) {
        for (let i=0; i<out.length; i++) {
          out[i] = this.buffer[i];
        }
        this.buffer = this.buffer.slice(out.length);
      }

      return true;
    }
  }

  var audioCtx;
  var workletNode;

  window.AudioStream = {
    init: async (bufSize, waitingBufSize, channels, sampleRate) => {
      audioCtx = new AudioContext({sampleRate:sampleRate});

      const proc = Processor;
      const f = `data:text/javascript,${encodeURI(proc.toString())}; registerProcessor("${proc.name}",${proc.name});`;
      await audioCtx.audioWorklet.addModule(f);

      workletNode = new AudioWorkletNode(audioCtx, 'Processor');
      workletNode.connect(audioCtx.destination);
    },
    resume: async () => {
      await audioCtx.resume();
    },
    push: async (data) => {
      workletNode.port.postMessage(data);
    },
    uninit: async () => {}
  };
})();
""";

/// Contol class for AudioStream on web platform. Use `getAudioStream()` to get its instance.
class AudioStreamImpl extends AudioStream {
  AudioStreamImpl() {
    final scriptTag = html.ScriptElement()..text = _jsText();
    html.document.head?.children.add(scriptTag);
  }

  @override
  void resume() {
    js.context['AudioStream']?.callMethod('resume', []);
  }

  @override
  int push(Float32List buf) {
    // TODO: returns the result for calling `push`
    js.context['AudioStream']?.callMethod('push', [buf]);
    return 0;
  }

  @override
  int init(
      {int bufferMilliSec = 3000,
      int waitingBufferMilliSec = 100,
      int channels = 1,
      int sampleRate = 44100}) {
    js.context['AudioStream']?.callMethod('init', [
      bufferMilliSec * sampleRate / 1000,
      waitingBufferMilliSec * sampleRate / 1000,
      channels,
      sampleRate
    ]);
    return 0;
  }

  @override
  void uninit() {
    js.context['AudioStream']?.callMethod('uninit', []);
  }
}
