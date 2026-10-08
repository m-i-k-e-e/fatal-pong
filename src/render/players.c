#include "render/players.h"
#include "gameplay/rift.h"
#include <stdio.h>

// Each player is four stacked 32x80 layers (body, legs, left arm, racket arm), generated with their palette by
// tools/gen_sprites.py, drawn 2x wide (64 px) and stretched to the paddle's height, so the grow/shrink/full
// bonuses visibly resize the player. Art faces right with the racket on the front side; the right-hand player is mirrored.
// The collision box stays the paddle rect, its front edge lined up with art column FRONT_COL.
// A shrunk player is drawn as a baby: the same layers and poses on a 32x40 canvas (square pixels at half height).
// A grown one keeps their normal size at the bottom of the double-height paddle and wears a hat that fills the
// rest: a tall top hat for the men, a towering Ascot hat for the ladies.
// With the full-height bonus a force user stands in instead (Vader or Luke, Paddle.force_user): FORCE_SCALE
// times the art, in the middle of the lane, pushing the Force (force_push) where the ball comes back.
#define SPRITE_W            32
#define SPRITE_H            80
#define BABY_H              40
#define SPRITE_SCALE        2
#define FRONT_COL           24
#define STEP_POSES          4
#define LEFT_ARM_POSES      3        // Hanging, charge, thrust
#define ARM_POSES           7        // Ready, forward, contact, follow-through, charge, thrust, unarmed
#define POSE_CHARGE         4
#define POSE_THRUST         5
#define POSE_UNARMED        6        // Thrust with the racket thrown away (fatality)
#define STEP_DISTANCE       40.0f    // Pixels moved per step frame
#define FEET_COL            12.75f   // Art column centered between the shoes
#define FEET_ROW            77.0f    // Art row just below the soles
#define SHADOW_RX           30       // Ground shadow half-size in screen pixels
#define BABY_SHADOW_RX      22
#define SHADOW_RY           9
#define SHADOW_DX           10       // Offset toward the lower right, away from the upper-left light
#define SHADOW_ALPHA        90
#define RACKET_SCALE        3        // Thrown racket, a bit larger than in hand so it reads in flight
#define IRON_BALL_ART       12       // Ball and chain (slow bonus): ball art size, drawn at SPRITE_SCALE
#define CHAIN_BEHIND        34       // Screen pixels from the ankle back to the ball's rest spot
#define CHAIN_DROP          6        // ... and down from the feet line
#define CHAIN_MAX           70       // Farthest the ball trails behind its rest spot as the player moves
#define CHAIN_FOLLOW        0.12f    // Fraction of the way the dragged ball closes on its rest spot per frame
#define CHAIN_LINKS         9
#define FORCE_USERS         2        // Vader, Luke
#define FORCE_ARM_POSES     2        // Idle, push
#define FORCE_SCALE         3
#define FORCE_FRAMES        24       // A Force wave's length, also how long the push pose holds
#define FORCE_RIPPLES       5
#define SHIVER_PX           2        // Scared: trembling this far either side,
#define SHIVER_MS           40       // switching every this many milliseconds

// Art coordinates of points the fatality and the rift aim at (see tools/gen_sprites.py)
static const float POINTS[][2] = {
    [POINT_HAND] = { 27.0f, 47.0f },    // Racket hand in the thrust pose
    [POINT_FACE] = { 13.0f, 27.0f },    // Middle of the face
    [POINT_NECK] = { 13.0f, 40.0f },    // Top of the neck stump
    [POINT_FEET] = { FEET_COL, FEET_ROW },  // Ground between the shoes
    [POINT_ANKLE] = { 9.5f, 72.0f },    // Back leg, above the sock, where the ball and chain's cuff goes
};
static const float FORCE_POINTS[][2] = {
    [POINT_HAND] = { 29.0f, 46.0f },    // Open palm in the push pose (FORCE_HAND in the generator)
    [POINT_FACE] = { 13.0f, 27.0f },
    [POINT_NECK] = { 13.0f, 40.0f },
    [POINT_FEET] = { FEET_COL, FEET_ROW },
    [POINT_ANKLE] = { 10.0f, 72.0f },
};
static const float BABY_POINTS[][2] = {
    [POINT_HAND] = { 25.0f, 20.0f },
    [POINT_FACE] = { 12.5f, 11.0f },
    [POINT_NECK] = { 12.5f, 17.5f },
    [POINT_FEET] = { FEET_COL, 39.0f },
    [POINT_ANKLE] = { 9.5f, 35.5f },
};

