#ifndef MP_AUDIO_STREAM_BUFFER_H
#define MP_AUDIO_STREAM_BUFFER_H

#include "./miniaudio/miniaudio.h"

typedef struct {
    ma_pcm_rb buffer;
    ma_bool32 initialized;
    ma_uint32 channels;
} mp_audio_stream_buffer;

static ma_result mp_audio_stream_buffer_init(
    mp_audio_stream_buffer* buffer,
    ma_uint32 size_in_frames,
    ma_uint32 channels)
{
    if (buffer == NULL || size_in_frames == 0 || channels == 0) {
        return MA_INVALID_ARGS;
    }

    if (buffer->initialized) {
        ma_pcm_rb_uninit(&buffer->buffer);
    }

    ma_result result = ma_pcm_rb_init(
        ma_format_f32,
        channels,
        size_in_frames,
        NULL,
        NULL,
        &buffer->buffer);
    if (result != MA_SUCCESS) {
        buffer->initialized = MA_FALSE;
        buffer->channels = 0;
        return result;
    }

    buffer->initialized = MA_TRUE;
    buffer->channels = channels;
    return MA_SUCCESS;
}

static void mp_audio_stream_buffer_uninit(mp_audio_stream_buffer* buffer)
{
    if (buffer == NULL || !buffer->initialized) {
        return;
    }

    ma_pcm_rb_uninit(&buffer->buffer);
    buffer->initialized = MA_FALSE;
    buffer->channels = 0;
}

static ma_uint32 mp_audio_stream_buffer_available_read(mp_audio_stream_buffer* buffer)
{
    if (buffer == NULL || !buffer->initialized) {
        return 0;
    }
    return ma_pcm_rb_available_read(&buffer->buffer);
}

static ma_result mp_audio_stream_buffer_write(
    mp_audio_stream_buffer* buffer,
    const float* input,
    ma_uint32 frame_count)
{
    if (buffer == NULL || !buffer->initialized || input == NULL) {
        return MA_INVALID_ARGS;
    }
    if (ma_pcm_rb_available_write(&buffer->buffer) < frame_count) {
        return MA_NO_SPACE;
    }

    ma_uint32 frames_written = 0;
    while (frames_written < frame_count) {
        ma_uint32 frames_to_write = frame_count - frames_written;
        void* output = NULL;
        ma_result result = ma_pcm_rb_acquire_write(
            &buffer->buffer,
            &frames_to_write,
            &output);
        if (result != MA_SUCCESS || frames_to_write == 0) {
            return result != MA_SUCCESS ? result : MA_ERROR;
        }

        memcpy(
            output,
            &input[frames_written * buffer->channels],
            frames_to_write * buffer->channels * sizeof(float));

        result = ma_pcm_rb_commit_write(&buffer->buffer, frames_to_write);
        if (result != MA_SUCCESS && result != MA_AT_END) {
            return result;
        }
        frames_written += frames_to_write;
    }

    return MA_SUCCESS;
}

static ma_uint32 mp_audio_stream_buffer_read(
    mp_audio_stream_buffer* buffer,
    float* output,
    ma_uint32 frame_count)
{
    if (buffer == NULL || !buffer->initialized || output == NULL) {
        return 0;
    }

    ma_uint32 frames_to_read = ma_pcm_rb_available_read(&buffer->buffer);
    if (frames_to_read > frame_count) {
        frames_to_read = frame_count;
    }

    ma_uint32 frames_read = 0;
    while (frames_read < frames_to_read) {
        ma_uint32 contiguous_frames = frames_to_read - frames_read;
        void* input = NULL;
        ma_result result = ma_pcm_rb_acquire_read(
            &buffer->buffer,
            &contiguous_frames,
            &input);
        if (result != MA_SUCCESS || contiguous_frames == 0) {
            break;
        }

        memcpy(
            &output[frames_read * buffer->channels],
            input,
            contiguous_frames * buffer->channels * sizeof(float));

        result = ma_pcm_rb_commit_read(&buffer->buffer, contiguous_frames);
        if (result != MA_SUCCESS && result != MA_AT_END) {
            break;
        }
        frames_read += contiguous_frames;
    }

    return frames_read;
}

#endif
