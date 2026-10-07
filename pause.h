// Pause overlay explaining the special move and the bonuses, with the settings sliders
#ifndef PONG_PAUSE_H
#define PONG_PAUSE_H

#include "SDL2/SDL.h"

void update_pause_menu(SDL_GameController *pad1, SDL_GameController *pad2);  // Every paused frame: the settings
void draw_pause_menu(SDL_Renderer *renderer);

#endif
