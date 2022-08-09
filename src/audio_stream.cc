#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include "./miniaudio/miniaudio.h"

#include "audio_stream.h"

#define DEVICE_FORMAT       ma_format_f32

_Float32 *_ma_stream_buf = NULL;
ma_uint32 _ma_stream_buf_end;
ma_uint32 _ma_stream_buf_start;

ma_uint32 _ma_stream_max_buf_size = 128 * 1024;
ma_uint32 _ma_stream_keep_buf_size = 2 * 1024;

ma_device _ma_stream_device;

bool _ma_stream_initialized = false;

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
#ifdef DEBUG
    printf("callback: frameCount:%d _start:%d _length:%d\n", frameCount, _ma_stream_buf_start, _ma_stream_buf_length);
#endif
    // ignore if not enough waiting buffer remains
    if (_ma_stream_buf_end - _ma_stream_buf_start < frameCount + _ma_stream_keep_buf_size) {
        return;
    }

    memcpy((_Float32 *)pOutput, &(_ma_stream_buf[_ma_stream_buf_start]), frameCount * sizeof(_Float32));

    _ma_stream_buf_start += frameCount;
}

EXPORT
int ma_stream_push(_Float32* buf, int length) {
#ifdef DEBUG
    printf("push: length:%d _length:%d _start:%d\n", length, _ma_stream_buf_length, _ma_stream_buf_start);
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
    if (_ma_stream_buf_end - _ma_stream_buf_start + length > _ma_stream_max_buf_size) {
        return -1;
    }

    // move the waiting buffer to the head of the buffer, if needed
    if (_ma_stream_buf_end + length > _ma_stream_max_buf_size) {
        memcpy(_ma_stream_buf, &_ma_stream_buf[_ma_stream_buf_start], (_ma_stream_buf_end - _ma_stream_buf_start)*sizeof(float));
        _ma_stream_buf_end -= _ma_stream_buf_start;
        _ma_stream_buf_start = 0;
    }

    memcpy(&_ma_stream_buf[_ma_stream_buf_end], buf, length * sizeof(float));

    _ma_stream_buf_end += length;

    return 0;
}


EXPORT
void ma_stream_uninit() {
    ma_device_uninit(&_ma_stream_device);
}

EXPORT
int ma_stream_init(int max_buffer_size, int keep_buffer_size, int channels, int sample_rate)
{
    if (_ma_stream_initialized) {
        ma_device_uninit(&_ma_stream_device);
    }

    ma_device_config deviceConfig;
 
    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = DEVICE_FORMAT;
    deviceConfig.playback.channels = channels;
    deviceConfig.sampleRate        = sample_rate;
    deviceConfig.dataCallback      = data_callback;

    if (ma_device_init(NULL, &deviceConfig, &_ma_stream_device) != MA_SUCCESS) {
        printf("Failed to open playback device.\n");
        return -4;
    }

#ifdef DEBUG
    printf("Device Name: %s\n", _ma_stream_device.playback.name);
#endif

    _ma_stream_max_buf_size = max_buffer_size;
    _ma_stream_keep_buf_size = keep_buffer_size;

    if (_ma_stream_buf != NULL) {
        free(_ma_stream_buf);
    }

    _ma_stream_buf = (_Float32 *)calloc(_ma_stream_max_buf_size, sizeof(_Float32));
    _ma_stream_buf_end = 0;
    _ma_stream_buf_start = 0;

    if (ma_device_start(&_ma_stream_device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&_ma_stream_device);
        return -5;
    }

    _ma_stream_initialized = true;

    return 0;
}
