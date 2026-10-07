// Minimal 5x7 bitmap font (uppercase, digits, basic punctuation; lowercase is drawn as uppercase)
#ifndef PONG_TEXT_H
#define PONG_TEXT_H

#include "SDL2/SDL.h"
#include <stdbool.h>

int text_width(const char *s, int scale);
int draw_text(SDL_Renderer *renderer, const char *s, int x, int y, int scale);  // Returns the drawn width
void draw_text_centered(SDL_Renderer *renderer, const char *s, int center_x, int y, int scale);

// Glyph bitmap for custom text effects: 7 rows, bit 4 is the leftmost of 5 columns, 1 column gap after.
// False (rows cleared) for spaces and unsupported characters.
#define TEXT_GLYPH_W    5
#define TEXT_GLYPH_H    7
bool text_glyph(char c, unsigned char rows[TEXT_GLYPH_H]);

#endif
