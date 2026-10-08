// Small drawing helpers shared by modules
#ifndef PONG_DRAW_H
#define PONG_DRAW_H

#include "SDL2/SDL.h"

// Filled oval in `pixel`-sized steps, for a chunky look that matches the pixel art
void fill_pixel_oval(SDL_Renderer *renderer, int cx, int cy, float rx, float ry, int pixel);

#endif
