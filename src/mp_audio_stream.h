#ifdef WIN32
    #define EXPORT extern "C" __declspec(dllexport)
    #define EXPORT extern
#elif __cplusplus
    #define EXPORT extern "C" __attribute__((visibility("default"))) __attribute__((used))
    #include <cstdio>
    #include <cstdlib>
    #include <cstring>
#else //OBJC
    #define EXPORT extern
    #include "stdio.h"
    #include "stdlib.h"
    #include "string.h"
#endif


EXPORT
int ma_stream_init(int max_buffer_size, int keep_buffer_size, int channels, int sample_rate);

EXPORT
void ma_stream_uninit();

EXPORT
int ma_stream_push(float*, int);