// Agassi, early 90s: bleached mullet, black and neon-pink shirt, acid-wash denim shorts.
// Nadal, mid 2000s: long hair and red headband, sleeveless lime top, white pirate capris.
// Graf, late 80s: golden blond bob with bangs, white top with a red V collar, white pleated skirt.
// Sharapova, mid 2000s: platinum ponytail, red lipstick, black dress with crystals at the neckline.
#include "player_sprites.inc"

typedef struct {
    SDL_Texture *body;
    SDL_Texture *scared;                // White face, over the body (see draw_player)
    SDL_Texture *headless;
    SDL_Texture *legs[STEP_POSES];
    SDL_Texture *left_arm[LEFT_ARM_POSES];
    SDL_Texture *arm[ARM_POSES];
} PlayerTextures;

static const char *const *const BODIES[PLAYER_COUNT] = { AGASSI_BODY, NADAL_BODY, GRAF_BODY, SHARAPOVA_BODY };
static const char *const *const HEADLESS[PLAYER_COUNT] = { AGASSI_HEADLESS, NADAL_HEADLESS, GRAF_HEADLESS, SHARAPOVA_HEADLESS };
static const char *const *const SCARED[PLAYER_COUNT] = { AGASSI_SCARED, NADAL_SCARED, GRAF_SCARED, SHARAPOVA_SCARED };
static const char *const *const BABY_SCARED[PLAYER_COUNT] = {
    AGASSI_BABY_SCARED, NADAL_BABY_SCARED, GRAF_BABY_SCARED, SHARAPOVA_BABY_SCARED,
};
static const char *const *const RACKETS[PLAYER_COUNT] = { AGASSI_RACKET, NADAL_RACKET, GRAF_RACKET, SHARAPOVA_RACKET };
static const char *const (*const LEGS[PLAYER_COUNT])[SPRITE_H] = { AGASSI_LEGS, NADAL_LEGS, GRAF_LEGS, SHARAPOVA_LEGS };
static const char *const (*const LEFT_ARMS[PLAYER_COUNT])[SPRITE_H] = { AGASSI_LEFT_ARM, NADAL_LEFT_ARM, GRAF_LEFT_ARM, SHARAPOVA_LEFT_ARM };
static const char *const (*const ARMS[PLAYER_COUNT])[SPRITE_H] = { AGASSI_ARM, NADAL_ARM, GRAF_ARM, SHARAPOVA_ARM };
static const char *const *const BABY_BODIES[PLAYER_COUNT] = { AGASSI_BABY_BODY, NADAL_BABY_BODY, GRAF_BABY_BODY, SHARAPOVA_BABY_BODY };
static const char *const *const BABY_HEADLESS[PLAYER_COUNT] = { AGASSI_BABY_HEADLESS, NADAL_BABY_HEADLESS, GRAF_BABY_HEADLESS, SHARAPOVA_BABY_HEADLESS };
static const char *const (*const BABY_LEGS[PLAYER_COUNT])[BABY_H] = { AGASSI_BABY_LEGS, NADAL_BABY_LEGS, GRAF_BABY_LEGS, SHARAPOVA_BABY_LEGS };
static const char *const (*const BABY_LEFT_ARMS[PLAYER_COUNT])[BABY_H] = { AGASSI_BABY_LEFT_ARM, NADAL_BABY_LEFT_ARM, GRAF_BABY_LEFT_ARM, SHARAPOVA_BABY_LEFT_ARM };
static const char *const (*const BABY_ARMS[PLAYER_COUNT])[BABY_H] = { AGASSI_BABY_ARM, NADAL_BABY_ARM, GRAF_BABY_ARM, SHARAPOVA_BABY_ARM };
static PlayerTextures textures[PLAYER_COUNT], babies[PLAYER_COUNT];
typedef struct {
    SDL_Texture *body, *headless, *scared, *arm[FORCE_ARM_POSES];
} ForceTextures;
static const char *const *const FORCE_BODIES[FORCE_USERS] = { VADER_FORCE_BODY, LUKE_FORCE_BODY };
static const char *const *const FORCE_HEADLESS[FORCE_USERS] = { VADER_FORCE_HEADLESS, LUKE_FORCE_HEADLESS };
static const char *const *const FORCE_SCARED[FORCE_USERS] = { VADER_FORCE_SCARED, LUKE_FORCE_SCARED };
static const char *const (*const FORCE_ARMS[FORCE_USERS])[SPRITE_H] = { VADER_FORCE_ARM, LUKE_FORCE_ARM };
static ForceTextures force_tex[FORCE_USERS];
static int force_timer[2];              // Per side (0 = left player): frames left of the current Force wave
static float force_x[2], force_y[2];    // Where the wave is aimed
static SDL_Texture *rackets[PLAYER_COUNT];
static bool sprites_ok;
static SDL_Texture *iron_ball;
static SDL_Texture *crossed_eyes;       // Zig-zag bonus, over the face
static SDL_Texture *hats[2];            // Grow bonus: top hat (men), Ascot hat (ladies)
static float chain_ball_y[2];           // Dragged ball and chain, per side (0 = left player): its screen y
static bool chain_on[2];                // Drawn last frame, so a fresh one starts at rest

