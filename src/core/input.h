// Controls: each player's gamepad (or half of a shared one) and keys, read once per frame into the same actions
#ifndef PONG_INPUT_H
#define PONG_INPUT_H

#include "SDL2/SDL.h"
#include <stdbool.h>

typedef struct {
    float move;                 // Vertical movement, -1 (up) to 1 (down)
    bool up, down, left, right; // Digital directions, for special moves and menus
    bool punch;                 // Special move button (Square / R1)
    bool finish;                // Fatality button (Triangle)
    bool confirm;               // Cross
    bool pause;                 // Options
    bool quit;                  // Touchpad + Options: quit at once
    bool quit_key;              // Q: quits from the pause screen
} PlayerControls;

void read_controls(SDL_GameController *pad1, SDL_GameController *pad2, PlayerControls out[2]);

// On-screen hint for a control: the pad's name, plus the keyboard's outside the PS5 build
#ifdef __PROSPERO__
#define KEY_HINT(pad, keys) pad
#else
#define KEY_HINT(pad, keys) pad " / " keys
#endif

#endif
