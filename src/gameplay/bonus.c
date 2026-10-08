#include "gameplay/bonus.h"
#include "gameplay/paddle.h"
#include "core/audio.h"
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

// Pixel-art icons in the players' style, generated with their palette by tools/gen_bonus_icons.py; the art
// array starts at BONUS_GROW (BonusType order, without BONUS_NONE)
#include "bonus_icons.inc"
#define ICON_FIELD_SCALE    3           // 48 px, filling the inside of the 56 px tile
#define ICON_HUD_SCALE      2           // Under the scores

static SDL_Texture *icon_tex[BONUS_COUNT];

// Names and one-line explanations shown on the pause screen, indexed by BonusType
static const char *const BONUS_NAMES[BONUS_COUNT] = {
    "", "GROW", "SHRINK", "GHOST", "FULL", "FAST", "SLOW", "INVERT", "ZIG-ZAG",
};

static const char *const BONUS_DESCRIPTIONS[BONUS_COUNT] = {
    "",
    "DOUBLE PADDLE HEIGHT",
    "HALF PADDLE HEIGHT",
    "INVISIBLE, STILL BLOCKS",
    "VADER OR LUKE HOLDS THE LANE",
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

// Give the paddle bonus `t` for BONUS_DURATION, replacing any active one; resizes it for the size bonuses. Full
// height brings in Vader or Luke at random, igniting a lightsaber.
static void apply_bonus(Paddle *p, BonusType t) {
    clear_paddle_effect(p);
    p->effect = t;
    p->effect_timer = BONUS_DURATION;
    if (t == BONUS_GROW) set_paddle_height(p, PADDLE_HEIGHT * 2);
    else if (t == BONUS_SHRINK) set_paddle_height(p, PADDLE_HEIGHT / 2);
    else if (t == BONUS_FULL) {
        set_paddle_height(p, SCREEN_HEIGHT);
        p->force_user = rand() % 2;
        play_sound(&snd_saber);
    }
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
// Build the icon textures from their character art (once, at startup); a missing one just isn't drawn
void init_bonus_icons(SDL_Renderer *renderer) {
    for (int t = 1; t < BONUS_COUNT; t++) {
        SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, BONUS_ICON_SIZE, BONUS_ICON_SIZE, 32, SDL_PIXELFORMAT_ARGB8888);
        if (!surf) continue;
        for (int y = 0; y < BONUS_ICON_SIZE; y++) {
            Uint32 *row = (Uint32 *)((Uint8 *)surf->pixels + y * surf->pitch);
            for (int x = 0; x < BONUS_ICON_SIZE; x++) {
                SDL_Color c = bonus_icon_color(BONUS_ICON_ART[t - 1][y][x]);
                row[x] = SDL_MapRGBA(surf->format, c.r, c.g, c.b, c.a);
            }
        }
        icon_tex[t] = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_FreeSurface(surf);
        if (icon_tex[t]) SDL_SetTextureBlendMode(icon_tex[t], SDL_BLENDMODE_BLEND);
    }
}

// Destroy the textures made by init_bonus_icons()
void free_bonus_icons(void) {
    for (int t = 0; t < BONUS_COUNT; t++) {
        if (icon_tex[t]) SDL_DestroyTexture(icon_tex[t]);
        icon_tex[t] = NULL;
    }
}

// A bonus's icon with its top-left at (x, y), `scale` screen pixels per art pixel
static void draw_icon(SDL_Renderer *renderer, BonusType t, int x, int y, int scale) {
    if (!icon_tex[t]) return;
    SDL_Rect dst = { x, y, BONUS_ICON_SIZE * scale, BONUS_ICON_SIZE * scale };
    SDL_RenderCopy(renderer, icon_tex[t], NULL, &dst);
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

// Tile with a bevelled frame (green good, red bad: lighter on the top and left edges, darker on the others),
// a slate inside light enough for the icon's dark outline to read, and the icon filling it
void draw_bonus_box(SDL_Renderer *renderer, BonusType t, int x, int y) {
    SDL_Rect box = { x, y, BONUS_SIZE, BONUS_SIZE };
    set_bonus_color(renderer, t);
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 90);
    SDL_Rect top = { x, y, BONUS_SIZE, 2 }, left = { x, y, 2, BONUS_SIZE };
    SDL_RenderFillRect(renderer, &top);
    SDL_RenderFillRect(renderer, &left);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 90);
    SDL_Rect bottom = { x, y + BONUS_SIZE - 2, BONUS_SIZE, 2 }, right = { x + BONUS_SIZE - 2, y, 2, BONUS_SIZE };
    SDL_RenderFillRect(renderer, &bottom);
    SDL_RenderFillRect(renderer, &right);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_Rect inner = { x + 4, y + 4, BONUS_SIZE - 8, BONUS_SIZE - 8 };
    SDL_SetRenderDrawColor(renderer, 52, 56, 72, 255);
    SDL_RenderFillRect(renderer, &inner);
    draw_icon(renderer, t, x + 4, y + 4, ICON_FIELD_SCALE);
}

// Active effect icon and a shrinking time bar under the paddle's score
void draw_effect_indicator(SDL_Renderer *renderer, const Paddle *p, int x, int y) {
    if (p->effect == BONUS_NONE) return;
    draw_icon(renderer, p->effect, x, y, ICON_HUD_SCALE);
    set_bonus_color(renderer, p->effect);
    SDL_Rect bar = { x + BONUS_ICON_SIZE * ICON_HUD_SCALE + 8, y + 11, 100 * p->effect_timer / BONUS_DURATION, 10 };
    SDL_RenderFillRect(renderer, &bar);
}
