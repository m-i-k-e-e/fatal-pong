#include "gameplay/calamity.h"
#include "core/audio.h"
#include "gameplay/ball.h"
#include "core/draw.h"
#include "render/hud.h"
#include "core/particles.h"
#include <stdlib.h>

// A calamity puts obstacles on the court for CALAMITY_DURATION: the first few come quickly, then one is added
// now and then (an old one going away when there's no room or at random), with CALAMITY_MIN..CALAMITY_MAX
// standing until it ends. A ball that runs into one is either knocked off at an angle or bounces off it.
//   The mole digs molehills and pops out of each new one.
//   The earthquake shakes the ground every time, and each tremor opens a crack. A ball that runs into a crack
//   drops into it and comes out of another one a moment later, still heading the same way (knocked off when
//   there's no other crack).
//   The frog rain drops frogs from the sky: the big ones land and sit in the way (hopping off when replaced),
//   while smaller ones keep raining down all over the court, land and leap away without blocking anything.
//   A big frog doesn't knock the ball: it swallows it, either on contact or by catching it with its tongue
//   from a distance, keeps it half a second, then spits it out in a random direction.
#define FIRST_EVENT         60          // Frames after the arrival, while the title is still up
#define EVENT_GAP           50          // Between the first obstacles
#define MOVE_GAP_MIN        120         // Between later ones
#define MOVE_GAP_MAX        240
#define OBSTACLE_SPACING    230         // Minimum distance between obstacles
#define HIT_COOLDOWN        25          // Frames an obstacle ignores the ball after knocking it, so it can't re-hit
#define MIN_VX_RATIO        0.4f        // Keep the ball travelling across the court after a knock
#define PIXEL               4

#define HILL_RISE           70          // Frames a new hill takes: mound growing, mole popping out and back
#define HILL_GROW           24
#define HILL_SOLID          12          // Frames into the rise before the ball can hit it
#define HILL_FALL           20
#define HILL_RX             46          // Ground footprint, also the collision oval
#define HILL_RY             24
#define MOLE_SCALE_FIELD    3
#define MOLE_SCALE_TITLE    10

#define CRACK_POINTS        7           // Jagged line, plus a short branch
#define BRANCH_POINTS       3
#define CRACK_OPEN          16          // Frames for a crack to split open from its middle
#define CRACK_SOLID         6
#define CRACK_CLOSE         24
#define CRACK_HALF_WIDTH    7.0f        // At the middle; tapers toward the ends
#define CRACK_REACH         6.0f        // Extra collision margin beyond the fissure
#define TUNNEL_FRAMES       20          // Frames the ball spends underground between two cracks
#define TREMOR_FRAMES       30
#define TREMOR_SHAKE        14          // Pixels at the start of a tremor
#define ARRIVAL_TREMOR      50          // The first, longer quake under the title

#define FROG_FALL           36          // Frames falling from the sky
#define FROG_SQUASH         10          // Landing squash
#define FROG_RISE           (FROG_FALL + FROG_SQUASH)
#define FROG_LEAVE          28          // Hopping off when replaced
#define FROG_DROP           900.0f      // Height a frog falls from, in screen pixels
#define FROG_RADIUS         30          // Collision circle around its body
#define FROG_BODY_Y         26          // Body center above its feet
#define FROG_SCALE_FIELD    4
#define FROG_SCALE_RAIN     3
#define FROG_SCALE_TITLE    10
#define FROG_MOUTH_Y        28          // Mouth above its feet
#define TONGUE_RANGE        280.0f      // How far a frog's tongue reaches for the ball
#define TONGUE_OUT          5           // Frames to shoot the tongue out to the ball
#define TONGUE_IN           9           // Frames to reel it back in
#define SWALLOW_FRAMES      30          // Half a second with the ball in its mouth
#define FROG_REST           90          // Frames after spitting before that frog catches again
#define CATCH_BLOCK         20          // Frames after a release before any frog can catch
#define RAIN_MAX            16          // Frogs raining at once (decoration only)
#define RAIN_GAP_MIN        5
#define RAIN_GAP_MAX        14
#define RAIN_FALL           30
#define RAIN_SIT            12
#define RAIN_LEAP           30

// Mole popping out of its hole, paws on the rim. Left half only: the right half is its mirror.
#define MOLE_W              20
#define MOLE_H              16
#define MOLE_BODY_ROWS      12          // Without the dirt rim, for drawing on top of a molehill
static const char *const MOLE_HALF[MOLE_H] = {
    "......kkkk",
    "....kkdfff",
    "...kdfffff",
    "..kdffflll",
    "..kdffllll",
    ".kdfffelll",
    ".kdffffllp",
    ".kdfffflpP",
    ".kddfffflk",
    "kppkddffff",
    "kpppkdffff",
    "kpwpwkdfff",
    "nnnnnnnnnn",
    "mnmmnmmmmm",
    "mmMmmmMmmm",
    ".MMMMMMMMM",
};

// Palette for MOLE_HALF; anything else is transparent
static SDL_Color mole_color(char c) {
    switch (c) {
    case 'k': return (SDL_Color){ 34, 22, 18, 255 };
    case 'd': return (SDL_Color){ 58, 46, 42, 255 };
    case 'f': return (SDL_Color){ 84, 68, 60, 255 };
    case 'l': return (SDL_Color){ 112, 94, 82, 255 };
    case 'e': return (SDL_Color){ 8, 4, 4, 255 };
    case 'p': return (SDL_Color){ 236, 150, 160, 255 };
    case 'P': return (SDL_Color){ 196, 98, 112, 255 };
    case 'w': return (SDL_Color){ 245, 240, 230, 255 };
    case 'n': return (SDL_Color){ 150, 108, 64, 255 };
    case 'm': return (SDL_Color){ 112, 76, 42, 255 };
    case 'M': return (SDL_Color){ 74, 50, 26, 255 };
    default:  return (SDL_Color){ 0, 0, 0, 0 };
    }
}

