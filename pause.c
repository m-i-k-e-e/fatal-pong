#include "pause.h"
#include "bonus.h"
#include "fireball.h"
#include "rift.h"
#include "ball.h"
#include "calamity.h"
#include "text.h"
#include <stdio.h>

#define PANEL_W     1400
#ifdef __PROSPERO__
#define PANEL_H     900
#else
#define PANEL_H     980         // Room for the keyboard controls
#endif
#define PANEL_X     ((SCREEN_WIDTH - PANEL_W) / 2)
#define PANEL_Y     ((SCREEN_HEIGHT - PANEL_H) / 2)
#define TEXT_X      (PANEL_X + 70)
#define SLIDER_W    420
#define SLIDER_H    24
#define REPEAT_DELAY 20         // Frames a direction is held before it starts repeating
#define REPEAT_GAP  6

// A setting adjusted with a slider: `label` is a format string given CALAMITY_EVERY_HITS
typedef struct {
    const char *label;
    int *value;
    int min, max, step;
} Setting;

static Setting settings[] = {
    { "BALL AND PLAYER SPEED", &start_speed_percent, 50, 200, 10 },
    { "EVERY %dTH HIT, CALAMITY CHANCE", &calamity_chance, 0, 100, CALAMITY_CHANCE_STEP },
};
#define SETTING_COUNT ((int)(sizeof(settings) / sizeof(settings[0])))

static int selected;            // The setting left/right changes
static int held_x, held_y;      // -1, 0 or 1: the directions pushed last frame
static int held_x_frames, held_y_frames;

// Direction either player pushes along one axis: -1 left/up, 1 right/down, 0 neither
static int pushed(const PlayerControls c[2], bool horizontal) {
    for (int i = 0; i < 2; i++) {
        if (horizontal ? c[i].left : c[i].up) return -1;
        if (horizontal ? c[i].right : c[i].down) return 1;
    }
    return 0;
}

// Track how long `dir` has been held in *held / *frames; returns it on the press and then repeatedly while
// held, 0 otherwise
static int repeat(int dir, int *held, int *frames) {
    *frames = dir == *held ? *frames + 1 : 0;
    *held = dir;
    if (dir && (*frames == 0 || (*frames >= REPEAT_DELAY && (*frames - REPEAT_DELAY) % REPEAT_GAP == 0))) return dir;
    return 0;
}

// Up/down from either player picks a setting, left/right changes it by its step: once per press, then
// repeating while held. Changes start_speed_percent and calamity_chance.
void update_pause_menu(const PlayerControls controls[2]) {
    int dy = repeat(pushed(controls, false), &held_y, &held_y_frames);
    selected = SDL_clamp(selected + dy, 0, SETTING_COUNT - 1);
    int dx = repeat(pushed(controls, true), &held_x, &held_x_frames);
    Setting *s = &settings[selected];
    *s->value = SDL_clamp(*s->value + dx * s->step, s->min, s->max);
}

// The settings, one row each with the bars lined up: label, a bar filled from min to max, the percentage, and
// on the selected row (label in orange) the "< >" hint
static void draw_settings(SDL_Renderer *renderer, int y) {
    char labels[SETTING_COUNT][64];
    int gap = 30, label_w = 0, value_w = text_width("100%", 3), hint_w = text_width("< >", 3);
    for (int i = 0; i < SETTING_COUNT; i++) {
        snprintf(labels[i], sizeof(labels[i]), settings[i].label, CALAMITY_EVERY_HITS);
        label_w = SDL_max(label_w, text_width(labels[i], 3));
    }
    int left = (SCREEN_WIDTH - (label_w + gap + SLIDER_W + gap + value_w + gap + hint_w)) / 2;

    for (int i = 0; i < SETTING_COUNT; i++, y += 40) {
        const Setting *s = &settings[i];
        int x = left;
        if (i == selected) SDL_SetRenderDrawColor(renderer, 255, 140, 0, 255);
        else SDL_SetRenderDrawColor(renderer, 160, 160, 170, 255);
        draw_text(renderer, labels[i], x + label_w - text_width(labels[i], 3), y, 3);
        x += label_w + gap;
        SDL_Rect track = { x, y - (SLIDER_H - 21) / 2, SLIDER_W, SLIDER_H };
        SDL_SetRenderDrawColor(renderer, 160, 160, 170, 255);
        SDL_RenderFillRect(renderer, &track);
        SDL_Rect inside = { track.x + 3, track.y + 3, track.w - 6, track.h - 6 };
        SDL_SetRenderDrawColor(renderer, 40, 40, 48, 255);
        SDL_RenderFillRect(renderer, &inside);
        inside.w = inside.w * (*s->value - s->min) / (s->max - s->min);
        SDL_SetRenderDrawColor(renderer, 200, 30, 30, 255);
        SDL_RenderFillRect(renderer, &inside);
        x += SLIDER_W + gap;
        char value[8];
        snprintf(value, sizeof(value), "%d%%", *s->value);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        draw_text(renderer, value, x + value_w - text_width(value, 3), y, 3);
        x += value_w + gap;
        if (i == selected) {
            SDL_SetRenderDrawColor(renderer, 0, 220, 255, 255);
            draw_text(renderer, "< >", x, y, 3);
        }
    }
}

