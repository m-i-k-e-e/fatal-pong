#include "bonus.h"
#include "paddle.h"
#include <stdlib.h>

// Spawn in a paddle's lane; the paddle collects one by moving onto it and gets its effect.
typedef struct {
    float x, y;
    BonusType type;
    int life;
    bool active;
} Bonus;

static Bonus bonuses[MAX_BONUSES];
static int spawn_timer = BONUS_SPAWN_MIN;

// 5x5 icons drawn inside the bonus box, indexed by BonusType
static const unsigned char BONUS_ICONS[BONUS_COUNT][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // none
    {0x04, 0x04, 0x1F, 0x04, 0x04}, // grow: +
    {0x00, 0x00, 0x1F, 0x00, 0x00}, // shrink: -
    {0x1F, 0x11, 0x11, 0x11, 0x1F}, // ghost: hollow box
    {0x0E, 0x0E, 0x0E, 0x0E, 0x0E}, // full: solid column
    {0x14, 0x0A, 0x05, 0x0A, 0x14}, // fast: >>
    {0x1F, 0x0E, 0x04, 0x0E, 0x1F}, // slow: hourglass
    {0x08, 0x1C, 0x0A, 0x07, 0x02}, // invert: up/down arrows
    {0x06, 0x0C, 0x1F, 0x06, 0x0C}, // zigzag: lightning bolt
};

// Names and one-line explanations shown on the pause screen, indexed by BonusType
static const char *const BONUS_NAMES[BONUS_COUNT] = {
    "", "GROW", "SHRINK", "GHOST", "FULL", "FAST", "SLOW", "INVERT", "ZIG-ZAG",
};

static const char *const BONUS_DESCRIPTIONS[BONUS_COUNT] = {
    "",
    "DOUBLE PADDLE HEIGHT",
    "HALF PADDLE HEIGHT",
    "INVISIBLE, STILL BLOCKS",
    "PADDLE FILLS THE SCREEN",
    "FASTER PADDLE",
    "SLOWER PADDLE",
    "UP AND DOWN SWAPPED",
    "YOUR HITS ZIG-ZAG",
};

// Name and one-line description of a bonus, for the pause screen's legend
const char *bonus_name(BonusType t) { return BONUS_NAMES[t]; }
const char *bonus_description(BonusType t) { return BONUS_DESCRIPTIONS[t]; }

// Whether a bonus helps the paddle that collects it (drawn green) or hinders it (red)
static bool bonus_is_good(BonusType t) {
    return t == BONUS_GROW || t == BONUS_FULL || t == BONUS_FAST || t == BONUS_ZIGZAG;
}

// New match: clear the field and restart the spawn timer
void reset_bonuses(void) {
    for (int i = 0; i < MAX_BONUSES; i++) bonuses[i].active = false;
    spawn_timer = BONUS_SPAWN_MIN;
}

// Remove the paddle's active bonus and restore its normal height
void clear_paddle_effect(Paddle *p) {
    p->effect = BONUS_NONE;
    p->effect_timer = 0;
    set_paddle_height(p, PADDLE_HEIGHT);
}

// Give the paddle bonus `t` for BONUS_DURATION, replacing any active one; resizes it for the size bonuses
static void apply_bonus(Paddle *p, BonusType t) {
    clear_paddle_effect(p);
    p->effect = t;
    p->effect_timer = BONUS_DURATION;
    if (t == BONUS_GROW) set_paddle_height(p, PADDLE_HEIGHT * 2);
    else if (t == BONUS_SHRINK) set_paddle_height(p, PADDLE_HEIGHT / 2);
    else if (t == BONUS_FULL) set_paddle_height(p, SCREEN_HEIGHT);
}

// Count down the paddle's active bonus and clear it when it runs out
void update_paddle_effect(Paddle *p) {
    if (p->effect_timer > 0 && --p->effect_timer == 0) clear_paddle_effect(p);
}

