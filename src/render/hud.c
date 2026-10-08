#include "render/hud.h"
#include "gameplay/bonus.h"
#include "core/text.h"
#include "core/input.h"
#include <string.h>
#include <stdio.h>

// Minimal 3x5 font renderer for scores
static const unsigned char FONT_3x5[10][5] = {
    {0x7, 0x5, 0x5, 0x5, 0x7}, // 0
    {0x2, 0x6, 0x2, 0x2, 0x7}, // 1
    {0x7, 0x1, 0x7, 0x4, 0x7}, // 2
    {0x7, 0x1, 0x7, 0x1, 0x7}, // 3
    {0x5, 0x5, 0x7, 0x1, 0x1}, // 4
    {0x7, 0x4, 0x7, 0x1, 0x7}, // 5
    {0x7, 0x4, 0x7, 0x5, 0x7}, // 6
    {0x7, 0x1, 0x2, 0x2, 0x2}, // 7
    {0x7, 0x5, 0x7, 0x5, 0x7}, // 8
    {0x7, 0x5, 0x7, 0x1, 0x7}  // 9
};

// Score digits in the 3x5 font at (x, y), `pixel_size` screen pixels per font pixel, in the current color
static void draw_number(SDL_Renderer *renderer, int number, int x, int y, int pixel_size) {
    if (number < 0) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", number);

    int cur_x = x;
    for (int i = 0; buf[i] != '\0'; i++) {
        int digit = buf[i] - '0';
        if (digit < 0 || digit > 9) continue;

        for (int r = 0; r < 5; r++) {
            for (int c = 0; c < 3; c++) {
                if ((FONT_3x5[digit][r] >> (2 - c)) & 1) {
                    SDL_Rect rect = { cur_x + c * pixel_size, y + r * pixel_size, pixel_size, pixel_size };
                    SDL_RenderFillRect(renderer, &rect);
                }
            }
        }
        cur_x += (3 + 1) * pixel_size;
    }
}

// --- Grass court ---
// Painted once into a texture at startup (per-pixel work is too slow for the software renderer every
// frame): mowing stripes, blade texture, worn baselines where the players stand, lines and the net.
#define STRIPE_W        160
#define LINE_W          6
#define SIDELINE_INSET  12      // Doubles sidelines and baselines, just inside the screen edges
#define SINGLES_Y       150     // Singles sidelines
#define SERVICE_DX      520     // Service lines, from the net

static SDL_Texture *court_tex;

