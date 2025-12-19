#define _XOPEN_SOURCE_EXTENDED
#include "display.h"
#include <locale.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

// UTF-8 block characters for smooth vertical bars
static const wchar_t BAR_CHARS[] = {L' ', L'▁', L'▂', L'▃', L'▄', L'▅', L'▆', L'▇', L'█'};

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
        init_pair(1, COLOR_GREEN, -1);
        init_pair(2, COLOR_YELLOW, -1);
        init_pair(3, COLOR_RED, -1);
        init_pair(4, COLOR_CYAN, -1);
    }

    getmaxyx(ctx->win, ctx->height, ctx->width);
    ctx->num_bars = ctx->width;
    ctx->bar_values = calloc(ctx->num_bars, sizeof(double));

    return ctx->bar_values ? 0 : -1;
}

void display_shutdown(display_ctx_t *ctx) {
    free(ctx->bar_values);
    endwin();
    memset(ctx, 0, sizeof(*ctx));
}

void display_resize(display_ctx_t *ctx) {
    endwin();
    refresh();
    getmaxyx(ctx->win, ctx->height, ctx->width);

    free(ctx->bar_values);
    ctx->num_bars = ctx->width;
    ctx->bar_values = calloc(ctx->num_bars, sizeof(double));
    clear();
}

void display_update(display_ctx_t *ctx, const double *spectrum, size_t spectrum_size) {
    if (!ctx->bar_values) return;

    // Map spectrum bins to display bars (logarithmic frequency scale)
    for (int bar = 0; bar < ctx->num_bars; bar++) {
        double freq_ratio = (double)bar / ctx->num_bars;
        // Logarithmic mapping: more resolution at lower frequencies
        double log_pos = pow(freq_ratio, 2.0) * spectrum_size;
        size_t bin = (size_t)log_pos;
        if (bin >= spectrum_size) bin = spectrum_size - 1;

        ctx->bar_values[bar] = spectrum[bin];
    }

    // Reserve space for status line
    int bar_height = ctx->height - 1;

    // Draw bars
    for (int x = 0; x < ctx->num_bars && x < ctx->width; x++) {
        double value = ctx->bar_values[x];
        double full_height = value * bar_height * BAR_LEVELS;

        int color_pair = 1;  // Green for low
        if (value > 0.6) color_pair = 3;       // Red for high
        else if (value > 0.3) color_pair = 2;  // Yellow for medium

        for (int y = 0; y < bar_height; y++) {
            int row = bar_height - 1 - y;
            double cell_value = full_height - (y * BAR_LEVELS);

            int char_idx = 0;
            if (cell_value >= BAR_LEVELS) {
                char_idx = BAR_LEVELS;
            } else if (cell_value > 0) {
                char_idx = (int)cell_value;
            }

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

        case KEY_RESIZE:
            display_resize(ctx);
            break;
    }

    // Draw status line
    if (ctx->use_color) {
        attron(COLOR_PAIR(4));
    }
    mvprintw(ctx->height - 1, 0, " tspec | Smoothing: %2d%% | +/- adjust | q quit ",
             *smoothing_percent);
    if (ctx->use_color) {
        attroff(COLOR_PAIR(4));
    }
    clrtoeol();

    return true;
}
