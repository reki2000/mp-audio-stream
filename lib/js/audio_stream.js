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
        
        this.maxBufferSize = 1024 * 128;
        this.keepBufferSize = 1024 * 4;
        this.buffer = [];
        this.exhaustCount = 0;
        this.fullCount = 0;
        this.isExhaust = false;

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

      process(_, outputs, __) {
        const out = outputs[0];
        const channels = outputs[0].length
  
        const playableSize = this.buffer.length;
  
        if (this.isExhaust && this.keepBufferSize > playableSize) {
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

        for (let channel=0; channel<channels; channel++) {
          var dest = 0;
          for (let source=channel; source<copyLength; source+=channels) {
            out[channel][dest++] = this.buffer[source];
          }
        }
        this.buffer = this.buffer.slice(copyLength);

        this.postStatistics();

        return true;
      }
    }
  
    var audioCtx;
    var workletNode;
  
    window.AudioStream = {
      init: async (bufSize, waitingBufSize, channels, sampleRate) => {
        this.maxBufferSize = bufSize;
        this.keepBufferSize = waitingBufSize;

        audioCtx = new AudioContext({sampleRate:sampleRate});
  
        const proc = Processor;
        const f = `data:text/javascript,${encodeURI(proc.toString())}; registerProcessor("${proc.name}",${proc.name});`;
        await audioCtx.audioWorklet.addModule(f);
  
        workletNode = new AudioWorkletNode(audioCtx, 'Processor', {outputChannelCount : [channels]});
        workletNode.port.onmessage = (event) => { window.AudioStream.stat = event.data; };
        workletNode.connect(audioCtx.destination);
      },
  
      resume: async () => {
        await audioCtx.resume();
      },
  
      push: async (data) => {
        workletNode.port.postMessage({"type":"data", "data":data});
      },
  
      uninit: async () => {},

      stat: {"exhaustCount":0, "fullCount":0},

      resetStat: () => {
        workletNode.port.postMessage({"type":"resetStat"});
      },
  
    };
  })();
