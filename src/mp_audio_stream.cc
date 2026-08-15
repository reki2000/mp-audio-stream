#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include "./miniaudio/miniaudio.h"

#include "mp_audio_stream.h"
#include "mp_audio_stream_buffer.h"

#define DEVICE_FORMAT       ma_format_f32

typedef struct {
    ma_device device;
    ma_bool32 device_initialized;

    mp_audio_stream_buffer buffer;

    ma_uint32 channels;

    bool is_exhaust;
    ma_uint32 exhaust_recover_size;

    ma_atomic_uint32 exhaust_count;
    ma_atomic_uint32 full_count;
    ma_atomic_bool32 recovery_requested;

    int max_buffer_size;
    int keep_buffer_size;
    int sample_rate;

} _ctx_t;

_ctx_t * _ctx = NULL;

static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frame_count)
{
    _ctx_t* ctx = (_ctx_t*)pDevice->pUserData;
#ifdef MP_AUDIO_STREAM_DEBUG
    printf(
        "callback: frameCount:%d available:%d\n",
        frame_count,
        mp_audio_stream_buffer_available_read(&ctx->buffer));
#endif
    float * out = (float *)pOutput;

    ma_uint32 playable_frames = mp_audio_stream_buffer_available_read(&ctx->buffer);
    ma_uint32 samples = frame_count * ctx->channels;

    if (ctx->is_exhaust && ctx->exhaust_recover_size > playable_frames) {
        memset(out, 0, samples * sizeof(float));
        ma_atomic_uint32_fetch_add(&ctx->exhaust_count, 1);
        return;
    }

    ctx->is_exhaust = false;
    ma_uint32 frames_read = mp_audio_stream_buffer_read(
        &ctx->buffer,
        out,
        frame_count);
    if (frames_read < frame_count) {
        ma_uint32 samples_read = frames_read * ctx->channels;
        memset(&out[samples_read], 0, (samples - samples_read) * sizeof(float));
        ctx->is_exhaust = true;
        ma_atomic_uint32_fetch_add(&ctx->exhaust_count, 1);
    }
}

static void notification_callback(const ma_device_notification* notification)
{
    _ctx_t* ctx = (_ctx_t*)notification->pDevice->pUserData;
    if (ctx == NULL) {
        return;
    }

    switch (notification->type) {
        case ma_device_notification_type_started:
            ma_atomic_bool32_set(&ctx->recovery_requested, MA_FALSE);
            break;
        case ma_device_notification_type_stopped:
        case ma_device_notification_type_rerouted:
        case ma_device_notification_type_interruption_began:
        case ma_device_notification_type_interruption_ended:
            ma_atomic_bool32_set(&ctx->recovery_requested, MA_TRUE);
            break;
        default:
            break;
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

    if (ma_atomic_bool32_get(&_ctx->recovery_requested) &&
        ma_stream_resume() != 0) {
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
    if (_ctx->device_initialized) {
        ma_device_uninit(&_ctx->device);
        _ctx->device_initialized = MA_FALSE;
    }
    mp_audio_stream_buffer_uninit(&_ctx->buffer);
    _ctx->channels = 0;
    ma_atomic_bool32_set(&_ctx->recovery_requested, MA_FALSE);
}

int ma_stream_resume() {
    if (_ctx == NULL || _ctx->channels == 0) {
        return -1;
    }

    if (!_ctx->device_initialized) {
        return ma_stream_init(
            _ctx->max_buffer_size,
            _ctx->keep_buffer_size,
            _ctx->channels,
            _ctx->sample_rate);
    }

    ma_device_state state = ma_device_get_state(&_ctx->device);
    if (state == ma_device_state_started || state == ma_device_state_starting) {
        ma_atomic_bool32_set(&_ctx->recovery_requested, MA_FALSE);
        return 0;
    }

    if (state == ma_device_state_stopped) {
        ma_pcm_rb_reset(&_ctx->buffer.buffer);
        _ctx->is_exhaust = true;
        if (ma_device_start(&_ctx->device) == MA_SUCCESS) {
            ma_atomic_bool32_set(&_ctx->recovery_requested, MA_FALSE);
            return 0;
        }
    }

    int max_buffer_size = _ctx->max_buffer_size;
    int keep_buffer_size = _ctx->keep_buffer_size;
    int channels = _ctx->channels;
    int sample_rate = _ctx->sample_rate;
    return ma_stream_init(
        max_buffer_size,
        keep_buffer_size,
        channels,
        sample_rate);
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
        ma_atomic_bool32_set(&_ctx->recovery_requested, MA_FALSE);
    } else if (_ctx->device_initialized) {
        ma_device_uninit(&_ctx->device);
        _ctx->device_initialized = MA_FALSE;
    }

    _ctx->channels = channels;
    _ctx->exhaust_recover_size = keep_buffer_size / channels;
    _ctx->max_buffer_size = max_buffer_size;
    _ctx->keep_buffer_size = keep_buffer_size;
    _ctx->sample_rate = sample_rate;

    ma_device_config deviceConfig;
 
    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = DEVICE_FORMAT;
    deviceConfig.playback.channels = channels;
    deviceConfig.sampleRate        = sample_rate;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.notificationCallback = notification_callback;
    deviceConfig.pUserData         = _ctx;

    if (ma_device_init(NULL, &deviceConfig, &_ctx->device) != MA_SUCCESS) {
        printf("Failed to open playback device.\n");
        return -4;
    }
    _ctx->device_initialized = MA_TRUE;

#ifdef MP_AUDIO_STREAM_DEBUG
    printf("Device Name: %s\n", _ctx->device.playback.name);
#endif

    if (mp_audio_stream_buffer_init(
            &_ctx->buffer,
            max_buffer_size / channels,
            channels) != MA_SUCCESS) {
        ma_device_uninit(&_ctx->device);
        _ctx->device_initialized = MA_FALSE;
        return -7;
    }

    if (ma_device_start(&_ctx->device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&_ctx->device);
        _ctx->device_initialized = MA_FALSE;
        return -5;
    }

    ma_atomic_bool32_set(&_ctx->recovery_requested, MA_FALSE);

    return 0;
}
