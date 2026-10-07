#include "pause.h"
#include "bonus.h"
#include "fireball.h"
#include "rift.h"
#include "calamity.h"
#include "text.h"
#include <stdio.h>

#define PANEL_W     1400
#define PANEL_H     900
#define PANEL_X     ((SCREEN_WIDTH - PANEL_W) / 2)
#define PANEL_Y     ((SCREEN_HEIGHT - PANEL_H) / 2)
#define TEXT_X      (PANEL_X + 70)

// Orange section heading on the panel's left margin
static void draw_heading(SDL_Renderer *renderer, const char *s, int y) {
    SDL_SetRenderDrawColor(renderer, 255, 140, 0, 255);
    draw_text(renderer, s, TEXT_X, y, 5);
}

// Drawn over the frozen game: dims the court, then a framed panel with the move list and bonus legend
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
    SDL_SetRenderDrawColor(renderer, 160, 160, 170, 255);
    char calamity_line[96];
    snprintf(calamity_line, sizeof(calamity_line), "EVERY HIT SPEEDS UP THE GAME. EVERY %dTH HIT: %d%% CHANCE OF A CALAMITY",
             CALAMITY_EVERY_HITS, CALAMITY_CHANCE);
    draw_text_centered(renderer, calamity_line, SCREEN_WIDTH / 2, PANEL_Y + PANEL_H - 100, 3);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    draw_text_centered(renderer, "OPTIONS: RESUME      TOUCHPAD + OPTIONS: QUIT", SCREEN_WIDTH / 2, PANEL_Y + PANEL_H - 55, 3);
}