// Frog sitting, seen from the front: eyes on top, feet splayed out. Bright lime so it stands out on the grass.
// Left half only, mirrored like the mole.
#define FROG_W              20
#define FROG_H              13
static const char *const FROG_HALF[FROG_H] = {
    "...kkk....",
    "..kwwwk...",
    "..kweek...",
    ".kgwwwgkkk",
    ".kglgggggg",
    ".kgggggggg",
    ".kggkkkkkk",
    ".kGgyyyyyy",
    "kkGGgyyyyy",
    "kgkGGgyyyy",
    "kglkGGgggg",
    "kgglkkGGgg",
    "kkkkk.kkkk",
};

// Palette for FROG_HALF; anything else is transparent
static SDL_Color frog_color(char c) {
    switch (c) {
    case 'k': return (SDL_Color){ 16, 36, 6, 255 };
    case 'g': return (SDL_Color){ 150, 226, 40, 255 };
    case 'G': return (SDL_Color){ 86, 160, 24, 255 };
    case 'l': return (SDL_Color){ 214, 250, 120, 255 };
    case 'w': return (SDL_Color){ 250, 250, 240, 255 };
    case 'e': return (SDL_Color){ 12, 12, 12, 255 };
    case 'y': return (SDL_Color){ 248, 232, 120, 255 };
    default:  return (SDL_Color){ 0, 0, 0, 0 };
    }
}

typedef enum { CALAMITY_NONE, CALAMITY_MOLE, CALAMITY_EARTHQUAKE, CALAMITY_FROGS } CalamityType;
typedef enum { OB_OFF, OB_RISING, OB_UP, OB_FALLING } ObstacleState;
typedef enum { FROG_IDLE, FROG_TONGUE_OUT, FROG_TONGUE_IN, FROG_SWALLOWED } FrogAction;

typedef struct {
    float x[CRACK_POINTS], y[CRACK_POINTS];
    float bx[BRANCH_POINTS], by[BRANCH_POINTS];     // Branch, starting on the main line
} Crack;

typedef struct {
    ObstacleState state;
    CalamityType type;                  // Molehill or crack
    float x, y;                         // Center
    int timer;                          // Frames in the current state
    int cooldown;
    int age;                            // Placement order, so the oldest goes first
    Crack crack;
    float dir;                          // Frog: which way it hops off
    FrogAction action;                  // Frog: catching or holding the ball
    int action_timer;
    float tip_x, tip_y;                 // Tongue tip
    float reel_x, reel_y;               // Where the tongue started reeling in from
    bool has_ball;                      // The tongue caught it
} Obstacle;

typedef struct {
    bool active;
    float x, y;                         // Where it lands
    int timer;
    float dir;                          // Which way it leaps off
} RainFrog;

static SDL_Texture *mole_tex, *frog_tex;
static Obstacle *holder;                // The frog catching or holding the ball, one at a time
static Ball *held_ball;
static int catch_block;
static Obstacle *tunnel_exit;          // The crack the ball will come out of while it's underground
static int tunnel_timer;
static RainFrog rain[RAIN_MAX];
static int rain_timer;
static Obstacle obstacles[CALAMITY_MAX];
static CalamityType active;
static int calamity_timer;              // Frames since the calamity arrived
static int title_timer = -1;            // Frames the title has been up, -1 when it's gone
static CalamityType title_type;
static int next_event;
static int placed;
static int tremor_timer, tremor_length;
static int checked_hits;                // paddle_hits already rolled for
int calamity_chance = CALAMITY_CHANCE_DEFAULT;

// Random float in [lo, hi]
static float frand(float lo, float hi) {
    return lo + (hi - lo) * rand01();
}

// Texture from mirrored half-width character art
static SDL_Texture *build_art(SDL_Renderer *renderer, const char *const *half, int w, int h, SDL_Color (*color)(char)) {
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surf) return NULL;
    for (int y = 0; y < h; y++) {
        Uint32 *row = (Uint32 *)((Uint8 *)surf->pixels + y * surf->pitch);
        for (int x = 0; x < w; x++) {
            SDL_Color col = color(half[y][x < w / 2 ? x : w - 1 - x]);
            row[x] = SDL_MapRGBA(surf->format, col.r, col.g, col.b, col.a);
        }
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    if (tex) SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    return tex;
}

// Build the mole and frog textures from their character art (once, at startup)
void init_calamities(SDL_Renderer *renderer) {
    mole_tex = build_art(renderer, MOLE_HALF, MOLE_W, MOLE_H, mole_color);
    frog_tex = build_art(renderer, FROG_HALF, FROG_W, FROG_H, frog_color);
}

// Destroy the textures made by init_calamities()
void free_calamities(void) {
    if (mole_tex) SDL_DestroyTexture(mole_tex);
    if (frog_tex) SDL_DestroyTexture(frog_tex);
    mole_tex = frog_tex = NULL;
}

// New match: clear every obstacle and raining frog, free the ball if a frog holds it, end any calamity,
// title or tremor, and restart the hit count for the next roll
void reset_calamities(void) {
    for (int i = 0; i < CALAMITY_MAX; i++) obstacles[i].state = OB_OFF;
    for (int i = 0; i < RAIN_MAX; i++) rain[i].active = false;
    if (held_ball) held_ball->held = held_ball->hidden = false;
    holder = NULL;
    held_ball = NULL;
    tunnel_exit = NULL;
    catch_block = 0;
    active = CALAMITY_NONE;
    title_timer = -1;
    tremor_timer = 0;
    checked_hits = 0;
}

// Shake the screen for `frames` frames, fading out, with the quake rumble
static void tremor(int frames) {
    tremor_timer = tremor_length = frames;
    play_sound(&snd_quake);
}

// Begin a calamity of `type`: shows its title and schedules its first obstacle
static void start(CalamityType type) {
    active = title_type = type;
    calamity_timer = 0;
    title_timer = 0;
    next_event = FIRST_EVENT;
    placed = 0;
}

// Start the mole, with its underground rumble
void start_mole(void) {
    start(CALAMITY_MOLE);
    play_sound(&snd_mole);
}

// Start the earthquake, with a long first tremor under the title
void start_earthquake(void) {
    start(CALAMITY_EARTHQUAKE);
    tremor(ARRIVAL_TREMOR);
}

// Start the frog rain, with a croak
void start_frog_rain(void) {
    start(CALAMITY_FROGS);
    rain_timer = 0;
    play_sound(&snd_ribbit);
}