// Orange section heading on the panel's left margin
static void draw_heading(SDL_Renderer *renderer, const char *s, int y) {
    SDL_SetRenderDrawColor(renderer, 255, 140, 0, 255);
    draw_text(renderer, s, TEXT_X, y, 5);
}

// Drawn over the frozen game: dims the court, then a framed panel with the move list, bonus legend and the
// settings (ball start speed, calamity chance)
void draw_pause_menu(SDL_Renderer *renderer) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 170);
    SDL_RenderFillRect(renderer, NULL);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    SDL_Rect frame = { PANEL_X, PANEL_Y, PANEL_W, PANEL_H };
    SDL_SetRenderDrawColor(renderer, 255, 140, 0, 255);
    SDL_RenderFillRect(renderer, &frame);
    SDL_Rect panel = { PANEL_X + 4, PANEL_Y + 4, PANEL_W - 8, PANEL_H - 8 };
    SDL_SetRenderDrawColor(renderer, 20, 20, 26, 255);
    SDL_RenderFillRect(renderer, &panel);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    draw_text_centered(renderer, "PAUSED", SCREEN_WIDTH / 2, PANEL_Y + 40, 8);

    // Special moves: the inputs stand out, each followed by what it does
    int y = PANEL_Y + 130;
    draw_heading(renderer, "SPECIAL MOVES", y);
    SDL_SetRenderDrawColor(renderer, 160, 160, 170, 255);       // Shared pad split, see update_special_input()
    draw_text(renderer, "SHARED PAD: LEFT PLAYER L1, RIGHT SQUARE OR R1",
              TEXT_X + text_width("SPECIAL MOVES", 5) + 40, y + 8, 3);
    y += 56;
    char hadouken_line[80], rift_line[80];
    snprintf(hadouken_line, sizeof(hadouken_line), "  FREEZES THE OPPONENT %d S OR KNOCKS THE BALL BACK",
             STUN_DURATION / 60);
    snprintf(rift_line, sizeof(rift_line), "  OPPONENT STILL ON IT AFTER 0.5 S? GONE FOR %d S",
             VANISH_DURATION / 60);
    const struct { bool input; const char *text; } move_lines[] = {
        { true,  "DOWN, FORWARD, SQUARE OR R1: HADOUKEN" },
        { false, hadouken_line },
        { true,  "DOWN, BACK, SQUARE OR R1: RIFT" },
        { false, rift_line },
        { true,  "MATCH WON? TRIANGLE X3 WITHIN 5 SECONDS: FATALITY" },
    };
    for (int i = 0; i < (int)(sizeof(move_lines) / sizeof(move_lines[0])); i++) {
        if (move_lines[i].input) SDL_SetRenderDrawColor(renderer, 0, 220, 255, 255);
        else SDL_SetRenderDrawColor(renderer, 220, 220, 230, 255);
        draw_text(renderer, move_lines[i].text, TEXT_X + 20, y, 4);
        y += 44;
    }

    // Bonus legend in two columns
    y += 30;
    draw_heading(renderer, "BONUSES", y);
    char sub[64];
    snprintf(sub, sizeof(sub), "MOVE YOUR PADDLE ONTO ONE. LASTS %d SECONDS.", BONUS_DURATION / 60);
    SDL_SetRenderDrawColor(renderer, 160, 160, 170, 255);
    draw_text(renderer, sub, TEXT_X + text_width("BONUSES", 5) + 40, y + 8, 3);
    y += 60;

    const int col_w = (PANEL_W - 140) / 2;
    for (int t = 1; t < BONUS_COUNT; t++) {
        int idx = t - 1;
        int x = TEXT_X + (idx % 2) * col_w;
        int row_y = y + (idx / 2) * 68;
        draw_bonus_box(renderer, (BonusType)t, x, row_y);

        int tx = x + BONUS_SIZE + 20;
        set_bonus_color(renderer, (BonusType)t);
        draw_text(renderer, bonus_name((BonusType)t), tx, row_y + 3, 3);
        SDL_SetRenderDrawColor(renderer, 220, 220, 230, 255);
        draw_text(renderer, bonus_description((BonusType)t), tx, row_y + 33, 3);
    }

    // Footer
#ifndef __PROSPERO__
    // Keyboard controls (see input.c)
    SDL_SetRenderDrawColor(renderer, 0, 220, 255, 255);
    draw_text(renderer, "KEYBOARD", TEXT_X, PANEL_Y + PANEL_H - 205, 3);
    SDL_SetRenderDrawColor(renderer, 220, 220, 230, 255);
    int keys_x = TEXT_X + text_width("KEYBOARD", 3) + 30;
    draw_text(renderer, "P1: WASD, F = SQUARE, G = TRIANGLE, SPACE = X", keys_x, PANEL_Y + PANEL_H - 205, 3);
    draw_text(renderer, "P2: ARROWS, RIGHT CTRL = SQUARE, RIGHT SHIFT = TRIANGLE", keys_x, PANEL_Y + PANEL_H - 175, 3);
#endif
    draw_settings(renderer, PANEL_Y + PANEL_H - 128);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    draw_text_centered(renderer, KEY_HINT("OPTIONS", "ESC") ": RESUME      " KEY_HINT("TOUCHPAD + OPTIONS", "Q") ": QUIT",
                       SCREEN_WIDTH / 2, PANEL_Y + PANEL_H - 42, 3);
}
