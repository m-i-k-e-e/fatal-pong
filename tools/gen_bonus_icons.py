#!/usr/bin/env python3
"""Bonus icons in the players' pixel-art style: 16x16 objects shaded with material ramps, lit from the upper
left (rounded parts shaded like spheres, flat parts bevelled), with a dark outline around the whole shape.

Writes bonus_icons.inc (palette + one char array per bonus, in BonusType order) and assets/bonus_icons.png (a
preview on the in-game tiles). Run from the project root: python3 tools/gen_bonus_icons.py (needs Pillow).
"""
import math
from PIL import Image

N = 16

PALETTE = {
    'k': (22, 18, 30),          # Outline
    'e': (10, 10, 14),          # Eyes
    'Q': (255, 150, 130), 'R': (228, 58, 52), 'r': (160, 28, 38),           # Red
    'W': (252, 252, 248), 'w': (205, 208, 220), 'v': (140, 144, 170),       # White
    'S': (246, 226, 186), 's': (204, 174, 132), 'u': (160, 128, 92),        # Cream
    'C': (178, 224, 255), 'B': (70, 140, 250), 'b': (36, 76, 180),          # Blue
    'N': (176, 122, 68), 'n': (110, 70, 36), 'x': (72, 44, 24),             # Brown
    'U': (236, 140, 96), 'O': (204, 94, 56), 'o': (140, 56, 38), 'm': (196, 186, 164),  # Brick and mortar
    'H': (255, 250, 190), 'Y': (255, 214, 60), 'y': (200, 140, 28),         # Yellow
    'g': (182, 212, 124), 'h': (112, 148, 80),                              # Snail green
    'L': (234, 180, 255), 'P': (182, 88, 230), 'p': (112, 44, 158),         # Purple
    'T': (238, 252, 112), 't': (200, 226, 50), 'j': (128, 158, 20),         # Tennis ball
    'Z': (255, 132, 222), 'z': (214, 56, 176),                              # Pink
}

RED, WHITE, CREAM, BLUE, BROWN = 'QRr', 'Wwv', 'Ssu', 'CBb', 'Nnx'
BRICK, YELLOW, GREEN, PURPLE, TENNIS, PINK = 'UOo', 'HYy', 'gh', 'LPp', 'Ttj', 'Zz'
LIGHT = (-0.55, -0.65, 0.52)    # Toward the upper left, a little out of the screen


