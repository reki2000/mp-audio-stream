#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include "./miniaudio/miniaudio.h"

#include "mp_audio_stream.h"

#define DEVICE_FORMAT       ma_format_f32

// fade duration to avoid pop noise when the buffer is exhausted / recovered
#define FADE_SEC            0.005f

typedef struct {
    ma_device device;

    ma_uint32 buf_size;

    float *buf;
    ma_uint32 buf_end;
    ma_uint32 buf_start;

    ma_uint32 channels;

    bool is_exhaust;
    ma_uint32 exhaust_recover_size;

    ma_uint32 exhaust_count;
    ma_uint32 full_count;

    // for fade-out on exhaust and fade-in on recovery
    float last[MA_MAX_CHANNELS];
    float gain;
    float gain_step;
    float decay;
    bool fade;

} _ctx_t;

_ctx_t * _ctx = NULL;

// writes `frame_count` frames: the first `copy_frames` from the buffer (fading in),
// and the rest by decaying the last output value toward zero
static void write_frames(float* out, ma_uint32 frame_count, ma_uint32 copy_frames)
{
    const ma_uint32 channels = _ctx->channels;
    const float* in = &_ctx->buf[_ctx->buf_start];

    // without fade: copy as is, and fill zero when exhausted
    const bool fade = _ctx->fade;
    const float decay = fade ? _ctx->decay : 0.0f;

    for (ma_uint32 f = 0; f < frame_count; f++) {
        if (f < copy_frames) {
            const float gain = fade ? _ctx->gain : 1.0f;
            for (ma_uint32 c = 0; c < channels; c++) {
                _ctx->last[c] = *in++ * gain;
                *out++ = _ctx->last[c];
            }
            _ctx->gain += _ctx->gain_step;
            if (_ctx->gain > 1.0f) _ctx->gain = 1.0f;
        } else {
            for (ma_uint32 c = 0; c < channels; c++) {
                _ctx->last[c] *= decay;
                *out++ = _ctx->last[c];
            }
            _ctx->gain = 0.0f;
        }
    }
}

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frame_count)
{
#ifdef MP_AUDIO_STREAM_DEBUG
    printf("callback: frameCount:%d start:%d end:%d\n", frame_count, _ctx->buf_start, _ctx->buf_end);
#endif
    float * out = (float *)pOutput;

    ma_uint32 plyable_size = _ctx->buf_end - _ctx->buf_start;
    ma_uint32 plyable_frames = plyable_size / _ctx->channels;

    if (_ctx->is_exhaust && _ctx->exhaust_recover_size > plyable_size) {
        write_frames(out, frame_count, 0);
        _ctx->exhaust_count++;
        return;
    }

    _ctx->is_exhaust = false;
    if (plyable_frames < frame_count) {
        // copy the buffer to the output, and fade out the rest
        write_frames(out, frame_count, plyable_frames);

        _ctx->buf_start = _ctx->buf_end;
        _ctx->is_exhaust = true;
        _ctx->exhaust_count++;
    } else {
        write_frames(out, frame_count, frame_count);
        _ctx->buf_start += frame_count * _ctx->channels;
    }
}

int ma_stream_push(float* buf, int length) {
#ifdef MP_AUDIO_STREAM_DEBUGB
    printf("push: length:%d _length:%d _start:%d\n", length, _ctx->buf_end, _ctx->buf_start);
    for (int i=0; i<100; i+=10) {
        for (int j=0; j<10; j++) {
            unsigned char *b = (unsigned char *)(&buf[i+j]);
            printf("%02x%02x%02x%02x %f ", *(b+3), *(b+2), *(b+1), *(b+0), buf[i+j]);
        }
        printf("\n");
    }
    fflush(stdout);
#endif

    // ignore if no buffer remains
    if (_ctx->buf_end - _ctx->buf_start + length > _ctx->buf_size) {
        _ctx->full_count++;
        return -1;
    }

    // move the waiting buffer to the head of the buffer, if needed
    if (_ctx->buf_end + length > _ctx->buf_size) {
        memcpy(_ctx->buf, &_ctx->buf[_ctx->buf_start], (_ctx->buf_end - _ctx->buf_start)*sizeof(float));
        _ctx->buf_end -= _ctx->buf_start;
        _ctx->buf_start = 0;
    }

    memcpy(&_ctx->buf[_ctx->buf_end], buf, length * sizeof(float));

    _ctx->buf_end += length;

    return 0;
}

ma_uint32 ma_stream_stat_exhaust_count() {
    return _ctx->exhaust_count;
}

ma_uint32 ma_stream_stat_full_count() {
    return _ctx->full_count;
}

void ma_stream_stat_reset() {
    _ctx->full_count = 0;
    _ctx->exhaust_count = 0;
}

void ma_stream_uninit() {
    ma_device_uninit(&_ctx->device);
}

int ma_stream_init(int max_buffer_size, int keep_buffer_size, int channels, int sample_rate, int fade_on_exhaust)
{
    if (_ctx == NULL) {
        _ctx = (_ctx_t *)calloc(1,sizeof(_ctx_t));

        _ctx->buf_size = 128 * 1024;
        _ctx->buf = NULL;
        _ctx->buf_end = 0;
        _ctx->buf_start = 0;
        _ctx->is_exhaust = false;
        _ctx->exhaust_recover_size = 10 * 1024;
        _ctx->exhaust_count = 0;
        _ctx->full_count = 0;
    } else {
        ma_device_uninit(&_ctx->device);
    }

    ma_device_config deviceConfig;
 
    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = DEVICE_FORMAT;
    deviceConfig.playback.channels = channels;
    deviceConfig.sampleRate        = sample_rate;
    deviceConfig.dataCallback      = data_callback;

    if (ma_device_init(NULL, &deviceConfig, &_ctx->device) != MA_SUCCESS) {
        printf("Failed to open playback device.\n");
        return -4;
    }

#ifdef MP_AUDIO_STREAM_DEBUG
    printf("Device Name: %s\n", _ctx->device.playback.name);
#endif

    _ctx->buf_size = max_buffer_size;
    _ctx->exhaust_recover_size = keep_buffer_size;

    if (_ctx->buf != NULL) {
        free(_ctx->buf);
    }

    _ctx->buf = (float *)calloc(_ctx->buf_size, sizeof(float));
    _ctx->buf_end = 0;
    _ctx->buf_start = 0;

    _ctx->channels = channels;

    memset(_ctx->last, 0, sizeof(_ctx->last));
    _ctx->gain = 0.0f;
    _ctx->gain_step = 1.0f / (FADE_SEC * sample_rate);
    _ctx->decay = expf(-1.0f / (FADE_SEC * sample_rate));
    _ctx->fade = fade_on_exhaust != 0;

    if (ma_device_start(&_ctx->device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&_ctx->device);
        return -5;
    }

    return 0;
}