// Deterministic hash of a pixel position, for the grass texture's random-looking variation
static Uint32 noise(int x, int y) {
    Uint32 h = (Uint32)x * 374761393u + (Uint32)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// Clamp to a 0..255 color channel
static Uint8 clamp8(int v) {
    return v < 0 ? 0 : v > 255 ? 255 : (Uint8)v;
}

// 0..1, strongest just in front of each baseline and toward the middle of it
static float wear(int x, int y) {
    int from_edge = x < SCREEN_WIDTH / 2 ? x : SCREEN_WIDTH - 1 - x;
    float across = 1.0f - SDL_fabsf(from_edge - 95.0f) / 110.0f;
    float along = 1.0f - SDL_fabsf(y - SCREEN_HEIGHT / 2.0f) / 650.0f;
    if (across <= 0 || along <= 0) return 0;
    float patchy = 0.55f + 0.45f * ((noise(x / 6, y / 6) & 255) / 255.0f);
    return across * along * patchy;
}

// Fill a rectangle of the court surface
static void fill(SDL_Surface *s, int x, int y, int w, int h, Uint32 color) {
    SDL_Rect r = { x, y, w, h };
    SDL_FillRect(s, &r, color);
}

// Paint the grass court once into a texture: mown stripes, blade noise, worn patches by the baselines,
// white lines and the net
void init_court(SDL_Renderer *renderer) {
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_WIDTH, SCREEN_HEIGHT, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return;
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        Uint32 *row = (Uint32 *)((Uint8 *)s->pixels + y * s->pitch);
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            bool light = (x / STRIPE_W) % 2 == 0;
            int r = light ? 80 : 62, g = light ? 160 : 140, b = light ? 66 : 56;
            Uint32 n = noise(x, y);
            int jitter = (int)(n & 15) - 8;
            if ((n >> 8) % 23 == 0) jitter -= 18;           // Darker blades
            r += jitter / 2; g += jitter; b += jitter / 2;
            float w = wear(x, y);                           // Worn toward sandy soil
            r += (int)((168 - r) * w); g += (int)((146 - g) * w); b += (int)((96 - b) * w);
            row[x] = SDL_MapRGB(s->format, clamp8(r), clamp8(g), clamp8(b));
        }
    }

    Uint32 white = SDL_MapRGB(s->format, 236, 240, 232);
    int left = SIDELINE_INSET, right = SCREEN_WIDTH - SIDELINE_INSET - LINE_W;
    int top = SIDELINE_INSET, bottom = SCREEN_HEIGHT - SIDELINE_INSET - LINE_W;
    int cx = SCREEN_WIDTH / 2, cy = SCREEN_HEIGHT / 2;
    fill(s, left, top, right - left + LINE_W, LINE_W, white);                       // Doubles sidelines
    fill(s, left, bottom, right - left + LINE_W, LINE_W, white);
    fill(s, left, top, LINE_W, bottom - top + LINE_W, white);                       // Baselines
    fill(s, right, top, LINE_W, bottom - top + LINE_W, white);
    fill(s, left, SINGLES_Y, right - left, LINE_W, white);                          // Singles sidelines
    fill(s, left, SCREEN_HEIGHT - SINGLES_Y - LINE_W, right - left, LINE_W, white);
    int service_h = SCREEN_HEIGHT - 2 * SINGLES_Y;
    fill(s, cx - SERVICE_DX, SINGLES_Y, LINE_W, service_h, white);                  // Service lines
    fill(s, cx + SERVICE_DX - LINE_W, SINGLES_Y, LINE_W, service_h, white);
    fill(s, cx - SERVICE_DX, cy - LINE_W / 2, 2 * SERVICE_DX, LINE_W, white);       // Center service line

    // Net: dark mesh with a white tape down the middle
    Uint32 mesh = SDL_MapRGB(s->format, 30, 34, 30), cord = SDL_MapRGB(s->format, 70, 76, 70);
    fill(s, cx - 7, 0, 14, SCREEN_HEIGHT, mesh);
    for (int y = 0; y < SCREEN_HEIGHT; y += 6) fill(s, cx - 7, y, 14, 1, cord);
    for (int x = cx - 7; x < cx + 7; x += 6) fill(s, x, 0, 1, SCREEN_HEIGHT, cord);
    fill(s, cx - 3, 0, 6, SCREEN_HEIGHT, white);

    court_tex = SDL_CreateTextureFromSurface(renderer, s);
    SDL_FreeSurface(s);
}

// Destroy the court texture
void free_court(void) {
    if (court_tex) SDL_DestroyTexture(court_tex);
    court_tex = NULL;
}

