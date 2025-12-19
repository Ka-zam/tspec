#ifndef DISPLAY_H
#define DISPLAY_H

#include <ncurses.h>
#include <stdbool.h>
#include <stddef.h>

constexpr int BAR_LEVELS = 8;
constexpr int NUM_COLORMAPS = 4;
constexpr double PEAK_FALL_SPEED = 0.005;

typedef enum {
    COLORMAP_FIRE,      // green -> yellow -> red
    COLORMAP_ICE,       // blue -> cyan -> white
    COLORMAP_RAINBOW,   // full spectrum
    COLORMAP_MONO       // single color (green)
} colormap_t;

typedef struct {
    WINDOW *win;
    int width;
    int height;
    int num_bars;
    double *bar_values;
    double *peak_values;
    bool use_color;
    bool use_truecolor;
    bool show_info;
    colormap_t colormap;
    double gain;
} display_ctx_t;

int display_init(display_ctx_t *ctx);
void display_shutdown(display_ctx_t *ctx);
void display_update(display_ctx_t *ctx, const double *spectrum, size_t spectrum_size);
void display_resize(display_ctx_t *ctx);
bool display_handle_input(display_ctx_t *ctx, int *smoothing_percent);

#endif
