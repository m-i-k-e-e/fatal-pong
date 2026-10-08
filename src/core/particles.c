#include "core/particles.h"
#include <stdbool.h>
#include <stdlib.h>

#define MAX_PARTICLES       256

typedef struct {
    float x, y;
    float vx, vy;
    float size;
    int r, g, b;
    int life;
    bool active;
} Particle;

static Particle particles[MAX_PARTICLES];

// Spawn visual particles (fireball trail, explosions)
void spawn_particle(float x, float y, float vx, float vy, float size, int r, int g, int b, int life) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active) {
            particles[i].x = x;
            particles[i].y = y;
            particles[i].vx = vx;
            particles[i].vy = vy;
            particles[i].size = size;
            particles[i].r = r;
            particles[i].g = g;
            particles[i].b = b;
            particles[i].life = life;
            particles[i].active = true;
            break;
        }
    }
}

// Burst of fire particles centered on a point
void spawn_explosion(float x, float y, int count) {
    for (int i = 0; i < count; i++) {
        float vx = ((rand() % 100) / 10.0f - 5.0f) * 2.0f;
        float vy = ((rand() % 100) / 10.0f - 5.0f) * 2.0f;
        spawn_particle(x, y, vx, vy, 6 + rand() % 8, 255, 100 + rand() % 100, 50, 30 + rand() % 25);
    }
}

// Move every particle one frame and retire the ones whose life ran out
void update_particles(void) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].active) {
            particles[i].x += particles[i].vx;
            particles[i].y += particles[i].vy;
            particles[i].life--;
            if (particles[i].life <= 0) particles[i].active = false;
        }
    }
}

// Every live particle as a square in its color
void draw_particles(SDL_Renderer *renderer) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].active) {
            SDL_SetRenderDrawColor(renderer, particles[i].r, particles[i].g, particles[i].b, 255);
            SDL_Rect pr = { (int)particles[i].x, (int)particles[i].y, (int)particles[i].size, (int)particles[i].size };
            SDL_RenderFillRect(renderer, &pr);
        }
    }
}