// Texture from w x h character art in the generated palette; NULL on failure
static SDL_Texture *build_sized(SDL_Renderer *renderer, const char *const *art, int w, int h) {
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surf) return NULL;
    for (int y = 0; y < h; y++) {
        Uint32 *row = (Uint32 *)((Uint8 *)surf->pixels + y * surf->pitch);
        for (int x = 0; x < w; x++) {
            SDL_Color c = palette_color(art[y][x]);
            row[x] = SDL_MapRGBA(surf->format, c.r, c.g, c.b, c.a);
        }
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    if (tex) SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    return tex;
}

// The ball of the ball and chain: an IRON_BALL_ART pixel iron sphere lit from the upper left, with a dark rim;
// NULL on failure (then no ball is drawn)
static SDL_Texture *build_iron_ball(SDL_Renderer *renderer) {
    static const SDL_Color TONES[] = { { 30, 30, 36, 255 }, { 56, 56, 66, 255 }, { 88, 88, 102, 255 },
                                       { 128, 128, 144, 255 }, { 190, 190, 204, 255 } };
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, IRON_BALL_ART, IRON_BALL_ART, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surf) return NULL;
    float c = IRON_BALL_ART / 2.0f, r = c - 0.5f;
    for (int y = 0; y < IRON_BALL_ART; y++) {
        Uint32 *row = (Uint32 *)((Uint8 *)surf->pixels + y * surf->pitch);
        for (int x = 0; x < IRON_BALL_ART; x++) {
            float nx = (x + 0.5f - c) / r, ny = (y + 0.5f - c) / r, d = nx * nx + ny * ny;
            float light = -0.55f * nx - 0.65f * ny + 0.5f * SDL_sqrtf(SDL_max(0.0f, 1.0f - d));
            int tone = d > 0.72f ? 0 : light > 0.75f ? 4 : light > 0.4f ? 3 : light > 0.05f ? 2 : 1;
            SDL_Color t = TONES[tone];
            row[x] = SDL_MapRGBA(surf->format, t.r, t.g, t.b, d > 1.0f ? 0 : 255);
        }
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    if (tex) SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    return tex;
}

// Build one size's layers and poses (`h` rows each) into `t`; false if any texture failed.
// The pose arrays are flattened: pose f starts at row f * h.
static bool build_set(SDL_Renderer *renderer, PlayerTextures *t, const char *const *body,
                      const char *const *headless, const char *const *legs, const char *const *left_arm,
                      const char *const *arm, int h) {
    bool ok = (t->body = build_sized(renderer, body, SPRITE_W, h)) != NULL;
    ok &= (t->headless = build_sized(renderer, headless, SPRITE_W, h)) != NULL;
    for (int f = 0; f < STEP_POSES; f++) ok &= (t->legs[f] = build_sized(renderer, legs + f * h, SPRITE_W, h)) != NULL;
    for (int f = 0; f < LEFT_ARM_POSES; f++) ok &= (t->left_arm[f] = build_sized(renderer, left_arm + f * h, SPRITE_W, h)) != NULL;
    for (int f = 0; f < ARM_POSES; f++) ok &= (t->arm[f] = build_sized(renderer, arm + f * h, SPRITE_W, h)) != NULL;
    return ok;
}

