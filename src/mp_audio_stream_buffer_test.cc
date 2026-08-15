#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include "./miniaudio/miniaudio.h"

#include <cassert>
#include <thread>
#include <vector>

#include "mp_audio_stream_buffer.h"

void test_wraparound() {
    mp_audio_stream_buffer buffer = {};
    assert(mp_audio_stream_buffer_init(&buffer, 4, 2) == MA_SUCCESS);

    const float first[] = {0, 1, 2, 3, 4, 5};
    assert(mp_audio_stream_buffer_write(&buffer, first, 3) == MA_SUCCESS);

    float output[8] = {};
    assert(mp_audio_stream_buffer_read(&buffer, output, 2) == 2);
    assert(output[0] == 0 && output[1] == 1);
    assert(output[2] == 2 && output[3] == 3);

    const float second[] = {6, 7, 8, 9, 10, 11};
    assert(mp_audio_stream_buffer_write(&buffer, second, 3) == MA_SUCCESS);
    assert(mp_audio_stream_buffer_available_read(&buffer) == 4);
    assert(mp_audio_stream_buffer_read(&buffer, output, 4) == 4);

    for (int i = 0; i < 8; i++) {
        assert(output[i] == i + 4);
    }
    mp_audio_stream_buffer_uninit(&buffer);
}

void test_overflow_is_atomic() {
    mp_audio_stream_buffer buffer = {};
    assert(mp_audio_stream_buffer_init(&buffer, 4, 1) == MA_SUCCESS);

    const float first[] = {0, 1, 2};
    const float overflow[] = {3, 4};
    assert(mp_audio_stream_buffer_write(&buffer, first, 3) == MA_SUCCESS);
    assert(mp_audio_stream_buffer_write(&buffer, overflow, 2) == MA_NO_SPACE);
    assert(mp_audio_stream_buffer_available_read(&buffer) == 3);

    float output[3] = {};
    assert(mp_audio_stream_buffer_read(&buffer, output, 3) == 3);
    for (int i = 0; i < 3; i++) {
        assert(output[i] == i);
    }
    mp_audio_stream_buffer_uninit(&buffer);
}

void test_concurrent_access() {
    const ma_uint32 frame_count = 200000;
    mp_audio_stream_buffer buffer = {};
    assert(mp_audio_stream_buffer_init(&buffer, 257, 1) == MA_SUCCESS);

    std::vector<float> output(frame_count);
    std::thread producer([&]() {
        ma_uint32 frame = 0;
        while (frame < frame_count) {
            float sample = static_cast<float>(frame);
            if (mp_audio_stream_buffer_write(&buffer, &sample, 1) == MA_SUCCESS) {
                frame++;
            } else {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&]() {
        ma_uint32 frame = 0;
        while (frame < frame_count) {
            if (mp_audio_stream_buffer_read(&buffer, &output[frame], 1) == 1) {
                frame++;
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    for (ma_uint32 frame = 0; frame < frame_count; frame++) {
        assert(output[frame] == static_cast<float>(frame));
    }
    mp_audio_stream_buffer_uninit(&buffer);
}

int main() {
    test_wraparound();
    test_overflow_is_atomic();
    test_concurrent_access();
    return 0;
}
