// Short-lived visual particles (fireball trails, explosions)
#ifndef PONG_PARTICLES_H
#define PONG_PARTICLES_H

#include "SDL2/SDL.h"

void spawn_particle(float x, float y, float vx, float vy, float size, int r, int g, int b, int life);
void spawn_explosion(float x, float y, int count);
void update_particles(void);
void draw_particles(SDL_Renderer *renderer);

#endif