// Build every player texture (adult and baby layers and poses, headless bodies, racket) once at startup; if
// any fails, draw_player() falls back to plain paddles
void init_player_sprites(SDL_Renderer *renderer) {
    sprites_ok = true;
    for (int i = 0; i < PLAYER_COUNT; i++) {
        sprites_ok &= build_set(renderer, &textures[i], BODIES[i], HEADLESS[i], LEGS[i][0], LEFT_ARMS[i][0],
                                ARMS[i][0], SPRITE_H);
        sprites_ok &= build_set(renderer, &babies[i], BABY_BODIES[i], BABY_HEADLESS[i], BABY_LEGS[i][0],
                                BABY_LEFT_ARMS[i][0], BABY_ARMS[i][0], BABY_H);
        sprites_ok &= (textures[i].scared = build_sized(renderer, SCARED[i], SPRITE_W, SPRITE_H)) != NULL;
        sprites_ok &= (babies[i].scared = build_sized(renderer, BABY_SCARED[i], SPRITE_W, BABY_H)) != NULL;
        sprites_ok &= (rackets[i] = build_sized(renderer, RACKETS[i], RACKET_W, RACKET_H)) != NULL;
    }
    for (int i = 0; i < FORCE_USERS; i++) {
        ForceTextures *f = &force_tex[i];
        sprites_ok &= (f->body = build_sized(renderer, FORCE_BODIES[i], SPRITE_W, SPRITE_H)) != NULL;
        sprites_ok &= (f->headless = build_sized(renderer, FORCE_HEADLESS[i], SPRITE_W, SPRITE_H)) != NULL;
        sprites_ok &= (f->scared = build_sized(renderer, FORCE_SCARED[i], SPRITE_W, SPRITE_H)) != NULL;
        for (int a = 0; a < FORCE_ARM_POSES; a++)
            sprites_ok &= (f->arm[a] = build_sized(renderer, FORCE_ARMS[i][a], SPRITE_W, SPRITE_H)) != NULL;
    }
    iron_ball = build_iron_ball(renderer);
    sprites_ok &= (crossed_eyes = build_sized(renderer, CROSSED_EYES, SPRITE_W, SPRITE_H)) != NULL;
    sprites_ok &= (hats[0] = build_sized(renderer, TOP_HAT, SPRITE_W, HAT_H)) != NULL;
    sprites_ok &= (hats[1] = build_sized(renderer, LADY_HAT, SPRITE_W, HAT_H)) != NULL;
    if (!sprites_ok) printf("[pong] player sprites failed, drawing plain paddles: %s\n", SDL_GetError());
}

// Destroy a texture and clear the pointer
static void destroy(SDL_Texture **tex) {
    if (*tex) SDL_DestroyTexture(*tex);
    *tex = NULL;
}

// Destroy one size's layers and poses
static void destroy_set(PlayerTextures *t) {
    destroy(&t->body);
    destroy(&t->headless);
    destroy(&t->scared);
    for (int f = 0; f < STEP_POSES; f++) destroy(&t->legs[f]);
    for (int f = 0; f < LEFT_ARM_POSES; f++) destroy(&t->left_arm[f]);
    for (int f = 0; f < ARM_POSES; f++) destroy(&t->arm[f]);
}

// Destroy every texture made by init_player_sprites()
void free_player_sprites(void) {
    for (int i = 0; i < PLAYER_COUNT; i++) {
        destroy_set(&textures[i]);
        destroy_set(&babies[i]);
        destroy(&rackets[i]);
    }
    for (int i = 0; i < FORCE_USERS; i++) {
        destroy(&force_tex[i].body);
        destroy(&force_tex[i].headless);
        destroy(&force_tex[i].scared);
        for (int a = 0; a < FORCE_ARM_POSES; a++) destroy(&force_tex[i].arm[a]);
    }
    destroy(&iron_ball);
    destroy(&crossed_eyes);
    destroy(&hats[0]);
    destroy(&hats[1]);
    sprites_ok = false;
}

