// Paddle movement and sizing
#ifndef PONG_PADDLE_H
#define PONG_PADDLE_H

#include "SDL2/SDL.h"
#include "core/game.h"

Paddle make_paddle(float x);
void set_paddle_height(Paddle *p, float h);
void move_paddle(Paddle *p, float dir);

#endif
