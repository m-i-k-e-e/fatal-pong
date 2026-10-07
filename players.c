#include "players.h"
#include "rift.h"
#include <stdio.h>

// Each player is four stacked 32x80 layers (body, legs, left arm, racket arm), generated with their palette by
// tools/gen_sprites.py, drawn 2x wide (64 px) and stretched to the paddle's height, so the grow/shrink/full
// bonuses visibly resize the player. Art faces right with the racket on the front side; the right-hand player is mirrored.
// The collision box stays the paddle rect, its front edge lined up with art column FRONT_COL.
#define SPRITE_W            32
#define SPRITE_H            80
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
#define SHADOW_RY           9
#define SHADOW_DX           10       // Offset toward the lower right, away from the upper-left light
#define SHADOW_ALPHA        90
#define RACKET_SCALE        3        // Thrown racket, a bit larger than in hand so it reads in flight

// Art coordinates of points the fatality and the rift aim at (see tools/gen_sprites.py)
static const float POINTS[][2] = {
    [POINT_HAND] = { 27.0f, 47.0f },    // Racket hand in the thrust pose
    [POINT_FACE] = { 13.0f, 27.0f },    // Middle of the face
    [POINT_NECK] = { 13.0f, 40.0f },    // Top of the neck stump
    [POINT_FEET] = { FEET_COL, FEET_ROW },  // Ground between the shoes
};

// Agassi, early 90s: bleached mullet, black and neon-pink shirt, acid-wash denim shorts.
// Nadal, mid 2000s: long hair and red headband, sleeveless lime top, white pirate capris.
#include "player_sprites.inc"

typedef struct {
    SDL_Texture *body;
    SDL_Texture *headless;
    SDL_Texture *racket;
    SDL_Texture *legs[STEP_POSES];
    SDL_Texture *left_arm[LEFT_ARM_POSES];
    SDL_Texture *arm[ARM_POSES];
} PlayerTextures;

static const char *const *const BODIES[PLAYER_COUNT] = { AGASSI_BODY, NADAL_BODY };
static const char *const *const HEADLESS[PLAYER_COUNT] = { AGASSI_HEADLESS, NADAL_HEADLESS };
static const char *const *const RACKETS[PLAYER_COUNT] = { AGASSI_RACKET, NADAL_RACKET };
static const char *const (*const LEGS[PLAYER_COUNT])[SPRITE_H] = { AGASSI_LEGS, NADAL_LEGS };
static const char *const (*const LEFT_ARMS[PLAYER_COUNT])[SPRITE_H] = { AGASSI_LEFT_ARM, NADAL_LEFT_ARM };
static const char *const (*const ARMS[PLAYER_COUNT])[SPRITE_H] = { AGASSI_ARM, NADAL_ARM };
static PlayerTextures textures[PLAYER_COUNT];
static bool sprites_ok;

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

// Texture from one SPRITE_W x SPRITE_H sprite layer
static SDL_Texture *build_texture(SDL_Renderer *renderer, const char *const *art) {
    return build_sized(renderer, art, SPRITE_W, SPRITE_H);
}

// Build every player texture (layers and poses, headless body, racket) once at startup; if any fails,
// draw_player() falls back to plain paddles
void init_player_sprites(SDL_Renderer *renderer) {
    sprites_ok = true;
    for (int i = 0; i < PLAYER_COUNT; i++) {
        PlayerTextures *t = &textures[i];
        sprites_ok &= (t->body = build_texture(renderer, BODIES[i])) != NULL;
        sprites_ok &= (t->headless = build_texture(renderer, HEADLESS[i])) != NULL;
        sprites_ok &= (t->racket = build_sized(renderer, RACKETS[i], RACKET_W, RACKET_H)) != NULL;
        for (int f = 0; f < STEP_POSES; f++) sprites_ok &= (t->legs[f] = build_texture(renderer, LEGS[i][f])) != NULL;
        for (int f = 0; f < LEFT_ARM_POSES; f++) sprites_ok &= (t->left_arm[f] = build_texture(renderer, LEFT_ARMS[i][f])) != NULL;
        for (int f = 0; f < ARM_POSES; f++) sprites_ok &= (t->arm[f] = build_texture(renderer, ARMS[i][f])) != NULL;
    }
    if (!sprites_ok) printf("[pong] player sprites failed, drawing plain paddles: %s\n", SDL_GetError());
}

// Destroy a texture and clear the pointer
static void destroy(SDL_Texture **tex) {
    if (*tex) SDL_DestroyTexture(*tex);
    *tex = NULL;
}

