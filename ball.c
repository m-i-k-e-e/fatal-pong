#include "ball.h"
#include "audio.h"
#include <stdlib.h>

#define GRUNT_CHANCE        25          // Percent of hits the player grunts on

float speed_scale = 1.0f;
int paddle_hits = 0;
int start_speed_percent = 100;

// INITIAL_BALL_SPEED scaled by the pause menu's ball speed setting
float ball_base_speed(void) {
    return INITIAL_BALL_SPEED * start_speed_percent / 100.0f;
}

// Every ball hit speeds up both the ball and the paddles, up to MAX_SPEED_SCALE; a serve faster than
// INITIAL_BALL_SPEED stops sooner, so the ball never passes MAX_SPEED_SCALE * INITIAL_BALL_SPEED
void speed_up(void) {
    float max_scale = SDL_min(MAX_SPEED_SCALE, MAX_SPEED_SCALE * INITIAL_BALL_SPEED / ball_base_speed());
    speed_scale *= SPEED_UP_PER_HIT;
    if (speed_scale > max_scale) speed_scale = max_scale;
}

// Serve from the center toward `serve_direction` (1 = right, -1 = left), angled up or down at random; resets
// the rally speed-up, the zig-zag and any frog's hold on the ball
void reset_ball(Ball *ball, int serve_direction) {
    speed_scale = 1.0f;
    ball->x = (SCREEN_WIDTH - ball->size) / 2.0f;
    ball->y = (SCREEN_HEIGHT - ball->size) / 2.0f;
    ball->vx = serve_direction * ball_base_speed();
    ball->vy = (rand() % 2 == 0 ? 1 : -1) * (ball_base_speed() / 2.0f);
    ball->zigzag = false;
    ball->held = ball->hidden = false;
}

// Bounce off a paddle toward `direction` (1 = right, -1 = left), angled by where it struck; plays the hit,
// and now and then (GRUNT_CHANCE) one of the grunts
static void hit_by_paddle(Ball *ball, Paddle *p, int direction) {
    p->swing_timer = SWING_DURATION;
    paddle_hits++;
    play_sound(&snd_paddle_hit);
    if (rand() % 100 < GRUNT_CHANCE) play_sound(&snd_grunts[rand() % GRUNT_COUNT]);
    float impact = ((ball->y + ball->size / 2.0f) - (p->y + p->h / 2.0f)) / (p->h / 2.0f);
    speed_up();
    ball->vx = direction * ball_base_speed() * speed_scale;
    ball->vy = impact * ball_base_speed() * speed_scale;
    ball->x = direction > 0 ? p->x + p->w : p->x - ball->size;
    ball->zigzag = p->effect == BONUS_ZIGZAG;
    ball->zigzag_timer = ZIGZAG_MIN_FRAMES;
}

// One frame of ball physics: zig-zag swings, movement, wall bounces and paddle hits (the paddles' vanish
// timers let it pass through). Does nothing while a frog holds the ball.
void update_ball(Ball *ball, Paddle *p1, Paddle *p2) {
    if (ball->held) return;             // A frog has it

    // Zig-zag: at random intervals, swing the ball the other way vertically at a random steepness
    if (ball->zigzag && --ball->zigzag_timer <= 0) {
        float steepness = (0.5f + (rand() % 100) / 200.0f) * ball_base_speed() * speed_scale;
        ball->vy = ball->vy > 0 ? -steepness : steepness;
        ball->zigzag_timer = ZIGZAG_MIN_FRAMES + rand() % (ZIGZAG_MAX_FRAMES - ZIGZAG_MIN_FRAMES);
    }

    ball->x += ball->vx;
    ball->y += ball->vy;

    // Wall Bounces
    if (ball->y <= 0) { ball->y = 0; ball->vy = -ball->vy; }
    else if (ball->y + ball->size >= SCREEN_HEIGHT) { ball->y = SCREEN_HEIGHT - ball->size; ball->vy = -ball->vy; }

    // Paddle collisions; a player gone through a rift lets the ball past
    if (ball->vx < 0 && p1->vanish_timer == 0 &&
        ball->x <= p1->x + p1->w && ball->x + ball->size >= p1->x &&
        ball->y + ball->size >= p1->y && ball->y <= p1->y + p1->h) {
        hit_by_paddle(ball, p1, 1);
    }

    if (ball->vx > 0 && p2->vanish_timer == 0 &&
        ball->x + ball->size >= p2->x && ball->x <= p2->x + p2->w &&
        ball->y + ball->size >= p2->y && ball->y <= p2->y + p2->h) {
        hit_by_paddle(ball, p2, -1);
    }
}

// Tennis-ball yellow, hot pink while zig-zagging so the opponent sees it coming
void draw_ball(SDL_Renderer *renderer, const Ball *ball) {
    if (ball->hidden) return;
    if (ball->zigzag) SDL_SetRenderDrawColor(renderer, 255, 70, 200, 255);
    else SDL_SetRenderDrawColor(renderer, 225, 245, 60, 255);
    SDL_Rect r = { (int)ball->x, (int)ball->y, (int)ball->size, (int)ball->size };
    SDL_RenderFillRect(renderer, &r);
}
