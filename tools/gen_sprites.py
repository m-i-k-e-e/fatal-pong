#!/usr/bin/env python3
"""Generate the player sprite layers and palette (src/render/player_sprites.inc) and preview images
(target/screenshots/previews/).

Each player is drawn as four 32x80 layers stacked in the game: a body (big Pop!-figure head, torso,
shorts), one of 4 leg frames (step cycle), a left arm (hanging, or the hadouken charge and thrust) and
a racket arm (4 swing poses plus the hadouken charge and thrust). Each also has a baby version, the same
layers and poses on a 32x40 canvas, drawn while the player is shrunk.

Style: shaded pixel art. Shapes are painted as regions of a material (skin, blond hair, denim...);
a lighting pass picks one of each material's 4 tones (light from the upper left: the head is lit as
a sphere, everything else by its edges) and a final pass adds a selective outline in each material's
darkest colour. Facial features are placed by hand on top.

Run from the project root:  python3 tools/gen_sprites.py  (--previews-only skips the .inc; make screenshots uses it)
"""
import math
import os
import sys

W, H = 32, 80
STEP_FRAMES = 4
SWING_FRAMES = 4                        # Racket arm: 4 swing poses, then charge and thrust
ARM_FRAMES = SWING_FRAMES + 3              # ... then charge, thrust, and the empty-handed fatality throw
CHARGE, THRUST, UNARMED = SWING_FRAMES, SWING_FRAMES + 1, SWING_FRAMES + 2
LEFT_FRAMES = 3                         # Left arm: hanging, charge, thrust

# --- Palette -----------------------------------------------------------------------------------------
# material: (outline char, [4 tone chars, dark to light]); every char maps to a colour in COLORS.

MATERIALS = {
    'skin':   ('e', 'dsSa'),
    'blond':  ('Z', 'zyY1'),
    'honey':  ('F', 'ftAD'),            # Graf's darker golden blond
    'plat':   ('U', '{}|_'),            # Sharapova's platinum blond
    'brown':  ('x', 'hH23'),
    'black':  ('K', 'B456'),
    'pink':   ('7', 'Pp89'),
    'denim':  ('0', 'Jj!$'),
    'lime':   (';', 'gG+='),
    'white':  ('#', 'wW@~'),
    'red':    ('%', 'rR^<'),
    'grey':   ('&', 'oO*:'),
    'yellow': ('(', 'qQ)['),
    'string': (None, 'nnNN'),
    'energy': ('v', 'ucCV'),
    'blood':  ('X', 'bmMT'),
}

COLORS = {
    'e': (96, 56, 38),    'd': (150, 94, 66),   's': (198, 136, 98),  'S': (230, 174, 134), 'a': (250, 212, 176),
    'Z': (80, 56, 24),    'z': (134, 98, 42),   'y': (186, 146, 64),  'Y': (228, 192, 100), '1': (252, 232, 156),
    'F': (78, 52, 22),    'f': (136, 96, 44),   't': (176, 132, 62),  'A': (206, 166, 90),  'D': (232, 200, 128),
    'U': (96, 84, 56),    '{': (176, 160, 112), '}': (220, 204, 150), '|': (244, 232, 184), '_': (255, 250, 220),
    'x': (20, 12, 6),     'h': (46, 28, 16),    'H': (74, 46, 28),    '2': (108, 72, 46),   '3': (146, 104, 70),
    'K': (10, 10, 14),    'B': (28, 28, 36),    '4': (46, 46, 58),    '5': (70, 70, 86),    '6': (100, 100, 120),
    '7': (120, 16, 72),   'P': (190, 36, 130),  'p': (240, 76, 170),  '8': (255, 130, 205), '9': (255, 190, 230),
    '0': (28, 44, 84),    'J': (50, 80, 140),   'j': (76, 114, 180),  '!': (110, 150, 215), '$': (160, 195, 240),
    ';': (40, 80, 8),     'g': (84, 140, 18),   'G': (130, 196, 36),  '+': (172, 232, 70),  '=': (214, 252, 140),
    '#': (96, 96, 110),   'w': (156, 156, 172), 'W': (204, 204, 214), '@': (236, 236, 242), '~': (255, 255, 255),
    '%': (100, 12, 12),   'r': (156, 24, 24),   'R': (210, 40, 40),   '^': (240, 90, 80),   '<': (255, 150, 140),
    '&': (34, 34, 42),    'o': (70, 70, 84),    'O': (112, 112, 128), '*': (160, 160, 176), ':': (210, 210, 222),
    '(': (100, 80, 8),    'q': (170, 140, 16),  'Q': (232, 200, 32),  ')': (252, 232, 90),  '[': (255, 248, 170),
    'n': (150, 150, 156), 'N': (206, 206, 212),
    'v': (20, 60, 90),    'u': (40, 140, 200),  'c': (80, 200, 240),  'C': (160, 240, 255), 'V': (240, 255, 255),
    'X': (40, 0, 0),      'b': (90, 0, 0),      'm': (140, 8, 8),     'M': (185, 18, 18),   'T': (225, 50, 40),
    'E': (232, 226, 218), 'i': (104, 66, 36),   'I': (150, 104, 62),  'k': (16, 10, 6),                        # Eyes
    'L': (160, 80, 72),   'l': (200, 116, 104),                                                               # Lips
}

LIGHT = (-0.45, -0.55, 0.70)
BAYER = [[0.0, 0.5], [0.75, 0.25]]

# --- Layer: regions of materials, then shading and outline -------------------------------------------

class Layer:
    def __init__(self, h=H):
        self.h = h                                      # H for the players, BABY_H for the babies
        self.region = [[None] * W for _ in range(h)]
        self.mat = [[None] * W for _ in range(h)]
        self.bias = [[0.0] * W for _ in range(h)]
        self.over = [[None] * W for _ in range(h)]     # Hand-placed pixels drawn after shading

    def paint(self, x, y, region, mat, bias=0.0):
        if 0 <= x < W and 0 <= y < self.h:
            self.region[y][x], self.mat[y][x], self.bias[y][x] = region, mat, bias

    def rect(self, x0, y0, x1, y1, region, mat, bias=0.0):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.paint(x, y, region, mat, bias)

    def pixel(self, x, y, c):
        if 0 <= x < W and 0 <= y < self.h:
            self.over[y][x] = c

    def inside(self, x, y, region):
        return 0 <= x < W and 0 <= y < self.h and self.region[y][x] == region

    def render(self, sphere=None):
        """Pick a tone per pixel, add outlines, apply hand-placed pixels. Returns rows of chars."""
        out = [['.'] * W for _ in range(self.h)]
        for y in range(self.h):
            for x in range(W):
                r, m = self.region[y][x], self.mat[y][x]
                if r is None:
                    continue
                if sphere and r in sphere['regions']:
                    cx, cy, rx, ry = sphere['ellipse']
                    nx, ny = (x + 0.5 - cx) / rx, (y + 0.5 - cy) / ry
                    nz = math.sqrt(max(0.0, 1 - nx * nx - ny * ny))
                    light = (nx * LIGHT[0] + ny * LIGHT[1] + nz * LIGHT[2]) * 1.5 - 0.15
                else:
                    # Edge lighting: rims toward the light are bright, the far side falls into shadow
                    light = 0.35
                    if not self.inside(x - 1, y, r) or not self.inside(x, y - 1, r):
                        light += 0.65
                    if not self.inside(x + 1, y, r) or not self.inside(x, y + 1, r):
                        light -= 0.7
                    elif not self.inside(x + 2, y, r) or not self.inside(x + 1, y + 1, r):
                        light -= 0.35
                dither = 0.12 if r in ('face', 'neck') else 0.45    # Skin stays smooth
                light += self.bias[y][x] + (BAYER[y % 2][x % 2] - 0.375) * dither
                tones = MATERIALS[m][1]
                out[y][x] = tones[max(0, min(3, int(round(1.5 + light * 1.6))))]
        # Selective outline: empty pixels next to a region take the neighbour material's darkest colour
        for y in range(self.h):
            for x in range(W):
                if self.region[y][x] is not None:
                    continue
                for dx, dy in ((0, -1), (-1, 0), (1, 0), (0, 1)):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < W and 0 <= ny < self.h and self.mat[ny][nx] and MATERIALS[self.mat[ny][nx]][0]:
                        out[y][x] = MATERIALS[self.mat[ny][nx]][0]
                        break
        for y in range(self.h):
            for x in range(W):
                if self.over[y][x]:
                    out[y][x] = self.over[y][x]
        return [''.join(r) for r in out]