// Obstacles currently appearing or standing (not the ones going away)
static int standing_obstacles(void) {
    int n = 0;
    for (int i = 0; i < CALAMITY_MAX; i++)
        if (obstacles[i].state == OB_RISING || obstacles[i].state == OB_UP) n++;
    return n;
}

// Grass and mud flicked up by a landing frog
static void splash(float x, float y, int count) {
    for (int i = 0; i < count; i++) {
        bool mud = rand() % 3 == 0;
        spawn_particle(x + frand(-14, 14), y + frand(-4, 4), frand(-3, 3), -frand(1, 4), frand(3, 7),
                       mud ? 110 : 70, mud ? 80 : (int)frand(140, 190), mud ? 50 : 50, (int)frand(10, 18));
    }
}

// `count` brown dirt particles thrown up from (x, y), up to `speed` pixels per frame
static void dirt_burst(float x, float y, int count, float speed) {
    for (int i = 0; i < count; i++) {
        int shade = (int)frand(70, 150);
        spawn_particle(x + frand(-10, 10), y + frand(-6, 6), frand(-speed, speed), -frand(1, speed * 1.4f),
                       frand(4, 9), shade, shade * 7 / 10, shade * 4 / 10, (int)frand(12, 24));
    }
}

// A jagged line through (x, y) at angle `a` (radians, 0 = horizontal), with a short branch
static void make_crack(Crack *c, float x, float y, float a, float length, float jag) {
    float dx = SDL_cosf(a), dy = SDL_sinf(a);
    for (int i = 0; i < CRACK_POINTS; i++) {
        float t = -0.5f + (float)i / (CRACK_POINTS - 1), side = i == 0 || i == CRACK_POINTS - 1 ? 0 : frand(-jag, jag);
        c->x[i] = x + dx * t * length - dy * side;
        c->y[i] = y + dy * t * length + dx * side;
    }
    int from = rand() % 2 ? 2 : 4;
    float b = a + (rand() % 2 ? 0.8f : -0.8f);
    for (int i = 0; i < BRANCH_POINTS; i++) {
        float t = i * length * 0.14f;
        c->bx[i] = c->x[from] + SDL_cosf(b) * t + (i ? frand(-5, 5) : 0);
        c->by[i] = c->y[from] + SDL_sinf(b) * t + (i ? frand(-5, 5) : 0);
    }
}

// A free spot in the middle of the court, clear of the other obstacles, the ball and the serve point
static void place(CalamityType type, const Ball *ball) {
    Obstacle *slot = NULL;
    for (int i = 0; i < CALAMITY_MAX && !slot; i++)
        if (obstacles[i].state == OB_OFF) slot = &obstacles[i];
    if (!slot) return;
    for (int attempt = 0; attempt < 40; attempt++) {
        float x = frand(380, SCREEN_WIDTH - 380), y = frand(200, SCREEN_HEIGHT - 140);
        float bx = ball->x + ball->size / 2 - x, by = ball->y + ball->size / 2 - y;
        if (bx * bx + by * by < 220 * 220) continue;
        if (SDL_fabsf(x - SCREEN_WIDTH / 2) < 130 && SDL_fabsf(y - SCREEN_HEIGHT / 2) < 130) continue;
        bool crowded = false;
        for (int i = 0; i < CALAMITY_MAX; i++) {
            const Obstacle *o = &obstacles[i];
            if (o->state == OB_OFF) continue;
            float dx = o->x - x, dy = o->y - y;
            if (dx * dx + dy * dy < OBSTACLE_SPACING * OBSTACLE_SPACING) crowded = true;
        }
        if (crowded) continue;
        *slot = (Obstacle){ OB_RISING, type, x, y, 0, 0, placed++, { { 0 } }, rand() % 2 ? 1.0f : -1.0f,
                            FROG_IDLE, 0, 0, 0, 0, 0, false };
        if (type == CALAMITY_EARTHQUAKE)              // Slanted mostly across the ball's path
            make_crack(&slot->crack, x, y, (float)M_PI / 2 + frand(-1.2f, 1.2f), frand(170, 240), 14);
        return;
    }
}

// Send the oldest standing obstacle away (never the frog busy with the ball or the crack it's tunnelling to),
// to make room
static void remove_oldest(void) {
    Obstacle *oldest = NULL;
    for (int i = 0; i < CALAMITY_MAX; i++)
        if (obstacles[i].state == OB_UP && &obstacles[i] != holder && &obstacles[i] != tunnel_exit && (!oldest || obstacles[i].age < oldest->age))
            oldest = &obstacles[i];
    if (!oldest) return;
    oldest->state = OB_FALLING;
    oldest->timer = 0;
}

// An earthquake places its cracks with a tremor each
static void event(const Ball *ball) {
    if (active == CALAMITY_EARTHQUAKE) tremor(TREMOR_FRAMES);
    place(active, ball);
}

// After a direction change, keep the ball travelling across the court rather than almost straight up or down
static void keep_crossing(Ball *ball, float speed) {
    if (SDL_fabsf(ball->vx) < MIN_VX_RATIO * speed) {
        ball->vx = (ball->vx < 0 ? -1 : 1) * MIN_VX_RATIO * speed;
        ball->vy = (ball->vy < 0 ? -1 : 1) * SDL_sqrtf(speed * speed - ball->vx * ball->vx);
    }
}

// Knock the ball off an obstacle: either turned off at an angle, or bounced off along the normal (nx, ny)
static void knock(Ball *ball, Obstacle *o, float nx, float ny) {
    float speed = SDL_sqrtf(ball->vx * ball->vx + ball->vy * ball->vy);
    float len = SDL_sqrtf(nx * nx + ny * ny);
    if (len < 0.0001f) { nx = -ball->vx; ny = -ball->vy; len = speed; }
    nx /= len; ny /= len;
    float along = ball->vx * nx + ball->vy * ny;
    if (rand() % 2 && along < 0) {
        ball->vx -= 2 * along * nx;
        ball->vy -= 2 * along * ny;
    } else {
        float a = frand(25, 50) * (float)M_PI / 180 * (rand() % 2 ? 1 : -1);
        float c = SDL_cosf(a), s = SDL_sinf(a), vx = ball->vx;
        ball->vx = vx * c - ball->vy * s;
        ball->vy = vx * s + ball->vy * c;
    }
    keep_crossing(ball, speed);
    o->cooldown = HIT_COOLDOWN;
    if (o->type == CALAMITY_FROGS) {
        splash(ball->x + ball->size / 2, ball->y + ball->size / 2, 10);
        play_sound(&snd_ribbit);
    } else {
        dirt_burst(ball->x + ball->size / 2, ball->y + ball->size / 2, 14, 4);
        play_sound(&snd_thud);
    }
}