// A shrunk player is drawn (and aimed at) as the baby
static bool is_baby(const Paddle *p) {
    return p->effect == BONUS_SHRINK;
}

// A grown paddle is drawn (and aimed at) as the player at normal size, standing at its bottom, under a hat
static bool is_hatted(const Paddle *p) {
    return p->effect == BONUS_GROW;
}

// A full-height paddle is drawn (and aimed at) as its force user
static bool is_force_user(const Paddle *p) {
    return p->effect == BONUS_FULL;
}

// Steps through stand / left foot up / stand / right foot up while moving
static int step_frame(const Paddle *p) {
    return p->moving ? (int)(p->stride / STEP_DISTANCE) % STEP_POSES : 0;
}

// Swing frames: 0 ready, 1 forward, 2 contact, 3 follow-through. Starts at contact since the hit already happened.
static int swing_frame(const Paddle *p) {
    if (p->swing_timer <= 0) return 0;
    int elapsed = SWING_DURATION - p->swing_timer;
    if (elapsed < 4) return 2;
    if (elapsed < 10) return 3;
    return 1;
}

// Specials: both arms thrust forward after a hadouken or rift, hands cupped at the hip while the motion is
// charged (down then forward or back entered, waiting for the punch button); otherwise the left arm hangs
static void arm_poses(const Paddle *p, int *left, int *right) {
    if (p->unarmed) { *left = 2; *right = POSE_UNARMED; }
    else if (p->throw_timer > 0) { *left = 2; *right = POSE_THRUST; }
    else if (p->motion_state >= 2 && p->stun_timer == 0) { *left = 1; *right = POSE_CHARGE; }
    else { *left = 0; *right = swing_frame(p); }
}

