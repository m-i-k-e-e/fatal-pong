// Shared constants and core types used across the game modules
#ifndef PONG_GAME_H
#define PONG_GAME_H

#include <stdbool.h>
#include <stdlib.h>

#define SCREEN_WIDTH        1920
#define SCREEN_HEIGHT       1080

#define PADDLE_WIDTH        24
#define PADDLE_HEIGHT       160
#define PADDLE_SPEED        14          // At 100% in the pause menu, before the rally speed-up
#define PADDLE_MARGIN       60
#define SWING_DURATION      14          // Racket swing animation length after a hit
#define THROW_DURATION      18          // Hadouken thrust pose length after a fireball throw

#define BALL_SIZE           22
#define INITIAL_BALL_SPEED  12          // Serve speed at 100% in the pause menu
#define WINNING_SCORE       10

typedef enum {
    BONUS_NONE,
    BONUS_GROW,     // Double height
    BONUS_SHRINK,   // Half height
    BONUS_GHOST,    // Invisible (still blocks the ball)
    BONUS_FULL,     // Full screen height
    BONUS_FAST,
    BONUS_SLOW,
    BONUS_INVERT,   // Up/down controls swapped
    BONUS_ZIGZAG,   // Balls this paddle hits zig-zag randomly
    BONUS_COUNT
} BonusType;

typedef struct {
    float x, y;
    float w, h;
    int score;
    int stun_timer;           // Frozen after a fireball hit
    int motion_state;         // 0: Idle, 1: Down detected, 2: then Forward (hadouken), 3: then Back (rift)
    int motion_timer;
    BonusType effect;         // Active bonus effect, replaced by the next one collected
    int effect_timer;
    float stride;             // Distance moved, drives the step animation
    bool moving;              // Moved this frame
    int swing_timer;          // Counts down through the racket swing
    int throw_timer;          // Counts down through the hadouken thrust pose
    bool unarmed;             // Threw the racket in a fatality
    bool headless;            // Lost the head to a fatality
    int vanish_timer;         // Fell through a rift: sinking, gone, then rising (see rift.h)
    int force_user;           // Full bonus: who stands in, 0 Vader or 1 Luke (see players.c)
} Paddle;

typedef struct {
    float x, y;
    float vx, vy;
    float size;
    bool zigzag;        // Hit by a paddle with the zig-zag bonus
    int zigzag_timer;   // Frames until the next direction change
    bool held;          // Caught by a frog: frozen, moved by the frog's tongue (see calamity.c)
    bool hidden;        // Swallowed by a frog: not drawn
} Ball;

// Uniform random float in [0, 1]. Uses only rand()'s low 15 bits: the SDK's stdlib.h says RAND_MAX is
// 0x7ffffffd, but the PS5's libc rand() only goes up to 32767, so rand() / RAND_MAX was always about 0.
static inline float rand01(void) {
    return (rand() & 0x7fff) / 32767.0f;
}

static inline bool rects_overlap(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

#endif
