// Court markings, scores, bonus indicators, start and win screens
#ifndef PONG_HUD_H
#define PONG_HUD_H

#include "SDL2/SDL.h"
#include "game.h"

void init_court(SDL_Renderer *renderer);
void free_court(void);
void draw_court(SDL_Renderer *renderer);
void draw_hud(SDL_Renderer *renderer, const Paddle *p1, const Paddle *p2);
void draw_start_screen(SDL_Renderer *renderer, Uint32 ticks);
void draw_blood_text(SDL_Renderer *renderer, const char *s, int center_x, int y, int scale, int frames);
void draw_mud_text(SDL_Renderer *renderer, const char *s, int center_x, int y, int scale, int frames);
void draw_rock_text(SDL_Renderer *renderer, const char *s, int center_x, int y, int scale, int frames);
void draw_slime_text(SDL_Renderer *renderer, const char *s, int center_x, int y, int scale, int frames);
void draw_finish_screen(SDL_Renderer *renderer, int frames, int frames_left, int window, int presses, int needed);
void draw_fatality_screen(SDL_Renderer *renderer, int since_impact);
void draw_win_screen(SDL_Renderer *renderer, const char *winner, int winner_score, int loser_score,
                     bool show_prompt, int frames);

#endif
