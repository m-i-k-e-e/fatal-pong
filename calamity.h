// Calamities: every CALAMITY_EVERY_HITS paddle hits there's a CALAMITY_CHANCE % chance of one, picked at random:
// the mole, who digs molehills, the earthquake, whose tremors open cracks, or the frog rain, whose frogs land in
// the way. Any of them knocks the ball off course.
#ifndef PONG_CALAMITY_H
#define PONG_CALAMITY_H

#include "SDL2/SDL.h"
#include "game.h"

#define CALAMITY_EVERY_HITS 10
#define CALAMITY_CHANCE     26          // Percent
#define CALAMITY_DURATION   (20 * 60)   // Frames a calamity lasts, from its arrival
#define CALAMITY_MIN        3           // Molehills, cracks or frogs standing at once
#define CALAMITY_MAX        5
#define CALAMITY_TITLE_FRAMES 150       // "INCOMING MOLE" / "THE EARTHQUAKE" / "THE FROG RAIN" on screen

void init_calamities(SDL_Renderer *renderer);
void free_calamities(void);
void update_calamities(Ball *ball);     // While a point is being played: rolls on every 10th hit, runs the calamity
void start_mole(void);
void start_earthquake(void);
void start_frog_rain(void);
void reset_calamities(void);            // New match
void calamity_shake(int *dx, int *dy);  // Screen offset while the ground shakes
void draw_calamity_ground(SDL_Renderer *renderer);  // Molehills, cracks, sitting frogs and shadows, under the players
void draw_calamity_sky(SDL_Renderer *renderer);     // Frogs in the air, over the players
void draw_calamity_title(SDL_Renderer *renderer);   // Overlay announcing the calamity

#endif
