#include "gameplay/rift.h"
#include "core/audio.h"
#include "core/draw.h"
#include "core/particles.h"
#include "render/players.h"
#include <stdlib.h>

// A rift opens on the ground under the opponent's feet and crackles for RIFT_WARNING frames, pulsing faster
// as it's about to snap. If their feet are still on it then, they sink through, stay gone (the ball goes
// past) and rise back out of it; otherwise it fizzles. Drawn in 4 px steps to match the pixel art.
#define RIFT_RX             72          // Full half-size in screen pixels
#define RIFT_RY             22
#define RIFT_CATCH          60          // Feet within this many pixels of the rift's center line get caught
#define RIFT_GROW           8           // Frames to open to full size
#define RIFT_CLOSE          14          // Frames to shrink shut
#define PIXEL               4

typedef enum { RIFT_OFF, RIFT_OPENING, RIFT_HOLDING, RIFT_CLOSING } RiftState;

typedef struct {
    RiftState state;
    Paddle *target;
    bool target_faces_right;
    int x, y;                           // Center, on the ground where the target stood
    int timer;                          // Frames in the current state
} Rift;

static Rift rifts[2];                   // By caster: 0 = P1, 1 = P2
static int ticks;                       // Drives the swirl

// Random float in [lo, hi]
static float frand(float lo, float hi) {
    return lo + (hi - lo) * rand01();
}

// Keep the paddle fully on screen
static void clamp_to_screen(Paddle *p) {
    if (p->y < 0) p->y = 0;
    if (p->y + p->h > SCREEN_HEIGHT) p->y = SCREEN_HEIGHT - p->h;
}

// `count` purple sparks thrown out from the rift, up to `speed` pixels per frame
static void burst(const Rift *r, int count, float speed) {
    for (int i = 0; i < count; i++) {
        float a = frand(0, 2 * (float)M_PI);
        spawn_particle(r->x + SDL_cosf(a) * RIFT_RX * 0.6f, r->y + SDL_sinf(a) * RIFT_RY * 0.6f,
                       SDL_cosf(a) * frand(1, speed), SDL_sinf(a) * frand(1, speed) - frand(1, speed),
                       frand(5, 10), 200, (int)frand(60, 160), 255, (int)frand(14, 26));
    }
}

// Open player `owner`'s rift under the target's feet, unless theirs is already open or the target is gone
void open_rift(int owner, Paddle *target) {
    Rift *r = &rifts[owner];
    if (r->state != RIFT_OFF || target->vanish_timer > 0) return;
    r->target = target;
    r->target_faces_right = owner == 1;           // P2's rift targets P1, who faces right
    player_point(target, r->target_faces_right, POINT_FEET, &r->x, &r->y);
    r->state = RIFT_OPENING;
    r->timer = 0;
    play_sound(&snd_rift);
}

// Every frame of play: warns, then catches a target still on the rift (snapping them onto it and starting
// their vanish_timer) or fizzles; counts a caught player down and closes the rift once they're back
void update_rifts(void) {
    ticks++;
    for (int k = 0; k < 2; k++) {
        Rift *r = &rifts[k];
        if (r->state == RIFT_OFF) continue;
        r->timer++;
        Paddle *p = r->target;

        if (r->state == RIFT_OPENING) {
            // Sparks rising off the rim as a warning
            float a = frand(0, 2 * (float)M_PI);
            spawn_particle(r->x + SDL_cosf(a) * RIFT_RX, r->y + SDL_sinf(a) * RIFT_RY, 0, -frand(1.5f, 4),
                           frand(4, 8), 190, (int)frand(50, 140), 255, (int)frand(12, 22));
            if (r->timer < RIFT_WARNING) continue;
            int fx, fy;
            player_point(p, r->target_faces_right, POINT_FEET, &fx, &fy);
            if (abs(fy - r->y) <= RIFT_CATCH && p->vanish_timer == 0) {
                p->y += r->y - fy;                      // Stand right on it while sinking
                clamp_to_screen(p);
                p->vanish_timer = VANISH_TOTAL;
                p->stun_timer = 0;
                p->motion_state = 0;
                p->throw_timer = 0;
                r->state = RIFT_HOLDING;
                play_sound(&snd_rift_snap);
                burst(r, 30, 6);
            } else {
                r->state = RIFT_CLOSING;                // Dodged
                burst(r, 10, 3);
            }
            r->timer = 0;
        } else if (r->state == RIFT_HOLDING) {
            if (--p->vanish_timer <= 0) {
                p->vanish_timer = 0;
                r->state = RIFT_CLOSING;
                r->timer = 0;
            } else if (p->vanish_timer == RIFT_SINK) {
                burst(r, 16, 4);                        // Coming back out
            }
        } else if (r->timer >= RIFT_CLOSE) {
            r->state = RIFT_OFF;
        }
    }
}