// Closest point to (px, py) on the crack's lines; returns the squared distance
static float crack_closest(const Crack *c, float px, float py, float *cx, float *cy) {
    float best = 1e12f;
    for (int line = 0; line < 2; line++) {
        const float *xs = line ? c->bx : c->x, *ys = line ? c->by : c->y;
        int n = line ? BRANCH_POINTS : CRACK_POINTS;
        for (int i = 0; i + 1 < n; i++) {
            float ax = xs[i], ay = ys[i], dx = xs[i + 1] - ax, dy = ys[i + 1] - ay;
            float t = SDL_clamp(((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy), 0.0f, 1.0f);
            float qx = ax + t * dx, qy = ay + t * dy, d = (px - qx) * (px - qx) + (py - qy) * (py - qy);
            if (d < best) { best = d; *cx = qx; *cy = qy; }
        }
    }
    return best;
}

// --- Frogs and the ball ---

// Screen position of a big frog's mouth, where its tongue starts and the ball goes in and out
static void frog_mouth(const Obstacle *o, float *x, float *y) {
    *x = o->x;
    *y = o->y - FROG_MOUTH_Y;
}

// Move the ball so its center is at (cx, cy)
static void put_ball(Ball *ball, float cx, float cy) {
    ball->x = cx - ball->size / 2;
    ball->y = cy - ball->size / 2;
}

// The frog takes the ball into its mouth: hidden and held there until spit() (SWALLOW_FRAMES later)
static void swallow(Obstacle *o, Ball *ball) {
    float mx, my;
    frog_mouth(o, &mx, &my);
    put_ball(ball, mx, my);
    ball->held = ball->hidden = true;
    holder = o;
    held_ball = ball;
    o->action = FROG_SWALLOWED;
    o->action_timer = 0;
    play_sound(&snd_ribbit);
}

// Out of the mouth in any direction at the speed it came in, clear of the frog, which rests a moment
static void spit(Obstacle *o) {
    Ball *ball = held_ball;
    float speed = SDL_max(SDL_sqrtf(ball->vx * ball->vx + ball->vy * ball->vy), ball_base_speed());
    float a = frand(0, 2 * (float)M_PI);
    ball->vx = SDL_cosf(a) * speed;
    ball->vy = SDL_sinf(a) * speed;
    keep_crossing(ball, speed);
    float mx, my, clear = FROG_RADIUS + ball->size / 2 + 6;
    frog_mouth(o, &mx, &my);
    put_ball(ball, mx + ball->vx / speed * clear, my + ball->vy / speed * clear);
    ball->held = ball->hidden = false;
    o->action = FROG_IDLE;
    o->has_ball = false;
    o->cooldown = FROG_REST;
    holder = NULL;
    held_ball = NULL;
    catch_block = CATCH_BLOCK;
    splash(mx, my, 8);
    play_sound(&snd_spit);
}

// Tongue: shot at a ball coming its way within range, sticks to it and reels it in to be swallowed
static void update_frog(Obstacle *o, Ball *ball) {
    float mx, my, bx = ball->x + ball->size / 2, by = ball->y + ball->size / 2;
    frog_mouth(o, &mx, &my);
    if (o->action == FROG_IDLE) {
        if (o->state != OB_UP || o->cooldown > 0 || holder || catch_block > 0 || ball->held) return;
        float dx = bx - mx, dy = by - my, d2 = dx * dx + dy * dy, near = FROG_RADIUS + ball->size;
        bool coming = ball->vx * -dx + ball->vy * -dy > 0;
        if (coming && d2 < TONGUE_RANGE * TONGUE_RANGE && d2 > near * near) {
            o->action = FROG_TONGUE_OUT;
            o->action_timer = 0;
            o->tip_x = mx;
            o->tip_y = my;
            holder = o;
            play_sound(&snd_tongue);
        }
    } else if (o->action == FROG_TONGUE_OUT) {
        float k = (float)++o->action_timer / TONGUE_OUT;               // Homes in on the moving ball
        o->tip_x = mx + (bx - mx) * k;
        o->tip_y = my + (by - my) * k;
        if (o->action_timer >= TONGUE_OUT) {
            float dx = bx - mx, dy = by - my, reach = TONGUE_RANGE * 1.2f;
            o->has_ball = !ball->held && dx * dx + dy * dy < reach * reach;
            if (o->has_ball) {
                ball->held = true;
                held_ball = ball;
            }
            o->action = FROG_TONGUE_IN;
            o->action_timer = 0;
            o->reel_x = o->tip_x;
            o->reel_y = o->tip_y;
        }
    } else if (o->action == FROG_TONGUE_IN) {
        float k = (float)++o->action_timer / TONGUE_IN;
        o->tip_x = o->reel_x + (mx - o->reel_x) * k;
        o->tip_y = o->reel_y + (my - o->reel_y) * k;
        if (o->has_ball) put_ball(ball, o->tip_x, o->tip_y);
        if (o->action_timer >= TONGUE_IN) {
            if (o->has_ball) {
                swallow(o, ball);
            } else {                                                    // Missed
                o->action = FROG_IDLE;
                o->cooldown = HIT_COOLDOWN;
                holder = NULL;
            }
        }
    } else if (++o->action_timer >= SWALLOW_FRAMES) {
        spit(o);
    }
}

// --- Cracks and the ball ---

// The ball drops into crack `o` at (cx, cy): hidden and held underground until emerge() (TUNNEL_FRAMES later)
// out of another open crack, picked at random. With no other crack open it's knocked off this one instead.
static void enter_crack(Obstacle *o, Ball *ball, float cx, float cy) {
    Obstacle *exits[CALAMITY_MAX];
    int n = 0;
    for (int i = 0; i < CALAMITY_MAX; i++)
        if (&obstacles[i] != o && obstacles[i].state == OB_UP && obstacles[i].type == CALAMITY_EARTHQUAKE)
            exits[n++] = &obstacles[i];
    float half = ball->size / 2;
    if (n == 0) {
        knock(ball, o, ball->x + half - cx, ball->y + half - cy);
        return;
    }
    put_ball(ball, cx, cy);
    ball->held = ball->hidden = true;
    held_ball = ball;
    tunnel_exit = exits[rand() % n];
    tunnel_timer = TUNNEL_FRAMES;
    o->cooldown = HIT_COOLDOWN;
    dirt_burst(cx, cy, 14, 4);
    play_sound(&snd_thud);
}

// The ball pops out of the middle of the exit crack, moving as it went in, clear of the fissure
static void emerge(void) {
    Ball *ball = held_ball;
    Obstacle *o = tunnel_exit;
    float speed = SDL_max(SDL_sqrtf(ball->vx * ball->vx + ball->vy * ball->vy), 0.0001f);
    float mx = o->crack.x[CRACK_POINTS / 2], my = o->crack.y[CRACK_POINTS / 2];
    float clear = CRACK_HALF_WIDTH + CRACK_REACH + ball->size / 2 + 4;
    put_ball(ball, mx + ball->vx / speed * clear, my + ball->vy / speed * clear);
    ball->held = ball->hidden = false;
    o->cooldown = HIT_COOLDOWN;
    tunnel_exit = NULL;
    held_ball = NULL;
    dirt_burst(mx, my, 14, 4);
    play_sound(&snd_thud);
}

// The frog holding the ball lets go, or the ball comes out of its crack, when the calamity ends
static void release_ball(void) {
    if (tunnel_exit) {
        emerge();
        return;
    }
    if (!holder) return;
    if (held_ball) spit(holder);
    else {
        holder->action = FROG_IDLE;
        holder = NULL;
    }
}

// Ball against one standing obstacle: knocks it off a molehill, a crack swallows it (see enter_crack()) and a big
// frog swallows it unless another frog already has the ball
static void collide(Obstacle *o, Ball *ball) {
    float half = ball->size / 2, bx = ball->x + half, by = ball->y + half;
    if (ball->held) return;
    if (o->type == CALAMITY_MOLE) {
        float dx = bx - o->x, dy = by - (o->y - 6);
        float ex = dx / (HILL_RX + half), ey = dy / (HILL_RY + half);
        if (ex * ex + ey * ey < 1.0f) knock(ball, o, ex / (HILL_RX + half), ey / (HILL_RY + half));
    } else if (o->type == CALAMITY_FROGS) {
        float dx = bx - o->x, dy = by - (o->y - FROG_BODY_Y), reach = FROG_RADIUS + half;
        if (dx * dx + dy * dy >= reach * reach || o->action != FROG_IDLE) return;
        if (!holder && catch_block == 0) swallow(o, ball);              // Gulp
        else knock(ball, o, dx, dy);                                    // Another frog is busy with it
    } else {
        float cx = bx, cy = by, reach = CRACK_HALF_WIDTH + CRACK_REACH + half;
        if (crack_closest(&o->crack, bx, by, &cx, &cy) < reach * reach) enter_crack(o, ball, cx, cy);
    }
}

// Every frame of play: rolls for a calamity on every CALAMITY_EVERY_HITS-th paddle hit, runs the active
// calamity's timeline (placing and removing obstacles, tremors, raining frogs), and checks the ball against
// the obstacles. May move, knock, hold or hide `ball`.
void update_calamities(Ball *ball) {
    if (paddle_hits >= checked_hits + CALAMITY_EVERY_HITS) {
        checked_hits += CALAMITY_EVERY_HITS;
        if (active == CALAMITY_NONE && rand() % 100 < calamity_chance) {
            int pick = rand() % 3;
            if (pick == 0) start_mole();
            else if (pick == 1) start_earthquake();
            else start_frog_rain();
        }
    }
    if (title_timer >= 0 && ++title_timer >= CALAMITY_TITLE_FRAMES) title_timer = -1;
    if (tremor_timer > 0) tremor_timer--;
    if (catch_block > 0) catch_block--;
    if (tunnel_exit && --tunnel_timer <= 0) emerge();

    if (active != CALAMITY_NONE) {
        calamity_timer++;
        if (calamity_timer >= CALAMITY_DURATION) {
            release_ball();
            for (int i = 0; i < CALAMITY_MAX; i++)
                if (obstacles[i].state == OB_RISING || obstacles[i].state == OB_UP) {
                    obstacles[i].state = OB_FALLING;
                    obstacles[i].timer = 0;
                }
            active = CALAMITY_NONE;
        } else if (calamity_timer >= next_event) {
            int n = standing_obstacles();
            if (n < CALAMITY_MIN) {
                event(ball);
                next_event = calamity_timer + EVENT_GAP;
            } else {
                if (n >= CALAMITY_MAX || rand() % 2) remove_oldest();
                event(ball);
                next_event = calamity_timer + MOVE_GAP_MIN + rand() % (MOVE_GAP_MAX - MOVE_GAP_MIN);
            }
        }
    }

    for (int i = 0; i < CALAMITY_MAX; i++) {
        Obstacle *o = &obstacles[i];
        if (o->state == OB_OFF) continue;
        o->timer++;
        bool hill = o->type == CALAMITY_MOLE, frog = o->type == CALAMITY_FROGS;
        int rise = hill ? HILL_RISE : frog ? FROG_RISE : CRACK_OPEN;
        int gone = hill ? HILL_FALL : frog ? FROG_LEAVE : CRACK_CLOSE;
        int solid_at = hill ? HILL_SOLID : frog ? FROG_FALL : CRACK_SOLID;
        if (o->state == OB_RISING) {
            if (hill && o->timer < HILL_GROW && o->timer % 2 == 0) dirt_burst(o->x, o->y - 10, 2, 3);
            if (o->type == CALAMITY_EARTHQUAKE && o->timer < CRACK_OPEN && o->timer % 2 == 0) {  // Dust along the split
                int k = rand() % CRACK_POINTS;
                dirt_burst(o->crack.x[k], o->crack.y[k], 2, 2.5f);
            }
            if (frog && o->timer == FROG_FALL) {                                    // Landed
                splash(o->x, o->y, 16);
                play_sound(&snd_ribbit);
            }
            if (o->timer >= rise) o->state = OB_UP;
        } else if (o->state == OB_FALLING && o->timer >= gone) {
            o->state = OB_OFF;
            continue;
        }

        if (frog && o->state != OB_FALLING) update_frog(o, ball);
        if (o->cooldown > 0) { o->cooldown--; continue; }
        bool solid = o->state == OB_UP || (o->state == OB_RISING && o->timer >= solid_at);
        if (solid) collide(o, ball);
    }

    // Frogs raining down for show: they land anywhere, sit a moment and leap off
    if (active == CALAMITY_FROGS && --rain_timer <= 0) {
        for (int i = 0; i < RAIN_MAX; i++) {
            if (rain[i].active) continue;
            rain[i] = (RainFrog){ true, frand(80, SCREEN_WIDTH - 80), frand(160, SCREEN_HEIGHT - 60), 0,
                                  rand() % 2 ? 1.0f : -1.0f };
            break;
        }
        rain_timer = RAIN_GAP_MIN + rand() % (RAIN_GAP_MAX - RAIN_GAP_MIN);
    }
    for (int i = 0; i < RAIN_MAX; i++) {
        RainFrog *f = &rain[i];
        if (!f->active) continue;
        if (++f->timer == RAIN_FALL) splash(f->x, f->y, 6);
        if (f->timer >= RAIN_FALL + RAIN_SIT + RAIN_LEAP) f->active = false;
    }
}

// Screen offset for the current tremor, weakening as it ends; (0, 0) when the ground is still
void calamity_shake(int *dx, int *dy) {
    *dx = *dy = 0;
    if (tremor_timer <= 0) return;
    int strength = TREMOR_SHAKE * tremor_timer / tremor_length + 1;
    *dx = rand() % (2 * strength + 1) - strength;
    *dy = rand() % (2 * strength + 1) - strength;
}

// --- Drawing ---

// How far the mole is out of a rising hill, 0..1
static float mole_pop(const Obstacle *o) {
    if (o->state != OB_RISING) return 0.0f;
    int t = o->timer;
    if (t < 20) return 0.0f;
    if (t < 28) return (t - 20) / 8.0f;
    if (t < 56) return 1.0f;
    if (t < 64) return (64 - t) / 8.0f;
    return 0.0f;
}

// A molehill growing, standing or caving in, with its shadow, clods and the mole popping out of a new one
static void draw_molehill(SDL_Renderer *renderer, const Obstacle *o) {
    float g = o->state == OB_RISING ? SDL_min(1.0f, (float)o->timer / HILL_GROW)
            : o->state == OB_FALLING ? 1.0f - (float)o->timer / HILL_FALL : 1.0f;
    int x = (int)o->x, y = (int)o->y;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 15, 30, 10, 90);                   // Shadow, light from the upper left
    fill_pixel_oval(renderer, x + 8, y + 4, 52 * g, 20 * g, PIXEL);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 74, 50, 26, 255);
    fill_pixel_oval(renderer, x, y, HILL_RX * g, HILL_RY * g, PIXEL);
    SDL_SetRenderDrawColor(renderer, 112, 76, 42, 255);
    fill_pixel_oval(renderer, x - 2, y - (int)(8 * g), 38 * g, 18 * g, PIXEL);
    SDL_SetRenderDrawColor(renderer, 150, 108, 64, 255);
    fill_pixel_oval(renderer, x - 4, y - (int)(14 * g), 26 * g, 13 * g, PIXEL);
    SDL_SetRenderDrawColor(renderer, 184, 142, 94, 255);
    fill_pixel_oval(renderer, x - 8, y - (int)(18 * g), 12 * g, 6 * g, PIXEL);

    // Clods on the slopes, fixed per hill
    SDL_SetRenderDrawColor(renderer, 60, 40, 20, 255);
    for (int k = 0; k < 4; k++) {
        int seed = o->age * 7 + k * 13;
        SDL_Rect clod = { x + (int)(((seed * 37) % 60 - 30) * g) / PIXEL * PIXEL,
                          y + (int)(((seed * 23) % 20 - 8) * g) / PIXEL * PIXEL, PIXEL * 2, PIXEL };
        SDL_RenderFillRect(renderer, &clod);
    }

    float pop = mole_pop(o);
    if (pop > 0 && mole_tex) {
        int hole = y - 14;
        SDL_SetRenderDrawColor(renderer, 34, 22, 18, 255);
        fill_pixel_oval(renderer, x, hole, 18, 6, PIXEL);
        int w = MOLE_W * MOLE_SCALE_FIELD, hgt = MOLE_BODY_ROWS * MOLE_SCALE_FIELD;
        SDL_Rect clip = { 0, 0, SCREEN_WIDTH, hole + 4 };
        SDL_RenderSetClipRect(renderer, &clip);
        SDL_Rect src = { 0, 0, MOLE_W, MOLE_BODY_ROWS };
        SDL_Rect dst = { x - w / 2, hole + 4 - (int)(hgt * pop), w, hgt };
        SDL_RenderCopy(renderer, mole_tex, &src, &dst);
        SDL_RenderSetClipRect(renderer, NULL);
    }
}

