#ifdef WIN32
    #define EXPORT extern "C" __declspec(dllexport)
#else
    #define EXPORT extern "C" __attribute__((visibility("default"))) __attribute__((used))
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>

EXPORT
int ma_stream_init(int max_buffer_size, int keep_buffer_size, int channels, int sample_rate);

EXPORT
void ma_stream_uninit();

EXPORT
int ma_stream_push(float*, int);
