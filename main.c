#include "SDL2/SDL.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>

#include "game.h"
#include "audio.h"
#include "ball.h"
#include "bonus.h"
#include "fatality.h"
#include "fireball.h"
#include "input.h"
#include "rift.h"
#include "calamity.h"
#include "hud.h"
#include "paddle.h"
#include "pause.h"
#include "players.h"
#include "particles.h"

#define WIN_SCREEN_MIN_FRAMES   90      // Win screen shows at least 1.5 s before Cross starts a new match

#ifdef __PROSPERO__
// libkernel direct memory queries (no public header in the SDK)
size_t sceKernelGetDirectMemorySize(void);
int sceKernelAvailableDirectMemorySize(off_t start, off_t end, size_t align, off_t *phys_out, size_t *size_out);
#else
#define WINDOW_W                1280    // Desktop window; the 1920x1080 game is scaled to fit (F11: fullscreen)
#define WINDOW_H                720
#endif

// True while `p` is level with an invisible (ghost bonus) opponent: their vertical spans overlap, so it's
// staring the ghost in the face
static bool scared_of_ghost(const Paddle *p, const Paddle *ghost) {
    return ghost->effect == BONUS_GHOST && p->y < ghost->y + ghost->h && ghost->y < p->y + p->h;
}

// Fill the first free player slot. SDL also sends an "added" event for controllers already opened
// at startup, and reopening one returns the same handle, so skip it or one pad would drive both players.
static void open_pad(int device_index, SDL_GameController **pad1, SDL_GameController **pad2) {
    if (*pad1 && *pad2) return;
    SDL_GameController *pad = SDL_GameControllerOpen(device_index);
    if (!pad) return;
    if (pad == *pad1 || pad == *pad2) { SDL_GameControllerClose(pad); return; }
    if (!*pad1) *pad1 = pad;
    else *pad2 = pad;
}

// Set up SDL, the controllers and every module, then run the game at 60 FPS: start screen, play, pause,
// and after the last point the FINISH / FATALITY / RESULT phases, until touchpad + Options (or Q from the pause
// screen, or closing the window) quits
int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    printf("[pong] starting\n");
    srand((unsigned int)time(NULL));               // A different game every launch
