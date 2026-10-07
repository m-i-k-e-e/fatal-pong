#include "draw.h"

// Rows of `pixel`-high rectangles approximating the oval centered on (cx, cy) with half-sizes rx, ry, each
// row's width rounded to whole pixels; ovals smaller than a pixel aren't drawn
void fill_pixel_oval(SDL_Renderer *renderer, int cx, int cy, float rx, float ry, int pixel) {
    if (rx < pixel || ry < pixel / 2) return;
    int rows = (int)(ry / pixel);
    for (int i = -rows; i <= rows; i++) {
        float k = 1.0f - (float)(i * i) / ((ry / pixel) * (ry / pixel));
        if (k <= 0) continue;
        int half = (int)(rx * SDL_sqrtf(k) / pixel + 0.5f) * pixel;
        SDL_Rect row = { cx - half, cy + i * pixel - pixel / 2, half * 2, pixel };
        SDL_RenderFillRect(renderer, &row);
    }
}
