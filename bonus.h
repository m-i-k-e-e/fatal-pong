// Field bonuses: spawning, collection, paddle effects and their drawing
#ifndef PONG_BONUS_H
#define PONG_BONUS_H

#include "SDL2/SDL.h"
#include "game.h"

#define BONUS_SIZE          56
#define BONUS_DURATION      (10 * 60)   // Frames an effect lasts on a paddle (game runs at 60 FPS)
#define BONUS_FIELD_LIFE    (8 * 60)    // Frames an uncollected bonus stays on the field
#define BONUS_SPAWN_MIN     (5 * 60)
#define BONUS_SPAWN_MAX     (10 * 60)
#define MAX_BONUSES         2

void reset_bonuses(void);
void clear_paddle_effect(Paddle *p);
void update_paddle_effect(Paddle *p);
void update_bonuses(Paddle *p1, Paddle *p2);
void draw_bonuses(SDL_Renderer *renderer);
void draw_effect_indicator(SDL_Renderer *renderer, const Paddle *p, int x, int y);

// Shared with the pause screen's bonus legend
const char *bonus_name(BonusType t);
const char *bonus_description(BonusType t);
void set_bonus_color(SDL_Renderer *renderer, BonusType t);
void draw_bonus_box(SDL_Renderer *renderer, BonusType t, int x, int y);

#endif