// One line of a crack in PIXEL blocks: `open` is how much of the whole crack shows, spreading from the
// middle (s = 0) to the ends (s = +-0.5); the fissure tapers toward the ends. Pass 0 draws the broken soil
// around it, pass 1 the dark gap.
static void draw_crack_line(SDL_Renderer *renderer, const float *xs, const float *ys, int n, float s0, float s1,
                            float open, float width, int pass) {
    float total = 0;
    for (int i = 0; i + 1 < n; i++) total += SDL_sqrtf((xs[i + 1] - xs[i]) * (xs[i + 1] - xs[i]) + (ys[i + 1] - ys[i]) * (ys[i + 1] - ys[i]));
    float run = 0;
    for (int i = 0; i + 1 < n; i++) {
        float dx = xs[i + 1] - xs[i], dy = ys[i + 1] - ys[i], len = SDL_sqrtf(dx * dx + dy * dy);
        for (float d = 0; d < len; d += PIXEL / 2.0f) {
            float s = s0 + (s1 - s0) * (run + d) / total;
            if (SDL_fabsf(s) > open / 2) continue;
            float w = width * (1.0f - 1.6f * SDL_fabsf(s)) + (pass == 0 ? 5 : 0);
            if (w < 1) continue;
            int half = ((int)w + PIXEL - 1) / PIXEL * PIXEL;
            int px = (int)(xs[i] + dx * d / len) / PIXEL * PIXEL, py = (int)(ys[i] + dy * d / len) / PIXEL * PIXEL;
            SDL_Rect r = { px - half / 2, py - half / 2, half, half };
            SDL_RenderFillRect(renderer, &r);
        }
        run += len;
    }
}

