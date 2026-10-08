#include "core/input.h"
#include <stdlib.h>

// A shared pad is split in two: player 1 on the left half (D-pad, left stick, L1), player 2 on the right half
// (right stick, Square / R1). Cross, Options and the quit combo stay with player 1; Triangle works for both.
// Keyboard: player 1 on WASD, F punch, G fatality, Space / Enter confirm, Esc pause, Q quit (when paused);
// player 2 on the arrows, Right Ctrl or '.' punch, Right Shift or '/' fatality.
#define MOVE_DEAD_ZONE      8000        // Stick travel before it moves a player
#define DIGITAL_DEAD_ZONE   16000       // Stick travel that counts as a direction press

typedef enum { PAD_WHOLE, PAD_LEFT_HALF, PAD_RIGHT_HALF } PadPart;

// Merge `part` of `pad` into `c`: movement from the D-pad or the half's stick, directions, buttons
static void read_pad(SDL_GameController *pad, PadPart part, PlayerControls *c) {
    if (!pad) return;
    bool dpad = part != PAD_RIGHT_HALF, left_half = part != PAD_RIGHT_HALF;
    Sint16 x = SDL_GameControllerGetAxis(pad, dpad ? SDL_CONTROLLER_AXIS_LEFTX : SDL_CONTROLLER_AXIS_RIGHTX);
    Sint16 y = SDL_GameControllerGetAxis(pad, dpad ? SDL_CONTROLLER_AXIS_LEFTY : SDL_CONTROLLER_AXIS_RIGHTY);
    bool d_up = dpad && SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP);
    bool d_down = dpad && SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    bool d_left = dpad && SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    bool d_right = dpad && SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);

    if (d_up) c->move = -1.0f;
    else if (d_down) c->move = 1.0f;
    else if (abs(y) > MOVE_DEAD_ZONE) c->move = y / 32767.0f;
    c->up |= d_up || y < -DIGITAL_DEAD_ZONE;
    c->down |= d_down || y > DIGITAL_DEAD_ZONE;
    c->left |= d_left || x < -DIGITAL_DEAD_ZONE;
    c->right |= d_right || x > DIGITAL_DEAD_ZONE;

    if (part == PAD_LEFT_HALF) c->punch |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
    else c->punch |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_X) ||
                     SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
    c->finish |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_Y);
    if (left_half) {
        bool options = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START);
        c->confirm |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A);
        c->pause |= options;
        c->quit |= options && SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_TOUCHPAD);
    }
}

// Merge one player's keys into `c`: up / down / left / right, punch and fatality (two keys each)
static void read_keys(const Uint8 *keys, SDL_Scancode up, SDL_Scancode down, SDL_Scancode left, SDL_Scancode right,
                      SDL_Scancode punch, SDL_Scancode punch2, SDL_Scancode finish, SDL_Scancode finish2,
                      PlayerControls *c) {
    if (keys[up]) c->move = -1.0f;
    else if (keys[down]) c->move = 1.0f;
    c->up |= keys[up];
    c->down |= keys[down];
    c->left |= keys[left];
    c->right |= keys[right];
    c->punch |= keys[punch] || keys[punch2];
    c->finish |= keys[finish] || keys[finish2];
}

// This frame's controls for both players (see the top of this file for the layout). Keys win over a stick
// for movement. Call after the frame's events have been pumped.
void read_controls(SDL_GameController *pad1, SDL_GameController *pad2, PlayerControls out[2]) {
    SDL_memset(out, 0, 2 * sizeof(PlayerControls));
    read_pad(pad1, pad2 ? PAD_WHOLE : PAD_LEFT_HALF, &out[0]);
    if (pad2) read_pad(pad2, PAD_WHOLE, &out[1]);
    else read_pad(pad1, PAD_RIGHT_HALF, &out[1]);

    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    read_keys(keys, SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_A, SDL_SCANCODE_D,
              SDL_SCANCODE_F, SDL_SCANCODE_F, SDL_SCANCODE_G, SDL_SCANCODE_G, &out[0]);
    read_keys(keys, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
              SDL_SCANCODE_RCTRL, SDL_SCANCODE_PERIOD, SDL_SCANCODE_RSHIFT, SDL_SCANCODE_SLASH, &out[1]);
    out[0].confirm |= keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_KP_ENTER];
    out[0].pause |= keys[SDL_SCANCODE_ESCAPE];
    out[0].quit_key = keys[SDL_SCANCODE_Q];
}