class Icon:
    def __init__(self):
        self.px = [['.'] * N for _ in range(N)]

    def set(self, x, y, c):
        if 0 <= x < N and 0 <= y < N:
            self.px[y][x] = c

    def fill(self, inside, shade):
        """Paint every pixel whose center is inside, with the char shade(x, y) returns"""
        for y in range(N):
            for x in range(N):
                if inside(x + 0.5, y + 0.5):
                    self.px[y][x] = shade(x + 0.5, y + 0.5)

    def sphere(self, inside, cx, cy, r, ramp, highlight=0.62):
        """Shade like a ball centered on (cx, cy): bright toward the light, dark away from it. `highlight` is how
        lit a pixel must be to get the brightest shade (raise it for a smaller shine)."""
        def shade(x, y):
            nx, ny = (x - cx) / r, (y - cy) / r
            nz = math.sqrt(max(0.0, 1 - nx * nx - ny * ny))
            lit = nx * LIGHT[0] + ny * LIGHT[1] + nz * LIGHT[2]
            return ramp[0] if lit > highlight else ramp[1] if lit > 0.12 or len(ramp) < 3 else ramp[2]
        self.fill(inside, shade)

    def bevel(self, inside, ramp):
        """Flat shape: light on edges facing the upper left, dark on the others"""
        def shade(x, y):
            up_left = not inside(x - 1, y) or not inside(x, y - 1)
            down_right = not inside(x + 1, y) or not inside(x, y + 1)
            if up_left and not down_right:
                return ramp[0]
            if down_right and len(ramp) > 2:
                return ramp[2]
            return ramp[1]
        self.fill(inside, shade)

    def outline(self):
        """Dark outline on every empty pixel touching the shape (4-neighbours, plus diagonals for a solid edge)"""
        filled = {(x, y) for y in range(N) for x in range(N) if self.px[y][x] != '.'}
        for y in range(N):
            for x in range(N):
                if (x, y) in filled:
                    continue
                if any((x + dx, y + dy) in filled for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
                    self.px[y][x] = 'k'

    def rows(self):
        return [''.join(r) for r in self.px]


def circle(cx, cy, r):
    return lambda x, y: (x - cx) ** 2 + (y - cy) ** 2 <= r * r


def ellipse(cx, cy, rx, ry):
    return lambda x, y: ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1


def rect(x0, y0, x1, y1):
    """Inclusive pixel columns x0..x1, rows y0..y1"""
    return lambda x, y: x0 <= x < x1 + 1 and y0 <= y < y1 + 1


def polygon(points):
    def inside(x, y):
        hit = False
        for (x1, y1), (x2, y2) in zip(points, points[1:] + points[:1]):
            if (y1 > y) != (y2 > y) and x < x1 + (y - y1) * (x2 - x1) / (y2 - y1):
                hit = not hit
        return hit
    return inside


def both(a, b):
    return lambda x, y: a(x, y) and b(x, y)


# --- The icons, in BonusType order (BONUS_GROW first) ---

def grow():
    """Mushroom: spotted red cap, cream stem with a face"""
    i = Icon()
    i.bevel(rect(5, 8, 10, 14), CREAM)
    i.sphere(both(ellipse(8, 8.5, 7, 7), lambda x, y: y < 9), 8, 9, 7.5, RED, highlight=0.82)
    for sx, sy, sr in ((5, 4.5, 1.6), (10.5, 3.5, 1.4), (12, 7, 1.2), (2.8, 7.4, 1.0), (8, 7, 1.0)):
        i.sphere(both(circle(sx, sy, sr), lambda x, y: y < 9), sx, sy, sr + 0.6, WHITE)
    i.fill(rect(4, 9, 11, 9), lambda x, y: 'u')             # Shadow under the cap
    i.set(6, 11, 'e'); i.set(6, 12, 'e'); i.set(9, 11, 'e'); i.set(9, 12, 'e')
    i.outline()
    return i


def shrink():
    """Potion: round flask of blue liquid, cork stopper"""
    i = Icon()
    flask = circle(8, 10, 5.2)
    i.sphere(flask, 8, 10, 5.5, 'wvv')                      # Glass
    i.sphere(both(flask, lambda x, y: y > 8.5), 8, 10, 5.5, BLUE)
    i.bevel(rect(6, 3, 9, 5), 'wvv')                        # Neck
    i.bevel(rect(5, 1, 10, 2), BROWN)                       # Cork
    i.set(5, 8, 'W'); i.set(5, 9, 'W'); i.set(6, 7, 'W')    # Shine on the glass
    i.set(7, 11, 'C'); i.set(10, 12, 'C')                   # Bubbles
    i.outline()
    return i


def ghost():
    """Ghost: white sheet with a wavy hem and big eyes"""
    i = Icon()
    hem = lambda x, y: y < 13 or (y < 14.5 and int(x) % 4 in (0, 1))
    body = lambda x, y: (circle(8, 7, 6)(x, y) or (rect(2, 7, 13, 14)(x, y) and hem(x, y)))
    i.sphere(body, 7, 6, 8, WHITE)
    for ex in (5, 9):
        i.fill(rect(ex, 5, ex + 1, 7), lambda x, y: 'e')
    i.set(6, 5, 'C'); i.set(10, 5, 'C')                     # Glint
    i.fill(rect(6, 10, 9, 10), lambda x, y: 'v')            # Mouth
    i.outline()
    return i


def full():
    """Brick wall filling the tile from top to bottom"""
    i = Icon()
    i.fill(rect(1, 1, 14, 14), lambda x, y: 'm')
    for row, y0 in enumerate(range(1, 15, 3)):
        offset = 0 if row % 2 == 0 else -3
        for x0 in range(1 + offset, 15, 6):
            a, b = max(x0, 1), min(x0 + 4, 14)
            if a <= b:
                i.bevel(rect(a, y0, b, min(y0 + 1, 14)), BRICK)
    i.outline()
    return i


def fast():
    """Lightning bolt"""
    i = Icon()
    bolt = polygon([(10.5, 0.5), (3, 9), (7.2, 9), (5, 15.5), (13.5, 6), (9, 6), (11.8, 0.5)])
    i.bevel(bolt, YELLOW)
    i.outline()
    return i


def slow():
    """Snail: spiral shell on a green foot, round head with eyes on stalks"""
    i = Icon()
    i.bevel(rect(2, 12, 14, 13), GREEN)                                                    # Foot
    i.sphere(circle(3, 10.5, 2.3), 3, 10.5, 2.8, 'gh')                                     # Head
    for sx, top in ((1, 5), (4, 5)):
        i.fill(rect(sx, top + 1, sx, 8), lambda x, y: 'h')                                 # Stalks
        i.set(sx, top, 'W'); i.set(sx, top - 1, 'e')                                       # Eyes
    i.set(2, 11, 'e')                                                                      # Smile
    shell = circle(9.5, 7.5, 5)
    i.sphere(shell, 9.5, 7.5, 5.5, 'UOo')
    for k in range(40):                                                                     # Spiral line
        a = k * 0.32
        rr = 0.35 * a
        x, y = 9.5 + rr * math.cos(a), 7.5 + rr * math.sin(a)
        if shell(x, y) and rr < 4.3:
            i.set(int(x), int(y), 'x')
    i.outline()
    return i


def invert():
    """Two arrows, up and down, swapped side by side"""
    i = Icon()
    up = lambda x, y: (polygon([(0.6, 6), (4.5, 1), (8.4, 6)])(x, y) or rect(3, 6, 5, 14)(x, y))
    down = lambda x, y: (polygon([(7.6, 9), (11.5, 14.5), (15.4, 9)])(x, y) or rect(10, 1, 12, 9)(x, y))
    i.bevel(up, PURPLE)
    i.bevel(down, 'ZZz')
    i.outline()
    return i


def zigzag():
    """Tennis ball darting along a zig-zag trail"""
    i = Icon()
    trail = [(1, 14), (5, 12), (3, 9), (7, 8)]
    for (x1, y1), (x2, y2) in zip(trail, trail[1:]):
        steps = max(abs(x2 - x1), abs(y2 - y1)) * 2
        for s in range(steps + 1):
            x = round(x1 + (x2 - x1) * s / steps)
            y = round(y1 + (y2 - y1) * s / steps)
            i.set(x, y, 'Z'); i.set(x + 1, y, 'Z'); i.set(x, y + 1, 'z'); i.set(x + 1, y + 1, 'z')
    ball = circle(10.5, 5.5, 4.4)
    i.sphere(ball, 10.5, 5.5, 4.8, TENNIS)
    for y in range(N):                                      # Curved seam: an arc of a circle off the ball's edge
        for x in range(N):
            d = math.hypot(x + 0.5 - 15.5, y + 0.5 - 0.5)
            if ball(x + 0.5, y + 0.5) and abs(d - 5.6) < 0.55:
                i.set(x, y, 'W')
    i.outline()
    return i


ICONS = [('GROW', grow), ('SHRINK', shrink), ('GHOST', ghost), ('FULL', full),
         ('FAST', fast), ('SLOW', slow), ('INVERT', invert), ('ZIGZAG', zigzag)]
GOOD = {'GROW', 'FULL', 'FAST', 'ZIGZAG'}       # Same split as bonus_is_good() in bonus.c


def emit_inc(icons):
    out = ['// Generated by tools/gen_bonus_icons.py; do not edit by hand.',
           f'#define BONUS_ICON_SIZE {N}', '',
           'static SDL_Color bonus_icon_color(char c) {', '    switch (c) {']
    for c, (r, g, b) in PALETTE.items():
        out.append(f"    case '{c}': return (SDL_Color){{ {r}, {g}, {b}, 255 }};")
    out += ['    default:  return (SDL_Color){ 0, 0, 0, 0 };', '    }', '}', '',
            'static const char *const BONUS_ICON_ART[][BONUS_ICON_SIZE] = {']
    for name, icon in icons:
        out.append(f'    {{   // {name}')
        out += [f'        "{row}",' for row in icon.rows()]
        out.append('    },')
    out += ['};', '']
    open('bonus_icons.inc', 'w').write('\n'.join(out))


def preview(icons, path):
    """Each icon on its in-game tile (green or red frame), 3x like on the field"""
    scale, tile, gap = 3, 56, 16
    img = Image.new('RGB', (len(icons) * (tile + gap) + gap, tile + 2 * gap), (60, 130, 50))
    for k, (name, icon) in enumerate(icons):
        x0, y0 = gap + k * (tile + gap), gap
        frame = (60, 220, 90) if name in GOOD else (235, 60, 60)
        for y in range(tile):
            for x in range(tile):
                border = x < 4 or y < 4 or x >= tile - 4 or y >= tile - 4
                img.putpixel((x0 + x, y0 + y), frame if border else (52, 56, 72))
        for y, row in enumerate(icon.rows()):
            for x, c in enumerate(row):
                if c == '.':
                    continue
                for dy in range(scale):
                    for dx in range(scale):
                        img.putpixel((x0 + 4 + x * scale + dx, y0 + 4 + y * scale + dy), PALETTE[c])
    img = img.resize((img.width * 3, img.height * 3), Image.NEAREST)
    img.save(path)


if __name__ == '__main__':
    icons = [(name, make()) for name, make in ICONS]
    emit_inc(icons)
    preview(icons, 'assets/bonus_icons.png')
    print('wrote bonus_icons.inc and assets/bonus_icons.png')