// Match over: rifts still warning fizzle, and a vanished player starts rising right away
void end_rifts(void) {
    for (int k = 0; k < 2; k++) {
        Rift *r = &rifts[k];
        if (r->state == RIFT_OPENING) {
            r->state = RIFT_CLOSING;
            r->timer = 0;
        } else if (r->state == RIFT_HOLDING && r->target->vanish_timer > RIFT_SINK) {
            r->target->vanish_timer = RIFT_SINK;       // Start rising now
        }
    }
}

// New match: close both rifts
void reset_rifts(void) {
    rifts[0].state = rifts[1].state = RIFT_OFF;
}

// How far a caught player has sunk: 0 to 1 while sinking, 1 while gone, back down to 0 while rising
float vanish_depth(const Paddle *p) {
    if (p->vanish_timer <= 0) return 0.0f;
    int elapsed = VANISH_TOTAL - p->vanish_timer;
    if (elapsed < RIFT_SINK) return (float)elapsed / RIFT_SINK;
    if (p->vanish_timer < RIFT_SINK) return (float)p->vanish_timer / RIFT_SINK;
    return 1.0f;
}

// Rift scale for its state: growing open, full, or shrinking shut
static float rift_size(const Rift *r) {
    if (r->state == RIFT_OPENING) return r->timer < RIFT_GROW ? (float)r->timer / RIFT_GROW : 1.0f;
    if (r->state == RIFT_CLOSING) return 1.0f - (float)r->timer / RIFT_CLOSE;
    return 1.0f;
}

// Open rifts as glowing purple ovals with a dark void and swirling specks; pulses faster and flashes white
// just before it snaps
void draw_rifts(SDL_Renderer *renderer) {
    for (int k = 0; k < 2; k++) {
        const Rift *r = &rifts[k];
        if (r->state == RIFT_OFF) continue;
        float s = rift_size(r), rx = RIFT_RX * s, ry = RIFT_RY * s;
        // The pulse speeds up toward the snap
        float pulse = r->state == RIFT_OPENING ? SDL_sinf(0.3f * r->timer + 0.012f * r->timer * r->timer)
                                               : SDL_sinf(ticks * 0.15f);
        bool flash = r->state == RIFT_OPENING && r->timer > RIFT_WARNING - 8 && r->timer % 4 < 2;

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 170, 40, 255, (Uint8)(60 + 35 * pulse));
        fill_pixel_oval(renderer, r->x, r->y, rx * 1.35f, ry * 1.6f, PIXEL);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        if (flash) SDL_SetRenderDrawColor(renderer, 255, 235, 255, 255);
        else SDL_SetRenderDrawColor(renderer, 200, (Uint8)(90 + 50 * pulse), 255, 255);
        fill_pixel_oval(renderer, r->x, r->y, rx, ry, PIXEL);
        SDL_SetRenderDrawColor(renderer, 90, 10, 150, 255);
        fill_pixel_oval(renderer, r->x, r->y, rx * 0.82f, ry * 0.75f, PIXEL);
        SDL_SetRenderDrawColor(renderer, 10, 0, 22, 255);
        fill_pixel_oval(renderer, r->x, r->y, rx * 0.6f, ry * 0.5f, PIXEL);

        // Specks swirling into the void
        SDL_SetRenderDrawColor(renderer, 255, 190, 255, 255);
        for (int i = 0; i < 6; i++) {
            float a = ticks * 0.14f + i * (2 * (float)M_PI / 6), d = 0.45f + 0.25f * ((ticks / 3 + i * 5) % 8) / 8.0f;
            int px = r->x + (int)(SDL_cosf(a) * rx * d) / PIXEL * PIXEL;
            int py = r->y + (int)(SDL_sinf(a) * ry * d) / PIXEL * PIXEL;
            SDL_Rect speck = { px - PIXEL / 2, py - PIXEL / 2, PIXEL, PIXEL };
            SDL_RenderFillRect(renderer, &speck);
        }
    }
}
