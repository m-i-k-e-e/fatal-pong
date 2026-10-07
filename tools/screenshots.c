// Renders promo screenshots with the real game code on the host, off-screen, as BMP frames in
// target/frames: the start screen, the pause screen, the win screen, a hadouken, a rift, the mole, the
// earthquake, the frog rain, a fatality and a whole match ending. `make screenshots` builds and runs it, then
// turns the frames into assets/screenshots/: start, pause, win, mole, earthquake, frog-rain and finale .png, and
// hadouken, rift, mole, earthquake, frog-rain, win, fatality and finale .gif.
#include "SDL2/SDL.h"
#include <stdio.h>
#include "game.h"
#include "ball.h"
#include "bonus.h"
#include "fatality.h"
#include "hud.h"
#include "paddle.h"
#include "particles.h"
#include "pause.h"
#include "players.h"
#include "rift.h"
#include "fireball.c"       // Pulled in whole for its file-static throw_fireball(): the real throw needs a controller
#include "calamity.c"       // Likewise, to aim the rally at the molehills and cracks

#define FINALE_MAX_FRAMES   (20 * 60)        // Sanity cap on the finale scene

static SDL_Surface *surf;
static SDL_Renderer *renderer;

// Same layering as main.c, including the fatality's stains, flying blood and impact shake, and tremors
static void draw_scene(const Paddle *p1, const Paddle *p2, const Ball *ball) {
    int sx, sy, qx, qy;
    fatality_shake(&sx, &sy);
    calamity_shake(&qx, &qy);
    sx += qx;
    sy += qy;
    if (sx || sy) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_Rect shaken = { sx, sy, SCREEN_WIDTH, SCREEN_HEIGHT };
        SDL_RenderSetViewport(renderer, &shaken);
    }
    draw_court(renderer);
    draw_fatality_stains(renderer);
    draw_hud(renderer, p1, p2);
    draw_bonuses(renderer);
    draw_calamity_ground(renderer);
    draw_rifts(renderer);
    draw_particles(renderer);
    draw_player(renderer, p1, PLAYER_AGASSI, true, false);
    draw_player(renderer, p2, PLAYER_NADAL, false, false);
    draw_fireballs(renderer);
    draw_ball(renderer, ball);
    draw_calamity_sky(renderer);
    draw_fatality_front(renderer);
    if (sx || sy) SDL_RenderSetViewport(renderer, NULL);
}

// Rally AI for the scripted scenes: go for the ball when it's coming, otherwise drift back to the middle
static float track(const Paddle *p, const Ball *ball, bool coming) {
    float target = coming ? ball->y + ball->size / 2 : SCREEN_HEIGHT / 2.0f;
    return SDL_clamp((target - (p->y + p->h / 2)) / 60.0f, -1.0f, 1.0f) * (coming ? 1.0f : 0.3f);
}

// The first standing molehill or crack other than `skip`, or NULL
static Obstacle *standing_obstacle(const Obstacle *skip) {
    for (int i = 0; i < CALAMITY_MAX; i++)
        if (obstacles[i].state == OB_UP && &obstacles[i] != skip) return &obstacles[i];
    return NULL;
}

// Write the current frame to `path` as a BMP
static void save(const char *path) {
    SDL_RenderPresent(renderer);
    SDL_SaveBMP(surf, path);
}