// A whole crack (main line and branch): broken soil first, then the dark gap on top
static void draw_crack_shape(SDL_Renderer *renderer, const Crack *c, float open, float width) {
    for (int pass = 0; pass < 2; pass++) {
        if (pass == 0) SDL_SetRenderDrawColor(renderer, 92, 64, 38, 255);
        else SDL_SetRenderDrawColor(renderer, 18, 11, 8, 255);
        draw_crack_line(renderer, c->x, c->y, CRACK_POINTS, -0.5f, 0.5f, open, width, pass);
        // The branch starts partway along the main line and splits off once the crack has opened that far
        draw_crack_line(renderer, c->bx, c->by, BRANCH_POINTS, 0.15f, 0.45f, open, width * 0.6f, pass);
    }
}

// An obstacle crack: splitting open from its middle when new, grinding shut when it goes
static void draw_crack(SDL_Renderer *renderer, const Obstacle *o) {
    float open = 1.0f, width = CRACK_HALF_WIDTH * 2;
    if (o->state == OB_RISING) open = SDL_min(1.0f, (float)o->timer / CRACK_OPEN);
    else if (o->state == OB_FALLING) width *= 1.0f - (float)o->timer / CRACK_CLOSE;   // Grinds shut
    draw_crack_shape(renderer, &o->crack, open, width);
}

