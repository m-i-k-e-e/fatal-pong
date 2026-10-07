// Pixel-art tennis players drawn over the paddles
#ifndef PONG_PLAYERS_H
#define PONG_PLAYERS_H

#include "SDL2/SDL.h"
#include "game.h"

typedef enum { PLAYER_AGASSI, PLAYER_NADAL, PLAYER_GRAF, PLAYER_SHARAPOVA, PLAYER_COUNT } PlayerLook;

void init_player_sprites(SDL_Renderer *renderer);
void free_player_sprites(void);
void draw_player(SDL_Renderer *renderer, const Paddle *p, PlayerLook look, bool faces_right, bool scared);
void force_push(bool faces_right, float x, float y);  // Full bonus: the force user's wave to (x, y)
void draw_player_portrait(SDL_Renderer *renderer, PlayerLook look, int x, int y, int scale, bool faces_right);
const char *player_name(PlayerLook look);
bool player_is_female(PlayerLook look);      // FINISH HER instead of FINISH HIM
SDL_Color player_hair_color(PlayerLook look);  // For the head's chunks in a fatality

// Screen positions of sprite features, for the fatality and the rift
typedef enum { POINT_HAND, POINT_FACE, POINT_NECK, POINT_FEET, POINT_ANKLE } PlayerPoint;
void player_point(const Paddle *p, bool faces_right, PlayerPoint point, int *x, int *y);
void draw_racket(SDL_Renderer *renderer, PlayerLook look, int cx, int cy, double angle);

#endif