// A rally during a calamity, saved as <name>_NNN.bmp at 20 fps plus a still <name>.bmp at frame `still_at`
static void calamity_scene(void (*start_calamity)(void), const char *name, int still_at) {
    Paddle p1 = make_paddle(PADDLE_MARGIN), p2 = make_paddle(SCREEN_WIDTH - PADDLE_MARGIN - PADDLE_WIDTH);
    Ball ball = { 0, 0, 0, 0, BALL_SIZE };
    p1.score = 3; p2.score = 4;
    reset_fireballs();
    for (int i = 0; i < 60; i++) update_particles();
    reset_ball(&ball, 1);
    reset_calamities();
    start_calamity();
    int frame = 0, aims = 0, last_hits = paddle_hits, aim_after = 240;
    Obstacle *aimed = NULL;
    char path[64];
    for (int t = 0; t < 560; t++) {
        move_paddle(&p1, track(&p1, &ball, ball.vx < 0));
        move_paddle(&p2, track(&p2, &ball, ball.vx > 0));
        update_ball(&ball, &p1, &p2);
        if (paddle_hits != last_hits) {                                 // Just returned: maybe aim it at an obstacle
            last_hits = paddle_hits;
            Obstacle *o = aims < 2 && t >= aim_after ? standing_obstacle(aimed) : NULL;
            if (o && (ball.vx > 0) == (o->x > ball.x)) {
                float bx = ball.x + ball.size / 2, by = ball.y + ball.size / 2;
                float aim_y = o->type == CALAMITY_MOLE ? o->y - 6 : o->type == CALAMITY_FROGS ? o->y - FROG_BODY_Y : o->y;
                ball.vy = (aim_y - by) / (o->x - bx) * ball.vx;
                aimed = o;
                aims++;
                aim_after = t + 90;
            }
        }
        update_calamities(&ball);
        if (ball.x < -ball.size || ball.x > SCREEN_WIDTH) reset_ball(&ball, ball.x < 0 ? 1 : -1);
        update_particles();
        bool still = t == still_at;
        if (t % 3 && !still) continue;                                  // 20 fps
        draw_scene(&p1, &p2, &ball);
        draw_calamity_title(renderer);
        if (still) {
            snprintf(path, sizeof(path), "target/frames/%s.bmp", name);
            save(path);
        }
        if (t % 3) continue;
        snprintf(path, sizeof(path), "target/frames/%s_%03d.bmp", name, frame++);
        save(path);
    }
    reset_calamities();
}

