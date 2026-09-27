#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include "./miniaudio/miniaudio.h"

#include "mp_audio_stream.h"

#define DEVICE_FORMAT       ma_format_f32

typedef struct {
    ma_device device;

    // lock-free single producer (push) / single consumer (data_callback) ring buffer
    ma_pcm_rb rb;
    bool rb_initialized;

    ma_uint32 channels;

    bool is_exhaust;
    ma_uint32 exhaust_recover_size;

    ma_uint32 exhaust_count;
    ma_uint32 full_count;

} _ctx_t;

_ctx_t * _ctx = NULL;

// reads up to `frame_count` frames from the ring buffer, returns the number of frames read
static ma_uint32 rb_read(float* out, ma_uint32 frame_count)
{
    ma_uint32 total = 0;
    while (total < frame_count) {
        ma_uint32 frames = frame_count - total;
        void* p;
        if (ma_pcm_rb_acquire_read(&_ctx->rb, &frames, &p) != MA_SUCCESS || frames == 0) {
            break;
        }
        memcpy(&out[total * _ctx->channels], p, frames * _ctx->channels * sizeof(float));
        ma_pcm_rb_commit_read(&_ctx->rb, frames);
        total += frames;
    }
    return total;
}

// writes `frame_count` frames into the ring buffer (the caller checks the available space)
static void rb_write(const float* in, ma_uint32 frame_count)
{
    ma_uint32 total = 0;
    while (total < frame_count) {
        ma_uint32 frames = frame_count - total;
        void* p;
        if (ma_pcm_rb_acquire_write(&_ctx->rb, &frames, &p) != MA_SUCCESS || frames == 0) {
            break;
        }
        memcpy(p, &in[total * _ctx->channels], frames * _ctx->channels * sizeof(float));
        ma_pcm_rb_commit_write(&_ctx->rb, frames);
        total += frames;
    }
}

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frame_count)
{
#ifdef MP_AUDIO_STREAM_DEBUG
    printf("callback: frameCount:%d available:%d\n", frame_count, ma_pcm_rb_available_read(&_ctx->rb));
#endif
    float * out = (float *)pOutput;

    ma_uint32 plyable_size = ma_pcm_rb_available_read(&_ctx->rb) * _ctx->channels;
    ma_uint32 samples = frame_count * _ctx->channels;

    if (_ctx->is_exhaust && _ctx->exhaust_recover_size > plyable_size) {
        memset(out, 0, samples * sizeof(float));
        _ctx->exhaust_count++;
        return;
    }

    _ctx->is_exhaust = false;
    ma_uint32 read_frames = rb_read(out, frame_count);
    if (read_frames < frame_count) {
        // fill the rest
        ma_uint32 read_samples = read_frames * _ctx->channels;
        memset(&out[read_samples], 0, (samples - read_samples) * sizeof(float));

        _ctx->is_exhaust = true;
        _ctx->exhaust_count++;
    }
}

int64_t ma_stream_push(float* buf, int64_t length) {
#ifdef MP_AUDIO_STREAM_DEBUGB
    printf("push: length:%lld available:%u\n", (long long)length, ma_pcm_rb_available_read(&_ctx->rb));
    for (int i=0; i<100; i+=10) {
        for (int j=0; j<10; j++) {
            unsigned char *b = (unsigned char *)(&buf[i+j]);
            printf("%02x%02x%02x%02x %f ", *(b+3), *(b+2), *(b+1), *(b+0), buf[i+j]);
        }
        printf("\n");
    }
    fflush(stdout);
#endif

    // reject partial frames, same as web
    if (length % _ctx->channels != 0) {
        return -1;
    }
    ma_uint32 frames = length / _ctx->channels;

    // ignore if no buffer remains
    if (ma_pcm_rb_available_write(&_ctx->rb) < frames) {
        _ctx->full_count++;
        return -1;
    }

    rb_write(buf, frames);

    return 0;
}

int64_t ma_stream_stat_exhaust_count() {
    return _ctx->exhaust_count;
}

int64_t ma_stream_stat_full_count() {
    return _ctx->full_count;
}

void ma_stream_stat_reset() {
    _ctx->full_count = 0;
    _ctx->exhaust_count = 0;
}

void ma_stream_uninit() {
    ma_device_uninit(&_ctx->device);
}

int64_t ma_stream_init(int64_t max_buffer_size, int64_t keep_buffer_size, int64_t channels, int64_t sample_rate)
{
    if (_ctx == NULL) {
        _ctx = (_ctx_t *)calloc(1,sizeof(_ctx_t));

        _ctx->rb_initialized = false;
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

    _ctx->exhaust_recover_size = keep_buffer_size;

    // the device is not started yet, so the callback does not touch the buffer here
    if (_ctx->rb_initialized) {
        ma_pcm_rb_uninit(&_ctx->rb);
        _ctx->rb_initialized = false;
    }

    if (ma_pcm_rb_init(DEVICE_FORMAT, channels, max_buffer_size / channels, NULL, NULL, &_ctx->rb) != MA_SUCCESS) {
        printf("Failed to allocate buffer.\n");
        ma_device_uninit(&_ctx->device);
        return -6;
    }
    _ctx->rb_initialized = true;

    _ctx->channels = channels;

    if (ma_device_start(&_ctx->device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&_ctx->device);
        return -5;
    }

    return 0;
}
