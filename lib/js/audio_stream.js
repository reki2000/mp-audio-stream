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
        const out = outputs[0][0];
  
        const playableSize = this.buffer.length;
  
        if (this.isExhaust && this.keepBufferSize > playableSize) {
            this.exhaustCount++;
            return true;
        }
  
        this.isExhaust = false;
        var copyLength = 0;

        if (this.buffer.length < out.length) {
          copyLength = this.buffer.length;
          this.exhaustCount++;
          this.isExhaust = true;
        } else {
          copyLength = out.length;
        }

        for (let i=0; i<copyLength; i++) {
            out[i] = this.buffer[i];
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
        audioCtx = new AudioContext({sampleRate:sampleRate});
  
        const proc = Processor;
        const f = `data:text/javascript,${encodeURI(proc.toString())}; registerProcessor("${proc.name}",${proc.name});`;
        await audioCtx.audioWorklet.addModule(f);
  
        workletNode = new AudioWorkletNode(audioCtx, 'Processor');
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
