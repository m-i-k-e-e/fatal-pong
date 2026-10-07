// Pixel-art tennis players drawn over the paddles
#ifndef PONG_PLAYERS_H
#define PONG_PLAYERS_H

#include "SDL2/SDL.h"
#include "game.h"

typedef enum { PLAYER_AGASSI, PLAYER_NADAL, PLAYER_COUNT } PlayerLook;

void init_player_sprites(SDL_Renderer *renderer);
void free_player_sprites(void);
void draw_player(SDL_Renderer *renderer, const Paddle *p, PlayerLook look, bool faces_right);
const char *player_name(PlayerLook look);

// Screen positions of sprite features, for the fatality and the rift
typedef enum { POINT_HAND, POINT_FACE, POINT_NECK, POINT_FEET } PlayerPoint;
void player_point(const Paddle *p, bool faces_right, PlayerPoint point, int *x, int *y);
void draw_racket(SDL_Renderer *renderer, PlayerLook look, int cx, int cy, double angle);

#endif
