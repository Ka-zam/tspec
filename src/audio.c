#include "audio.h"
#include <string.h>
#include <stdio.h>

static int process_callback(jack_nframes_t nframes, void *arg) {
    audio_ctx_t *ctx = (audio_ctx_t *)arg;

    jack_default_audio_sample_t *in = jack_port_get_buffer(ctx->input_port, nframes);

    for (jack_nframes_t i = 0; i < nframes; i++) {
        ctx->buffer[ctx->write_pos] = in[i];
        ctx->write_pos = (ctx->write_pos + 1) % AUDIO_BUFFER_SIZE;
    }

    return 0;
}

static void shutdown_callback(void *arg) {
    audio_ctx_t *ctx = (audio_ctx_t *)arg;
    ctx->running = false;
}

int audio_init(audio_ctx_t *ctx, const char *client_name) {
    memset(ctx, 0, sizeof(*ctx));

    jack_status_t status;
    ctx->client = jack_client_open(client_name, JackNoStartServer, &status);
    if (!ctx->client) {
        fprintf(stderr, "Failed to connect to JACK server\n");
        return -1;
    }

    jack_set_process_callback(ctx->client, process_callback, ctx);
    jack_on_shutdown(ctx->client, shutdown_callback, ctx);

    ctx->input_port = jack_port_register(ctx->client, "input",
                                          JACK_DEFAULT_AUDIO_TYPE,
                                          JackPortIsInput, 0);
    if (!ctx->input_port) {
        fprintf(stderr, "Failed to register JACK port\n");
        jack_client_close(ctx->client);
        return -1;
    }

    if (jack_activate(ctx->client)) {
        fprintf(stderr, "Failed to activate JACK client\n");
        jack_client_close(ctx->client);
        return -1;
    }

    // Auto-connect to first available monitor port
    const char **ports = jack_get_ports(ctx->client, "monitor", JACK_DEFAULT_AUDIO_TYPE,
                                         JackPortIsOutput);
    if (ports && ports[0]) {
        jack_connect(ctx->client, ports[0], jack_port_name(ctx->input_port));
        jack_free(ports);
    } else {
        // Fallback: try any output port
        if (ports) jack_free(ports);
        ports = jack_get_ports(ctx->client, NULL, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput);
        if (ports && ports[0]) {
            jack_connect(ctx->client, ports[0], jack_port_name(ctx->input_port));
            jack_free(ports);
        }
    }

    ctx->running = true;
    return 0;
}

void audio_shutdown(audio_ctx_t *ctx) {
    if (ctx->client) {
        jack_client_close(ctx->client);
        ctx->client = NULL;
    }
    ctx->running = false;
}

size_t audio_get_samples(audio_ctx_t *ctx, float *dest, size_t count) {
    if (count > AUDIO_BUFFER_SIZE) {
        count = AUDIO_BUFFER_SIZE;
    }

    size_t start = (ctx->write_pos + AUDIO_BUFFER_SIZE - count) % AUDIO_BUFFER_SIZE;

    for (size_t i = 0; i < count; i++) {
        dest[i] = ctx->buffer[(start + i) % AUDIO_BUFFER_SIZE];
    }

    return count;
}

jack_nframes_t audio_get_sample_rate(audio_ctx_t *ctx) {
    return jack_get_sample_rate(ctx->client);
}
