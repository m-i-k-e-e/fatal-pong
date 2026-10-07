#include "fatality.h"
#include "audio.h"
#include <stdlib.h>

// Timeline: the racket spins from the winner's hand to the loser's face; on impact the head bursts into
// blood and chunks, the screen shakes and the neck fountains for a while. Blood falls under gravity and
// leaves stains where it ends up, kept until the next match.
#define RACKET_SPEED        42.0f       // Pixels per frame
#define RACKET_SPIN         38.0        // Degrees per frame
#define BLOOD_MAX           900
#define STAIN_MAX           1200
#define GRAVITY             0.45f
#define BURST_BLOOD         420
#define BURST_CHUNKS        36
#define FOUNTAIN_FRAMES     150
#define SHAKE_FRAMES        28
#define VOICE_DELAY         45          // "Fatality!" after the splat

typedef struct {
    float x, y, vx, vy;
    float size;
    SDL_Color color;
    int life;
    bool stains;                        // Leaves a mark where it dies
} Drop;

typedef struct {
    Sint16 x, y;
    Uint8 w, h;
    SDL_Color color;
} Stain;

static Drop drops[BLOOD_MAX];
static Stain stains[STAIN_MAX];
static int stain_count;

static Paddle *loser;
static bool loser_right;                // Loser faces right
static PlayerLook thrower_look, victim_look;
static float racket_x, racket_y, target_x, target_y;
static double racket_angle;
static bool started;
static int frame;                       // Since the throw
static int impact_frame = -1;           // Frame the head exploded, -1 before

// Random float in [lo, hi]
static float frand(float lo, float hi) {
    return lo + (hi - lo) * rand01();
}

// Start a blood drop or chunk in a free slot (dropped when all BLOOD_MAX are in flight); `stains_` makes it
// leave a mark where it dies
static void spawn(float x, float y, float vx, float vy, float size, SDL_Color color, int life, bool stains_) {
    for (int i = 0; i < BLOOD_MAX; i++) {
        if (drops[i].life > 0) continue;
        drops[i] = (Drop){ x, y, vx, vy, size, color, life, stains_ };
        return;
    }
}

// A random shade of blood red
static SDL_Color blood_color(void) {
    Uint8 r = (Uint8)frand(110, 210);
    return (SDL_Color){ r, (Uint8)(r / 14), (Uint8)(r / 16), 255 };
}

// Leave a dark mark on the grass where drop `d` died, if it's on screen and there's room
static void add_stain(const Drop *d) {
    if (stain_count >= STAIN_MAX || d->x < 0 || d->y < 0 || d->x >= SCREEN_WIDTH || d->y >= SCREEN_HEIGHT) return;
    int s = (int)(d->size * frand(0.8f, 1.6f)) + 1;
    stains[stain_count++] = (Stain){ (Sint16)d->x, (Sint16)d->y, (Uint8)(s + rand() % 3), (Uint8)s,
                                     { (Uint8)(d->color.r * 0.6f), 0, 0, 255 } };
}

void fatality_start(Paddle *winner, PlayerLook winner_look, bool winner_faces_right,
                    Paddle *loser_, PlayerLook loser_look) {
    loser = loser_;
    loser_right = !winner_faces_right;
    thrower_look = winner_look;
    victim_look = loser_look;
    int hx, hy, fx, fy;
    player_point(winner, winner_faces_right, POINT_HAND, &hx, &hy);
    player_point(loser, loser_right, POINT_FACE, &fx, &fy);
    racket_x = (float)hx; racket_y = (float)hy;
    target_x = (float)fx; target_y = (float)fy;
    racket_angle = 0.0;
    winner->unarmed = true;
    started = true;
    frame = 0;
    impact_frame = -1;
    play_sound(&snd_boomerang);             // The racket spinning through the air
}

// The racket hits: the loser loses their head in a burst of blood and chunks (skin, hair, Nadal's headband); plays the splat and the scream
static void explode(void) {
    loser->headless = true;
    impact_frame = frame;
    play_sound(&snd_splat);
    play_sound(&snd_scream);
    for (int i = 0; i < BURST_BLOOD; i++) {
        float a = frand(0, 2 * (float)M_PI), speed = frand(2, 19);
        spawn(target_x + frand(-12, 12), target_y + frand(-14, 14), SDL_cosf(a) * speed, SDL_sinf(a) * speed - 6,
              frand(3, 9), blood_color(), (int)frand(30, 75), true);
    }
    // Chunks of the head: skin, hair and (for Nadal) bits of headband
    SDL_Color skin = { 230, 174, 134, 255 };
    SDL_Color hair = victim_look == PLAYER_AGASSI ? (SDL_Color){ 228, 192, 100, 255 } : (SDL_Color){ 74, 46, 28, 255 };
    SDL_Color band = { 210, 40, 40, 255 };
    for (int i = 0; i < BURST_CHUNKS; i++) {
        float a = frand(0, 2 * (float)M_PI), speed = frand(4, 15);
        SDL_Color c = i % 3 == 0 ? skin : i % 3 == 1 ? hair : (victim_look == PLAYER_NADAL && i % 2 ? band : blood_color());
        spawn(target_x, target_y, SDL_cosf(a) * speed, SDL_sinf(a) * speed - 8, frand(7, 13), c, (int)frand(40, 80), true);
    }
}

