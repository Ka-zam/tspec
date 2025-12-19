#ifndef SPECTRUM_H
#define SPECTRUM_H

#include <fftw3.h>
#include <stddef.h>

constexpr size_t FFT_SIZE = 2048;
constexpr size_t SPECTRUM_BINS = FFT_SIZE / 2;

typedef struct {
    double *input;
    fftw_complex *output;
    fftw_plan plan;
    double *magnitudes;
    double *smoothed;
    double smoothing;
} spectrum_ctx_t;

int spectrum_init(spectrum_ctx_t *ctx);
void spectrum_shutdown(spectrum_ctx_t *ctx);
void spectrum_process(spectrum_ctx_t *ctx, const float *samples, size_t count);
void spectrum_set_smoothing(spectrum_ctx_t *ctx, double smoothing);

#endif