// Falls back to the plain dark court with a dashed center line if the texture couldn't be made
void draw_court(SDL_Renderer *renderer) {
    if (court_tex) {
        SDL_RenderCopy(renderer, court_tex, NULL, NULL);
        return;
    }
    SDL_SetRenderDrawColor(renderer, 14, 14, 18, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
    for (int y = 0; y < SCREEN_HEIGHT; y += 40) {
        SDL_Rect dash = { SCREEN_WIDTH / 2 - 2, y, 4, 20 };
        SDL_RenderFillRect(renderer, &dash);
    }
}

// Scores with each player's active bonus underneath
void draw_hud(SDL_Renderer *renderer, const Paddle *p1, const Paddle *p2) {
    SDL_SetRenderDrawColor(renderer, 20, 40, 20, 255);       // Shadow, for contrast on the grass
    draw_number(renderer, p1->score, SCREEN_WIDTH / 2 - 160 + 5, 65, 10);
    draw_number(renderer, p2->score, SCREEN_WIDTH / 2 + 100 + 5, 65, 10);
    SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
    draw_number(renderer, p1->score, SCREEN_WIDTH / 2 - 160, 60, 10);
    draw_number(renderer, p2->score, SCREEN_WIDTH / 2 + 100, 60, 10);

    draw_effect_indicator(renderer, p1, SCREEN_WIDTH / 2 - 160, 130);
    draw_effect_indicator(renderer, p2, SCREEN_WIDTH / 2 + 100, 130);
}

// One side of the character select: "PLAYER n" over the chosen player's portrait between "<" and ">", the name
// underneath. Centered on `cx`; the right-hand player faces left as in game.
static void draw_player_card(SDL_Renderer *renderer, int n, PlayerLook look, int cx, bool faces_right) {
    const int scale = 4, top = 350, w = 32 * scale, h = 80 * scale;
    char label[16];
    snprintf(label, sizeof(label), "PLAYER %d", n);
    SDL_SetRenderDrawColor(renderer, 160, 160, 170, 255);
    draw_text_centered(renderer, label, cx, top - 50, 5);
    draw_player_portrait(renderer, look, cx - w / 2, top, scale, faces_right);
    SDL_SetRenderDrawColor(renderer, 0, 220, 255, 255);
    draw_text(renderer, "<", cx - w / 2 - 90, top + h / 2 - 28, 8);
    draw_text(renderer, ">", cx + w / 2 + 50, top + h / 2 - 28, 8);
    SDL_SetRenderDrawColor(renderer, 255, 140, 0, 255);
    draw_text_centered(renderer, player_name(look), cx, top + h + 30, 7);
}

// Title and character select over the dimmed, frozen court until player 1 presses Cross: each player's pick
// with "VS" between them
void draw_start_screen(SDL_Renderer *renderer, Uint32 ticks, PlayerLook p1_look, PlayerLook p2_look) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 170);
    SDL_RenderFillRect(renderer, NULL);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    // "FATAL" in dripping blood like the win screen, "PONG" in white
    const int scale = 14, title_y = 110;
    int frames = (int)(ticks * 60 / 1000);
    int x0 = SCREEN_WIDTH / 2 - text_width("FATAL PONG", scale) / 2;
    int pong_x = x0 + 6 * (TEXT_GLYPH_W + 1) * scale;
    draw_blood_text(renderer, "FATAL", x0 + text_width("FATAL", scale) / 2, title_y, scale, frames);
    SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255);
    draw_text(renderer, "PONG", pong_x + 7, title_y + 7, scale);
    SDL_SetRenderDrawColor(renderer, 240, 240, 245, 255);
    draw_text(renderer, "PONG", pong_x, title_y, scale);

    draw_player_card(renderer, 1, p1_look, SCREEN_WIDTH / 2 - 420, true);
    draw_player_card(renderer, 2, p2_look, SCREEN_WIDTH / 2 + 420, false);
    draw_blood_text(renderer, "VS", SCREEN_WIDTH / 2, 470, 12, frames);

    if ((ticks / 500) % 2 == 0) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        draw_text_centered(renderer, "PLAYER 1: PRESS " KEY_HINT("X", "SPACE") " TO START", SCREEN_WIDTH / 2, 860, 6);
    }
    SDL_SetRenderDrawColor(renderer, 160, 160, 170, 255);
    draw_text_centered(renderer, "LEFT / RIGHT: CHOOSE PLAYER      " KEY_HINT("OPTIONS", "ESC") ": HOW TO PLAY",
                       SCREEN_WIDTH / 2, 960, 4);
}

// --- Dripping text (blood, mud, crumbling rock, slime) ---
// Letters drawn block by block from the 5x7 font: a pulsing dark glow, a dark outline, a wet fill
// darkening toward the bottom, and drips that run from the letters' undersides and shed falling drops.

typedef struct {
    SDL_Color glow, outline;
    SDL_Color fill;         // Top row; each row down is `darken` darker per channel
    int darken;
    SDL_Color shine;        // Wet highlight on top edges
    SDL_Color drip, drip_shine;
    float drip_length;      // Scales how far drips run
} DripStyle;

