#include "SDL2/SDL.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "game.h"
#include "audio.h"
#include "ball.h"
#include "bonus.h"
#include "fatality.h"
#include "fireball.h"
#include "rift.h"
#include "calamity.h"
#include "hud.h"
#include "paddle.h"
#include "pause.h"
#include "players.h"
#include "particles.h"

#define WIN_SCREEN_MIN_FRAMES   90      // Win screen shows at least 1.5 s before Cross starts a new match

// libkernel direct memory queries (no public header in the SDK)
size_t sceKernelGetDirectMemorySize(void);
int sceKernelAvailableDirectMemorySize(off_t start, off_t end, size_t align, off_t *phys_out, size_t *size_out);

// Vertical input in [-1, 1] from the D-pad, falling back to a stick axis past its dead zone
static float read_direction(SDL_GameController *pad, SDL_GameControllerAxis axis) {
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP)) return -1.0f;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) return 1.0f;
    Sint16 value = SDL_GameControllerGetAxis(pad, axis);
    return abs(value) > 8000 ? value / 32767.0f : 0.0f;
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
// and after the last point the FINISH / FATALITY / RESULT phases, until touchpad + Options quits
int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    printf("[pong] starting\n");
    {
        off_t phys = 0;
        size_t avail = 0;
        int rc = sceKernelAvailableDirectMemorySize(0, (off_t)sceKernelGetDirectMemorySize(), 0x10000, &phys, &avail);
        printf("[pong] direct memory: total=%zu MiB, largest free block=%zu MiB (rc=0x%x)\n",
               sceKernelGetDirectMemorySize() >> 20, avail >> 20, rc);
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
        printf("[pong] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("Fatal Pong",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        printf("[pong] SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // The PS5 SDL port only ships the software renderer (no GPU driver, no vsync flag)
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
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

    printf("[pong] video ok, %d joystick(s), pad1=%p pad2=%p\n", SDL_NumJoysticks(), (void *)pad1, (void *)pad2);

    Paddle p1 = make_paddle(PADDLE_MARGIN);
    Paddle p2 = make_paddle(SCREEN_WIDTH - PADDLE_MARGIN - PADDLE_WIDTH);

    Ball ball = { 0, 0, 0, 0, BALL_SIZE };
    reset_ball(&ball, 1);

    int winner = 0;
    bool running = true;
    bool paused = false;
    bool started = false;                          // Waits on the start screen for player 1's Cross
    bool cross_was_down = true;                    // So a Cross still held from launching the app doesn't start it
    // After the last point: FINISH (the winner may enter the fatality), FATALITY, then the RESULT screen
    enum { END_FINISH, END_FATALITY, END_RESULT } end_phase = END_FINISH;
    int end_timer = 0;                             // Frames since the current end phase began
    int triangle_presses = 0;
    bool triangle_was_down = true;
    bool options_was_down[2] = { false, false };   // For press-edge detection of the pause button

    const Uint32 frame_ms = 1000 / 60;

    while (running) {
        Uint32 frame_start = SDL_GetTicks();
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_CONTROLLERDEVICEADDED) open_pad(e.cdevice.which, &pad1, &pad2);
        }

        // Options toggles pause; touchpad click + Options quits (PS5 has no window close button, and the
        // SDK's pad driver never reports the Create/Share button)
        bool toggle_pause = false;
        for (int i = 0; i < 2; i++) {
            SDL_GameController *pad = i == 0 ? pad1 : pad2;
            if (!pad) continue;
            bool touchpad = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_TOUCHPAD);
            bool options = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START);
            if (touchpad && options) running = false;
            else if (options && !options_was_down[i]) toggle_pause = true;
            options_was_down[i] = options;
        }
        if (toggle_pause) paused = !paused;

        // Cross presses (edge) start the game from the start screen and a new match from the win screen
        bool cross = (pad1 && SDL_GameControllerGetButton(pad1, SDL_CONTROLLER_BUTTON_A)) ||
                     (started && pad2 && SDL_GameControllerGetButton(pad2, SDL_CONTROLLER_BUTTON_A));
        bool cross_pressed = cross && !cross_was_down && !paused;
        cross_was_down = cross;
        if (!started && cross_pressed) started = true;
        bool playing = started && !paused;

        // Special move motions (hadouken, rift)
        if (winner == 0 && playing) {
            update_special_input(&p1, &p2, true, pad1);
            update_special_input(&p2, &p1, false, pad2 ? pad2 : pad1);
        }

        // Movement Inputs (with a single pad, P2 uses its right stick)
        float p1_dir = 0, p2_dir = 0;
        if (pad1) p1_dir = read_direction(pad1, SDL_CONTROLLER_AXIS_LEFTY);
        if (pad2) p2_dir = read_direction(pad2, SDL_CONTROLLER_AXIS_LEFTY);
        else if (pad1) {
            Sint16 axis_r = SDL_GameControllerGetAxis(pad1, SDL_CONTROLLER_AXIS_RIGHTY);
            if (abs(axis_r) > 8000) p2_dir = axis_r / 32767.0f;
        }

        // End of match
        if (winner != 0 && playing) {
            end_timer++;
            Paddle *champ = winner == 1 ? &p1 : &p2, *loser = winner == 1 ? &p2 : &p1;
            PlayerLook champ_look = winner == 1 ? PLAYER_AGASSI : PLAYER_NADAL;
            PlayerLook loser_look = winner == 1 ? PLAYER_NADAL : PLAYER_AGASSI;
            fatality_update();

            if (end_phase == END_FINISH) {
                // Triangle presses on the winner's controller (player 2 on the shared pad if there's only one)
                SDL_GameController *champ_pad = winner == 1 ? pad1 : (pad2 ? pad2 : pad1);
                bool triangle = champ_pad && SDL_GameControllerGetButton(champ_pad, SDL_CONTROLLER_BUTTON_Y);
                if (triangle && !triangle_was_down && ++triangle_presses >= FATALITY_PRESSES) {
                    fatality_start(champ, champ_look, winner == 1, loser, loser_look);
                    end_phase = END_FATALITY;
                } else if (end_timer >= FATALITY_WINDOW) {
                    end_phase = END_RESULT;
                    end_timer = 0;
                    play_sound(winner == 1 ? &snd_agassi_wins : &snd_nadal_wins);
                }
                triangle_was_down = triangle;
            } else if (end_phase == END_FATALITY) {
                if (!fatality_active()) {
                    end_phase = END_RESULT;
                    end_timer = 0;
                    play_sound(winner == 1 ? &snd_agassi_wins : &snd_nadal_wins);
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
                play_sound(&snd_finish_him);
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
        draw_player(renderer, &p1, PLAYER_AGASSI, true);
        draw_player(renderer, &p2, PLAYER_NADAL, false);
        draw_fireballs(renderer);
        draw_ball(renderer, &ball);
        draw_calamity_sky(renderer);
        draw_fatality_front(renderer);
        if (shake_x || shake_y) SDL_RenderSetViewport(renderer, NULL);

        if (started && winner == 0) draw_calamity_title(renderer);
        if (paused) draw_pause_menu(renderer);
        else if (!started) draw_start_screen(renderer, SDL_GetTicks());
        else if (winner != 0 && end_phase == END_FINISH)
            draw_finish_screen(renderer, end_timer, FATALITY_WINDOW - end_timer, FATALITY_WINDOW,
                               triangle_presses, FATALITY_PRESSES);
        else if (winner != 0 && end_phase == END_FATALITY)
            draw_fatality_screen(renderer, fatality_since_impact());
        else if (winner != 0) {
            const Paddle *champ = winner == 1 ? &p1 : &p2, *loser = winner == 1 ? &p2 : &p1;
            draw_win_screen(renderer, player_name(winner == 1 ? PLAYER_AGASSI : PLAYER_NADAL), champ->score,
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
    free_court();
    if (pad1) SDL_GameControllerClose(pad1);
    if (pad2) SDL_GameControllerClose(pad2);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
