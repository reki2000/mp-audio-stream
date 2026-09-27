(async () => {
    class AudioWorkletProcessor{}
  
    class Processor extends AudioWorkletProcessor {

      postStatistics() {
        this.port.postMessage({
          "exhaustCount": this.exhaustCount,
          "fullCount": this.fullCount
        });
      }

      constructor() {
        super();
        
        this.maxBufferSize = 0;
        this.keepBufferSize = 0;

        this.buffer = [];
        this.exhaustCount = 0;
        this.fullCount = 0;
        this.isExhaust = false;

        // for fade-out on exhaust and fade-in on recovery (5ms)
        this.last = [];
        this.gain = 0;
        this.gainStep = 1 / (0.005 * sampleRate);
        this.decay = Math.exp(-1 / (0.005 * sampleRate));

        this.port.onmessage = (event) => {
          if (event.data.type == "data") {
            if (this.buffer.length < this.maxBufferSize) {
              this.buffer.push(...event.data.data);
            } else {
              this.fullCount++;
            }

          } else if (event.data.type == "resetStat") {
            this.exhaustCount = 0;
            this.fullCount = 0;
          }

          this.postStatistics();
        }
      }

      // writes all frames: the first `copyFrames` from the buffer (fading in),
      // and the rest by decaying the last output value toward zero
      writeFrames(out, copyFrames) {
        const channels = out.length;
        const frames = out[0].length;

        for (let f=0; f<frames; f++) {
          if (f < copyFrames) {
            for (let channel=0; channel<channels; channel++) {
              this.last[channel] = this.buffer[f * channels + channel] * this.gain;
              out[channel][f] = this.last[channel];
            }
            this.gain = Math.min(1, this.gain + this.gainStep);
          } else {
            for (let channel=0; channel<channels; channel++) {
              this.last[channel] = (this.last[channel] || 0) * this.decay;
              out[channel][f] = this.last[channel];
            }
            this.gain = 0;
          }
        }
      }

      process(_, outputs, __) {
        const out = outputs[0];
        const channels = outputs[0].length

        const playableSize = this.buffer.length;

        if (this.isExhaust && this.keepBufferSize > playableSize) {
          this.writeFrames(out, 0);
          this.exhaustCount++;
          return true;
        }

        this.isExhaust = false;
        var copyLength = 0;

        if (this.buffer.length < out[0].length * channels) {
          copyLength = this.buffer.length;
          this.exhaustCount++;
          this.isExhaust = true;
        } else {
          copyLength = out[0].length * channels;
        }

        this.writeFrames(out, Math.floor(copyLength / channels));
        this.buffer = this.buffer.slice(copyLength);

        this.postStatistics();

        return true;
      }
    }
  
    var audioCtx;
    var workletNode;

    const init =  async (bufSize, waitingBufSize, channels, sampleRate) => {
      audioCtx = new AudioContext({sampleRate:sampleRate});

      const proc = Processor;
      let procCode = proc.toString();
      procCode = procCode.split("this.maxBufferSize = 0;").join(`this.maxBufferSize = ${bufSize};`); // replace
      procCode = procCode.split("this.keepBufferSize = 0;").join(`this.keepBufferSize = ${waitingBufSize}`); // replace
      const f = `data:text/javascript,${encodeURI(procCode)}; registerProcessor("${proc.name}",${proc.name});`;
      await audioCtx.audioWorklet.addModule(f);

      workletNode = new AudioWorkletNode(audioCtx, 'Processor', {outputChannelCount : [channels]});
      workletNode.port.onmessage = (event) => { window.AudioStream.stat = event.data; };
      workletNode.connect(audioCtx.destination);
        
      console.log(`mp-audio-stream initialized. sampleRate:${audioCtx.sampleRate} channels:${channels}`);
    }

    const push = async (data) => {
      const postPush = async (data) => {
        await workletNode?.port.postMessage({"type":"data", "data":data});
      }

      const stackSize = 48000
      if (data.length > stackSize) {
        await postPush(data.subarray(0,stackSize));
        await push(data.subarray(stackSize));
      } else {
        await postPush(data);
      }
    }

    window.AudioStream = {
      init: init,
  
      resume: async () => {
        if (audioCtx == null) {
          console.log("mp-audio-stream is not initialized.");
          return;
        }
        await audioCtx?.resume();
      },
  
      push: push,
  
      uninit: async () => {
        await audioCtx?.close();
        audioCtx = null;
      },

      stat: {"exhaustCount":0, "fullCount":0}, // overwritten in workletNode.port.onmessage

      resetStat: () => {
        workletNode?.port.postMessage({"type":"resetStat"});
      },
  
    };

    console.log("mp-audio-stream loaded.");
  })();