// Every frame after the match: moves the blood, flies the racket until it reaches the face, then runs the
// neck fountain and plays "Fatality!"
void fatality_update(void) {
    // Drops fly and fall; the ones that stain leave a mark where they stop
    for (int i = 0; i < BLOOD_MAX; i++) {
        Drop *d = &drops[i];
        if (d->life <= 0) continue;
        d->x += d->vx;
        d->y += d->vy;
        d->vy += GRAVITY;
        d->vx *= 0.985f;
        if (--d->life == 0 && d->stains) add_stain(d);
    }
    if (!fatality_active()) return;
    frame++;

    if (impact_frame < 0) {
        float dx = target_x - racket_x, dy = target_y - racket_y, dist = SDL_sqrtf(dx * dx + dy * dy);
        racket_angle += RACKET_SPIN;
        if (dist <= RACKET_SPEED) {
            explode();
        } else {
            racket_x += dx / dist * RACKET_SPEED;
            racket_y += dy / dist * RACKET_SPEED;
        }
        return;
    }

    int since = frame - impact_frame;
    if (since == VOICE_DELAY) play_sound(&snd_fatality);
    if (since < FOUNTAIN_FRAMES) {          // Fountain from the neck, weakening
        int nx, ny;
        player_point(loser, loser_right, POINT_NECK, &nx, &ny);
        float power = 1.0f - (float)since / FOUNTAIN_FRAMES;
        for (int k = 0; k < 4; k++)
            spawn(nx + frand(-4, 4), (float)ny, frand(-3, 3), -frand(6, 15) * power - 2, frand(3, 7),
                  blood_color(), (int)frand(25, 60), true);
    }
}

// True from the throw until FATALITY_LENGTH frames after the impact
bool fatality_active(void) {
    return started && (impact_frame < 0 || frame - impact_frame < FATALITY_LENGTH);
}

// Frames since the head exploded, -1 before (and when no fatality is running)
int fatality_since_impact(void) {
    return impact_frame < 0 ? -1 : frame - impact_frame;
}

// Screen offset for the impact shake, weakening over SHAKE_FRAMES; (0, 0) otherwise
void fatality_shake(int *dx, int *dy) {
    int since = fatality_since_impact();
    *dx = *dy = 0;
    if (since < 0 || since >= SHAKE_FRAMES) return;
    int strength = 18 * (SHAKE_FRAMES - since) / SHAKE_FRAMES;
    *dx = rand() % (2 * strength + 1) - strength;
    *dy = rand() % (2 * strength + 1) - strength;
}

// New match: no fatality, no blood in the air, no stains
void fatality_reset(void) {
    for (int i = 0; i < BLOOD_MAX; i++) drops[i].life = 0;
    stain_count = 0;
    started = false;
    frame = 0;
    impact_frame = -1;
    loser = NULL;
}

// Blood stains on the grass, drawn under the players
void draw_fatality_stains(SDL_Renderer *renderer) {
    for (int i = 0; i < stain_count; i++) {
        const Stain *s = &stains[i];
        SDL_SetRenderDrawColor(renderer, s->color.r, s->color.g, s->color.b, 255);
        SDL_Rect r = { s->x, s->y, s->w, s->h };
        SDL_RenderFillRect(renderer, &r);
    }
}

// The racket in flight and the flying blood, drawn over the players
void draw_fatality_front(SDL_Renderer *renderer) {
    if (started && impact_frame < 0)
        draw_racket(renderer, thrower_look, (int)racket_x, (int)racket_y, racket_angle);
    for (int i = 0; i < BLOOD_MAX; i++) {
        const Drop *d = &drops[i];
        if (d->life <= 0) continue;
        SDL_SetRenderDrawColor(renderer, d->color.r, d->color.g, d->color.b, 255);
        SDL_Rect r = { (int)d->x, (int)d->y, (int)d->size, (int)d->size };
        SDL_RenderFillRect(renderer, &r);
    }
}
