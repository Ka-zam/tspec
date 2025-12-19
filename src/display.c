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

static const char *COLORMAP_NAMES[] = {"fire", "ice", "rainbow", "mono"};

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
    ctx->gain = 1.5;
    ctx->show_info = false;

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
    endwin();
    memset(ctx, 0, sizeof(*ctx));
}

void display_resize(display_ctx_t *ctx) {
    endwin();
    refresh();
    getmaxyx(ctx->win, ctx->height, ctx->width);

    free(ctx->bar_values);
    free(ctx->peak_values);
    ctx->num_bars = ctx->width;
    ctx->bar_values = calloc(ctx->num_bars, sizeof(double));
    ctx->peak_values = calloc(ctx->num_bars, sizeof(double));
    clear();
}

void display_update(display_ctx_t *ctx, const double *spectrum, size_t spectrum_size) {
    if (!ctx->bar_values || !ctx->peak_values) return;

    int bar_height = ctx->height - 1;

    // Map spectrum bins to display bars (logarithmic frequency scale)
    for (int bar = 0; bar < ctx->num_bars; bar++) {
        double freq_ratio = (double)bar / ctx->num_bars;
        double log_pos = pow(freq_ratio, 2.0) * spectrum_size;
        size_t bin = (size_t)log_pos;
        if (bin >= spectrum_size) bin = spectrum_size - 1;

        double scaled = spectrum[bin] * ctx->gain;
        if (scaled > 1.0) scaled = 1.0;
        ctx->bar_values[bar] = scaled;

        // Update peak: rise instantly, fall slowly
        if (scaled >= ctx->peak_values[bar]) {
            ctx->peak_values[bar] = scaled;
        } else {
            ctx->peak_values[bar] -= PEAK_FALL_SPEED;
            if (ctx->peak_values[bar] < 0) ctx->peak_values[bar] = 0;
        }
    }

    // Draw bars
    for (int x = 0; x < ctx->num_bars && x < ctx->width; x++) {
        double value = ctx->bar_values[x];
        double full_height = value * bar_height * BAR_LEVELS;
        double peak_pos = ctx->peak_values[x] * bar_height;
        int peak_row = bar_height - 1 - (int)peak_pos;
        double peak_frac = peak_pos - (int)peak_pos;  // 0.0-1.0 within cell
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

            move(row, x);

            if (is_peak && char_idx == 0) {
                // Draw peak marker with sub-cell positioning
                if (ctx->use_truecolor) {
                    printf("\033[%d;%dH\033[38;2;180;0;0;48;2;30;30;30m%s",
                           row + 1, x + 1, PEAK_CHARS[peak_char_idx]);
                } else if (ctx->use_color) {
                    attron(COLOR_PAIR(PAIR_PEAK) | A_BOLD);
                    addch('_');
                    attroff(COLOR_PAIR(PAIR_PEAK) | A_BOLD);
                } else {
                    addch('_');
                }
            } else if (char_idx > 0) {
                if (ctx->use_truecolor) {
                    rgb_t c = get_gradient_color(ctx->colormap, height_ratio);
                    printf("\033[%d;%dH\033[38;2;%d;%d;%d;48;2;30;30;30m%s",
                           row + 1, x + 1, c.r, c.g, c.b, BAR_CHARS_UTF8[char_idx]);
                } else if (ctx->use_color) {
                    int color_pair = 1 + (int)(height_ratio * 7);
                    if (color_pair > 8) color_pair = 8;
                    attron(COLOR_PAIR(color_pair));
                    wchar_t wstr[2] = {BAR_CHARS[char_idx], L'\0'};
                    addwstr(wstr);
                    attroff(COLOR_PAIR(color_pair));
                } else {
                    wchar_t wstr[2] = {BAR_CHARS[char_idx], L'\0'};
                    addwstr(wstr);
                }
            } else {
                if (ctx->use_truecolor) {
                    printf("\033[%d;%dH\033[48;2;30;30;30m ", row + 1, x + 1);
                } else {
                    addch(' ');
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
            return false;

        case 27:  // ESC
            return false;

        case 'i':
        case 'I':
            ctx->show_info = !ctx->show_info;
            break;

        case KEY_UP:
        case '+':
        case '=':
            if (*smoothing_percent < 99) {
                (*smoothing_percent) += 5;
            }
            break;

        case KEY_DOWN:
        case '-':
            if (*smoothing_percent > 0) {
                (*smoothing_percent) -= 5;
            }
            break;

        case KEY_LEFT:
        case '[':
        case 's':
        case 'S':
            if (ctx->gain > 0.5) ctx->gain -= 0.25;
            break;

        case KEY_RIGHT:
        case ']':
        case 'w':
        case 'W':
            if (ctx->gain < 8.0) ctx->gain += 0.25;
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

    // Draw status line
    const char *color_mode = ctx->use_truecolor ? "24bit" : "8";
    if (ctx->use_color) {
        attron(COLOR_PAIR(PAIR_STATUS));
    }
    mvprintw(ctx->height - 1, 0, " tspec | %s [%s] | Gain: %.1fx | Smooth: %2d%% | i:info q:quit ",
             COLORMAP_NAMES[ctx->colormap], color_mode, ctx->gain, *smoothing_percent);
    if (ctx->use_color) {
        attroff(COLOR_PAIR(PAIR_STATUS));
    }
    clrtoeol();

    // Draw info window (top right corner)
    if (ctx->show_info) {
        int info_w = 30;
        int info_h = 9;
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
            printf("\033[%d;%dH  c      colormap", info_y + 2, info_x + 1);
            printf("\033[%d;%dH  +/-    smoothing", info_y + 3, info_x + 1);
            printf("\033[%d;%dH  w/s    gain", info_y + 4, info_x + 1);
            printf("\033[%d;%dH  i      info", info_y + 5, info_x + 1);
            printf("\033[%d;%dH  q/ESC  quit", info_y + 6, info_x + 1);
            printf("\033[0m");
            fflush(stdout);
        } else {
            // ncurses fallback
            for (int y = 0; y < info_h; y++) {
                mvhline(info_y + y, info_x, ' ', info_w);
            }
            mvprintw(info_y + 2, info_x + 2, "c      colormap");
            mvprintw(info_y + 3, info_x + 2, "+/-    smoothing");
            mvprintw(info_y + 4, info_x + 2, "w/s    gain");
            mvprintw(info_y + 5, info_x + 2, "i      info");
            mvprintw(info_y + 6, info_x + 2, "q/ESC  quit");
        }
    }

    return true;
}
