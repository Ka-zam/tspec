#ifndef DISPLAY_H
#define DISPLAY_H

#include <ncurses.h>
#include <stdbool.h>
#include <stddef.h>

constexpr int BAR_LEVELS = 8;
constexpr int NUM_COLORMAPS = 4;
constexpr int WATERFALL_HISTORY = 256;

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
    int *peak_hold_frames;      // frames remaining before peak starts falling
    double *waterfall;          // 2D array [height][num_bars]
    int waterfall_pos;
    bool use_color;
    bool use_truecolor;
    bool show_info;
    bool show_stats;
    bool waterfall_mode;
    colormap_t colormap;
    double gain;
    double peak_hold_time;      // seconds before peak starts falling
    double max_sample;          // max absolute sample value (for stats)
    double rms_left;            // RMS level left channel
    double rms_right;           // RMS level right channel
    double peak_history[180];   // 3 sec @ 60fps for rolling peak
    double rms_history_l[15];   // 250ms @ 60fps for RMS window (left)
    double rms_history_r[15];   // 250ms @ 60fps for RMS window (right)
    int stats_frame;            // frame counter for stats update
    int sample_rate;            // audio sample rate for frequency calculation
    bool stereo;                // stereo input available
} display_ctx_t;

int display_init(display_ctx_t *ctx);
void display_shutdown(display_ctx_t *ctx);
void display_update_stats(display_ctx_t *ctx, const float *samples_l, const float *samples_r, size_t count);
void display_update(display_ctx_t *ctx, const double *spectrum, size_t spectrum_size);
void display_resize(display_ctx_t *ctx);
bool display_handle_input(display_ctx_t *ctx, int *smoothing_percent);

#endif