#ifdef __PROSPERO__
    {
        off_t phys = 0;
        size_t avail = 0;
        int rc = sceKernelAvailableDirectMemorySize(0, (off_t)sceKernelGetDirectMemorySize(), 0x10000, &phys, &avail);
        printf("[pong] direct memory: total=%zu MiB, largest free block=%zu MiB (rc=0x%x)\n",
               sceKernelGetDirectMemorySize() >> 20, avail >> 20, rc);
    }
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
        printf("[pong] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

#ifdef __PROSPERO__
    SDL_Window *window = SDL_CreateWindow("Fatal Pong", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
#else
    SDL_Window *window = SDL_CreateWindow("Fatal Pong", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          WINDOW_W, WINDOW_H, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
#endif
    if (!window) {
        printf("[pong] SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

#ifdef __PROSPERO__
    // The PS5 SDL port only ships the software renderer (no GPU driver, no vsync flag)
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
#else
    // On a desktop the GPU renderer when there is one; the game always draws 1920x1080, scaled to the window
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (renderer) SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
#endif
    if (!renderer) {
        printf("[pong] SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_GameController *pad1 = NULL;
    SDL_GameController *pad2 = NULL;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) open_pad(i, &pad1, &pad2);
    }

    init_audio();
    init_player_sprites(renderer);
    init_court(renderer);
    init_calamities(renderer);
    init_bonus_icons(renderer);

    printf("[pong] video ok, %d joystick(s), pad1=%p pad2=%p\n", SDL_NumJoysticks(), (void *)pad1, (void *)pad2);

    Paddle p1 = make_paddle(PADDLE_MARGIN);
    Paddle p2 = make_paddle(SCREEN_WIDTH - PADDLE_MARGIN - PADDLE_WIDTH);

    Ball ball = { 0, 0, 0, 0, BALL_SIZE };
    reset_ball(&ball, 1);

    int winner = 0;
    bool running = true;
    bool paused = false;
    bool started = false;                          // Waits on the start screen for player 1's Cross
    PlayerLook looks[2] = { PLAYER_AGASSI, PLAYER_NADAL };  // Picked on the start screen
    int choice_was[2] = { 0, 0 };                  // Last frame's left/right, so each push moves one step
    bool cross_was_down = true;                    // So a Cross still held from launching the app doesn't start it
    // After the last point: FINISH (the winner may enter the fatality), FATALITY, then the RESULT screen
    enum { END_FINISH, END_FATALITY, END_RESULT } end_phase = END_FINISH;
    int end_timer = 0;                             // Frames since the current end phase began
    int triangle_presses = 0;
    bool triangle_was_down = true;
    bool pause_was_down[2] = { false, false };     // For press-edge detection of the pause button

    const Uint32 frame_ms = 1000 / 60;

    while (running) {
        Uint32 frame_start = SDL_GetTicks();
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_CONTROLLERDEVICEADDED) open_pad(e.cdevice.which, &pad1, &pad2);
#ifndef __PROSPERO__
            if (e.type == SDL_KEYDOWN && e.key.keysym.scancode == SDL_SCANCODE_F11 && !e.key.repeat) {
                bool full = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP;
                SDL_SetWindowFullscreen(window, full ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
            }
#endif
        }
        PlayerControls controls[2];
        read_controls(pad1, pad2, controls);

        // Options (Esc) toggles pause; touchpad click + Options quits (PS5 has no window close button, and the
        // SDK's pad driver never reports the Create/Share button), and so does Q from the pause screen
        bool toggle_pause = false;
        for (int i = 0; i < 2; i++) {
            if (controls[i].quit || (paused && controls[i].quit_key)) running = false;
            else if (controls[i].pause && !pause_was_down[i]) toggle_pause = true;
            pause_was_down[i] = controls[i].pause;
        }
        if (toggle_pause) paused = !paused;
        if (paused) update_pause_menu(controls);

        // Cross presses (edge) start the game from the start screen and a new match from the win screen
        bool cross = controls[0].confirm || (started && controls[1].confirm);
        bool cross_pressed = cross && !cross_was_down && !paused;
        cross_was_down = cross;
        if (!started && cross_pressed) started = true;
        bool playing = started && !paused;

        // Character select: each player cycles through the players with left/right (player 2 on the right
        // stick when sharing player 1's pad)
        if (!started && !paused) {
            for (int i = 0; i < 2; i++) {
                int dir = controls[i].left ? -1 : controls[i].right ? 1 : 0;
                if (dir && dir != choice_was[i]) {
                    looks[i] = (PlayerLook)((looks[i] + dir + PLAYER_COUNT) % PLAYER_COUNT);
                    play_sound(&snd_paddle_hit);
                }
                choice_was[i] = dir;
            }
        }

        // Special move motions (hadouken, rift)
        if (winner == 0 && playing) {
            update_special_input(&p1, &p2, true, &controls[0]);
            update_special_input(&p2, &p1, false, &controls[1]);
        }

        // Movement (with a single pad, P2 uses its right stick: see input.c)
        float p1_dir = controls[0].move, p2_dir = controls[1].move;

        // End of match
        if (winner != 0 && playing) {
            end_timer++;
            Paddle *champ = winner == 1 ? &p1 : &p2, *loser = winner == 1 ? &p2 : &p1;
            PlayerLook champ_look = looks[winner - 1], loser_look = looks[2 - winner];
            fatality_update();

            if (end_phase == END_FINISH) {
                // Triangle presses (G / Right Shift) on the winner's controls
                bool triangle = controls[winner - 1].finish;
                if (triangle && !triangle_was_down && ++triangle_presses >= FATALITY_PRESSES) {
                    fatality_start(champ, champ_look, winner == 1, loser, loser_look);
                    end_phase = END_FATALITY;
                } else if (end_timer >= FATALITY_WINDOW) {
                    end_phase = END_RESULT;
                    end_timer = 0;
                    play_sound(&snd_wins[champ_look]);
                }
                triangle_was_down = triangle;
            } else if (end_phase == END_FATALITY) {
                if (!fatality_active()) {
                    end_phase = END_RESULT;
                    end_timer = 0;
                    play_sound(&snd_wins[champ_look]);
                }
            } else {
                // Result screen: the winner celebrates (if they still have a racket); Cross starts a new
                // match once the announcer has had a moment, so a player still mashing doesn't skip it
                if (!champ->unarmed) {
                    if (champ->swing_timer > 0) champ->swing_timer--;
                    else champ->swing_timer = SWING_DURATION;
                }
                if (cross_pressed && end_timer >= WIN_SCREEN_MIN_FRAMES) {
                    p1.score = 0; p2.score = 0;
                    p1.stun_timer = 0; p2.stun_timer = 0;
                    p1.swing_timer = 0; p2.swing_timer = 0;
                    p1.throw_timer = 0; p2.throw_timer = 0;
                    p1.unarmed = p2.unarmed = false;
                    p1.headless = p2.headless = false;
                    p1.vanish_timer = p2.vanish_timer = 0;
                    winner = 0;
                    clear_paddle_effect(&p1); clear_paddle_effect(&p2);
                    reset_fireballs();
                    reset_rifts();
                    reset_calamities();
                    paddle_hits = 0;
                    reset_bonuses();
                    fatality_reset();
                    reset_ball(&ball, 1);
                }
            }
        }

        // --- Game Logic ---
        if (winner == 0 && playing) {
            update_paddle_effect(&p1);
            update_paddle_effect(&p2);

            move_paddle(&p1, p1_dir);
            move_paddle(&p2, p2_dir);

            update_bonuses(&p1, &p2);
            update_fireballs(&p1, &p2, &ball);
            update_ball(&ball, &p1, &p2);
            update_calamities(&ball);

            // Scoring & Round Reset
            if (ball.x < 0) {
                p2.score++;
                if (p2.score >= WINNING_SCORE) winner = 2;
                else reset_ball(&ball, 1);
            } else if (ball.x + ball.size > SCREEN_WIDTH) {
                p1.score++;
                if (p1.score >= WINNING_SCORE) winner = 1;
                else reset_ball(&ball, -1);
            }
            if (winner != 0) {
                end_phase = END_FINISH;
                end_timer = 0;
                triangle_presses = 0;
                triangle_was_down = true;          // A Triangle already held doesn't count
                end_rifts();                       // Nobody stays sunk through the end screens
                play_sound(player_is_female(looks[2 - winner]) ? &snd_finish_her : &snd_finish_him);
            }
        }

        if (playing) {
            update_rifts();
            update_particles();
        }

        // --- Rendering ---
        // The court and everything on it shake after a fatality's impact and in an earthquake's tremors (frozen
        // with the game, so not while paused or after the match); the overlays stay put
        int shake_x, shake_y;
        fatality_shake(&shake_x, &shake_y);
        if (playing && winner == 0) {
            int quake_x, quake_y;
            calamity_shake(&quake_x, &quake_y);
            shake_x += quake_x;
            shake_y += quake_y;
        }
        if (shake_x || shake_y) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            SDL_Rect shaken = { shake_x, shake_y, SCREEN_WIDTH, SCREEN_HEIGHT };
            SDL_RenderSetViewport(renderer, &shaken);
        }
        draw_court(renderer);
        draw_fatality_stains(renderer);
        draw_hud(renderer, &p1, &p2);
        draw_bonuses(renderer);
        draw_calamity_ground(renderer);
        draw_rifts(renderer);
        draw_particles(renderer);
        draw_player(renderer, &p1, looks[0], true, scared_of_ghost(&p1, &p2));
        draw_player(renderer, &p2, looks[1], false, scared_of_ghost(&p2, &p1));
        draw_fireballs(renderer);
        draw_ball(renderer, &ball);
        draw_calamity_sky(renderer);
        draw_fatality_front(renderer);
        if (shake_x || shake_y) SDL_RenderSetViewport(renderer, NULL);

        if (started && winner == 0) draw_calamity_title(renderer);
        if (paused) draw_pause_menu(renderer);
        else if (!started) draw_start_screen(renderer, SDL_GetTicks(), looks[0], looks[1]);
        else if (winner != 0 && end_phase == END_FINISH)
            draw_finish_screen(renderer, end_timer, FATALITY_WINDOW - end_timer, FATALITY_WINDOW,
                               triangle_presses, FATALITY_PRESSES, player_is_female(looks[2 - winner]));
        else if (winner != 0 && end_phase == END_FATALITY)
            draw_fatality_screen(renderer, fatality_since_impact());
        else if (winner != 0) {
            const Paddle *champ = winner == 1 ? &p1 : &p2, *loser = winner == 1 ? &p2 : &p1;
            draw_win_screen(renderer, player_name(looks[winner - 1]), champ->score,
                            loser->score, end_timer >= WIN_SCREEN_MIN_FRAMES, end_timer);
        }

        SDL_RenderPresent(renderer);

        // Cap at ~60 FPS since game speed is per-frame
        Uint32 elapsed = SDL_GetTicks() - frame_start;
        if (elapsed < frame_ms) SDL_Delay(frame_ms - elapsed);
    }

    printf("[pong] exiting\n");
    shutdown_audio();
    free_player_sprites();
    free_calamities();
    free_bonus_icons();
    free_court();
    if (pad1) SDL_GameControllerClose(pad1);
    if (pad2) SDL_GameControllerClose(pad2);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