# --- Head (24x44 Pop! proportions) --------------------------------------------------------------------
# Mask as left halves, mirrored: 'h' hair, 'f' face, 'n' neck. Placed at x=1 so the outline fits.

HEAD_X = 1
HEAD_MASK = [
    "......hhhhhh",
    "....hhhhhhhh",
    "...hhhhhhhhh",
    "..hhhhhhhhhh",
    ".hhhhhhhhhhh",
    ".hhhhhhhhhhh",
] + ["hhhhhhhhhhhh"] * 6 + [
    "hhhhhhhfhhhf",
    "hhhhhhfffhff",
    "hhhhffffffff",
    "hhhfffffffff",
] + ["hhffffffffff"] * 17 + [
    "hhhfffffffff",
    "hhhfffffffff",
    "hhhhffffffff",
    "hhhhhfffffff",
    ".hhhhhffffff",
    ".hhhhhhfffff",
    "..hhhhhhffff",
    "..hhhhhhhhnn",
    "...hhhhhhhnn",
    "...hhhhhhhnn",
    "....hhhhhhnn",
]
HEAD_ROWS = len(HEAD_MASK)
SPHERE = dict(regions={'face', 'hair', 'band'}, ellipse=(HEAD_X + 12, 21, 12.5, 22))

def head_mask(style):
    """HEAD_MASK for a hairstyle: 'long' as drawn, 'bob' cut at the chin with straight bangs, 'short' cut above
    the jaw, 'pulled' back off the face (into a ponytail, painted separately)."""
    rows = []
    for y, half in enumerate(HEAD_MASK):
        if style == 'bob' and y <= 16:
            half = half.replace('f', 'h')
        elif style == 'bob' and y >= 36 or style == 'short' and y >= 32 or style == 'pulled' and y >= 26:
            half = half.replace('h', '.')
        rows.append(half)
    return rows

def paint_head(L, hair_mat, headband, style='long'):
    mask = head_mask(style)
    for y, half in enumerate(mask):
        for x, c in enumerate(half + half[::-1]):
            px = HEAD_X + x
            if c == 'h':
                # Strand streaks; long hair darkens toward the ends
                streak = 0.3 * math.sin(px * 1.9 + 0.5 * math.sin(y * 0.35))
                L.paint(px, y, 'hair', hair_mat, streak - max(0, y - 24) * 0.035)
            elif c == 'f':
                # Hair shades the face just below and beside it
                near_hair = any(0 <= y - dy and mask[y - dy][min(x, 23 - x)] == 'h' for dy in (1, 2))
                L.paint(px, y, 'face', 'skin', -0.5 if near_hair else 0.0)
            elif c == 'n':
                L.paint(px, y, 'neck', 'skin', -0.6)
    if headband:
        for y in (11, 12, 13):
            for x in range(W):
                if L.region[y][x] in ('hair', 'face'):
                    L.paint(x, y, 'band', 'red')

def face(L, smile, brows, lips="Ll", lashes=False):
    """Brows, eyes (with lashes at the outer corners if `lashes`), nose and mouth; `lips` are the upper and lower
    lip colours."""
    # Brows
    for x, c in brows:
        L.pixel(x, 20, c[0])
        if len(c) > 1:
            L.pixel(x, 19, c[1])
    # Eyes: socket shadow, upper lid, white / iris / pupil, lower lid
    for x0, iris in ((5, "EIkiE"), (16, "EikIE")):
        for i in range(5):
            L.pixel(x0 + i, 21, 'd' if i in (0, 4) else 's')
            L.pixel(x0 + i, 23, iris[i])
        for i in (1, 2, 3):
            L.pixel(x0 + i, 22, 'e')
            L.pixel(x0 + i, 24, 's')
    if lashes:
        L.pixel(4, 22, 'k'); L.pixel(4, 21, 'k')
        L.pixel(21, 22, 'k'); L.pixel(21, 21, 'k')
    # Nose: lit bridge on the left, shadow on the right, nostrils
    for y in range(24, 29):
        L.pixel(12, y, 'a')
        L.pixel(14, y, 's')
    L.pixel(13, 28, 'a')
    for x, c in zip(range(11, 15), "dssd"):
        L.pixel(x, 29, c)
    L.pixel(14, 30, 's')
    # Mouth
    if smile:
        L.pixel(9, 32, 'd'); L.pixel(16, 32, 'd')
        for x in range(10, 16):
            L.pixel(x, 33, lips[0])
        for x in range(11, 15):
            L.pixel(x, 34, lips[1])
    else:
        for x in range(10, 16):
            L.pixel(x, 33, 'e')
        for x in range(11, 15):
            L.pixel(x, 34, lips[0])
    for x in range(11, 15):
        L.pixel(x, 35, 's')

# --- Body layer ----------------------------------------------------------------------------------------

TORSO_TOP, TORSO_BOTTOM = 45, 57        # Fill rows; the outline adds a row above
TORSO_LEFT, TORSO_RIGHT = 8, 17
ARM_X = 5                               # Hanging left arm, 2 px of skin

