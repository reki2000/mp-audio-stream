#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include "./miniaudio/miniaudio.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef WIN32
    #define EXPORT __declspec(dllexport)
    #define _Float32 float
#else
    #define EXPORT extern "C" __attribute__((visibility("default"))) __attribute__((used))
#endif

#define DEVICE_FORMAT       ma_format_f32
#define DEVICE_CHANNELS     1
#define DEVICE_SAMPLE_RATE  44100

#define MA_STREAM_BUF_SIZE_MAX (128 * 1024)
#define MA_STREAM_WAIT_BUF_SIZE (2 * 1024)

_Float32 *_ma_stream_buf;
ma_uint32 _ma_stream_buf_length;
ma_uint32 _ma_stream_buf_start;

ma_device _ma_stream_device;

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    // printf("callback: frameCount:%d _start:%d _length:%d\n", frameCount, _ma_stream_buf_start, _ma_stream_buf_length);
    if (_ma_stream_buf_length - _ma_stream_buf_start < frameCount + MA_STREAM_WAIT_BUF_SIZE) {
        return;
    }

    memcpy((_Float32 *)pOutput, &(_ma_stream_buf[_ma_stream_buf_start]), frameCount * sizeof(_Float32));

    _ma_stream_buf_start += frameCount;
}

EXPORT
void ma_stream_push(_Float32* buf, int length) {
    // printf("push: length:%d _length:%d _start:%d\n", length, _ma_stream_buf_length, _ma_stream_buf_start);
    // for (int i=0; i<100; i+=10) {
    //     for (int j=0; j<10; j++) {
    //         unsigned char *b = (unsigned char *)(&buf[i+j]);
    //         printf("%02x%02x%02x%02x %f ", *(b+3), *(b+2), *(b+1), *(b+0), buf[i+j]);
    //     }
    //     printf("\n");
    // }
    // fflush(stdout);

    if (_ma_stream_buf_length > MA_STREAM_BUF_SIZE_MAX) {
        return;
    }

    memcpy(_ma_stream_buf, &(_ma_stream_buf[_ma_stream_buf_start]), (_ma_stream_buf_length - _ma_stream_buf_start)*sizeof(float));
    _ma_stream_buf_length -= _ma_stream_buf_start;
    _ma_stream_buf_start = 0;

    _ma_stream_buf = (_Float32 *)realloc(_ma_stream_buf, (_ma_stream_buf_length + length)*sizeof(_Float32));

    for (int i=0; i<length; i++) {
        _ma_stream_buf[_ma_stream_buf_length + i] = buf[i] * 2.0f - 1.0f;
    }

    _ma_stream_buf_length += length;
}


EXPORT
void ma_stream_uninit() {
    ma_device_uninit(&_ma_stream_device);
}

EXPORT
int ma_stream_init()
{
    ma_device_config deviceConfig;
 
    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = DEVICE_FORMAT;
    deviceConfig.playback.channels = DEVICE_CHANNELS;
    deviceConfig.sampleRate        = DEVICE_SAMPLE_RATE;
    deviceConfig.dataCallback      = data_callback;

    if (ma_device_init(NULL, &deviceConfig, &_ma_stream_device) != MA_SUCCESS) {
        printf("Failed to open playback device.\n");
        return -4;
    }

    printf("Device Name: %s\n", _ma_stream_device.playback.name);

    _ma_stream_buf = (_Float32 *)malloc(0);
    _ma_stream_buf_length = 0;
    _ma_stream_buf_start = 0;

    if (ma_device_start(&_ma_stream_device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&_ma_stream_device);
        return -5;
    }

    return 0;
}
