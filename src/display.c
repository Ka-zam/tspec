#define _XOPEN_SOURCE_EXTENDED
#include "display.h"
#include <locale.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

// UTF-8 block characters for smooth vertical bars
static const wchar_t BAR_CHARS[] = {L' ', L'▁', L'▂', L'▃', L'▄', L'▅', L'▆', L'▇', L'█'};
static const char *BAR_CHARS_UTF8[] = {" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};

// Peak marker characters for 8 sub-cell positions (top to bottom)
static const char *PEAK_CHARS[] = {"🭶", "🭷", "🭸", "🭹", "🭺", "🭻", "▁", "▁"};
constexpr int PEAK_POSITIONS = 8;

// Color pairs for 8-color fallback
enum {
    PAIR_STATUS = 9,
    PAIR_PEAK = 10
};

typedef struct { unsigned char r, g, b; } rgb_t;

static rgb_t lerp_rgb(rgb_t a, rgb_t b, double t) {
    return (rgb_t){
        .r = (unsigned char)(a.r + t * (b.r - a.r)),
        .g = (unsigned char)(a.g + t * (b.g - a.g)),
        .b = (unsigned char)(a.b + t * (b.b - a.b))
    };
}

static rgb_t hsv_to_rgb(double h, double s, double v) {
    double c = v * s;
    double x = c * (1 - fabs(fmod(h / 60.0, 2) - 1));
    double m = v - c;
    double r, g, b;

    if (h < 60)       { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else              { r = c; g = 0; b = x; }

    return (rgb_t){
        .r = (unsigned char)((r + m) * 255),
        .g = (unsigned char)((g + m) * 255),
        .b = (unsigned char)((b + m) * 255)
    };
}

static rgb_t get_gradient_color(colormap_t map, double t) {
    // t is 0.0 (bottom) to 1.0 (top)
    if (t < 0) t = 0;
    if (t > 1) t = 1;

    switch (map) {
        case COLORMAP_FIRE: {
            // green -> yellow -> orange -> red
            rgb_t green  = {0, 200, 0};
            rgb_t yellow = {255, 255, 0};
            rgb_t orange = {255, 128, 0};
            rgb_t red    = {255, 0, 0};
            if (t < 0.33) {
                return lerp_rgb(green, yellow, t / 0.33);
            } else if (t < 0.66) {
                return lerp_rgb(yellow, orange, (t - 0.33) / 0.33);
            } else {
                return lerp_rgb(orange, red, (t - 0.66) / 0.34);
            }
        }
        case COLORMAP_ICE: {
            // dark blue -> cyan -> white
            rgb_t blue  = {0, 50, 180};
            rgb_t cyan  = {0, 220, 255};
            rgb_t white = {255, 255, 255};
            if (t < 0.5) {
                return lerp_rgb(blue, cyan, t / 0.5);
            } else {
                return lerp_rgb(cyan, white, (t - 0.5) / 0.5);
            }
        }
        case COLORMAP_RAINBOW:
            // Full HSV sweep: red -> yellow -> green -> cyan -> blue -> magenta
            return hsv_to_rgb((1.0 - t) * 270, 1.0, 1.0);

        case COLORMAP_MONO:
        default: {
            // Green with varying brightness
            unsigned char v = (unsigned char)(80 + t * 175);
            return (rgb_t){0, v, 0};
        }
    }
}

static bool detect_truecolor(void) {
    const char *colorterm = getenv("COLORTERM");
    if (colorterm && (strcmp(colorterm, "truecolor") == 0 ||
                      strcmp(colorterm, "24bit") == 0)) {
        return true;
    }
    const char *term = getenv("TERM");
    if (term && strstr(term, "truecolor")) {
        return true;
    }
    return false;
}

static void init_colormap_8color(colormap_t map) {
    switch (map) {
        case COLORMAP_FIRE:
            init_pair(1, COLOR_GREEN, -1);
            init_pair(2, COLOR_GREEN, -1);
            init_pair(3, COLOR_YELLOW, -1);
            init_pair(4, COLOR_YELLOW, -1);
            init_pair(5, COLOR_RED, -1);
            init_pair(6, COLOR_RED, -1);
            init_pair(7, COLOR_RED, -1);
            init_pair(8, COLOR_RED, -1);
            break;
        case COLORMAP_ICE:
            init_pair(1, COLOR_BLUE, -1);
            init_pair(2, COLOR_BLUE, -1);
            init_pair(3, COLOR_CYAN, -1);
            init_pair(4, COLOR_CYAN, -1);
            init_pair(5, COLOR_CYAN, -1);
            init_pair(6, COLOR_WHITE, -1);
            init_pair(7, COLOR_WHITE, -1);
            init_pair(8, COLOR_WHITE, -1);
            break;
        case COLORMAP_RAINBOW:
            init_pair(1, COLOR_BLUE, -1);
            init_pair(2, COLOR_CYAN, -1);
            init_pair(3, COLOR_GREEN, -1);
            init_pair(4, COLOR_YELLOW, -1);
            init_pair(5, COLOR_RED, -1);
            init_pair(6, COLOR_MAGENTA, -1);
            init_pair(7, COLOR_WHITE, -1);
            init_pair(8, COLOR_WHITE, -1);
            break;
        case COLORMAP_MONO:
            for (int i = 1; i <= 8; i++) {
                init_pair(i, COLOR_GREEN, -1);
            }
            break;
    }
    init_pair(PAIR_STATUS, COLOR_CYAN, -1);
    init_pair(PAIR_PEAK, COLOR_WHITE, -1);
}

int display_init(display_ctx_t *ctx) {
    setlocale(LC_ALL, "");

    ctx->use_truecolor = detect_truecolor();

    set_escdelay(25);  // Fast ESC response (default is 1000ms)
    ctx->win = initscr();
    if (!ctx->win) {
        return -1;
    }

    cbreak();
    noecho();
    curs_set(0);
    nodelay(ctx->win, TRUE);
    keypad(ctx->win, TRUE);

    ctx->use_color = has_colors();
    if (ctx->use_color) {
        start_color();
        use_default_colors();
        ctx->colormap = COLORMAP_FIRE;
        init_colormap_8color(ctx->colormap);
    }

    getmaxyx(ctx->win, ctx->height, ctx->width);
    ctx->num_bars = ctx->width;
    ctx->bar_values = calloc(ctx->num_bars, sizeof(double));
    ctx->peak_values = calloc(ctx->num_bars, sizeof(double));
    ctx->peak_hold_frames = calloc(ctx->num_bars, sizeof(int));
    ctx->waterfall = calloc((size_t)WATERFALL_HISTORY * ctx->num_bars, sizeof(double));
    ctx->waterfall_pos = 0;
    ctx->gain = 1.5;
    ctx->show_info = false;
    ctx->show_stats = false;
    ctx->waterfall_mode = false;
    ctx->peak_hold_time = 0.5;  // 0.5 second default
    ctx->max_sample = 0;
    ctx->rms_left = 0;
    ctx->rms_right = 0;
    ctx->stats_frame = 0;
    ctx->stereo = false;
    memset(ctx->peak_history, 0, sizeof(ctx->peak_history));
    memset(ctx->rms_history_l, 0, sizeof(ctx->rms_history_l));
    memset(ctx->rms_history_r, 0, sizeof(ctx->rms_history_r));
    ctx->sample_rate = 48000;  // default, updated from audio

    // Set dark grey background for truecolor
    if (ctx->use_truecolor) {
        printf("\033[48;2;30;30;30m\033[2J\033[H");
        fflush(stdout);
    }

    return (ctx->bar_values && ctx->peak_values) ? 0 : -1;
}

void display_shutdown(display_ctx_t *ctx) {
    if (ctx->use_truecolor) {
        printf("\033[0m\033[2J\033[H");
        fflush(stdout);
    }
    free(ctx->bar_values);
    free(ctx->peak_values);
    free(ctx->peak_hold_frames);
    free(ctx->waterfall);
    endwin();
    memset(ctx, 0, sizeof(*ctx));
}

void display_resize(display_ctx_t *ctx) {
    endwin();
    refresh();
    getmaxyx(ctx->win, ctx->height, ctx->width);

    free(ctx->bar_values);
    free(ctx->peak_values);
    free(ctx->peak_hold_frames);
    free(ctx->waterfall);
    ctx->num_bars = ctx->width;
    ctx->bar_values = calloc(ctx->num_bars, sizeof(double));
    ctx->peak_values = calloc(ctx->num_bars, sizeof(double));
    ctx->peak_hold_frames = calloc(ctx->num_bars, sizeof(int));
    ctx->waterfall = calloc((size_t)WATERFALL_HISTORY * ctx->num_bars, sizeof(double));
    ctx->waterfall_pos = 0;
    clear();
}

void display_update_stats(display_ctx_t *ctx, const float *samples_l, const float *samples_r, size_t count) {
    // Calculate this frame's peak and RMS for both channels
    double frame_peak = 0;
    double sum_sq_l = 0;
    double sum_sq_r = 0;

    for (size_t i = 0; i < count; i++) {
        double abs_l = fabs(samples_l[i]);
        double abs_r = fabs(samples_r[i]);
        double abs_max = abs_l > abs_r ? abs_l : abs_r;
        if (abs_max > frame_peak) frame_peak = abs_max;
        sum_sq_l += samples_l[i] * samples_l[i];
        sum_sq_r += samples_r[i] * samples_r[i];
    }
    double frame_rms_l = sqrt(sum_sq_l / count);
    double frame_rms_r = sqrt(sum_sq_r / count);

    // Store in rolling buffers
    int peak_idx = ctx->stats_frame % 180;  // 3 sec window
    int rms_idx = ctx->stats_frame % 15;    // 250ms window
    ctx->peak_history[peak_idx] = frame_peak;
    ctx->rms_history_l[rms_idx] = frame_rms_l * frame_rms_l;
    ctx->rms_history_r[rms_idx] = frame_rms_r * frame_rms_r;

    // Update displayed values at 4Hz (every 15 frames at 60fps)
    if (ctx->stats_frame % 15 == 0) {
        // Peak: max over 3 second window
        double peak = 0;
        for (int i = 0; i < 180; i++) {
            if (ctx->peak_history[i] > peak) peak = ctx->peak_history[i];
        }
        ctx->max_sample = peak;

        // RMS: average over 250ms window for each channel
        double rms_sum_l = 0;
        double rms_sum_r = 0;
        for (int i = 0; i < 15; i++) {
            rms_sum_l += ctx->rms_history_l[i];
            rms_sum_r += ctx->rms_history_r[i];
        }
        ctx->rms_left = sqrt(rms_sum_l / 15);
        ctx->rms_right = sqrt(rms_sum_r / 15);
    }

    ctx->stats_frame++;
}

void display_update(display_ctx_t *ctx, const double *spectrum, size_t spectrum_size) {
    if (!ctx->bar_values || !ctx->peak_values) return;

    int stats_rows = ctx->show_stats ? 1 : 0;
    int bar_height = ctx->height - stats_rows;

    // Map spectrum bins to display bars using octave-based log scale
    // Each octave (frequency doubling) takes equal visual space
    constexpr double MIN_FREQ = 20.0;    // 20 Hz low end
    double max_freq = ctx->sample_rate > 0 ? ctx->sample_rate / 2.0 : 24000.0;
    double freq_ratio = max_freq / MIN_FREQ;
    double bin_width = ctx->sample_rate > 0 ? (double)ctx->sample_rate / (spectrum_size * 2) : 11.7;

    for (int bar = 0; bar < ctx->num_bars; bar++) {
        double t = (double)bar / (ctx->num_bars - 1);  // 0 to 1
        double freq = MIN_FREQ * pow(freq_ratio, t);   // exponential: octave spacing
        size_t bin = (size_t)(freq / bin_width);
        if (bin >= spectrum_size) bin = spectrum_size - 1;
        if (bin < 1) bin = 1;  // skip DC

        double scaled = spectrum[bin] * ctx->gain;
        if (scaled > 1.0) scaled = 1.0;
        ctx->bar_values[bar] = scaled;

        // Update peak with hold time
        if (scaled >= ctx->peak_values[bar]) {
            ctx->peak_values[bar] = scaled;
            ctx->peak_hold_frames[bar] = (int)(ctx->peak_hold_time * 60);  // hold for N frames
        } else if (ctx->peak_hold_frames[bar] > 0) {
            ctx->peak_hold_frames[bar]--;  // holding at peak
        } else {
            ctx->peak_values[bar] -= 0.02;  // falling
            if (ctx->peak_values[bar] < 0) ctx->peak_values[bar] = 0;
        }
    }

    // Store in waterfall history
    if (ctx->waterfall) {
        for (int bar = 0; bar < ctx->num_bars; bar++) {
            ctx->waterfall[ctx->waterfall_pos * ctx->num_bars + bar] = ctx->bar_values[bar];
        }
        ctx->waterfall_pos = (ctx->waterfall_pos + 1) % WATERFALL_HISTORY;
    }

    if (ctx->waterfall_mode && ctx->use_truecolor) {
        // Waterfall mode: draw history scrolling down
        for (int y = 0; y < bar_height && y < WATERFALL_HISTORY; y++) {
            int hist_idx = (ctx->waterfall_pos - 1 - y + WATERFALL_HISTORY) % WATERFALL_HISTORY;
            if (hist_idx < 0) hist_idx += WATERFALL_HISTORY;
            for (int x = 0; x < ctx->num_bars && x < ctx->width; x++) {
                double val = ctx->waterfall[hist_idx * ctx->num_bars + x];
                rgb_t c = get_gradient_color(ctx->colormap, val);
                printf("\033[%d;%dH\033[38;2;%d;%d;%d;48;2;30;30;30m█", y + 1 + stats_rows, x + 1, c.r, c.g, c.b);
            }
        }
    } else {
        // Normal spectrum mode
        for (int x = 0; x < ctx->num_bars && x < ctx->width; x++) {
            double value = ctx->bar_values[x];
            double full_height = value * bar_height * BAR_LEVELS;
            double peak_pos = ctx->peak_values[x] * bar_height;
            int peak_row = bar_height - 1 - (int)peak_pos;
            double peak_frac = peak_pos - (int)peak_pos;
            int peak_char_idx = (int)((1.0 - peak_frac) * PEAK_POSITIONS);
            if (peak_char_idx >= PEAK_POSITIONS) peak_char_idx = PEAK_POSITIONS - 1;

            for (int y = 0; y < bar_height; y++) {
                int row = bar_height - 1 - y;
                double cell_value = full_height - (y * BAR_LEVELS);

                int char_idx = 0;
                if (cell_value >= BAR_LEVELS) {
                    char_idx = BAR_LEVELS;
                } else if (cell_value > 0) {
                    char_idx = (int)cell_value;
                }

                double height_ratio = (double)y / bar_height;
                bool is_peak = (row == peak_row && ctx->peak_values[x] > 0.01);

                if (is_peak && char_idx == 0) {
                    if (ctx->use_truecolor) {
                        printf("\033[%d;%dH\033[38;2;180;0;0;48;2;30;30;30m%s",
                               row + 1 + stats_rows, x + 1, PEAK_CHARS[peak_char_idx]);
                    } else if (ctx->use_color) {
                        move(row + stats_rows, x);
                        attron(COLOR_PAIR(PAIR_PEAK) | A_BOLD);
                        addch('_');
                        attroff(COLOR_PAIR(PAIR_PEAK) | A_BOLD);
                    } else {
                        move(row + stats_rows, x);
                        addch('_');
                    }
                } else if (char_idx > 0) {
                    if (ctx->use_truecolor) {
                        rgb_t c = get_gradient_color(ctx->colormap, height_ratio);
                        printf("\033[%d;%dH\033[38;2;%d;%d;%d;48;2;30;30;30m%s",
                               row + 1 + stats_rows, x + 1, c.r, c.g, c.b, BAR_CHARS_UTF8[char_idx]);
                    } else if (ctx->use_color) {
                        move(row + stats_rows, x);
                        int color_pair = 1 + (int)(height_ratio * 7);
                        if (color_pair > 8) color_pair = 8;
                        attron(COLOR_PAIR(color_pair));
                        wchar_t wstr[2] = {BAR_CHARS[char_idx], L'\0'};
                        addwstr(wstr);
                        attroff(COLOR_PAIR(color_pair));
                    } else {
                        move(row + stats_rows, x);
                        wchar_t wstr[2] = {BAR_CHARS[char_idx], L'\0'};
                        addwstr(wstr);
                    }
                } else {
                    if (ctx->use_truecolor) {
                        printf("\033[%d;%dH\033[48;2;30;30;30m ", row + 1 + stats_rows, x + 1);
                    } else {
                        move(row + stats_rows, x);
                        addch(' ');
                    }
                }
            }
        }
    }

    if (ctx->use_truecolor) {
        fflush(stdout);
    }
    refresh();
}

bool display_handle_input(display_ctx_t *ctx, int *smoothing_percent) {
    int ch = getch();

    switch (ch) {
        case 'q':
        case 'Q':
        case 27:  // ESC
            return false;

        case 'i':
        case 'I':
            ctx->show_info = !ctx->show_info;
            break;

        case 'z':
        case 'Z':
            ctx->show_stats = !ctx->show_stats;
            break;

        case 'w':
        case 'W':
            ctx->waterfall_mode = !ctx->waterfall_mode;
            break;

        case 'r':
        case 'R':
            if (*smoothing_percent < 99) {
                (*smoothing_percent) += 5;
            }
            break;

        case 'f':
        case 'F':
            if (*smoothing_percent > 0) {
                (*smoothing_percent) -= 5;
            }
            break;

        case 's':
        case 'S':
            if (ctx->gain > 0.5) ctx->gain -= 0.25;
            break;

        case 'a':
        case 'A':
            if (ctx->gain < 8.0) ctx->gain += 0.25;
            break;

        case 'e':
        case 'E':
            ctx->peak_hold_time += 0.1;
            if (ctx->peak_hold_time > 5.0) ctx->peak_hold_time = 5.0;
            break;

        case 'd':
        case 'D':
            ctx->peak_hold_time -= 0.1;
            if (ctx->peak_hold_time < 0) ctx->peak_hold_time = 0;
            break;

        case 'c':
        case 'C':
            ctx->colormap = (ctx->colormap + 1) % NUM_COLORMAPS;
            if (ctx->use_color && !ctx->use_truecolor) {
                init_colormap_8color(ctx->colormap);
            }
            break;

        case KEY_RESIZE:
            display_resize(ctx);
            break;
    }

    // Draw stats bar (top row)
    if (ctx->show_stats) {
        double db_peak = 20.0 * log10(ctx->max_sample + 1e-10);
        int s16_peak = (int)(ctx->max_sample * 32767);
        double rms_avg = (ctx->rms_left + ctx->rms_right) / 2.0;
        double db_rms = 20.0 * log10(rms_avg + 1e-10);
        // L/R balance: positive = right louder, negative = left louder
        double balance_db = 20.0 * log10((ctx->rms_right + 1e-10) / (ctx->rms_left + 1e-10));

        if (ctx->use_truecolor) {
            printf("\033[1;1H\033[38;2;255;255;255;48;2;30;30;30m");
            printf(" s16 Peak: %5d %.4f %5.1fdBFS | RMS: %.4f %5.1fdBFS ",
                   s16_peak, ctx->max_sample, db_peak, rms_avg, db_rms);
            if (ctx->stereo) {
                printf("L/R: %+4.1fdB ", balance_db);
            } else {
                printf("(mono) ");
            }
            // Pad to width
            int len = ctx->stereo ? 78 : 72;
            for (int i = len; i < ctx->width; i++) putchar(' ');
            printf("\033[0m");
            fflush(stdout);
        } else {
            attron(A_BOLD);
            mvprintw(0, 1, "s16 Peak: %5d %.3f %5.1fdBFS  RMS: %.3f %5.1fdBFS",
                     s16_peak, ctx->max_sample, db_peak, rms_avg, db_rms);
            attroff(A_BOLD);
        }
    }

    // Draw info window (top right corner)
    if (ctx->show_info) {
        int info_w = 28;
        int info_h = 12;
        int info_x = ctx->width - info_w - 1;
        int info_y = 0;

        if (ctx->use_truecolor) {
            // Draw box with dark background
            for (int y = 0; y < info_h; y++) {
                printf("\033[%d;%dH\033[38;2;200;200;200;48;2;20;20;20m", info_y + y + 1, info_x + 1);
                for (int x = 0; x < info_w; x++) {
                    if (y == 0 || y == info_h - 1) {
                        putchar('-');
                    } else if (x == 0 || x == info_w - 1) {
                        putchar('|');
                    } else {
                        putchar(' ');
                    }
                }
            }
            // Content
            printf("\033[%d;%dH  w      waterfall", info_y + 2, info_x + 1);
            printf("\033[%d;%dH  c      colormap", info_y + 3, info_x + 1);
            printf("\033[%d;%dH  a/s    gain %.1fx", info_y + 4, info_x + 1, ctx->gain);
            printf("\033[%d;%dH  r/f    smooth %d%%", info_y + 5, info_x + 1, *smoothing_percent);
            printf("\033[%d;%dH  e/d    hold %.1fs", info_y + 6, info_x + 1, ctx->peak_hold_time);
            printf("\033[%d;%dH  z      stats", info_y + 7, info_x + 1);
            printf("\033[%d;%dH  i      info", info_y + 8, info_x + 1);
            printf("\033[%d;%dH  q/ESC  quit", info_y + 9, info_x + 1);
            printf("\033[0m");
            fflush(stdout);
        } else {
            // ncurses fallback
            for (int y = 0; y < info_h; y++) {
                mvhline(info_y + y, info_x, ' ', info_w);
            }
            mvprintw(info_y + 2, info_x + 2, "w      waterfall");
            mvprintw(info_y + 3, info_x + 2, "c      colormap");
            mvprintw(info_y + 4, info_x + 2, "a/s    gain");
            mvprintw(info_y + 5, info_x + 2, "r/f    smooth");
            mvprintw(info_y + 6, info_x + 2, "e/d    hold");
            mvprintw(info_y + 7, info_x + 2, "z      stats");
            mvprintw(info_y + 8, info_x + 2, "i      info");
            mvprintw(info_y + 9, info_x + 2, "q/ESC  quit");
        }
    }

    return true;
}
