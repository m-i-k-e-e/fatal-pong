#include "gameplay/paddle.h"
#include "gameplay/ball.h"
#include <math.h>

// A fresh paddle at column x, vertically centered, normal size, no bonus or timers
Paddle make_paddle(float x) {
    Paddle p = {0};
    p.x = x;
    p.y = (SCREEN_HEIGHT - PADDLE_HEIGHT) / 2.0f;
    p.w = PADDLE_WIDTH;
    p.h = PADDLE_HEIGHT;
    p.effect = BONUS_NONE;
    return p;
}

// Keep the paddle fully on screen
static void clamp_to_screen(Paddle *p) {
    if (p->y < 0) p->y = 0;
    if (p->y + p->h > SCREEN_HEIGHT) p->y = SCREEN_HEIGHT - p->h;
}

// Resize around the paddle's center, kept on screen
void set_paddle_height(Paddle *p, float h) {
    float center = p->y + p->h / 2.0f;
    p->h = h;
    p->y = center - h / 2.0f;
    clamp_to_screen(p);
}

// Move for an input direction in [-1, 1] at PADDLE_SPEED, scaled like the ball by the start speed setting and
// the rally speed-up; stunned paddles tick down their stun and stay put, and so do paddles gone through a rift
// (rift.c counts that down)
void move_paddle(Paddle *p, float dir) {
    if (p->swing_timer > 0) p->swing_timer--;
    if (p->throw_timer > 0) p->throw_timer--;
    p->moving = false;
    if (p->vanish_timer > 0) return;
    if (p->stun_timer > 0) { p->stun_timer--; return; }

    float speed = PADDLE_SPEED * start_speed_percent / 100.0f * speed_scale;
    if (p->effect == BONUS_FAST) speed *= 1.8f;
    else if (p->effect == BONUS_SLOW) speed *= 0.5f;
    if (p->effect == BONUS_INVERT) dir = -dir;
    float old_y = p->y;
    p->y += dir * speed;
    clamp_to_screen(p);
    float moved = fabsf(p->y - old_y);
    p->moving = moved > 0.5f;
    p->stride += moved;
}