// --- Frogs ---
// Heights are screen pixels above the frog's spot on the ground; anything in the air is drawn in the sky
// layer, over the players, with its shadow on the ground shrinking the higher it is.

// Frog with its feet `height` above (x, y), squashed flat by `squash` 0..1 and puffed up by `puff`
static void draw_frog(SDL_Renderer *renderer, float x, float y, float height, int scale, float squash, float puff) {
    if (!frog_tex) return;
    int w = (int)(FROG_W * scale * (1 + 0.3f * squash) * (1 + puff)), h = (int)(FROG_H * scale * (1 - 0.35f * squash) * (1 + puff));
    SDL_Rect dst = { (int)x - w / 2, (int)(y - height) - h, w, h };
    SDL_RenderCopy(renderer, frog_tex, NULL, &dst);
}

// A frog's shadow on the ground at (x, y), smaller and fainter the higher the frog is
static void frog_shadow(SDL_Renderer *renderer, float x, float y, float height, int scale) {
    float k = 1.0f / (1.0f + height / 300.0f);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 15, 30, 10, (Uint8)(110 * k));
    fill_pixel_oval(renderer, (int)x + 4, (int)y, FROG_W * scale * 0.5f * k, FROG_W * scale * 0.16f * k + 2, PIXEL);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

// A landing frog's height and squash for `t` frames after it started falling
static void frog_fall(int t, int fall, float drop, float *height, float *squash) {
    float p = (float)t / fall;
    *height = t < fall ? drop * (1 - p * p) : 0.0f;
    *squash = t >= fall && t < fall + FROG_SQUASH ? SDL_sinf((float)M_PI * (t - fall) / FROG_SQUASH) : 0.0f;
}

// Big frog: falling in, sitting with the odd hop in place, or leaping away
static void frog_pose(const Obstacle *o, float *x, float *height, float *squash) {
    *x = o->x;
    *squash = 0;
    if (o->state == OB_RISING) {
        frog_fall(o->timer, FROG_FALL, FROG_DROP, height, squash);
    } else if (o->state == OB_FALLING) {
        float t = (float)o->timer;
        *height = t * 22 + t * t * 0.8f;
        *x += o->dir * t * 9;
    } else if (o->action == FROG_IDLE) {
        int phase = (o->timer + o->age * 41) % 150;
        *height = phase < 16 ? SDL_sinf((float)M_PI * phase / 16) * 14 : 0.0f;
    } else {
        *height = 0;
    }
}

// Cheeks puffed and gulping while it holds the ball
static float frog_puff(const Obstacle *o) {
    return o->action == FROG_SWALLOWED ? 0.22f + 0.08f * SDL_sinf(o->action_timer * 0.7f) : 0.0f;
}

// Pink tongue from the mouth to its tip, a fat sticky blob on the end
static void draw_tongue(SDL_Renderer *renderer, const Obstacle *o) {
    float mx, my;
    frog_mouth(o, &mx, &my);
    float dx = o->tip_x - mx, dy = o->tip_y - my, len = SDL_sqrtf(dx * dx + dy * dy);
    for (int pass = 0; pass < 2; pass++) {
        int w = pass ? 8 : 14;
        if (pass) SDL_SetRenderDrawColor(renderer, 240, 110, 140, 255);
        else SDL_SetRenderDrawColor(renderer, 120, 20, 44, 255);
        for (float d = 0; d <= len; d += PIXEL / 2.0f) {
            float k = len > 0 ? d / len : 0;
            SDL_Rect r = { (int)(mx + dx * k) / 2 * 2 - w / 2, (int)(my + dy * k) / 2 * 2 - w / 2, w, w };
            SDL_RenderFillRect(renderer, &r);
        }
        int blob = pass ? 18 : 24;
        SDL_Rect tip = { (int)o->tip_x - blob / 2, (int)o->tip_y - blob / 2, blob, blob };
        SDL_RenderFillRect(renderer, &tip);
    }
}