static const DripStyle BLOOD = {
    { 120, 0, 0, 0 }, { 8, 0, 0, 255 }, { 205, 10, 10, 255 }, 14,
    { 255, 90, 80, 255 }, { 115, 0, 0, 255 }, { 175, 20, 20, 255 }, 1.0f,
};
static const DripStyle MUD = {
    { 70, 40, 10, 0 }, { 26, 15, 6, 255 }, { 150, 102, 56, 255 }, 10,
    { 196, 156, 106, 255 }, { 92, 60, 30, 255 }, { 128, 88, 50, 255 }, 1.0f,
};
static const DripStyle SLIME = {                            // Long, stringy green drips
    { 40, 110, 20, 0 }, { 14, 34, 10, 255 }, { 120, 204, 72, 255 }, 11,
    { 206, 250, 150, 255 }, { 72, 146, 44, 255 }, { 150, 220, 100, 255 }, 1.3f,
};
static const DripStyle ROCK = {                             // Crumbling: short stubs shedding grit
    { 40, 36, 30, 0 }, { 22, 20, 18, 255 }, { 158, 152, 140, 255 }, 9,
    { 214, 208, 196, 255 }, { 96, 90, 82, 255 }, { 130, 124, 114, 255 }, 0.25f,
};

typedef struct {
    int x, y, w;        // Top-left under the letter block, width
    float len;          // Current length
    float drop_y;       // Falling droplet below the tip, < 0 when none
} Drip;

// Integer hash (good avalanche) seeding each letter block's drip
static Uint32 mix(Uint32 h) {
    h ^= h >> 16; h *= 0x7feb352d; h ^= h >> 15; h *= 0x846ca68b;
    return h ^ (h >> 16);
}

// Deterministic per block so drips stay put from frame to frame; false if this block doesn't drip
static bool drip_at(int block_x, int block_y, int scale, int frames, Uint32 seed, float length, Drip *d) {
    Uint32 h = mix(seed);
    if (h % 5 >= 2) return false;                           // About 2 in 5 undersides drip
    int max_len = (int)((25 + (int)((h >> 3) % 85)) * length) + 1;
    int delay = (int)((h >> 9) % 45);
    float speed = 1.0f + ((h >> 15) % 16) / 10.0f;
    d->w = 4 + (int)((h >> 20) % 5);
    d->x = block_x + 2 + (int)((h >> 24) % (scale - d->w - 3));
    d->y = block_y + scale;
    float t = frames - delay;
    d->len = SDL_clamp(t * speed, 0.0f, (float)max_len);
    d->drop_y = -1;
    if (d->len >= max_len) {                                // Fully grown: a drop falls now and then
        float grown_at = delay + max_len / speed;
        int period = 60 + (int)((h >> 5) % 70);
        float since = SDL_fmodf(frames - grown_at, (float)period);
        float fall = 0.18f * since * since;
        if (fall < 500) d->drop_y = fall;
    }
    return true;
}

// Fill a rectangle in the current draw color
static void block(SDL_Renderer *r, int x, int y, int w, int h) {
    SDL_Rect rect = { x, y, w, h };
    SDL_RenderFillRect(r, &rect);
}

// A drip with its swollen tip and any falling drop, grown by `grow` pixels on every side (for its outline)
static void draw_drip(SDL_Renderer *r, const Drip *d, int grow) {
    int len = (int)d->len;
    if (len > 0) {
        block(r, d->x - grow, d->y - grow, d->w + 2 * grow, len + 2 * grow);
        block(r, d->x - 1 - grow, d->y + len - 3 - grow, d->w + 2 + 2 * grow, d->w + 2 * grow);   // Swollen tip
    }
    if (d->drop_y >= 0) {
        int dy = d->y + len + 4 + (int)d->drop_y;
        block(r, d->x - grow, dy - grow, d->w + 2 * grow, d->w + 3 + 2 * grow);
    }
}

