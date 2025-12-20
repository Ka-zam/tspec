#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

constexpr size_t AUDIO_BUFFER_SIZE = 4096;

typedef struct pw_thread_loop pw_thread_loop;
typedef struct pw_stream pw_stream;

typedef struct {
    pw_thread_loop *loop;
    pw_stream *stream;
    float buffer[AUDIO_BUFFER_SIZE];
    size_t write_pos;
    uint32_t sample_rate;
    bool running;
} audio_ctx_t;

int audio_init(audio_ctx_t *ctx, const char *client_name);
void audio_shutdown(audio_ctx_t *ctx);
size_t audio_get_samples(audio_ctx_t *ctx, float *dest, size_t count);
uint32_t audio_get_sample_rate(audio_ctx_t *ctx);

#endif
