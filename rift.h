// Rift: a portal opens under the opponent's feet; caught on it, they sink through and are gone for a while
#ifndef PONG_RIFT_H
#define PONG_RIFT_H

#include "SDL2/SDL.h"
#include "game.h"

#define RIFT_WARNING        30          // Frames the opponent has to step off the rift (half a second)
#define VANISH_DURATION     60          // Frames a caught player is gone (1 second)
#define RIFT_SINK           12          // Frames to sink in, and to rise back out
#define VANISH_TOTAL        (RIFT_SINK + VANISH_DURATION + RIFT_SINK)   // A caught player's vanish_timer

void open_rift(int owner, Paddle *target);      // owner 0 = P1 (targets P2), 1 = P2; ignored while theirs is open
void update_rifts(void);                        // Every frame while playing, match over included
void end_rifts(void);                           // Match over: pending rifts fizzle, vanished players rise
void reset_rifts(void);
float vanish_depth(const Paddle *p);            // How far a vanished player has sunk, 0..1 (1 = gone)
void draw_rifts(SDL_Renderer *renderer);        // On the ground, under the players

#endif