static void draw_drip_text(SDL_Renderer *r, const char *s, int center_x, int y, int scale, int frames,
                           const DripStyle *st) {
    int x0 = center_x - text_width(s, scale) / 2, n = (int)strlen(s);
    unsigned char rows[TEXT_GLYPH_H];

    // Passes: 0 glow, 1 black outline, 2 drips, 3 fill. Drips hang from the lowest block of each letter column.
    for (int pass = 0; pass < 4; pass++) {
        if (pass == 0) {
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, st->glow.r, st->glow.g, st->glow.b, (Uint8)(26 + 14 * SDL_sinf(frames * 0.08f)));
        } else if (pass == 1) {
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(r, st->outline.r, st->outline.g, st->outline.b, 255);
        }
        for (int i = 0; i < n; i++) {
            if (!text_glyph(s[i], rows)) continue;
            for (int row = 0; row < TEXT_GLYPH_H; row++) {
                for (int col = 0; col < TEXT_GLYPH_W; col++) {
                    if (!((rows[row] >> (TEXT_GLYPH_W - 1 - col)) & 1)) continue;
                    int bx = x0 + (i * (TEXT_GLYPH_W + 1) + col) * scale, by = y + row * scale;
                    bool above = row > 0 && ((rows[row - 1] >> (TEXT_GLYPH_W - 1 - col)) & 1);
                    bool below = row < TEXT_GLYPH_H - 1 && ((rows[row + 1] >> (TEXT_GLYPH_W - 1 - col)) & 1);
                    bool lowest = true;                                 // Nothing further down this column
                    for (int k = row + 1; k < TEXT_GLYPH_H; k++)
                        if ((rows[k] >> (TEXT_GLYPH_W - 1 - col)) & 1) lowest = false;
                    bool left = col > 0 && ((rows[row] >> (TEXT_GLYPH_W - col)) & 1);
                    Drip d;
                    bool drips = lowest && drip_at(bx, by, scale, frames, (Uint32)(i * 131 + row * 17 + col * 7), st->drip_length, &d);
                    if (pass == 0) {
                        block(r, bx - scale / 2, by - scale / 2, scale * 2, scale * 2);
                    } else if (pass == 1) {
                        block(r, bx - 4, by - 4, scale + 8, scale + 8);
                        if (drips) draw_drip(r, &d, 3);
                    } else if (pass == 3) {
                        int dk = row * st->darken;                      // Darker toward the bottom
                        int fr = SDL_max(st->fill.r - dk, 0), fg = SDL_max(st->fill.g - dk, 0), fb = SDL_max(st->fill.b - dk, 0);
                        SDL_SetRenderDrawColor(r, fr, fg, fb, 255);
                        block(r, bx, by, scale, scale);
                        if (!below) {                                   // Shadowed underside
                            SDL_SetRenderDrawColor(r, fr / 2 + 20, fg / 2, fb / 2, 255);
                            block(r, bx, by + scale - 3, scale, 3);
                        }
                        if (!above) {                                   // Wet shine on top edges
                            SDL_SetRenderDrawColor(r, st->shine.r, st->shine.g, st->shine.b, 255);
                            block(r, bx + (left ? 0 : 3), by + 2, scale - (left ? 3 : 6), 3);
                        }
                    } else if (drips) {
                        SDL_SetRenderDrawColor(r, st->drip.r, st->drip.g, st->drip.b, 255);
                        draw_drip(r, &d, 0);
                        SDL_SetRenderDrawColor(r, st->drip_shine.r, st->drip_shine.g, st->drip_shine.b, 255);  // Highlight down the drip
                        Drip shine = d;
                        shine.w = 1;
                        shine.x += 1;
                        draw_drip(r, &shine, 0);
                    }
                }
            }
        }
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

// Dripping lettering, centered on center_x with its top at y; `frames` since it appeared drives the drips
// (see draw_drip_text()). Blood: the end-of-match titles.
void draw_blood_text(SDL_Renderer *r, const char *s, int center_x, int y, int scale, int frames) {
    draw_drip_text(r, s, center_x, y, scale, frames, &BLOOD);
}

// Mud: the mole's title
void draw_mud_text(SDL_Renderer *r, const char *s, int center_x, int y, int scale, int frames) {
    draw_drip_text(r, s, center_x, y, scale, frames, &MUD);
}

// Crumbling stone, short drips: the earthquake's title
void draw_rock_text(SDL_Renderer *r, const char *s, int center_x, int y, int scale, int frames) {
    draw_drip_text(r, s, center_x, y, scale, frames, &ROCK);
}

// Green slime, long drips: the frog rain's title
void draw_slime_text(SDL_Renderer *r, const char *s, int center_x, int y, int scale, int frames) {
    draw_drip_text(r, s, center_x, y, scale, frames, &SLIME);
}

// Mortal Kombat style result: "<NAME> WINS" in dripping blood over a blood-red dim, "FLAWLESS VICTORY"
// for a shutout. `frames` counts from the end of the match and drives the drips and the prompt blink.
void draw_win_screen(SDL_Renderer *renderer, const char *winner, int winner_score, int loser_score,
                     bool show_prompt, int frames) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 30, 0, 0, 190);
    SDL_RenderFillRect(renderer, NULL);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    char title[32];
    snprintf(title, sizeof(title), "%s WINS", winner);
    draw_blood_text(renderer, title, SCREEN_WIDTH / 2, 260, 16, frames);

    if (loser_score == 0) {
        SDL_SetRenderDrawColor(renderer, 255, 200, 40, 255);
        draw_text_centered(renderer, "FLAWLESS VICTORY", SCREEN_WIDTH / 2, 520, 7);
    }

    char score[16];
    snprintf(score, sizeof(score), "%d - %d", winner_score, loser_score);
    SDL_SetRenderDrawColor(renderer, 220, 220, 230, 255);
    draw_text_centered(renderer, score, SCREEN_WIDTH / 2, 620, 8);

    if (show_prompt && (frames / 30) % 2 == 0) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        draw_text_centered(renderer, "PRESS " KEY_HINT("X", "SPACE") " FOR A NEW MATCH", SCREEN_WIDTH / 2, 760, 5);
    }
}

