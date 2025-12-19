#define _XOPEN_SOURCE_EXTENDED
#include "display.h"
#include <locale.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

// UTF-8 block characters for smooth vertical bars
static const wchar_t BAR_CHARS[] = {L' ', L'▁', L'▂', L'▃', L'▄', L'▅', L'▆', L'▇', L'█'};

static const char *COLORMAP_NAMES[] = {"fire", "ice", "rainbow", "mono"};

// Color pairs: 1-8 for height gradient, 9 for status, 10 for peak
enum {
    PAIR_STATUS = 9,
    PAIR_PEAK = 10
};

static void init_colormap(colormap_t map) {
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
            init_pair(PAIR_PEAK, COLOR_WHITE, -1);
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
            init_pair(PAIR_PEAK, COLOR_MAGENTA, -1);
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
            init_pair(PAIR_PEAK, COLOR_WHITE, -1);
            break;
        case COLORMAP_MONO:
            for (int i = 1; i <= 8; i++) {
                init_pair(i, COLOR_GREEN, -1);
            }
            init_pair(PAIR_PEAK, COLOR_WHITE, -1);
            break;
    }
    init_pair(PAIR_STATUS, COLOR_CYAN, -1);
}

int display_init(display_ctx_t *ctx) {
    setlocale(LC_ALL, "");

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
        init_colormap(ctx->colormap);
    }

    getmaxyx(ctx->win, ctx->height, ctx->width);
    ctx->num_bars = ctx->width;
    ctx->bar_values = calloc(ctx->num_bars, sizeof(double));
    ctx->peak_values = calloc(ctx->num_bars, sizeof(double));

    return (ctx->bar_values && ctx->peak_values) ? 0 : -1;
}

void display_shutdown(display_ctx_t *ctx) {
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

        ctx->bar_values[bar] = spectrum[bin];

        // Update peak: rise instantly, fall slowly
        if (spectrum[bin] >= ctx->peak_values[bar]) {
            ctx->peak_values[bar] = spectrum[bin];
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

        for (int y = 0; y < bar_height; y++) {
            int row = bar_height - 1 - y;
            double cell_value = full_height - (y * BAR_LEVELS);

            int char_idx = 0;
            if (cell_value >= BAR_LEVELS) {
                char_idx = BAR_LEVELS;
            } else if (cell_value > 0) {
                char_idx = (int)cell_value;
            }

            // Color based on height (row position)
            int color_pair = 1 + (y * 8 / bar_height);
            if (color_pair > 8) color_pair = 8;

            // Check if this is the peak marker row
            bool is_peak = (row == peak_row && ctx->peak_values[x] > 0.01);

            if (is_peak && char_idx == 0) {
                // Draw peak marker
                if (ctx->use_color) attron(COLOR_PAIR(PAIR_PEAK) | A_BOLD);
                mvaddch(row, x, '_');
                if (ctx->use_color) attroff(COLOR_PAIR(PAIR_PEAK) | A_BOLD);
            } else {
                if (ctx->use_color && char_idx > 0) {
                    attron(COLOR_PAIR(color_pair));
                }
                wchar_t wstr[2] = {BAR_CHARS[char_idx], L'\0'};
                mvaddwstr(row, x, wstr);
                if (ctx->use_color && char_idx > 0) {
                    attroff(COLOR_PAIR(color_pair));
                }
            }
        }
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

        case 'c':
        case 'C':
            ctx->colormap = (ctx->colormap + 1) % NUM_COLORMAPS;
            if (ctx->use_color) {
                init_colormap(ctx->colormap);
            }
            break;

        case KEY_RESIZE:
            display_resize(ctx);
            break;
    }

    // Draw status line
    if (ctx->use_color) {
        attron(COLOR_PAIR(PAIR_STATUS));
    }
    mvprintw(ctx->height - 1, 0, " tspec | [%s] | Smooth: %2d%% | c:color +/-:smooth q:quit ",
             COLORMAP_NAMES[ctx->colormap], *smoothing_percent);
    if (ctx->use_color) {
        attroff(COLOR_PAIR(PAIR_STATUS));
    }
    clrtoeol();

    return true;
}