// Render every scene to target/frames (see the top of this file); the Makefile turns them into PNGs and GIFs
int main(void) {
    srand(7);
    surf = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_WIDTH, SCREEN_HEIGHT, 32, SDL_PIXELFORMAT_ARGB8888);
    renderer = SDL_CreateSoftwareRenderer(surf);
    init_player_sprites(renderer);
    init_court(renderer);
    init_calamities(renderer);
    init_bonus_icons(renderer);

    // Start screen
    Paddle p1 = make_paddle(PADDLE_MARGIN), p2 = make_paddle(SCREEN_WIDTH - PADDLE_MARGIN - PADDLE_WIDTH);
    Ball ball = { 0, 0, 0, 0, BALL_SIZE };
    reset_ball(&ball, 1);
    draw_scene(&p1, &p2, &ball);
    draw_start_screen(renderer, 3000, PLAYER_AGASSI, PLAYER_SHARAPOVA);                   // Late enough for the title drips to have run
    save("target/frames/start.bmp");

    // Pause: the help panel over a match in progress, Agassi holding a bonus
    p1.score = 4; p2.score = 3;
    p1.y = 300; p2.y = 600;
    p1.effect = BONUS_ZIGZAG; p1.effect_timer = BONUS_DURATION * 2 / 3;
    ball.x = 1240; ball.y = 420;
    draw_scene(&p1, &p2, &ball);
    draw_pause_menu(renderer);
    save("target/frames/pause.bmp");
    p1 = make_paddle(PADDLE_MARGIN);
    p2 = make_paddle(SCREEN_WIDTH - PADDLE_MARGIN - PADDLE_WIDTH);

    // Hadouken: Agassi charges, throws, the fireball crosses while the rally goes on and freezes Nadal
    p1.score = 4; p2.score = 3;
    p1.y = p2.y = 420;
    ball.x = 1000; ball.y = 100; ball.vx = -11; ball.vy = 2.5f;     // Rally kept above the fireball's path
    int frame = 0;
    char path[64];
    for (int t = 0; t < 150; t++) {
        if (t < 12) p1.motion_state = 2;                                // Down, forward entered: charging
        if (t == 12) { p1.motion_state = 0; p1.throw_timer = THROW_DURATION; throw_fireball(0, &p1); }
        float p2_dir = (t > 30 && t < 50 && p2.stun_timer == 0) ? 0.25f : 0.0f;   // Nadal shuffles into it
        float p1_dir = ball.vx < 0 ? (ball.y > p1.y + p1.h / 2 ? 0.6f : -0.6f) : 0.0f;
        if (t < 20) p1_dir = 0;
        move_paddle(&p1, p1_dir);
        move_paddle(&p2, p2_dir);
        update_fireballs(&p1, &p2, &ball);
        update_ball(&ball, &p1, &p2);
        update_particles();
        if (t % 2 == 0) {                                               // 30 fps GIF
            draw_scene(&p1, &p2, &ball);
            snprintf(path, sizeof(path), "target/frames/hadouken_%03d.bmp", frame++);
            save(path);
        }
    }

    // Rift: Nadal opens one under Agassi, who steps off in time; the second one swallows him and the
    // ball goes straight past where he stood
    p1 = make_paddle(PADDLE_MARGIN);
    p2 = make_paddle(SCREEN_WIDTH - PADDLE_MARGIN - PADDLE_WIDTH);
    p1.score = 5; p2.score = 5;
    p1.y = 430; p2.y = 420;
    reset_fireballs();
    for (int i = 0; i < 60; i++) update_particles();
    ball.x = -100; ball.vx = ball.vy = 0;
    frame = 0;
    for (int t = 0; t < 216; t++) {
        if ((t < 8) || (t >= 60 && t < 68)) p2.motion_state = 3;        // Down, back entered: charging
        if (t == 8 || t == 68) { p2.motion_state = 0; p2.throw_timer = THROW_DURATION; open_rift(1, &p1); }
        if (t == 60) { ball.x = 1400; ball.y = 300; ball.vx = -15; ball.vy = 0.4f; }
        move_paddle(&p1, t >= 22 && t < 34 ? -1.0f : 0.0f);           // Reacts and dodges the first one
        move_paddle(&p2, 0);
        update_rifts();
        update_ball(&ball, &p1, &p2);
        if (ball.x < 0 && ball.vx < 0) { p2.score++; ball.x = -100; ball.vx = ball.vy = 0; }
        update_particles();
        if (t % 2 == 0) {
            draw_scene(&p1, &p2, &ball);
            snprintf(path, sizeof(path), "target/frames/rift_%03d.bmp", frame++);
            save(path);
        }
    }
    reset_rifts();

    // Calamities: each is announced mid-rally and puts its obstacles down; two returns are aimed through
    // obstacles, which knock the ball off course
    calamity_scene(start_mole, "mole", 96);                            // Title up, the mole out of its first hill
    calamity_scene(start_earthquake, "earthquake", 70);                // Title up, the first crack splitting
    calamity_scene(start_frog_rain, "frog-rain", 100);                 // Title up, the first big frog landed

    // Fatality: match point, Agassi taps Triangle three times, throws, Nadal loses his head
    p1 = make_paddle(PADDLE_MARGIN);
    p2 = make_paddle(SCREEN_WIDTH - PADDLE_MARGIN - PADDLE_WIDTH);
    p1.score = 10; p2.score = 6;
    p1.y = 430; p2.y = 380;
    reset_fireballs();
    for (int i = 0; i < 60; i++) update_particles();
    ball.x = -100;
    frame = 0;
    int presses = 0;
    for (int t = 0; t < 60 + 2 * FATALITY_LENGTH; t++) {
        if (t == 14 || t == 24 || t == 34) presses++;
        if (t == 34) fatality_start(&p1, PLAYER_AGASSI, true, &p2, PLAYER_NADAL);
        fatality_update();
        if (t > 34 && !fatality_active()) break;
        if (t % 3) continue;                                            // 20 fps keeps the GIF small
        draw_scene(&p1, &p2, &ball);
        if (t <= 34) draw_finish_screen(renderer, t, FATALITY_WINDOW - t, FATALITY_WINDOW, presses, FATALITY_PRESSES, false);
        else draw_fatality_screen(renderer, fatality_since_impact());
        snprintf(path, sizeof(path), "target/frames/fatality_%03d.bmp", frame++);
        save(path);
    }
    fatality_reset();

    // Finale, the way main.c sequences the end of a match: at 9-6 Agassi returns the ball past Nadal for the
    // last point, then FINISH HIM, Triangle x3, the fatality, and the result screen over the carnage
    p1 = make_paddle(PADDLE_MARGIN);
    p2 = make_paddle(SCREEN_WIDTH - PADDLE_MARGIN - PADDLE_WIDTH);
    p1.score = 9; p2.score = 6;
    p1.y = 430; p2.y = 470;
    reset_fireballs();
    for (int i = 0; i < 60; i++) update_particles();
    speed_scale = 1.6f;                                                 // Late in a rally
    ball.x = 420; ball.y = 430; ball.vx = -19; ball.vy = -1.0f; ball.zigzag = false;   // Off Agassi's top edge, steep
    frame = 0;
    int phase = 0, end_timer = 0;                                       // 0 rally, 1 finish, 2 fatality, 3 result
    presses = 0;
    for (int t = 0; phase < 3 || end_timer <= 150; t++) {
        if (t > FINALE_MAX_FRAMES) {               // The scripted point went wrong: fail instead of filling the disk
            fprintf(stderr, "finale: last point never scored (phase %d)\n", phase);
            return 1;
        }
        if (phase == 0) {
            float p2_dir = ball.vx > 0 && t > 30 && p2.y > 300 ? -0.6f : 0.0f;    // Nadal guesses wrong
            move_paddle(&p1, 0);
            move_paddle(&p2, p2_dir);
            update_ball(&ball, &p1, &p2);
            if (ball.x + ball.size > SCREEN_WIDTH) { p1.score++; phase = 1; end_timer = 0; ball.x = SCREEN_WIDTH + 100; ball.vx = ball.vy = 0; }
        } else {
            end_timer++;
            fatality_update();
            if (phase == 1 && (end_timer == 30 || end_timer == 42 || end_timer == 54) && ++presses == FATALITY_PRESSES) {
                fatality_start(&p1, PLAYER_AGASSI, true, &p2, PLAYER_NADAL);
                phase = 2;
            } else if (phase == 2 && !fatality_active()) {
                phase = 3;
                end_timer = 0;
            }
        }
        update_particles();
        bool still = phase == 3 && end_timer == 120;
        if (t % 3 && !still) continue;                                  // 20 fps
        draw_scene(&p1, &p2, &ball);
        if (phase == 1) draw_finish_screen(renderer, end_timer, FATALITY_WINDOW - end_timer, FATALITY_WINDOW, presses, FATALITY_PRESSES, false);
        else if (phase == 2) draw_fatality_screen(renderer, fatality_since_impact());
        else if (phase == 3) draw_win_screen(renderer, player_name(PLAYER_AGASSI), p1.score, p2.score, end_timer >= 90, end_timer);
        if (still) save("target/frames/finale.bmp");
        if (t % 3) continue;
        snprintf(path, sizeof(path), "target/frames/finale_%03d.bmp", frame++);
        save(path);
    }
    fatality_reset();

    // Win screen: Agassi takes it 10-0 and celebrates while the title drips; the still is taken once the
    // drips have grown
    p1 = make_paddle(PADDLE_MARGIN);
    p2 = make_paddle(SCREEN_WIDTH - PADDLE_MARGIN - PADDLE_WIDTH);
    p1.score = 10; p2.score = 0;
    reset_fireballs();
    for (int i = 0; i < 60; i++) update_particles();
    ball.x = -100;
    frame = 0;
    for (int t = 0; t <= 200; t++) {
        if (p1.swing_timer > 0) p1.swing_timer--;
        else p1.swing_timer = SWING_DURATION;
        if (t % 2 && t != 200) continue;
        draw_scene(&p1, &p2, &ball);
        draw_win_screen(renderer, player_name(PLAYER_AGASSI), p1.score, p2.score, t >= 90, t);
        if (t == 200) save("target/frames/win.bmp");
        else {
            snprintf(path, sizeof(path), "target/frames/win_%03d.bmp", frame++);
            save(path);
        }
    }

    free_player_sprites();
    free_calamities();
    free_bonus_icons();
    free_court();
    return 0;
}
