// Ball movement, paddle hits, rally speed-up and zig-zag
#ifndef PONG_BALL_H
#define PONG_BALL_H

#include "SDL2/SDL.h"
#include "game.h"

#define SPEED_UP_PER_HIT    1.05f       // Ball and paddle speed multiplier per ball hit, reset each serve
#define MAX_SPEED_SCALE     3.0f        // Beyond this the ball can skip past a paddle in one frame
#define ZIGZAG_MIN_FRAMES   8           // Random gap between a zig-zagging ball's direction changes
#define ZIGZAG_MAX_FRAMES   24

extern float speed_scale;    // Current rally's ball and paddle speed multiplier
extern int paddle_hits;      // Ball hits by paddles this match, for the calamities

void speed_up(void);
void reset_ball(Ball *ball, int serve_direction);
void update_ball(Ball *ball, Paddle *p1, Paddle *p2);
void draw_ball(SDL_Renderer *renderer, const Ball *ball);

#endif
