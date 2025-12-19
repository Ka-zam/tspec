#include "audio.h"
#include "spectrum.h"
#include "display.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static volatile sig_atomic_t running = 1;

static void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

int main(void) {
    audio_ctx_t audio = {0};
    spectrum_ctx_t spectrum = {0};
    display_ctx_t display = {0};
    int ret = EXIT_FAILURE;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (audio_init(&audio, "tspec") != 0) {
        fprintf(stderr, "Failed to initialize audio\n");
        fprintf(stderr, "Make sure PipeWire is running\n");
        goto cleanup;
    }

    if (spectrum_init(&spectrum) != 0) {
        fprintf(stderr, "Failed to initialize spectrum analyzer\n");
        goto cleanup;
    }

    if (display_init(&display) != 0) {
        fprintf(stderr, "Failed to initialize display\n");
        goto cleanup;
    }
    display.sample_rate = audio_get_sample_rate(&audio);
    display.stereo = audio.stereo;

    float samples_l[FFT_SIZE];
    float samples_r[FFT_SIZE];
    float samples_mono[FFT_SIZE];
    int smoothing_percent = 80;

    while (running && audio.running) {
        audio_get_samples(&audio, samples_l, samples_r, FFT_SIZE);

        // Mix to mono for spectrum analysis
        for (size_t i = 0; i < FFT_SIZE; i++) {
            samples_mono[i] = (samples_l[i] + samples_r[i]) * 0.5f;
        }

        spectrum_process(&spectrum, samples_mono, FFT_SIZE);
        spectrum_set_smoothing(&spectrum, smoothing_percent / 100.0);

        display_update_stats(&display, samples_l, samples_r, FFT_SIZE);
        display_update(&display, spectrum.smoothed, SPECTRUM_BINS);

        if (!display_handle_input(&display, &smoothing_percent)) {
            break;
        }

        usleep(16667);  // ~60 FPS
    }

    ret = EXIT_SUCCESS;

cleanup:
    display_shutdown(&display);
    spectrum_shutdown(&spectrum);
    audio_shutdown(&audio);

    return ret;
}
