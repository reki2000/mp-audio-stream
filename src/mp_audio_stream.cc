#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include "./miniaudio/miniaudio.h"

#include "mp_audio_stream.h"
#include "mp_audio_stream_buffer.h"

#define DEVICE_FORMAT       ma_format_f32

typedef struct {
    ma_device device;

    mp_audio_stream_buffer buffer;

    ma_uint32 channels;

    bool is_exhaust;
    ma_uint32 exhaust_recover_size;

    ma_atomic_uint32 exhaust_count;
    ma_atomic_uint32 full_count;

} _ctx_t;

_ctx_t * _ctx = NULL;

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frame_count)
{
#ifdef MP_AUDIO_STREAM_DEBUG
    printf(
        "callback: frameCount:%d available:%d\n",
        frame_count,
        mp_audio_stream_buffer_available_read(&_ctx->buffer));
#endif
    float * out = (float *)pOutput;

    ma_uint32 playable_frames = mp_audio_stream_buffer_available_read(&_ctx->buffer);
    ma_uint32 samples = frame_count * _ctx->channels;

    if (_ctx->is_exhaust && _ctx->exhaust_recover_size > playable_frames) {
        memset(out, 0, samples * sizeof(float));
        ma_atomic_uint32_fetch_add(&_ctx->exhaust_count, 1);
        return;
    }

    _ctx->is_exhaust = false;
    ma_uint32 frames_read = mp_audio_stream_buffer_read(
        &_ctx->buffer,
        out,
        frame_count);
    if (frames_read < frame_count) {
        ma_uint32 samples_read = frames_read * _ctx->channels;
        memset(&out[samples_read], 0, (samples - samples_read) * sizeof(float));
        _ctx->is_exhaust = true;
        ma_atomic_uint32_fetch_add(&_ctx->exhaust_count, 1);
    }
}

int ma_stream_push(float* buf, int length) {
#ifdef MP_AUDIO_STREAM_DEBUGB
    printf(
        "push: length:%d available:%d\n",
        length,
        mp_audio_stream_buffer_available_read(&_ctx->buffer));
    for (int i=0; i<100; i+=10) {
        for (int j=0; j<10; j++) {
            unsigned char *b = (unsigned char *)(&buf[i+j]);
            printf("%02x%02x%02x%02x %f ", *(b+3), *(b+2), *(b+1), *(b+0), buf[i+j]);
        }
        printf("\n");
    }
    fflush(stdout);
#endif

    if (_ctx == NULL || buf == NULL || _ctx->channels == 0 ||
        length < 0 || length % _ctx->channels != 0) {
        return -1;
    }

    ma_uint32 frame_count = length / _ctx->channels;
    if (mp_audio_stream_buffer_write(&_ctx->buffer, buf, frame_count) != MA_SUCCESS) {
        ma_atomic_uint32_fetch_add(&_ctx->full_count, 1);
        return -1;
    }

    return 0;
}

ma_uint32 ma_stream_stat_exhaust_count() {
    return _ctx == NULL ? 0 : ma_atomic_uint32_get(&_ctx->exhaust_count);
}

ma_uint32 ma_stream_stat_full_count() {
    return _ctx == NULL ? 0 : ma_atomic_uint32_get(&_ctx->full_count);
}

void ma_stream_stat_reset() {
    if (_ctx == NULL) {
        return;
    }
    ma_atomic_uint32_set(&_ctx->full_count, 0);
    ma_atomic_uint32_set(&_ctx->exhaust_count, 0);
}

void ma_stream_uninit() {
    if (_ctx == NULL) {
        return;
    }
    ma_device_uninit(&_ctx->device);
    mp_audio_stream_buffer_uninit(&_ctx->buffer);
    _ctx->channels = 0;
}

int ma_stream_init(int max_buffer_size, int keep_buffer_size, int channels, int sample_rate)
{
    if (channels <= 0 || sample_rate <= 0 || max_buffer_size <= 0 ||
        max_buffer_size % channels != 0 || keep_buffer_size < 0 ||
        keep_buffer_size % channels != 0) {
        return -6;
    }

    if (_ctx == NULL) {
        _ctx = (_ctx_t *)calloc(1,sizeof(_ctx_t));
        if (_ctx == NULL) {
            return -8;
        }

        _ctx->is_exhaust = false;
        _ctx->exhaust_recover_size = 10 * 1024;
        ma_atomic_uint32_set(&_ctx->exhaust_count, 0);
        ma_atomic_uint32_set(&_ctx->full_count, 0);
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

    _ctx->channels = channels;
    _ctx->exhaust_recover_size = keep_buffer_size / channels;

    if (mp_audio_stream_buffer_init(
            &_ctx->buffer,
            max_buffer_size / channels,
            channels) != MA_SUCCESS) {
        ma_device_uninit(&_ctx->device);
        return -7;
    }

    if (ma_device_start(&_ctx->device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&_ctx->device);
        return -5;
    }

    return 0;
}