// Place a random bonus in a random paddle's lane, away from the paddle so it isn't collected instantly.
// A full-screen paddle covers its whole lane, so that side never gets one.
static void spawn_bonus(const Paddle *p1, const Paddle *p2) {
    Bonus *slot = NULL;
    for (int i = 0; i < MAX_BONUSES; i++) if (!bonuses[i].active) { slot = &bonuses[i]; break; }
    if (!slot) return;

    for (int attempt = 0; attempt < 10; attempt++) {
        const Paddle *p = (rand() % 2) ? p2 : p1;
        float x = p->x + p->w / 2.0f - BONUS_SIZE / 2.0f;
        float y = 40 + rand() % (SCREEN_HEIGHT - 80 - BONUS_SIZE);

        if (rects_overlap(x, y, BONUS_SIZE, BONUS_SIZE, p->x, p->y - 60, p->w, p->h + 120)) continue;
        bool blocked = false;
        for (int i = 0; i < MAX_BONUSES; i++) {
            if (bonuses[i].active && rects_overlap(x, y, BONUS_SIZE, BONUS_SIZE,
                                                    bonuses[i].x, bonuses[i].y, BONUS_SIZE, BONUS_SIZE)) blocked = true;
        }
        if (blocked) continue;

        slot->x = x;
        slot->y = y;
        slot->type = (BonusType)(1 + rand() % (BONUS_COUNT - 1));
        slot->life = BONUS_FIELD_LIFE;
        slot->active = true;
        return;
    }
}

// Spawn on a random timer, expire uncollected bonuses, and hand collected ones to the paddle
void update_bonuses(Paddle *p1, Paddle *p2) {
    if (--spawn_timer <= 0) {
        spawn_bonus(p1, p2);
        spawn_timer = BONUS_SPAWN_MIN + rand() % (BONUS_SPAWN_MAX - BONUS_SPAWN_MIN);
    }

    for (int i = 0; i < MAX_BONUSES; i++) {
        Bonus *b = &bonuses[i];
        if (!b->active) continue;
        if (--b->life <= 0) { b->active = false; continue; }

        for (int k = 0; k < 2; k++) {
            Paddle *p = k == 0 ? p1 : p2;
            if (p->vanish_timer == 0 && rects_overlap(b->x, b->y, BONUS_SIZE, BONUS_SIZE, p->x, p->y, p->w, p->h)) {
                apply_bonus(p, b->type);
                b->active = false;
                break;
            }
        }
    }
}

// A bonus's 5x5 icon in the current draw color, each icon pixel `pixel_size` screen pixels wide
static void draw_icon(SDL_Renderer *renderer, BonusType t, int x, int y, int pixel_size) {
    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 5; c++) {
            if ((BONUS_ICONS[t][r] >> (4 - c)) & 1) {
                SDL_Rect rect = { x + c * pixel_size, y + r * pixel_size, pixel_size, pixel_size };
                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }
}

// Set the draw color for a bonus: green for good ones, red for bad ones
void set_bonus_color(SDL_Renderer *renderer, BonusType t) {
    if (bonus_is_good(t)) SDL_SetRenderDrawColor(renderer, 60, 220, 90, 255);
    else SDL_SetRenderDrawColor(renderer, 235, 60, 60, 255);
}

// Bonuses waiting on the field; they blink in their last two seconds
void draw_bonuses(SDL_Renderer *renderer) {
    for (int i = 0; i < MAX_BONUSES; i++) {
        const Bonus *b = &bonuses[i];
        if (!b->active) continue;
        if (b->life < 2 * 60 && (b->life / 8) % 2) continue; // Blink before vanishing

        draw_bonus_box(renderer, b->type, (int)b->x, (int)b->y);
    }
}

// Colored frame (green good, red bad) with the white icon inside
void draw_bonus_box(SDL_Renderer *renderer, BonusType t, int x, int y) {
    SDL_Rect box = { x, y, BONUS_SIZE, BONUS_SIZE };
    set_bonus_color(renderer, t);
    SDL_RenderFillRect(renderer, &box);
    SDL_Rect inner = { x + 4, y + 4, BONUS_SIZE - 8, BONUS_SIZE - 8 };
    SDL_SetRenderDrawColor(renderer, 24, 24, 30, 255);
    SDL_RenderFillRect(renderer, &inner);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    draw_icon(renderer, t, x + 8, y + 8, 8);
}

// Active effect icon and a shrinking time bar under the paddle's score
void draw_effect_indicator(SDL_Renderer *renderer, const Paddle *p, int x, int y) {
    if (p->effect == BONUS_NONE) return;
    set_bonus_color(renderer, p->effect);
    draw_icon(renderer, p->effect, x, y, 5);
    SDL_Rect bar = { x + 35, y + 8, 100 * p->effect_timer / BONUS_DURATION, 10 };
    SDL_RenderFillRect(renderer, &bar);
}
