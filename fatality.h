// Fatality: the winner throws their racket into the loser's face, which explodes in blood
#ifndef PONG_FATALITY_H
#define PONG_FATALITY_H

#include "SDL2/SDL.h"
#include "game.h"
#include "players.h"

#define FATALITY_WINDOW     (5 * 60)    // Frames the winner has after the match to enter it
#define FATALITY_PRESSES    3           // Triangle presses needed
#define FATALITY_LENGTH     (4 * 60)    // Frames from the impact until the result screen

void fatality_start(Paddle *winner, PlayerLook winner_look, bool winner_faces_right,
                    Paddle *loser, PlayerLook loser_look);
void fatality_update(void);
bool fatality_active(void);             // Started and not finished yet
int fatality_since_impact(void);        // Frames since the head exploded, -1 before
void fatality_shake(int *dx, int *dy);  // Screen offset for the impact shake
void fatality_reset(void);              // New match: clear blood, stains and state

void draw_fatality_stains(SDL_Renderer *renderer);    // On the grass, under the players
void draw_fatality_front(SDL_Renderer *renderer);     // Racket in flight and flying blood, over the players

#endif
