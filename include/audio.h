#ifndef AUDIO_H
#define AUDIO_H

#include <jack/jack.h>
#include <stdbool.h>
#include <stddef.h>

constexpr size_t AUDIO_BUFFER_SIZE = 4096;

typedef struct {
    jack_client_t *client;
    jack_port_t *input_port;
    float buffer[AUDIO_BUFFER_SIZE];
    size_t write_pos;
    bool running;
} audio_ctx_t;

int audio_init(audio_ctx_t *ctx, const char *client_name);
void audio_shutdown(audio_ctx_t *ctx);
size_t audio_get_samples(audio_ctx_t *ctx, float *dest, size_t count);
jack_nframes_t audio_get_sample_rate(audio_ctx_t *ctx);

#endif
