#include "fireball.h"
#include "audio.h"
#include "ball.h"
#include "particles.h"
#include "rift.h"
#include <stdlib.h>

typedef struct {
    float x, y;
    float vx;
    bool active;
} Fireball;

// One per player (indexed 0 = P1, 1 = P2); a player can throw a new one once theirs is gone.
static Fireball fireballs[2];

// New match: remove both players' fireballs
void reset_fireballs(void) {
    fireballs[0].active = fireballs[1].active = false;
}

// Launched from the player's hands (their height in the thrust pose), with a burst of sparks
static void throw_fireball(int owner, const Paddle *p) {
    Fireball *f = &fireballs[owner];
    if (f->active) return;
    f->vx = owner == 0 ? FIREBALL_SPEED : -FIREBALL_SPEED;
    f->x = owner == 0 ? p->x + p->w : p->x - FIREBALL_W;
    f->y = p->y + p->h * THROW_HAND_Y - FIREBALL_H / 2.0f;
    f->active = true;
    spawn_explosion(owner == 0 ? f->x : f->x + FIREBALL_W, f->y + FIREBALL_H / 2.0f, 14);
}

// Special move recognition state machine: Down -> Forward -> Attack button throws a fireball (hadouken),
// Down -> Back -> Attack button opens a rift under the opponent. On a whole pad the directions are the D-pad
// or left stick and the attack is Square or R1; a shared pad is split: the left half (D-pad, left stick, L1)
// and the right half (right stick, Square or R1).
void update_special_input(Paddle *p, Paddle *opponent, bool is_p1, SDL_GameController *pad, PadPart part) {
    if (!pad || p->stun_timer > 0 || p->vanish_timer > 0) return;

    if (p->motion_timer > 0) p->motion_timer--;
    else p->motion_state = 0;

    bool dpad = part != PAD_RIGHT_HALF;
    SDL_GameControllerAxis ax = part == PAD_RIGHT_HALF ? SDL_CONTROLLER_AXIS_RIGHTX : SDL_CONTROLLER_AXIS_LEFTX;
    SDL_GameControllerAxis ay = part == PAD_RIGHT_HALF ? SDL_CONTROLLER_AXIS_RIGHTY : SDL_CONTROLLER_AXIS_LEFTY;
    bool down = (dpad && SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) ||
                SDL_GameControllerGetAxis(pad, ay) > 16000;
    bool right = (dpad && SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) ||
                 SDL_GameControllerGetAxis(pad, ax) > 16000;
    bool left = (dpad && SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) ||
                SDL_GameControllerGetAxis(pad, ax) < -16000;
    bool fwd = is_p1 ? right : left;
    bool back = is_p1 ? left : right;

    // Punch button: Square (Button X) or R1, or L1 for the left half of a shared pad
    bool attack = part == PAD_LEFT_HALF
                ? SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER)
                : SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_X) ||
                  SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);

    if (p->motion_state == 0 && down) {
        p->motion_state = 1;
        p->motion_timer = 20; // 20 frames to hit Forward
    } else if (p->motion_state == 1 && (fwd || back)) {
        p->motion_state = fwd ? 2 : 3;
        p->motion_timer = 20; // 20 frames to hit Punch
    } else if (p->motion_state >= 2 && attack) {
        int owner = is_p1 ? 0 : 1;
        if (p->motion_state == 3) {
            open_rift(owner, opponent);
            p->throw_timer = THROW_DURATION;
        } else if (!fireballs[owner].active) {
            throw_fireball(owner, p);
            p->throw_timer = THROW_DURATION;
            play_sound(&snd_hadouken);
        }
        p->motion_state = 0;
        p->motion_timer = 0;
    }
}

// Move fireballs; one hitting the opposing paddle stuns it, one hitting the ball knocks it toward
// the opponent, and colliding fireballs cancel out
void update_fireballs(Paddle *p1, Paddle *p2, Ball *ball) {
    for (int k = 0; k < 2; k++) {
        Fireball *f = &fireballs[k];
        if (!f->active) continue;
        f->x += f->vx;
        spawn_particle(f->x + FIREBALL_W / 2.0f, f->y + FIREBALL_H / 2.0f + ((rand() % 20) - 10),
                       -f->vx * 0.15f + ((rand() % 20) - 10) / 10.0f, ((rand() % 20) - 10) / 8.0f,
                       10 + rand() % 8, 255, 60 + rand() % 120, 0, 14);

        Paddle *target = k == 0 ? p2 : p1;
        if (!ball->held && rects_overlap(f->x, f->y, FIREBALL_W, FIREBALL_H, ball->x, ball->y, ball->size, ball->size)) {
            // Send the ball the fireball's way; hitting it off-center angles it like a paddle hit
            float impact = ((ball->y + ball->size / 2.0f) - (f->y + FIREBALL_H / 2.0f)) / (FIREBALL_H / 2.0f + ball->size / 2.0f);
            speed_up();
            float speed = ball_base_speed() * speed_scale;
            ball->vx = f->vx > 0 ? speed : -speed;
            ball->vy = impact * speed;
            spawn_explosion(ball->x + ball->size / 2.0f, ball->y + ball->size / 2.0f, 25);
            f->active = false;
        } else if (target->vanish_timer == 0 &&
                   rects_overlap(f->x, f->y, FIREBALL_W, FIREBALL_H, target->x, target->y, target->w, target->h)) {
            target->stun_timer = STUN_DURATION;
            target->motion_state = 0;
            spawn_explosion(f->x + FIREBALL_W / 2.0f, f->y + FIREBALL_H / 2.0f, 40);
            f->active = false;
        } else if (f->x + FIREBALL_W < 0 || f->x > SCREEN_WIDTH) {
            f->active = false;
        }
    }

    Fireball *a = &fireballs[0], *b = &fireballs[1];
    if (a->active && b->active && rects_overlap(a->x, a->y, FIREBALL_W, FIREBALL_H, b->x, b->y, FIREBALL_W, FIREBALL_H)) {
        spawn_explosion((a->x + b->x + FIREBALL_W) / 2.0f, (a->y + b->y + FIREBALL_H) / 2.0f, 30);
        a->active = b->active = false;
    }
}

// Flying fireballs: a red flame with a yellow core toward the leading edge
void draw_fireballs(SDL_Renderer *renderer) {
    for (int k = 0; k < 2; k++) {
        const Fireball *f = &fireballs[k];
        if (!f->active) continue;
        SDL_SetRenderDrawColor(renderer, 255, 50, 0, 255);       // Flaming outer
        SDL_Rect outer = { (int)f->x, (int)f->y, FIREBALL_W, FIREBALL_H };
        SDL_RenderFillRect(renderer, &outer);
        SDL_SetRenderDrawColor(renderer, 255, 240, 60, 255);     // Hot core, toward the leading edge
        int core_x = f->vx > 0 ? (int)f->x + FIREBALL_W / 2 - 2 : (int)f->x + 6;
        SDL_Rect core = { core_x, (int)f->y + 7, FIREBALL_W / 2 - 4, FIREBALL_H - 14 };
        SDL_RenderFillRect(renderer, &core);
    }
}
