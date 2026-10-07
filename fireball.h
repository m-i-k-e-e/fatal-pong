// Special move input (hadouken, rift), fireballs and their hits
#ifndef PONG_FIREBALL_H
#define PONG_FIREBALL_H

#include "SDL2/SDL.h"
#include "game.h"

#define FIREBALL_W          44
#define FIREBALL_H          30
#define FIREBALL_SPEED      18
#define STUN_DURATION       60          // Frames a paddle hit by a fireball can't move (1 second)
#define THROW_HAND_Y        (49.0f / 80.0f)  // Hands' height in the player sprites' thrust pose, 0 = top

void reset_fireballs(void);
void update_special_input(Paddle *p, Paddle *opponent, bool is_p1, SDL_GameController *pad);
void update_fireballs(Paddle *p1, Paddle *p2, Ball *ball);
void draw_fireballs(SDL_Renderer *renderer);

#endif