// Soft oval `rx` wide (half-size) on the grass under the feet, drawn as blended scanlines
static void draw_shadow(SDL_Renderer *renderer, int cx, int cy, int rx) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 10, 30, 10, SHADOW_ALPHA);
    for (int dy = -SHADOW_RY; dy <= SHADOW_RY; dy++) {
        float k = 1.0f - (float)(dy * dy) / (SHADOW_RY * SHADOW_RY);
        int half = (int)(rx * SDL_sqrtf(k));
        SDL_Rect line = { cx - half, cy + dy, half * 2, 1 };
        SDL_RenderFillRect(renderer, &line);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

// Where the sprite is drawn: stretched to the paddle height, front edge on the paddle's front
static SDL_Rect sprite_rect(const Paddle *p, bool faces_right) {
    bool force = is_force_user(p);
    int scale = force ? FORCE_SCALE : SPRITE_SCALE, w = SPRITE_W * scale, front = FRONT_COL * scale;
    SDL_Rect dst = { faces_right ? (int)(p->x + p->w) - front : (int)p->x - (w - front), (int)p->y, w, (int)p->h };
    if (force) {                    // Life size in the middle of the lane, not stretched over the whole height
        dst.h = SPRITE_H * FORCE_SCALE;
        dst.y = (SCREEN_HEIGHT - dst.h) / 2;
    } else if (is_hatted(p)) {      // Normal size at the bottom; the hat fills the top
        dst.h = SPRITE_H * SPRITE_SCALE;
        dst.y = (int)(p->y + p->h) - dst.h;
    }
    return dst;
}

// Screen position of a sprite feature (POINTS, or BABY_POINTS while shrunk, FORCE_POINTS as a force user) for
// the paddle as currently drawn, mirrored for the right player
void player_point(const Paddle *p, bool faces_right, PlayerPoint point, int *x, int *y) {
    SDL_Rect dst = sprite_rect(p, faces_right);
    bool baby = is_baby(p), force = is_force_user(p);
    const float *pt = force ? FORCE_POINTS[point] : baby ? BABY_POINTS[point] : POINTS[point];
    float col = faces_right ? pt[0] : SPRITE_W - pt[0];
    *x = dst.x + (int)(col * (force ? FORCE_SCALE : SPRITE_SCALE));
    *y = dst.y + (int)(dst.h * pt[1] / (baby ? BABY_H : SPRITE_H));
}

// The thrown racket, centered on (cx, cy) and turned by angle degrees
void draw_racket(SDL_Renderer *renderer, PlayerLook look, int cx, int cy, double angle) {
    if (!sprites_ok) return;
    int w = RACKET_W * RACKET_SCALE, h = RACKET_H * RACKET_SCALE;
    SDL_Rect dst = { cx - w / 2, cy - h / 2, w, h };
    SDL_RenderCopyEx(renderer, rackets[look], NULL, &dst, angle, NULL, SDL_FLIP_NONE);
}

// Colour a layer for this frame: purple while sinking through a rift (`depth` > 0), blue on a stun `flash`,
// otherwise untouched
static void tint(SDL_Texture *tex, float depth, bool flash) {
    if (depth > 0.0f) SDL_SetTextureColorMod(tex, 200, 140, 255);
    else if (flash) SDL_SetTextureColorMod(tex, 90, 130, 255);
    else SDL_SetTextureColorMod(tex, 255, 255, 255);
}

// One chain link, a few pixels on the SPRITE_SCALE grid: flat (`flat`) or edge-on, lit along its top
static void draw_link(SDL_Renderer *renderer, int x, int y, bool flat) {
    x = x / SPRITE_SCALE * SPRITE_SCALE;
    y = y / SPRITE_SCALE * SPRITE_SCALE;
    SDL_Rect link = flat ? (SDL_Rect){ x - 4, y - 2, 8, 4 } : (SDL_Rect){ x - 2, y - 3, 4, 6 };
    SDL_SetRenderDrawColor(renderer, 40, 40, 48, 255);
    SDL_RenderFillRect(renderer, &link);
    SDL_Rect shine = { link.x, link.y, link.w, 2 };
    SDL_SetRenderDrawColor(renderer, 140, 140, 156, 255);
    SDL_RenderFillRect(renderer, &shine);
}

// Slow bonus, under the sprite: the iron ball on the grass behind the player (side 0 left, 1 right) with its
// shadow, and the sagging chain up to the back ankle. The ball trails behind the player's moves (chain_ball_y),
// up to CHAIN_MAX from its rest spot. The cuff goes on after the sprite (draw_cuff).
static void draw_ball_and_chain(SDL_Renderer *renderer, const Paddle *p, bool faces_right, int ground) {
    int side = faces_right ? 0 : 1, ax, ay;
    player_point(p, faces_right, POINT_ANKLE, &ax, &ay);
    float bx = ax + (faces_right ? -CHAIN_BEHIND : CHAIN_BEHIND), rest_y = (float)(ground + CHAIN_DROP);
    if (!chain_on[side]) chain_ball_y[side] = rest_y;
    chain_on[side] = true;
    chain_ball_y[side] += (rest_y - chain_ball_y[side]) * CHAIN_FOLLOW;
    chain_ball_y[side] = SDL_clamp(chain_ball_y[side], rest_y - CHAIN_MAX, rest_y + CHAIN_MAX);
    int by = (int)chain_ball_y[side], size = IRON_BALL_ART * SPRITE_SCALE;

    draw_shadow(renderer, (int)bx + SHADOW_DX / 2, by, size / 2 + 2);
    // Links from the ankle to the top of the ball, sagging less as the chain pulls taut
    float tx = bx, ty = (float)(by - size), dist = SDL_sqrtf((tx - ax) * (tx - ax) + (ty - ay) * (ty - ay));
    float sag = SDL_max(0.0f, 14.0f - dist * 0.12f);
    for (int i = 1; i < CHAIN_LINKS; i++) {
        float t = (float)i / CHAIN_LINKS;
        draw_link(renderer, (int)(ax + (tx - ax) * t), (int)(ay + (ty - ay) * t + sag * SDL_sinf((float)M_PI * t)), i % 2);
    }
    if (iron_ball) {
        SDL_Rect dst = { (int)bx - size / 2, by - size, size, size };
        SDL_RenderCopy(renderer, iron_ball, NULL, &dst);
    }
}

// Slow bonus, over the sprite: the iron cuff round the back ankle
static void draw_cuff(SDL_Renderer *renderer, const Paddle *p, bool faces_right) {
    int ax, ay;
    player_point(p, faces_right, POINT_ANKLE, &ax, &ay);
    SDL_Rect cuff = { ax - 5, ay - 3, 10, 6 };
    SDL_SetRenderDrawColor(renderer, 40, 40, 48, 255);
    SDL_RenderFillRect(renderer, &cuff);
    SDL_Rect band = { cuff.x + 2, cuff.y + 2, cuff.w - 4, 2 };
    SDL_SetRenderDrawColor(renderer, 120, 120, 136, 255);
    SDL_RenderFillRect(renderer, &band);
}

// Full bonus: start a Force wave from the palm of the force user facing right (or left) to (x, y), the ball as
// it's sent back; holds the push pose for FORCE_FRAMES
void force_push(bool faces_right, float x, float y) {
    int side = faces_right ? 0 : 1;
    force_timer[side] = FORCE_FRAMES;
    force_x[side] = x;
    force_y[side] = y;
}

// A ring of 6 px blocks of radius `r` around (cx, cy), in the current draw colour
static void draw_ring(SDL_Renderer *renderer, float cx, float cy, float r) {
    int blocks = 12 + (int)(r / 4);
    for (int i = 0; i < blocks; i++) {
        float a = 2.0f * (float)M_PI * i / blocks;
        SDL_Rect b = { (int)(cx + SDL_cosf(a) * r) / 6 * 6 - 3, (int)(cy + SDL_sinf(a) * r) / 6 * 6 - 3, 6, 6 };
        SDL_RenderFillRect(renderer, &b);
    }
}

// The Force wave, over the sprite: ripples running from the palm to the ball, and a ring spreading where it
// met the ball, fading out; red for Vader, pale blue for Luke. Counts force_timer down.
static void draw_force_wave(SDL_Renderer *renderer, const Paddle *p, bool faces_right) {
    int side = faces_right ? 0 : 1;
    if (force_timer[side] <= 0) return;
    int age = FORCE_FRAMES - force_timer[side]--, hx, hy;
    player_point(p, faces_right, POINT_HAND, &hx, &hy);
    float fade = (float)(force_timer[side] + 1) / FORCE_FRAMES;
    SDL_Color c = p->force_user == 0 ? (SDL_Color){ 255, 60, 50, 0 } : (SDL_Color){ 150, 205, 255, 0 };
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (int k = 0; k < FORCE_RIPPLES; k++) {
        float t = SDL_fmodf((k + age * 0.25f) / FORCE_RIPPLES, 1.0f);
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, (Uint8)(230 * fade * (1.0f - 0.5f * t)));
        draw_ring(renderer, hx + (force_x[side] - hx) * t, hy + (force_y[side] - hy) * t, 8 + 26 * t);
    }
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, (Uint8)(255 * fade));
    draw_ring(renderer, force_x[side], force_y[side], 14.0f + age * 4.0f);
    draw_ring(renderer, force_x[side], force_y[side], 8.0f + age * 2.5f);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