// Destroy every texture made by init_player_sprites()
void free_player_sprites(void) {
    for (int i = 0; i < PLAYER_COUNT; i++) {
        destroy(&textures[i].body);
        destroy(&textures[i].headless);
        destroy(&textures[i].racket);
        for (int f = 0; f < STEP_POSES; f++) destroy(&textures[i].legs[f]);
        for (int f = 0; f < LEFT_ARM_POSES; f++) destroy(&textures[i].left_arm[f]);
        for (int f = 0; f < ARM_POSES; f++) destroy(&textures[i].arm[f]);
    }
    sprites_ok = false;
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

// Soft oval on the grass under the feet, drawn as blended scanlines
static void draw_shadow(SDL_Renderer *renderer, int cx, int cy) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 10, 30, 10, SHADOW_ALPHA);
    for (int dy = -SHADOW_RY; dy <= SHADOW_RY; dy++) {
        float k = 1.0f - (float)(dy * dy) / (SHADOW_RY * SHADOW_RY);
        int half = (int)(SHADOW_RX * SDL_sqrtf(k));
        SDL_Rect line = { cx - half, cy + dy, half * 2, 1 };
        SDL_RenderFillRect(renderer, &line);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

// Where the sprite is drawn: stretched to the paddle height, front edge on the paddle's front
static SDL_Rect sprite_rect(const Paddle *p, bool faces_right) {
    int w = SPRITE_W * SPRITE_SCALE, front = FRONT_COL * SPRITE_SCALE;
    SDL_Rect dst = { faces_right ? (int)(p->x + p->w) - front : (int)p->x - (w - front), (int)p->y, w, (int)p->h };
    return dst;
}

// Screen position of a sprite feature (POINTS) for the paddle as currently drawn, mirrored for the right player
void player_point(const Paddle *p, bool faces_right, PlayerPoint point, int *x, int *y) {
    SDL_Rect dst = sprite_rect(p, faces_right);
    float col = faces_right ? POINTS[point][0] : SPRITE_W - POINTS[point][0];
    *x = dst.x + (int)(col * SPRITE_SCALE);
    *y = dst.y + (int)(dst.h * POINTS[point][1] / SPRITE_H);
}

// The thrown racket, centered on (cx, cy) and turned by angle degrees
void draw_racket(SDL_Renderer *renderer, PlayerLook look, int cx, int cy, double angle) {
    if (!sprites_ok) return;
    int w = RACKET_W * RACKET_SCALE, h = RACKET_H * RACKET_SCALE;
    SDL_Rect dst = { cx - w / 2, cy - h / 2, w, h };
    SDL_RenderCopyEx(renderer, textures[look].racket, NULL, &dst, angle, NULL, SDL_FLIP_NONE);
}

// Flashes blue while stunned; hidden, shadow included, while ghosted; sinks into the ground through a rift,
// tinted purple and cut off at the feet line
void draw_player(SDL_Renderer *renderer, const Paddle *p, PlayerLook look, bool faces_right) {
    float depth = vanish_depth(p);
    if (p->effect == BONUS_GHOST || depth >= 1.0f) return;
    bool flash = p->stun_timer > 0 && (p->stun_timer / 6) % 2;

    if (!sprites_ok) {
        if (flash) SDL_SetRenderDrawColor(renderer, 70, 110, 255, 255);
        else SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
        SDL_Rect r = { (int)p->x, (int)p->y, (int)p->w, (int)p->h };
        SDL_RenderFillRect(renderer, &r);
        return;
    }

    const PlayerTextures *t = &textures[look];
    int left, right;
    arm_poses(p, &left, &right);
    SDL_Texture *stack[4] = { p->headless ? t->headless : t->body, t->legs[step_frame(p)], t->left_arm[left], t->arm[right] };
    SDL_Rect dst = sprite_rect(p, faces_right);
    float feet_col = faces_right ? FEET_COL : SPRITE_W - FEET_COL;
    int ground = dst.y + (int)(dst.h * FEET_ROW / SPRITE_H);
    if (depth > 0.0f) {
        SDL_Rect above = { 0, 0, SCREEN_WIDTH, ground };
        SDL_RenderSetClipRect(renderer, &above);
        dst.y += (int)(depth * dst.h);
    } else {
        draw_shadow(renderer, dst.x + (int)(feet_col * SPRITE_SCALE) + SHADOW_DX, ground);
    }
    for (int i = 0; i < 4; i++) {
        if (depth > 0.0f) SDL_SetTextureColorMod(stack[i], 200, 140, 255);
        else if (flash) SDL_SetTextureColorMod(stack[i], 90, 130, 255);
        else SDL_SetTextureColorMod(stack[i], 255, 255, 255);
        SDL_RenderCopyEx(renderer, stack[i], NULL, &dst, 0.0, NULL, faces_right ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL);
    }
    if (depth > 0.0f) SDL_RenderSetClipRect(renderer, NULL);
}

// Display name for the win screen
const char *player_name(PlayerLook look) {
    return look == PLAYER_AGASSI ? "AGASSI" : "NADAL";
}