// Match point: the winner has a few seconds to enter the fatality ("FINISH HER!" if `her`, the loser being Graf
// or Sharapova). Light tint so the players stay visible.
void draw_finish_screen(SDL_Renderer *renderer, int frames, int frames_left, int window, int presses, int needed,
                        bool her) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 40, 0, 0, 90);
    SDL_RenderFillRect(renderer, NULL);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    draw_blood_text(renderer, her ? "FINISH HER!" : "FINISH HIM!", SCREEN_WIDTH / 2, 190, 14, frames);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
#ifdef __PROSPERO__
    const char *label = "TRIANGLE X3";
#else
    const char *label = "TRIANGLE (G OR RIGHT SHIFT) X3";
#endif
    int label_w = text_width(label, 4);
    int boxes_w = needed * 40 - 10, x = SCREEN_WIDTH / 2 - (label_w + 30 + boxes_w) / 2;
    draw_text(renderer, label, x, 430, 4);
    for (int i = 0; i < needed; i++) {                      // One box per press, filled as they land
        SDL_Rect box = { x + label_w + 30 + i * 40, 426, 30, 30 };
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(renderer, &box);
        SDL_Rect inner = { box.x + 4, box.y + 4, 22, 22 };
        if (i < presses) SDL_SetRenderDrawColor(renderer, 210, 20, 20, 255);
        else SDL_SetRenderDrawColor(renderer, 30, 0, 0, 255);
        SDL_RenderFillRect(renderer, &inner);
    }

    // Countdown bar
    SDL_Rect frame = { SCREEN_WIDTH / 2 - 300, 490, 600, 18 };
    SDL_SetRenderDrawColor(renderer, 20, 0, 0, 255);
    SDL_RenderFillRect(renderer, &frame);
    SDL_Rect fill = { frame.x + 3, frame.y + 3, (frame.w - 6) * frames_left / window, frame.h - 6 };
    SDL_SetRenderDrawColor(renderer, 220, 30, 20, 255);
    SDL_RenderFillRect(renderer, &fill);
}

// Over the carnage: "FATALITY" drips in once the head is gone
void draw_fatality_screen(SDL_Renderer *renderer, int since_impact) {
    if (since_impact < 30) return;
    draw_blood_text(renderer, "FATALITY", SCREEN_WIDTH / 2, 230, 18, since_impact - 30);
}