// Flashes blue while stunned; hidden, shadow included, while ghosted; sinks into the ground through a rift,
// tinted purple and cut off at the feet line; a baby while shrunk; dragging a ball and chain while slowed;
// Vader or Luke, pushing the Force, with the full-height bonus; cross-eyed with the zig-zag bonus; in a hat,
// at normal size, with the grow bonus; white in the face, terrified and trembling when `scared` (level with an
// invisible ghost)
void draw_player(SDL_Renderer *renderer, const Paddle *p, PlayerLook look, bool faces_right, bool scared) {
    float depth = vanish_depth(p);
    bool chained = p->effect == BONUS_SLOW && depth <= 0.0f && sprites_ok;
    if (!chained) chain_on[faces_right ? 0 : 1] = false;
    if (p->effect == BONUS_GHOST || depth >= 1.0f) return;
    bool flash = p->stun_timer > 0 && (p->stun_timer / 6) % 2;

    if (!sprites_ok) {
        if (flash) SDL_SetRenderDrawColor(renderer, 70, 110, 255, 255);
        else SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
        SDL_Rect r = { (int)p->x, (int)p->y, (int)p->w, (int)p->h };
        SDL_RenderFillRect(renderer, &r);
        return;
    }

    bool baby = is_baby(p), force = is_force_user(p);
    scared = scared && !p->headless;
    SDL_Texture *stack[6];
    int layers = 0;
    if (force) {
        const ForceTextures *f = &force_tex[p->force_user];
        bool push = force_timer[faces_right ? 0 : 1] > 0 || p->swing_timer > 0 || p->throw_timer > 0;
        stack[layers++] = p->headless ? f->headless : f->body;
        if (scared) stack[layers++] = f->scared;
        stack[layers++] = f->arm[push];
    } else {
        const PlayerTextures *t = baby ? &babies[look] : &textures[look];
        int left, right;
        arm_poses(p, &left, &right);
        stack[layers++] = p->headless ? t->headless : t->body;
        if (scared) stack[layers++] = t->scared;              // Over the face, under the arms and racket
        stack[layers++] = t->legs[step_frame(p)];
        stack[layers++] = t->left_arm[left];
        stack[layers++] = t->arm[right];
        if (p->effect == BONUS_ZIGZAG && !p->headless) stack[layers++] = crossed_eyes;
    }
    SDL_Rect dst = sprite_rect(p, faces_right);
    if (scared) dst.x += (SDL_GetTicks() / SHIVER_MS) % 2 ? SHIVER_PX : -SHIVER_PX;   // Trembling
    int feet_x, ground;
    player_point(p, faces_right, POINT_FEET, &feet_x, &ground);
    if (depth > 0.0f) {
        SDL_Rect above = { 0, 0, SCREEN_WIDTH, ground };
        SDL_RenderSetClipRect(renderer, &above);
        dst.y += (int)(depth * dst.h);
    } else {
        draw_shadow(renderer, feet_x + SHADOW_DX, ground, baby ? BABY_SHADOW_RX : force ? SHADOW_RX * 3 / 2 : SHADOW_RX);
        if (chained) draw_ball_and_chain(renderer, p, faces_right, ground);
    }
    for (int i = 0; i < layers; i++) {
        tint(stack[i], depth, flash);
        SDL_RenderCopyEx(renderer, stack[i], NULL, &dst, 0.0, NULL, faces_right ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL);
    }
    if (is_hatted(p) && !p->headless) {        // On the head, filling the paddle above it
        SDL_Texture *hat = hats[player_is_female(look)];
        SDL_Rect at = { dst.x, dst.y - (HAT_H - HAT_OVERLAP) * SPRITE_SCALE, SPRITE_W * SPRITE_SCALE, HAT_H * SPRITE_SCALE };
        tint(hat, depth, flash);
        SDL_RenderCopyEx(renderer, hat, NULL, &at, 0.0, NULL, faces_right ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL);
    }
    if (chained) draw_cuff(renderer, p, faces_right);
    if (depth > 0.0f) SDL_RenderSetClipRect(renderer, NULL);
    if (force) draw_force_wave(renderer, p, faces_right);
}

