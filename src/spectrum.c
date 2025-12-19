#include "spectrum.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

int spectrum_init(spectrum_ctx_t *ctx) {
    memset(ctx, 0, sizeof(*ctx));

    ctx->input = fftw_malloc(sizeof(double) * FFT_SIZE);
    ctx->output = fftw_malloc(sizeof(fftw_complex) * (FFT_SIZE / 2 + 1));
    ctx->magnitudes = malloc(sizeof(double) * SPECTRUM_BINS);
    ctx->smoothed = malloc(sizeof(double) * SPECTRUM_BINS);

    if (!ctx->input || !ctx->output || !ctx->magnitudes || !ctx->smoothed) {
        spectrum_shutdown(ctx);
        return -1;
    }

    ctx->plan = fftw_plan_dft_r2c_1d(FFT_SIZE, ctx->input, ctx->output, FFTW_MEASURE);
    if (!ctx->plan) {
        spectrum_shutdown(ctx);
        return -1;
    }

    memset(ctx->smoothed, 0, sizeof(double) * SPECTRUM_BINS);
    ctx->smoothing = 0.8;

    return 0;
}

void spectrum_shutdown(spectrum_ctx_t *ctx) {
    if (ctx->plan) {
        fftw_destroy_plan(ctx->plan);
    }
    if (ctx->input) {
        fftw_free(ctx->input);
    }
    if (ctx->output) {
        fftw_free(ctx->output);
    }
    free(ctx->magnitudes);
    free(ctx->smoothed);
    memset(ctx, 0, sizeof(*ctx));
}

void spectrum_process(spectrum_ctx_t *ctx, const float *samples, size_t count) {
    size_t copy_count = count < FFT_SIZE ? count : FFT_SIZE;
    size_t offset = FFT_SIZE - copy_count;

    // Zero-pad if needed
    for (size_t i = 0; i < offset; i++) {
        ctx->input[i] = 0.0;
    }

    // Apply Hann window
    for (size_t i = 0; i < copy_count; i++) {
        double window = 0.5 * (1.0 - cos(2.0 * M_PI * i / (copy_count - 1)));
        ctx->input[offset + i] = samples[i] * window;
    }

    fftw_execute(ctx->plan);

    // Calculate magnitudes (dB scale)
    for (size_t i = 0; i < SPECTRUM_BINS; i++) {
        double re = ctx->output[i][0];
        double im = ctx->output[i][1];
        double mag = sqrt(re * re + im * im) / FFT_SIZE;

        // Convert to dB, clamp to reasonable range
        double db = 20.0 * log10(mag + 1e-10);
        db = (db + 80.0) / 80.0;  // Normalize -80dB..0dB to 0..1
        if (db < 0.0) db = 0.0;
        if (db > 1.0) db = 1.0;

        ctx->magnitudes[i] = db;

        // Exponential smoothing
        ctx->smoothed[i] = ctx->smoothing * ctx->smoothed[i] +
                          (1.0 - ctx->smoothing) * db;
    }
}

void spectrum_set_smoothing(spectrum_ctx_t *ctx, double smoothing) {
    if (smoothing < 0.0) smoothing = 0.0;
    if (smoothing > 0.99) smoothing = 0.99;
    ctx->smoothing = smoothing;
}