// A raining frog's position, height and squash: falling, sitting, then leaping off
static void rain_pose(const RainFrog *f, float *x, float *height, float *squash) {
    *x = f->x;
    *squash = 0;
    if (f->timer < RAIN_FALL + RAIN_SIT) {
        frog_fall(f->timer, RAIN_FALL, FROG_DROP * 0.8f, height, squash);
    } else {
        float t = (float)(f->timer - RAIN_FALL - RAIN_SIT);
        *height = t * 18 + t * t * 0.6f;
        *x += f->dir * t * 8;
    }
}

// Ground layer, under the players: raining frogs' shadows and the ones sitting, molehills, cracks, and big
// frogs on the ground with their tongues
void draw_calamity_ground(SDL_Renderer *renderer) {
    for (int i = 0; i < RAIN_MAX; i++) {
        const RainFrog *f = &rain[i];
        if (!f->active) continue;
        float x, height, squash;
        rain_pose(f, &x, &height, &squash);
        frog_shadow(renderer, x, f->y, height, FROG_SCALE_RAIN);
        if (height < 1) draw_frog(renderer, x, f->y, 0, FROG_SCALE_RAIN, squash, 0);
    }
    for (int i = 0; i < CALAMITY_MAX; i++) {
        const Obstacle *o = &obstacles[i];
        if (o->state == OB_OFF) continue;
        if (o->type == CALAMITY_MOLE) {
            draw_molehill(renderer, o);
        } else if (o->type == CALAMITY_FROGS) {
            float x, height, squash;
            frog_pose(o, &x, &height, &squash);
            frog_shadow(renderer, x, o->y, height, FROG_SCALE_FIELD);
            if (o->state == OB_UP || (o->state == OB_RISING && o->timer >= FROG_FALL)) {   // On the ground (hops too)
                draw_frog(renderer, x, o->y, height, FROG_SCALE_FIELD, squash, frog_puff(o));
                if (o->action == FROG_TONGUE_OUT || o->action == FROG_TONGUE_IN) draw_tongue(renderer, o);
            }
        } else {
            draw_crack(renderer, o);
        }
    }
}

// Sky layer, over the players and the ball: frogs falling in or leaping away
void draw_calamity_sky(SDL_Renderer *renderer) {
    for (int i = 0; i < RAIN_MAX; i++) {
        const RainFrog *f = &rain[i];
        if (!f->active) continue;
        float x, height, squash;
        rain_pose(f, &x, &height, &squash);
        if (height >= 1) draw_frog(renderer, x, f->y, height, FROG_SCALE_RAIN, squash, 0);
    }
    for (int i = 0; i < CALAMITY_MAX; i++) {
        const Obstacle *o = &obstacles[i];
        if (o->type != CALAMITY_FROGS) continue;
        bool falling = o->state == OB_RISING && o->timer < FROG_FALL;
        if (!falling && o->state != OB_FALLING) continue;
        float x, height, squash;
        frog_pose(o, &x, &height, &squash);
        draw_frog(renderer, x, o->y, height, FROG_SCALE_FIELD, squash, 0);
    }
}

// The title arrives, stays a couple of seconds, then drops off the bottom of the screen. The mole pops up
// big over "INCOMING MOLE" dripping mud; the earthquake splits a crack open over "THE EARTHQUAKE" in
// crumbling stone, all shaking; a big frog drops onto "THE FROG RAIN" dripping slime.
void draw_calamity_title(SDL_Renderer *renderer) {
    if (title_timer < 0) return;
    int t = title_timer, exit_at = CALAMITY_TITLE_FRAMES - 22;
    int drop = t > exit_at ? (t - exit_at) * (t - exit_at) * 3 : 0;
    if (title_type == CALAMITY_MOLE) {
        int rise = t < 12 ? (12 - t) * 14 : 0;
        if (mole_tex) {
            int w = MOLE_W * MOLE_SCALE_TITLE, h = MOLE_H * MOLE_SCALE_TITLE;
            SDL_Rect dst = { SCREEN_WIDTH / 2 - w / 2, 150 + rise + drop, w, h };
            SDL_RenderCopy(renderer, mole_tex, NULL, &dst);
        }
        if (t >= 6) draw_mud_text(renderer, "INCOMING MOLE", SCREEN_WIDTH / 2, 340 + drop, 12, t - 6);
    } else if (title_type == CALAMITY_FROGS) {
        float height, squash;
        frog_fall(t, 14, 420, &height, &squash);
        draw_frog(renderer, SCREEN_WIDTH / 2, 312 + drop, height, FROG_SCALE_TITLE, squash, 0);
        if (t >= 6) draw_slime_text(renderer, "THE FROG RAIN", SCREEN_WIDTH / 2, 340 + drop, 12, t - 6);
    } else {
        // Same jagged crack every time: seeded once, then drawn from a fixed copy
        static Crack big;
        static bool made;
        if (!made) {
            unsigned int keep = (unsigned int)rand();
            srand(1906);
            make_crack(&big, 0, 0, frand(-0.08f, 0.08f), 820, 22);   // Across the screen, above the text
            srand(keep);
            made = true;
        }
        int shake = t < ARRIVAL_TREMOR ? 12 * (ARRIVAL_TREMOR - t) / ARRIVAL_TREMOR + 2 : 2;
        int sx = rand() % (2 * shake + 1) - shake, sy = rand() % (2 * shake + 1) - shake;
        Crack c = big;
        for (int i = 0; i < CRACK_POINTS; i++) { c.x[i] += SCREEN_WIDTH / 2 + sx; c.y[i] += 240 + sy + drop; }
        for (int i = 0; i < BRANCH_POINTS; i++) { c.bx[i] += SCREEN_WIDTH / 2 + sx; c.by[i] += 240 + sy + drop; }
        draw_crack_shape(renderer, &c, SDL_min(1.0f, t / 14.0f), 30);
        if (t >= 6) draw_rock_text(renderer, "THE EARTHQUAKE", SCREEN_WIDTH / 2 + sx, 340 + sy + drop, 12, t - 6);
    }
}
