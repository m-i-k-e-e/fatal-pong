// Shared constants and core types used across the game modules
#ifndef PONG_GAME_H
#define PONG_GAME_H

#include <stdbool.h>

#define SCREEN_WIDTH        1920
#define SCREEN_HEIGHT       1080

#define PADDLE_WIDTH        24
#define PADDLE_HEIGHT       160
#define PADDLE_SPEED        14
#define PADDLE_MARGIN       60
#define SWING_DURATION      14          // Racket swing animation length after a hit
#define THROW_DURATION      18          // Hadouken thrust pose length after a fireball throw

#define BALL_SIZE           22
#define INITIAL_BALL_SPEED  12
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

static inline bool rects_overlap(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

#endif