def body(p, headless=False):
    """Head, torso and shorts (or a skirt, flaring out); the left arm is its own layer so it can animate.
    Headless (after a fatality): a bloody neck stump instead of the head, blood soaking the collar."""
    L = Layer()
    # Torso, shaded as a cylinder; the big head casts a shadow on the shoulders
    for y in range(TORSO_TOP, TORSO_BOTTOM + 1):
        for x in range(TORSO_LEFT, TORSO_RIGHT + 1):
            bias = (12.5 - x) / 14 - (0.6 if y < TORSO_TOP + 2 else 0)
            mat = p['shirt'](x, y)
            if p['sleeveless'] and y < TORSO_TOP + 4 and x in (TORSO_LEFT, TORSO_RIGHT):
                mat = 'skin'
            L.paint(x, y, 'torso', mat, bias)
    if p.get('skirt'):
        # Pleated skirt flaring out from the waist
        for y in range(TORSO_BOTTOM + 1, p['shorts_bottom'] + 1):
            flare = (y - TORSO_BOTTOM) // 2
            for x in range(TORSO_LEFT - flare, TORSO_RIGHT + flare + 1):
                pleat = 0.25 if (x // 2) % 2 else -0.25
                L.paint(x, y, 'skirt', p['shorts'], (12.5 - x) / 16 + pleat)
    else:
        # Shorts, legs split a few rows down
        for y in range(TORSO_BOTTOM + 2, p['shorts_bottom'] + 1):
            for x in range(TORSO_LEFT, TORSO_RIGHT + 1):
                if y >= TORSO_BOTTOM + 4 and x in (12, 13):
                    continue
                L.paint(x, y, 'shorts', p['shorts'], (12.5 - x) / 16)
    if headless:
        L.rect(HEAD_X + 10, 41, HEAD_X + 13, 44, 'neck', 'skin', -0.3)
        L.rect(HEAD_X + 10, 40, HEAD_X + 13, 41, 'stump', 'blood', 0.4)
        for x, depth in ((9, 3), (10, 6), (11, 2), (12, 8), (13, 4), (14, 5), (15, 2)):
            L.rect(x, TORSO_TOP, x, TORSO_TOP + depth, 'gore', 'blood', -0.2)
        return L.render()
    paint_head(L, p['hair'], p['headband'], p.get('hairstyle', 'long'))
    if p.get('ponytail'):
        # Long ponytail from the back of the head down the back (the left, as the art faces right), behind the
        # left arm
        for y in range(24, 60):
            x0, w = (1, 3) if y < 54 else (2, 2)
            for x in range(x0, x0 + w):
                if L.region[y][x] is None:
                    L.paint(x, y, 'hair', p['hair'], 0.3 * math.sin(y * 0.9))
    face(L, p['smile'], p['brows'], p.get('lips', "Ll"), p.get('lashes', False))
    return L.render(SPHERE)

def agassi_shirt(x, y):
    return 'pink' if (x + y) % 4 == 0 else 'black'

def nadal_shirt(x, y):
    return 'lime'

def graf_shirt(x, y):
    return 'red' if y < TORSO_TOP + 3 and 11 <= x <= 14 else 'white'     # Red V collar

def sharapova_shirt(x, y):
    return 'grey' if y == TORSO_TOP and x % 3 == 1 else 'black'          # Crystals along the neckline

# --- Leg frames: stand, left foot up, stand, right foot up ----------------------------------------------

SHOE_Y = 75

def legs(p, frame):
    L = Layer()
    top = p['shorts_bottom'] + 1
    lift = {1: (3, 0), 3: (0, 3)}.get(frame, (0, 0))
    for side, (x0, up) in enumerate(((TORSO_LEFT, lift[0]), (TORSO_RIGHT - 3, lift[1]))):
        shoe_y = SHOE_Y - up
        L.rect(x0 + 1, top, x0 + 2, shoe_y - 3, f'leg{side}', 'skin', -0.1)
        L.rect(x0 + 1, shoe_y - 2, x0 + 2, shoe_y - 1, f'leg{side}', 'white')          # Socks
        L.rect(x0, shoe_y, x0 + 3, shoe_y + 1, f'shoe{side}', 'white', 0.2)
        L.pixel(x0 + 1, shoe_y + 1, MATERIALS[p['shoe_accent']][1][2])
    return L.render()

# --- Racket arm frames: ready, forward, contact, follow-through -----------------------------------------

SHOULDER = (18, 46)
SWING = [   # (elbow, hand, racket angle in degrees clockwise from straight up)
    ((20, 56), (22, 54), 15),       # Ready
    ((21, 53), (22, 50), 50),       # Forward
    ((20, 50), (20, 50), 85),       # Contact
    ((20, 54), (21, 57), 135),      # Follow-through
    ((15, 52), (11, 54), 160),      # Hadouken charge: hands cupped at the rear hip, racket angled away
    ((22, 46), (27, 47), 0),        # Hadouken thrust: both hands out front, racket held upright
]

# Left arm: (shoulder, elbow, hand) per pose, None for the plain hanging arm
LEFT_SHOULDER = (7, 46)
LEFT_POSES = [None, ((4, 51), (4, 56)), ((17, 51), (27, 51))]
ORB = (7, 55, 3.2)                  # Energy cupped between the hands while charging

def limb(L, a, b, region, mat):
    (x0, y0), (x1, y1) = a, b
    steps = max(abs(x1 - x0), abs(y1 - y0)) * 2 + 1
    for i in range(steps + 1):
        t = i / steps
        x, y = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
        for dx in (0, 1):
            for dy in (0, 1):
                L.paint(int(round(x - 0.5)) + dx, int(round(y - 0.5)) + dy, region, mat)

def left_arm(frame):
    L = Layer()
    pose = LEFT_POSES[frame]
    if pose is None:
        L.rect(ARM_X, TORSO_TOP + 1, ARM_X + 1, TORSO_BOTTOM - 1, 'arm', 'skin', -0.1)
        L.rect(ARM_X - 1, TORSO_BOTTOM - 1, ARM_X + 1, TORSO_BOTTOM + 1, 'arm', 'skin', -0.1)
        return L.render()
    elbow, hand = pose
    limb(L, LEFT_SHOULDER, elbow, 'arm', 'skin')
    limb(L, elbow, hand, 'arm', 'skin')
    L.rect(hand[0] - 1, hand[1] - 1, hand[0], hand[1], 'arm', 'skin', 0.2)
    return L.render()

def paint_racket(L, hand, angle, mat, grip=False, size=1.0):
    """Racket from the hand at `angle`; `size` scales it (a baby's is smaller)."""
    a = math.radians(angle)
    ux, uy = math.sin(a), -math.cos(a)          # Along the racket
    vx, vy = -uy, ux                            # Across
    hx, hy = hand
    for i in range(1, 1 + round(3 * size)):     # Throat
        L.paint(int(round(hx + ux * i)), int(round(hy + uy * i)), 'racket', mat)
    if grip:                                    # Only seen when no hand covers it
        for i in range(-3, 1):
            L.paint(int(round(hx + ux * i)), int(round(hy + uy * i)), 'grip', 'black')
    cx, cy = hx + ux * 7.5 * size, hy + uy * 7.5 * size    # Head center
    for y in range(L.h):
        for x in range(W):
            dx, dy = x - cx, y - cy
            along, across = dx * ux + dy * uy, dx * vx + dy * vy
            d = (along / (4.6 * size)) ** 2 + (across / (3.2 * size)) ** 2
            if d <= 1.0:
                if d < 0.45:
                    L.paint(x, y, 'strings', 'string', 0.6 if (x + y) % 2 else -0.6)   # String bed
                else:
                    L.paint(x, y, 'racket', mat)

def racket_arm(p, frame):
    elbow, hand, angle = SWING[THRUST if frame == UNARMED else frame]
    L = Layer()
    if frame != UNARMED:
        paint_racket(L, hand, angle, p['racket'])
    limb(L, SHOULDER, elbow, 'arm', 'skin')
    limb(L, elbow, hand, 'arm', 'skin')
    L.rect(hand[0] - 1, hand[1] - 1, hand[0], hand[1], 'arm', 'skin', 0.2)             # Fist over the grip
    if frame == CHARGE:                         # Glowing orb cupped between the hands, in front of everything
        ox, oy, r = ORB
        for y in range(L.h):
            for x in range(W):
                d = math.hypot(x - ox, y - oy)
                if d <= r:
                    L.paint(x, y, 'orb', 'energy', 0.8 - d / r)
        L.pixel(ox, oy, 'V')
    return L.render()

# --- Babies: the players while shrunk ------------------------------------------------------------------
# Same layers and poses on a 32x40 canvas, drawn at the shrunk paddle height with square pixels: a big round
# head with a tuft of the player's hair, big eyes and a pacifier, a onesie in the shirt's colours, a diaper,
# chubby limbs, booties and a small racket. Feature points (players.c BABY_POINTS) follow this layout.

BABY_H = 40
BABY_HEAD = (12.5, 10.0, 9.5, 9.0)      # Ellipse: center x, y, radii
BABY_TORSO = (19, 28)                   # Onesie rows; the diaper follows down to BABY_DIAPER_BOTTOM
BABY_DIAPER_BOTTOM = 33
BABY_SHOULDER = (18, 20)
BABY_SWING = [  # As SWING, shorter arms
    ((20, 24), (21, 26), 15),       # Ready
    ((21, 22), (22, 20), 50),       # Forward
    ((21, 21), (21, 20), 85),       # Contact
    ((20, 24), (20, 27), 135),      # Follow-through
    ((15, 24), (11, 26), 160),      # Hadouken charge
    ((21, 20), (25, 20), 0),        # Hadouken thrust
]
BABY_LEFT_SHOULDER = (8, 20)
BABY_LEFT_POSES = [None, ((5, 24), (6, 27)), ((15, 22), (24, 22))]
BABY_ORB = (8, 26, 2.3)
BABY_RACKET = 0.65

def baby_hair(L, p):
    """By p['baby']: 'tuft', a curl on a nearly bald head (Agassi); 'mop', with the sides down (Nadal, plus his
    headband); 'bow', a cap of hair with a red bow (Graf); 'pony', a little ponytail at the back (Sharapova)."""
    style = p['baby']
    cap = {'tuft': 3, 'mop': 6, 'bow': 5, 'pony': 4}[style]
    for y in range(BABY_H):
        for x in range(W):
            if L.region[y][x] != 'face':
                continue
            side = style == 'mop' and y < 10 and (x < 5 or x > 20)
            if y < cap or side:
                L.paint(x, y, 'hair', p['hair'], 0.3 * math.sin(x * 1.9))
    if style == 'tuft':                         # The curl sticking up
        for x, y in ((12, 0), (13, 0), (14, 0), (14, 1)):
            L.paint(x, y, 'hair', p['hair'], 0.4)
    elif style == 'bow':
        for x, y in ((6, 2), (6, 3), (6, 4), (7, 3), (8, 3), (9, 3), (10, 2), (10, 3), (10, 4)):
            L.paint(x, y, 'bow', 'red', 0.3)
    elif style == 'pony':
        for x, y in ((3, 7), (2, 8), (3, 8), (2, 9), (3, 9), (2, 10), (3, 10), (2, 11), (3, 12)):
            L.paint(x, y, 'pony', p['hair'], 0.2)
        L.paint(4, 6, 'tie', 'pink', 0.3)
    if p['headband']:
        for y in (5, 6):
            for x in range(W):
                if L.region[y][x] in ('hair', 'face'):
                    L.paint(x, y, 'band', 'red')

def baby_face(L, p):
    for x in (7, 8, 15, 16):
        L.pixel(x, 8, p['baby_brow'])
    for x0 in (7, 15):                          # Big eyes with a highlight
        for y in (10, 11, 12):
            L.pixel(x0, y, 'k')
            L.pixel(x0 + 1, y, 'E' if y == 10 else 'k')
    for x in (5, 6, 17, 18):                    # Rosy cheeks
        L.pixel(x, 13, 'l')
    L.pixel(12, 13, 'a'); L.pixel(13, 13, 's')  # Button nose
    tones = MATERIALS[p['shoe_accent']][1]      # Pacifier in the player's accent colour
    for x in range(10, 16):
        L.pixel(x, 15, tones[2])
    L.pixel(12, 16, tones[1]); L.pixel(13, 16, tones[1])
    L.pixel(12, 17, tones[3]); L.pixel(13, 17, tones[3])

def baby_body(p, headless=False):
    L = Layer(BABY_H)
    top, bottom = BABY_TORSO
    for y in range(top, bottom + 1):            # Chubby onesie, the belly bulging
        bulge = 1 if 22 <= y <= 27 else 0
        for x in range(7 - bulge, 19 + bulge):
            shirt = p['shirt'](x, y - top + TORSO_TOP)     # The adult pattern, collar at the top
            L.paint(x, y, 'torso', shirt, (12.5 - x) / 10 - (0.6 if y < top + 2 else 0))
    for y in range(bottom + 1, BABY_DIAPER_BOTTOM + 1):
        for x in range(7, 19):
            if y == BABY_DIAPER_BOTTOM and x in (12, 13):
                continue
            L.paint(x, y, 'diaper', 'white', (12.5 - x) / 14)
    if headless:
        L.rect(10, 17, 15, 18, 'stump', 'blood', 0.4)
        for x, depth in ((8, 2), (9, 4), (10, 1), (11, 5), (12, 3), (13, 6), (14, 2), (15, 3), (16, 1)):
            L.rect(x, top, x, top + depth, 'gore', 'blood', -0.2)
        return L.render()
    cx, cy, rx, ry = BABY_HEAD
    for y in range(BABY_H):
        for x in range(W):
            nx, ny = (x + 0.5 - cx) / rx, (y + 0.5 - cy) / ry
            if nx * nx + ny * ny <= 1:
                L.paint(x, y, 'face', 'skin')
    baby_hair(L, p)
    baby_face(L, p)
    return L.render(dict(regions={'face', 'hair', 'band'}, ellipse=BABY_HEAD))

def baby_legs(p, frame):
    L = Layer(BABY_H)
    lift = {1: (1, 0), 3: (0, 1)}.get(frame, (0, 0))
    for side, (x0, up) in enumerate(((8, lift[0]), (14, lift[1]))):
        foot_y = 37 - up
        L.rect(x0, BABY_DIAPER_BOTTOM + 1, x0 + 2, foot_y - 1, f'leg{side}', 'skin', -0.1)
        L.rect(x0, foot_y, x0 + 3, foot_y + 1, f'shoe{side}', 'white', 0.2)               # Booties
        L.pixel(x0 + 1, foot_y + 1, MATERIALS[p['shoe_accent']][1][2])
    return L.render()

def baby_left_arm(frame):
    L = Layer(BABY_H)
    pose = BABY_LEFT_POSES[frame]
    if pose is None:
        L.rect(5, 20, 6, 25, 'arm', 'skin', -0.1)
        L.rect(4, 25, 6, 27, 'arm', 'skin', -0.1)
        return L.render()
    elbow, hand = pose
    limb(L, BABY_LEFT_SHOULDER, elbow, 'arm', 'skin')
    limb(L, elbow, hand, 'arm', 'skin')
    L.rect(hand[0] - 1, hand[1] - 1, hand[0], hand[1], 'arm', 'skin', 0.2)
    return L.render()

def baby_racket_arm(p, frame):
    elbow, hand, angle = BABY_SWING[THRUST if frame == UNARMED else frame]
    L = Layer(BABY_H)
    if frame != UNARMED:
        paint_racket(L, hand, angle, p['racket'], size=BABY_RACKET)
    limb(L, BABY_SHOULDER, elbow, 'arm', 'skin')
    limb(L, elbow, hand, 'arm', 'skin')
    L.rect(hand[0] - 1, hand[1] - 1, hand[0], hand[1], 'arm', 'skin', 0.2)
    if frame == CHARGE:
        ox, oy, r = BABY_ORB
        for y in range(BABY_H):
            for x in range(W):
                d = math.hypot(x - ox, y - oy)
                if d <= r:
                    L.paint(x, y, 'orb', 'energy', 0.8 - d / r)
        L.pixel(ox, oy, 'V')
    return L.render()

def baby_layers(p):
    return dict(body=baby_body(p), headless=baby_body(p, headless=True),
                legs=[baby_legs(p, f) for f in range(STEP_FRAMES)],
                left=[baby_left_arm(f) for f in range(LEFT_FRAMES)],
                arm=[baby_racket_arm(p, f) for f in range(ARM_FRAMES)])

# --- Hats: worn with the grow bonus, filling the top half of the double-height paddle ----------------------
# HAT_H rows on the player's 32-wide canvas, drawn above the head at the same scale; the bottom HAT_OVERLAP rows
# sit over the top of the head. A tall top hat for the men, a towering Ascot hat for the ladies.

HAT_H, HAT_OVERLAP = 96, 16
HAT_BRIM = HAT_H - 6                    # Brim row: head row 10, on the hair above the brows

def top_hat():
    L = Layer(HAT_H)
    L.rect(5, 4, 20, HAT_BRIM - 1, 'crown', 'black', -0.2)              # Crown, a touch wider at the top
    L.rect(4, 4, 21, 8, 'crown', 'black', 0.0)
    L.rect(5, HAT_BRIM - 9, 20, HAT_BRIM - 4, 'band', 'red', 0.2)       # Band
    L.rect(0, HAT_BRIM, 25, HAT_BRIM + 2, 'brim', 'black', 0.1)
    return L.render()

def lady_hat():
    L = Layer(HAT_H)
    # Plumes of feathers sweeping up from the crown, painted first so the flowers sit in front
    for (x0, x1, lean, mat) in ((9, 3, -0.9, 'white'), (13, 13, 0.0, 'pink'), (17, 25, 0.9, 'white')):
        for y in range(4, HAT_BRIM - 12):
            t = (HAT_BRIM - 12 - y) / (HAT_BRIM - 16)               # 0 at the crown, 1 at the tip
            cx = x0 + (x1 - x0) * t + lean * 4 * math.sin(t * math.pi) + 0.8 * math.sin(y * 0.7)
            half = 0.8 + 2.4 * t * (1.2 - t) * 2                    # Thin quill, fluffy towards the tip
            for x in range(int(cx - half), int(cx + half) + 1):
                L.paint(x, y, f'plume{x0}', mat, 0.3 * math.sin(y * 0.8))
    L.rect(5, HAT_BRIM - 12, 20, HAT_BRIM - 1, 'crown', 'pink', 0.0)      # Crown
    L.rect(5, HAT_BRIM - 5, 20, HAT_BRIM - 3, 'ribbon', 'lime', 0.2)
    for cx, cy, mat in ((7, HAT_BRIM - 11, 'red'), (12, HAT_BRIM - 13, 'white'), (18, HAT_BRIM - 11, 'red')):
        for y in range(cy - 2, cy + 3):                                 # Flowers on the crown
            for x in range(cx - 2, cx + 3):
                if (x - cx) ** 2 + (y - cy) ** 2 <= 5:
                    L.paint(x, y, f'flower{cx}', mat, 0.4)
        L.pixel(cx, cy, 'Q')
    L.rect(0, HAT_BRIM, 27, HAT_BRIM + 2, 'brim', 'pink', 0.1)            # Wide brim
    return L.render()

# --- Scared: white face overlay, drawn while the player is level with an invisible (ghost) opponent --------
# The head's skin pixels from a body layer, recoloured in the white material's tones, then a terrified look:
# wide eyes with tiny pupils, brows arched up (the usual ones covered), and an open "O" mouth. Babies keep their
# pacifier. Hair and lashes stay as they are (left transparent).

SKIN_TO_WHITE = {'e': '#', 'd': 'w', 's': 'W', 'S': '@', 'a': '~'}

def scared(body_rows, head_rows, baby=False):
    rows = [[SKIN_TO_WHITE.get(c, '.') if y < head_rows else '.' for c in row] for y, row in enumerate(body_rows)]

    def put(x, y, c):
        rows[y][x] = c

    if baby:
        for x0 in (6, 14):                          # Big round eyes
            for dy, line in enumerate(("eKKe", "KEEK", "KkkK", "eKKe")):
                for i, c in enumerate(line):
                    put(x0 + i, 9 + dy, c)
        return [''.join(r) for r in rows]
    for x0, x1 in ((5, 9), (16, 20)):
        brow = body_rows[20][x0 + 2]                # The player's brow colour
        for x in range(x0, x1 + 1):                 # Cover the usual brows, arch new ones higher up
            put(x, 19, '@'); put(x, 20, '@')
            put(x, 17 if x0 < x < x1 else 18, brow)
        for dy, line in enumerate(("eKKKe", "KEEEK", "KEkEK", "eKKKe")):    # Ringed, so they read on white
            for i, c in enumerate(line):
                put(x0 + i, 21 + dy, c)
    for x in range(9, 17):                          # Mouth wiped, then a small open "O"
        for y in range(32, 36):
            put(x, y, '@')
    for x, y, c in ((11, 32, 'e'), (12, 32, 'e'), (13, 32, 'e'), (14, 32, 'e'),
                    (10, 33, 'e'), (11, 33, 'K'), (12, 33, 'K'), (13, 33, 'K'), (14, 33, 'K'), (15, 33, 'e'),
                    (10, 34, 'e'), (11, 34, 'K'), (12, 34, 'r'), (13, 34, 'r'), (14, 34, 'K'), (15, 34, 'e'),
                    (11, 35, 'e'), (12, 35, 'e'), (13, 35, 'e'), (14, 35, 'e')):
        put(x, y, c)
    return [''.join(r) for r in rows]

# --- Crossed eyes: overlay drawn on the face while a player has the zig-zag bonus ------------------------
# Only the eyes: bulging whites with the pupils in the inner corners. Every player shares the eye layout; no baby
# version, as a shrunk player can't have the zig-zag bonus at the same time.

def crossed_eyes():
    L = Layer()
    for x0, rows in ((5, ("eEEEe", "EEEkk", "eEEkk")), (16, ("eEEEe", "kkEEE", "kkEEe"))):
        for dy, row in enumerate(rows):
            for i, c in enumerate(row):
                L.pixel(x0 + i, 22 + dy, c)
    return L.render()

# --- Force users: Vader or Luke stands in for a player with the full-height bonus ------------------------
# Same 32x80 canvas and Pop! head; they don't walk, so it's a body (legs, cape and the back arm holding a lit
# lightsaber included), a headless body for fatalities, and the front arm idle or in the force push.

FORCE_ARM_POSES = 2                     # Idle, push
FORCE_HAND = (29, 46)                   # Open palm in the push pose (players.c FORCE_POINTS)

FORCE_USERS = {
    'VADER': dict(sleeve='black', hand='black', blade='red', legs='black', boots='black'),
    'LUKE': dict(sleeve='white', hand='skin', blade='energy', legs='white', boots='brown'),
}

def vader_helmet(L):
    """The Pop! head as a black helmet flaring out at the bottom: lenses, brow ridge, the triangular grille and
    cheek vents."""
    for y, half in enumerate(HEAD_MASK):
        for x, c in enumerate(half + half[::-1]):
            if c != '.':
                L.paint(HEAD_X + x, y, 'helmet', 'black', -0.7)
    for y in range(32, 43):                                     # Flared lower edge, over the shoulders
        ext = (y - 31) // 3
        for x in range(max(0, HEAD_X - ext), min(W, HEAD_X + 24 + ext)):
            L.paint(x, y, 'helmet', 'black', -0.9)
    for x in range(5, 21):
        L.pixel(x, 20, '5')                                     # Brow ridge
    for x0 in (5, 16):                                          # Lenses with a glint
        for y, (a, b) in zip(range(21, 25), ((1, 4), (0, 5), (0, 5), (1, 4))):
            for x in range(x0 + a, x0 + b):
                L.pixel(x, y, 'K')
        L.pixel(x0 + 1, 22, '6')
    for y in range(25, 29):                                     # Nose ridge
        L.pixel(12, y, '5'); L.pixel(13, y, '4')
    for i, y in enumerate(range(29, 37)):                       # Grille, widening down, with dark slits
        half = 1 + i // 2
        for x in range(13 - half, 13 + half):
            L.pixel(x, y, 'K' if (x + y) % 2 and i > 1 else 'O')
    for y in range(30, 35):                                     # Cheek vents
        L.pixel(7, y, '4'); L.pixel(18, y, '4')

def force_body(name, f, headless=False):
    L = Layer()
    vader = name == 'VADER'
    dark = -0.6 if vader else 0.0                               # Vader's black stays black
    if vader:                                                   # Cape flaring out behind him to the floor
        for y in range(42, 78):
            ext = (y - 42) // 7
            for x in range(5 - ext, 21 + ext):
                L.paint(x, y, 'cape', 'black', -1.0)
    for y in range(TORSO_TOP, TORSO_BOTTOM + 1):
        for x in range(TORSO_LEFT, TORSO_RIGHT + 1):
            L.paint(x, y, 'torso', f['sleeve'], (12.5 - x) / 14 - (0.6 if y < TORSO_TOP + 2 else 0) + dark)
    for side, x0 in enumerate((9, 14)):                         # Legs and boots, standing still
        L.rect(x0, TORSO_BOTTOM + 1, x0 + 2, 71, f'leg{side}', f['legs'], -0.3 + dark)
        L.rect(x0 - (1 - side), 72, x0 + 2 + side, 76, f'boot{side}', f['boots'], 0.1 + dark)
    # Back arm down to the hand holding the saber upright
    limb(L, (7, 46), (5, 52), 'back_arm', f['sleeve'])
    limb(L, (5, 52), (5, 56), 'back_arm', f['sleeve'])
    if headless:
        L.rect(HEAD_X + 10, 41, HEAD_X + 13, 44, 'neck', 'black' if vader else 'skin', -0.3)
        L.rect(HEAD_X + 10, 40, HEAD_X + 13, 41, 'stump', 'blood', 0.4)
        for x, depth in ((9, 3), (10, 6), (11, 2), (12, 8), (13, 4), (14, 5), (15, 2)):
            L.rect(x, TORSO_TOP, x, TORSO_TOP + depth, 'gore', 'blood', -0.2)
    elif vader:
        vader_helmet(L)
    else:
        paint_head(L, 'honey', False, 'short')
    L.rect(4, 54, 5, 61, 'hilt', 'grey', 0.2)                   # Saber: hilt, then the blade up past the head
    L.rect(4, 24, 5, 53, 'blade', f['blade'], 1.2)
    L.rect(4, 55, 6, 57, 'back_hand', f['hand'], 0.2)
    rows_sphere = dict(regions={'face', 'hair', 'helmet'}, ellipse=SPHERE['ellipse'])
    if not headless and not vader:
        face(L, True, [(x, 'f') for x in range(5, 10)] + [(x, 'f') for x in range(16, 21)])
    if not headless and vader:
        # Chest panel and belt boxes
        L.rect(10, 48, 15, 51, 'panel', 'grey', -0.2)
        for x, y, c in ((11, 49, 'R'), (12, 49, 'G'), (13, 49, 'c'), (14, 49, 'R'), (11, 50, 'c'), (13, 50, 'R')):
            L.pixel(x, y, c)
    if vader:
        for x in (9, 11, 14, 16):
            L.pixel(x, 56, '*'); L.pixel(x, 57, 'O')
    else:
        for y in range(TORSO_TOP, TORSO_TOP + 9):               # Tunic wrap, then the belt and buckle
            L.pixel(12 + (y - TORSO_TOP) // 2, y, 'w')
        for x in range(TORSO_LEFT, TORSO_RIGHT + 1):
            L.pixel(x, 56, 'H'); L.pixel(x, 57, 'h')
        L.pixel(12, 56, '*'); L.pixel(13, 56, '*')
    return L.render(rows_sphere)

def force_arm(f, pose):
    """Front arm: 0 idle at the side, 1 pushing the Force with the palm out."""
    L = Layer()
    if pose == 0:
        limb(L, SHOULDER, (19, 52), 'arm', f['sleeve'])
        limb(L, (19, 52), (19, 56), 'arm', f['sleeve'])
        L.rect(18, 56, 19, 58, 'hand', f['hand'], 0.2)
    else:
        limb(L, SHOULDER, (23, 47), 'arm', f['sleeve'])
        limb(L, (23, 47), (26, 46), 'arm', f['sleeve'])
        hx, hy = FORCE_HAND
        L.rect(hx - 1, hy - 3, hx, hy + 2, 'hand', f['hand'], 0.3)   # Palm out, fingers up
    return L.render()

def force_layers(name, f):
    return dict(body=force_body(name, f), headless=force_body(name, f, headless=True),
                arm=[force_arm(f, i) for i in range(FORCE_ARM_POSES)])

# --- Players ---------------------------------------------------------------------------------------------

PLAYERS = {
    # Agassi, early 90s: highlighted blond mullet, tan, easy smile, black and neon-pink shirt, acid-wash denim
    'AGASSI': dict(hair='blond', headband=False, smile=True, shirt=agassi_shirt, sleeveless=False,
                   shorts='denim', shorts_bottom=63, shoe_accent='pink', racket='grey', baby='tuft', baby_brow='z',
                   brows=[(x, 'z') for x in range(5, 10)] + [(x, 'z') for x in range(16, 21)]),
    # Nadal, mid 2000s: long dark hair, red headband, heavy brows, sleeveless lime top, white pirate capris
    'NADAL': dict(hair='brown', headband=True, smile=False, shirt=nadal_shirt, sleeveless=True,
                  shorts='white', shorts_bottom=67, shoe_accent='lime', racket='yellow', baby='mop', baby_brow='h',
                  brows=[(x, 'xh') for x in range(5, 10)] + [(x, 'xh') for x in range(16, 21)]),
    # Graf, late 80s: golden blond bob with bangs, lashes, serious look, white top with a red V collar, white pleated skirt
    'GRAF': dict(hair='honey', hairstyle='bob', headband=False, smile=False, shirt=graf_shirt, sleeveless=False, lashes=True,
                 shorts='white', skirt=True, shorts_bottom=63, shoe_accent='red', racket='red', baby='bow',
                 baby_brow='f', brows=[(x, 'F') for x in range(5, 10)] + [(x, 'F') for x in range(16, 21)]),
    # Sharapova, mid 2000s: platinum hair pulled back into a long ponytail, red lipstick, the black dress with crystals at the neckline
    'SHARAPOVA': dict(hair='plat', hairstyle='pulled', headband=False, smile=True, lips="r^", shirt=sharapova_shirt, sleeveless=True,
                      lashes=True, ponytail=True, shorts='black', skirt=True, shorts_bottom=61, shoe_accent='pink',
                      racket='white', baby='pony', baby_brow='z',
                      brows=[(x, 'z') for x in range(5, 10)] + [(x, 'z') for x in range(16, 21)]),
}

def racket(p):
    """The racket alone, upright with its grip, cropped to its bounding box (thrown in a fatality)."""
    L = Layer()
    paint_racket(L, (W // 2, H // 2 + 6), 0, p['racket'], grip=True)
    rows = L.render()
    ys = [y for y, r in enumerate(rows) if r.strip('.')]
    xs = [x for r in rows for x, c in enumerate(r) if c != '.']
    return [r[min(xs):max(xs) + 1] for r in rows[min(ys):max(ys) + 1]]

def layers(p):
    return dict(body=body(p), headless=body(p, headless=True), legs=[legs(p, f) for f in range(STEP_FRAMES)],
                left=[left_arm(f) for f in range(LEFT_FRAMES)], arm=[racket_arm(p, f) for f in range(ARM_FRAMES)],
                racket=racket(p), baby=baby_layers(p))

# --- Output ----------------------------------------------------------------------------------------------

def c_array(name, rows, indent=""):
    lines = [f"{indent}{{  // {name}"] + [f'{indent}    "{r}",' for r in rows] + [f"{indent}}},"]
    return "\n".join(lines)

def emit_inc(path, built, force):
    out = ["// Generated by tools/gen_sprites.py; edit the generator, not this file.", "",
           "// One character per art pixel; '.' is transparent",
           "static SDL_Color palette_color(char c) {", "    switch (c) {"]
    for c, (r, g, b) in COLORS.items():
        out.append(f"    case '{c}': return (SDL_Color){{ {r}, {g}, {b}, 255 }};")
    out += ["    default:  return (SDL_Color){ 0, 0, 0, 0 };", "    }", "}", ""]
    arm_names = ["ready", "forward", "contact", "follow-through", "charge", "thrust", "unarmed"]
    rackets = [l['racket'] for l in built.values()]
    assert all(len(r) == len(rackets[0]) and len(r[0]) == len(rackets[0][0]) for r in rackets)
    out += [f"#define RACKET_W {len(rackets[0][0])}", f"#define RACKET_H {len(rackets[0])}", ""]
    for name, l in built.items():
        out.append(f"static const char *const {name}_BODY[SPRITE_H] =")
        out.append(c_array("body", l['body'])[:-1] + ";")
        out.append(f"static const char *const {name}_HEADLESS[SPRITE_H] =")
        out.append(c_array("headless body", l['headless'])[:-1] + ";")
        out.append(f"static const char *const {name}_LEGS[STEP_POSES][SPRITE_H] = {{")
        out += [c_array(f"step {i}", f, "    ") for i, f in enumerate(l['legs'])]
        out.append("};")
        out.append(f"static const char *const {name}_LEFT_ARM[LEFT_ARM_POSES][SPRITE_H] = {{")
        out += [c_array(n, f, "    ") for n, f in zip(["hanging", "charge", "thrust"], l['left'])]
        out.append("};")
        out.append(f"static const char *const {name}_ARM[ARM_POSES][SPRITE_H] = {{")
        out += [c_array(n, f, "    ") for n, f in zip(arm_names, l['arm'])]
        out.append("};")
        out.append(f"static const char *const {name}_SCARED[SPRITE_H] =")
        out.append(c_array("scared face", scared(l['body'], HEAD_ROWS))[:-1] + ";")
        out.append(f"static const char *const {name}_RACKET[RACKET_H] =")
        out.append(c_array("thrown racket", l['racket'])[:-1] + ";")
        b = l['baby']
        out.append(f"static const char *const {name}_BABY_BODY[BABY_H] =")
        out.append(c_array("baby body", b['body'])[:-1] + ";")
        out.append(f"static const char *const {name}_BABY_SCARED[BABY_H] =")
        out.append(c_array("scared baby face", scared(b['body'], BABY_TORSO[0], baby=True))[:-1] + ";")
        out.append(f"static const char *const {name}_BABY_HEADLESS[BABY_H] =")
        out.append(c_array("headless baby body", b['headless'])[:-1] + ";")
        out.append(f"static const char *const {name}_BABY_LEGS[STEP_POSES][BABY_H] = {{")
        out += [c_array(f"baby step {i}", f, "    ") for i, f in enumerate(b['legs'])]
        out.append("};")
        out.append(f"static const char *const {name}_BABY_LEFT_ARM[LEFT_ARM_POSES][BABY_H] = {{")
        out += [c_array("baby " + n, f, "    ") for n, f in zip(["hanging", "charge", "thrust"], b['left'])]
        out.append("};")
        out.append(f"static const char *const {name}_BABY_ARM[ARM_POSES][BABY_H] = {{")
        out += [c_array("baby " + n, f, "    ") for n, f in zip(arm_names, b['arm'])]
        out.append("};")
        out.append("")
    out.append(f"#define HAT_H {HAT_H}")
    out.append(f"#define HAT_OVERLAP {HAT_OVERLAP}")
    out.append("static const char *const TOP_HAT[HAT_H] =")
    out.append(c_array("top hat (grow, men)", top_hat())[:-1] + ";")
    out.append("static const char *const LADY_HAT[HAT_H] =")
    out.append(c_array("Ascot hat (grow, ladies)", lady_hat())[:-1] + ";")
    out.append("static const char *const CROSSED_EYES[SPRITE_H] =")
    out.append(c_array("crossed eyes (zig-zag)", crossed_eyes())[:-1] + ";")
    out.append("")
    for name, l in force.items():
        out.append(f"static const char *const {name}_FORCE_BODY[SPRITE_H] =")
        out.append(c_array("force user body", l['body'])[:-1] + ";")
        out.append(f"static const char *const {name}_FORCE_SCARED[SPRITE_H] =")
        out.append(c_array("scared face (none under a helmet)", scared(l['body'], HEAD_ROWS) if name != 'VADER'
                           else ['.' * W] * H)[:-1] + ";")
        out.append(f"static const char *const {name}_FORCE_HEADLESS[SPRITE_H] =")
        out.append(c_array("headless force user body", l['headless'])[:-1] + ";")
        out.append(f"static const char *const {name}_FORCE_ARM[FORCE_ARM_POSES][SPRITE_H] = {{")
        out += [c_array(n, a, "    ") for n, a in zip(["idle", "push"], l['arm'])]
        out.append("};")
        out.append("")
    open(path, "w").write("\n".join(out))

# Writes the preview PNGs and GIF into out_dir (created if needed)
def previews(built, force, out_dir):
    from PIL import Image
    os.makedirs(out_dir, exist_ok=True)
    bg, scale, pad = (14, 14, 18), 8, 40

    def compose(stack, mirror=False, h=H):
        img = Image.new('RGB', (W, h), bg)
        for rows in stack:
            for y, row in enumerate(rows):
                for x, c in enumerate(row):
                    if c in COLORS:
                        img.putpixel((x, y), COLORS[c])
        if mirror:
            img = img.transpose(Image.FLIP_LEFT_RIGHT)
        return img.resize((W * scale, h * scale), Image.NEAREST)

    def lineup(stacks):
        """Side by side, every other one mirrored as the right-hand player"""
        imgs = [compose(st, mirror=i % 2 == 1) for i, st in enumerate(stacks)]
        img = Image.new('RGB', ((imgs[0].width + pad) * len(imgs) + pad, imgs[0].height + pad * 2), bg)
        for i, im in enumerate(imgs):
            img.paste(im, (pad + i * (im.width + pad), pad))
        return img

    def stack(pl, step=0, left=0, arm=0, headless=False):
        l = built[pl]
        return [l['headless' if headless else 'body'], l['legs'][step], l['left'][left], l['arm'][arm]]

    for name in built:
        compose(stack(name)).save(f"{out_dir}/{name.lower()}.png")
    lineup([stack(name) + [crossed_eyes()] for name in built]).save(f"{out_dir}/crossed_eyes.png")
    lineup([stack(name)[:1] + [scared(built[name]['body'], HEAD_ROWS)] + stack(name)[1:] for name in built]
           ).save(f"{out_dir}/scared.png")

    # Each player with their hat, the way it sits in game: body layers padded on top, the hat at the bottom
    def hatted(name, mirror):
        hat = top_hat() if name in ('AGASSI', 'NADAL') else lady_hat()
        above, below = HAT_H - HAT_OVERLAP, H - HAT_OVERLAP
        layers = [['.' * W] * above + rows for rows in stack(name)] + [hat + ['.' * W] * below]
        return compose(layers, mirror, above + H)
    imgs = [hatted(name, i % 2 == 1) for i, name in enumerate(built)]
    sheet = Image.new('RGB', ((imgs[0].width + pad) * len(imgs) + pad, imgs[0].height + pad * 2), bg)
    for i, im in enumerate(imgs):
        sheet.paste(im, (pad + i * (im.width + pad), pad))
    sheet.save(f"{out_dir}/hats.png")
    lineup([stack(name) for name in built]).save(f"{out_dir}/players.png")

    # Sheet: step cycle, swing, the hadouken charge and thrust, then the fatality (unarmed throw, headless)
    cells = ([stack('AGASSI', step=i) for i in range(STEP_FRAMES)] +
             [stack('AGASSI', arm=i) for i in range(SWING_FRAMES)] +
             [stack('AGASSI', left=1, arm=CHARGE), stack('AGASSI', left=2, arm=THRUST),
              stack('NADAL', left=1, arm=CHARGE), stack('NADAL', left=2, arm=THRUST)] +
             [stack('AGASSI', left=2, arm=UNARMED), stack('NADAL', headless=True),
              stack('NADAL', left=2, arm=UNARMED), stack('AGASSI', headless=True)])
    imgs = [compose(c) for c in cells]
    cw, ch = imgs[0].width + pad, imgs[0].height + pad
    sheet = Image.new('RGB', (cw * 4 + pad, ch * 4 + pad), bg)
    for i, im in enumerate(imgs):
        sheet.paste(im, (pad + (i % 4) * cw, pad + (i // 4) * ch))
    sheet.save(f"{out_dir}/agassi_frames.png")

    # Animated GIF: both players walking, swinging, then throwing a hadouken, as in game
    def both(**kw):
        return lineup([stack(name, **kw) for name in built])
    frames = [both(step=i % 4) for i in range(8)]
    frames += [both(arm=f) for f in (2, 2, 3, 3, 3, 1, 1, 0, 0, 0)]
    frames += [both(left=1, arm=CHARGE)] * 5 + [both(left=2, arm=THRUST)] * 5 + [both()] * 3
    frames[0].save(f"{out_dir}/players.gif", save_all=True, append_images=frames[1:], duration=90, loop=0)

    # Babies: each standing, swinging, charging and throwing, then headless
    def baby(pl, step=0, left=0, arm=0, headless=False, mirror=False):
        b = built[pl]['baby']
        return compose([b['headless' if headless else 'body'], b['legs'][step], b['left'][left], b['arm'][arm]],
                       mirror, BABY_H)
    cells = []
    for i, name in enumerate(built):
        m = i % 2 == 1
        cells += [baby(name, mirror=m), baby(name, step=1, arm=2, mirror=m), baby(name, left=1, arm=CHARGE, mirror=m),
                  baby(name, left=2, arm=THRUST, mirror=m), baby(name, headless=True, mirror=m)]
    cw, ch = cells[0].width + pad, cells[0].height + pad
    sheet = Image.new('RGB', (cw * 5 + pad, ch * len(built) + pad), bg)
    for i, im in enumerate(cells):
        sheet.paste(im, (pad + (i % 5) * cw, pad + (i // 5) * ch))
    sheet.save(f"{out_dir}/babies.png")

    # Force users: each idle, pushing, then headless
    cells = []
    for i, (name, l) in enumerate(force.items()):
        cells += [compose([l['body'], l['arm'][0]], i % 2 == 1), compose([l['body'], l['arm'][1]], i % 2 == 1),
                  compose([l['headless'], l['arm'][0]], i % 2 == 1)]
    cw, ch = cells[0].width + pad, cells[0].height + pad
    sheet = Image.new('RGB', (cw * 3 + pad, ch * len(force) + pad), bg)
    for i, im in enumerate(cells):
        sheet.paste(im, (pad + (i % 3) * cw, pad + (i // 3) * ch))
    sheet.save(f"{out_dir}/force.png")

PREVIEW_DIR = "target/screenshots/previews"

if __name__ == "__main__":
    assert len(HEAD_MASK) == 44 and all(len(r) == 12 for r in HEAD_MASK)
    assert len(set(COLORS)) == len(COLORS) and not set(COLORS) & set('."\\')
    for outline, tones in MATERIALS.values():
        assert all(c in COLORS for c in tones + (outline or ''))
    built = {name: layers(p) for name, p in PLAYERS.items()}
    force = {name: force_layers(name, f) for name, f in FORCE_USERS.items()}
    if "--previews-only" not in sys.argv:
        emit_inc("src/render/player_sprites.inc", built, force)
        print("wrote src/render/player_sprites.inc")
    previews(built, force, PREVIEW_DIR)
    print(f"wrote {PREVIEW_DIR}/{{agassi,nadal,players,agassi_frames,babies,force,crossed_eyes,hats,scared}}.png, "
          f"{PREVIEW_DIR}/players.gif")
