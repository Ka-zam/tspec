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

    float samples[FFT_SIZE];
    int smoothing_percent = 80;

    while (running && audio.running) {
        audio_get_samples(&audio, samples, FFT_SIZE);
        spectrum_process(&spectrum, samples, FFT_SIZE);
        spectrum_set_smoothing(&spectrum, smoothing_percent / 100.0);

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
