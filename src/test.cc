#define EXPORT extern "C" __attribute__((visibility("default"))) __attribute__((used))

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>

EXPORT
int ma_stream_init();

EXPORT
void ma_stream_uninit();

EXPORT
void ma_stream_push(float*, int);

int main() {
    ma_stream_init();

    int bufLength = 44100;

    float *buf = (float *)calloc(bufLength, sizeof(float));

    for (int freq=440; freq<441; freq+=5) {
        for (int j=0; j<bufLength; j++) {
            buf[j] = (float)sin(2*M_PI * ((j*freq)%44100)/44100);
        }
        ma_stream_push(buf, bufLength);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    free(buf);

    puts("push any key to exit:");
    getchar();

    ma_stream_uninit();
}
