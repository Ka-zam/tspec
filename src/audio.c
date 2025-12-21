#include "audio.h"
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <string.h>
#include <stdio.h>

static void on_process(void *userdata) {
    audio_ctx_t *ctx = userdata;
    struct pw_buffer *b;
    struct spa_buffer *buf;

    if ((b = pw_stream_dequeue_buffer(ctx->stream)) == NULL) {
        return;
    }

    buf = b->buffer;
    float *samples = buf->datas[0].data;

    if (samples == NULL) {
        pw_stream_queue_buffer(ctx->stream, b);
        return;
    }

    // Stereo interleaved: L,R,L,R... so divide by 2 for frame count
    uint32_t n_frames = buf->datas[0].chunk->size / sizeof(float) / 2;

    for (uint32_t i = 0; i < n_frames; i++) {
        ctx->buffer_l[ctx->write_pos] = samples[i * 2];
        ctx->buffer_r[ctx->write_pos] = samples[i * 2 + 1];
        ctx->write_pos = (ctx->write_pos + 1) % AUDIO_BUFFER_SIZE;
    }

    pw_stream_queue_buffer(ctx->stream, b);
}

static void on_stream_param_changed(void *userdata, uint32_t id, const struct spa_pod *param) {
    audio_ctx_t *ctx = userdata;

    if (param == NULL || id != SPA_PARAM_Format)
        return;

    struct spa_audio_info_raw info;
    if (spa_format_audio_raw_parse(param, &info) >= 0) {
        ctx->sample_rate = info.rate;
    }
}

static void on_stream_state_changed(void *userdata, enum pw_stream_state old,
                                     enum pw_stream_state state, const char *error) {
    audio_ctx_t *ctx = userdata;
    (void)old;

    if (state == PW_STREAM_STATE_ERROR) {
        fprintf(stderr, "Stream error: %s\n", error);
        ctx->running = false;
    } else if (state == PW_STREAM_STATE_UNCONNECTED) {
        ctx->running = false;
    }
}

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .param_changed = on_stream_param_changed,
    .state_changed = on_stream_state_changed,
    .process = on_process,
};

int audio_init(audio_ctx_t *ctx, const char *client_name) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->sample_rate = 48000;  // Default, will be updated when stream connects

    pw_init(NULL, NULL);

    ctx->loop = pw_thread_loop_new(client_name, NULL);
    if (!ctx->loop) {
        fprintf(stderr, "Failed to create PipeWire thread loop\n");
        return -1;
    }

    struct pw_properties *props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Music",
        PW_KEY_STREAM_CAPTURE_SINK, "true",  // Capture from sink (monitor)
        NULL
    );

    ctx->stream = pw_stream_new_simple(
        pw_thread_loop_get_loop(ctx->loop),
        client_name,
        props,
        &stream_events,
        ctx
    );

    if (!ctx->stream) {
        fprintf(stderr, "Failed to create PipeWire stream\n");
        pw_thread_loop_destroy(ctx->loop);
        return -1;
    }

    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    const struct spa_pod *params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat,
        &SPA_AUDIO_INFO_RAW_INIT(
            .format = SPA_AUDIO_FORMAT_F32,
            .channels = 2,
            .rate = 0  // Any rate
        ));

    if (pw_stream_connect(ctx->stream,
                          PW_DIRECTION_INPUT,
                          PW_ID_ANY,
                          PW_STREAM_FLAG_AUTOCONNECT |
                          PW_STREAM_FLAG_MAP_BUFFERS |
                          PW_STREAM_FLAG_RT_PROCESS,
                          params, 1) < 0) {
        fprintf(stderr, "Failed to connect PipeWire stream\n");
        pw_stream_destroy(ctx->stream);
        pw_thread_loop_destroy(ctx->loop);
        return -1;
    }

    pw_thread_loop_start(ctx->loop);
    ctx->stereo = true;  // PipeWire handles stereo via 2-channel format
    ctx->running = true;

    return 0;
}

void audio_shutdown(audio_ctx_t *ctx) {
    if (ctx->loop) {
        pw_thread_loop_stop(ctx->loop);
    }
    if (ctx->stream) {
        pw_stream_destroy(ctx->stream);
        ctx->stream = NULL;
    }
    if (ctx->loop) {
        pw_thread_loop_destroy(ctx->loop);
        ctx->loop = NULL;
    }
    pw_deinit();
    ctx->running = false;
}

size_t audio_get_samples(audio_ctx_t *ctx, float *dest_l, float *dest_r, size_t count) {
    if (count > AUDIO_BUFFER_SIZE) {
        count = AUDIO_BUFFER_SIZE;
    }

    size_t start = (ctx->write_pos + AUDIO_BUFFER_SIZE - count) % AUDIO_BUFFER_SIZE;

    for (size_t i = 0; i < count; i++) {
        size_t idx = (start + i) % AUDIO_BUFFER_SIZE;
        dest_l[i] = ctx->buffer_l[idx];
        dest_r[i] = ctx->buffer_r[idx];
    }

    return count;
}

uint32_t audio_get_sample_rate(audio_ctx_t *ctx) {
    return ctx->sample_rate;
}