// The player standing, unscaled art pixels `scale` screen pixels square, top-left at (x, y): for the character
// select. Nothing if the sprites failed to build.
void draw_player_portrait(SDL_Renderer *renderer, PlayerLook look, int x, int y, int scale, bool faces_right) {
    if (!sprites_ok) return;
    const PlayerTextures *t = &textures[look];
    SDL_Texture *stack[4] = { t->body, t->legs[0], t->left_arm[0], t->arm[0] };
    SDL_Rect dst = { x, y, SPRITE_W * scale, SPRITE_H * scale };
    for (int i = 0; i < 4; i++) {
        SDL_SetTextureColorMod(stack[i], 255, 255, 255);
        SDL_RenderCopyEx(renderer, stack[i], NULL, &dst, 0.0, NULL, faces_right ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL);
    }
}

// Display name for the character select and win screens
const char *player_name(PlayerLook look) {
    static const char *const NAMES[PLAYER_COUNT] = { "AGASSI", "NADAL", "GRAF", "SHARAPOVA" };
    return NAMES[look];
}

// True for Graf and Sharapova
bool player_is_female(PlayerLook look) {
    return look == PLAYER_GRAF || look == PLAYER_SHARAPOVA;
}

// Mid tone of the player's hair (see tools/gen_sprites.py)
SDL_Color player_hair_color(PlayerLook look) {
    static const SDL_Color HAIR[PLAYER_COUNT] = {
        { 228, 192, 100, 255 }, { 74, 46, 28, 255 }, { 206, 166, 90, 255 }, { 244, 232, 184, 255 },
    };
    return HAIR[look];
}
