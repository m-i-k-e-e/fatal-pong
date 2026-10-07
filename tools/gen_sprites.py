#!/usr/bin/env python3
"""Generate the player sprite layers and palette (player_sprites.inc) and preview images (assets/).

Each player is drawn as four 32x80 layers stacked in the game: a body (big Pop!-figure head, torso,
shorts), one of 4 leg frames (step cycle), a left arm (hanging, or the hadouken charge and thrust) and
a racket arm (4 swing poses plus the hadouken charge and thrust).

Style: shaded pixel art. Shapes are painted as regions of a material (skin, blond hair, denim...);
a lighting pass picks one of each material's 4 tones (light from the upper left: the head is lit as
a sphere, everything else by its edges) and a final pass adds a selective outline in each material's
darkest colour. Facial features are placed by hand on top.

Run from the project root:  python3 tools/gen_sprites.py
"""
import math

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
    def __init__(self):
        self.region = [[None] * W for _ in range(H)]
        self.mat = [[None] * W for _ in range(H)]
        self.bias = [[0.0] * W for _ in range(H)]
        self.over = [[None] * W for _ in range(H)]     # Hand-placed pixels drawn after shading

    def paint(self, x, y, region, mat, bias=0.0):
        if 0 <= x < W and 0 <= y < H:
            self.region[y][x], self.mat[y][x], self.bias[y][x] = region, mat, bias

    def rect(self, x0, y0, x1, y1, region, mat, bias=0.0):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.paint(x, y, region, mat, bias)

    def pixel(self, x, y, c):
        if 0 <= x < W and 0 <= y < H:
            self.over[y][x] = c

    def inside(self, x, y, region):
        return 0 <= x < W and 0 <= y < H and self.region[y][x] == region

    def render(self, sphere=None):
        """Pick a tone per pixel, add outlines, apply hand-placed pixels. Returns rows of chars."""
        out = [['.'] * W for _ in range(H)]
        for y in range(H):
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
        for y in range(H):
            for x in range(W):
                if self.region[y][x] is not None:
                    continue
                for dx, dy in ((0, -1), (-1, 0), (1, 0), (0, 1)):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < W and 0 <= ny < H and self.mat[ny][nx] and MATERIALS[self.mat[ny][nx]][0]:
                        out[y][x] = MATERIALS[self.mat[ny][nx]][0]
                        break
        for y in range(H):
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

def paint_head(L, hair_mat, headband):
    for y, half in enumerate(HEAD_MASK):
        for x, c in enumerate(half + half[::-1]):
            px = HEAD_X + x
            if c == 'h':
                # Strand streaks; long hair darkens toward the ends
                streak = 0.3 * math.sin(px * 1.9 + 0.5 * math.sin(y * 0.35))
                L.paint(px, y, 'hair', hair_mat, streak - max(0, y - 24) * 0.035)
            elif c == 'f':
                # Hair shades the face just below and beside it
                near_hair = any(0 <= y - dy and HEAD_MASK[y - dy][min(x, 23 - x)] == 'h' for dy in (1, 2))
                L.paint(px, y, 'face', 'skin', -0.5 if near_hair else 0.0)
            elif c == 'n':
                L.paint(px, y, 'neck', 'skin', -0.6)
    if headband:
        for y in (11, 12, 13):
            for x in range(W):
                if L.region[y][x] in ('hair', 'face'):
                    L.paint(x, y, 'band', 'red')

def face(L, smile, brows):
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
            L.pixel(x, 33, 'L')
        for x in range(11, 15):
            L.pixel(x, 34, 'l')
    else:
        for x in range(10, 16):
            L.pixel(x, 33, 'e')
        for x in range(11, 15):
            L.pixel(x, 34, 'L')
    for x in range(11, 15):
        L.pixel(x, 35, 's')

# --- Body layer ----------------------------------------------------------------------------------------

TORSO_TOP, TORSO_BOTTOM = 45, 57        # Fill rows; the outline adds a row above
TORSO_LEFT, TORSO_RIGHT = 8, 17
ARM_X = 5                               # Hanging left arm, 2 px of skin

def body(p, headless=False):
    """Head, torso and shorts; the left arm is its own layer so it can animate.
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
    paint_head(L, p['hair'], p['headband'])
    face(L, p['smile'], p['brows'])
    return L.render(SPHERE)

def agassi_shirt(x, y):
    return 'pink' if (x + y) % 4 == 0 else 'black'

def nadal_shirt(x, y):
    return 'lime'

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

def paint_racket(L, hand, angle, mat, grip=False):
    a = math.radians(angle)
    ux, uy = math.sin(a), -math.cos(a)          # Along the racket
    vx, vy = -uy, ux                            # Across
    hx, hy = hand
    for i in range(1, 4):                       # Throat
        L.paint(int(round(hx + ux * i)), int(round(hy + uy * i)), 'racket', mat)
    if grip:                                    # Only seen when no hand covers it
        for i in range(-3, 1):
            L.paint(int(round(hx + ux * i)), int(round(hy + uy * i)), 'grip', 'black')
    cx, cy = hx + ux * 7.5, hy + uy * 7.5       # Head center
    for y in range(H):
        for x in range(W):
            dx, dy = x - cx, y - cy
            along, across = dx * ux + dy * uy, dx * vx + dy * vy
            d = (along / 4.6) ** 2 + (across / 3.2) ** 2
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
        for y in range(H):
            for x in range(W):
                d = math.hypot(x - ox, y - oy)
                if d <= r:
                    L.paint(x, y, 'orb', 'energy', 0.8 - d / r)
        L.pixel(ox, oy, 'V')
    return L.render()

# --- Players ---------------------------------------------------------------------------------------------

PLAYERS = {
    # Agassi, early 90s: highlighted blond mullet, tan, easy smile, black and neon-pink shirt, acid-wash denim
    'AGASSI': dict(hair='blond', headband=False, smile=True, shirt=agassi_shirt, sleeveless=False,
                   shorts='denim', shorts_bottom=63, shoe_accent='pink', racket='grey',
                   brows=[(x, 'z') for x in range(5, 10)] + [(x, 'z') for x in range(16, 21)]),
    # Nadal, mid 2000s: long dark hair, red headband, heavy brows, sleeveless lime top, white pirate capris
    'NADAL': dict(hair='brown', headband=True, smile=False, shirt=nadal_shirt, sleeveless=True,
                  shorts='white', shorts_bottom=67, shoe_accent='lime', racket='yellow',
                  brows=[(x, 'xh') for x in range(5, 10)] + [(x, 'xh') for x in range(16, 21)]),
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
                racket=racket(p))

# --- Output ----------------------------------------------------------------------------------------------

def c_array(name, rows, indent=""):
    lines = [f"{indent}{{  // {name}"] + [f'{indent}    "{r}",' for r in rows] + [f"{indent}}},"]
    return "\n".join(lines)

def emit_inc(path, built):
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
        out.append(f"static const char *const {name}_RACKET[RACKET_H] =")
        out.append(c_array("thrown racket", l['racket'])[:-1] + ";")
        out.append("")
    open(path, "w").write("\n".join(out))

def previews(built):
    from PIL import Image
    bg, scale, pad = (14, 14, 18), 8, 40

    def compose(stack, mirror=False):
        img = Image.new('RGB', (W, H), bg)
        for rows in stack:
            for y, row in enumerate(rows):
                for x, c in enumerate(row):
                    if c in COLORS:
                        img.putpixel((x, y), COLORS[c])
        if mirror:
            img = img.transpose(Image.FLIP_LEFT_RIGHT)
        return img.resize((W * scale, H * scale), Image.NEAREST)

    def pair(a_stack, n_stack):
        a, n = compose(a_stack), compose(n_stack, mirror=True)
        img = Image.new('RGB', (a.width * 2 + pad * 3, a.height + pad * 2), bg)
        img.paste(a, (pad, pad)); img.paste(n, (a.width + pad * 2, pad))
        return img

    def stack(pl, step=0, left=0, arm=0, headless=False):
        l = built[pl]
        return [l['headless' if headless else 'body'], l['legs'][step], l['left'][left], l['arm'][arm]]

    for name in built:
        compose(stack(name)).save(f"assets/{name.lower()}.png")
    pair(stack('AGASSI'), stack('NADAL')).save("assets/players.png")

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
    sheet.save("assets/agassi_frames.png")

    # Animated GIF: both players walking, swinging, then throwing a hadouken, as in game
    def both(**kw):
        return pair(stack('AGASSI', **kw), stack('NADAL', **kw))
    frames = [both(step=i % 4) for i in range(8)]
    frames += [both(arm=f) for f in (2, 2, 3, 3, 3, 1, 1, 0, 0, 0)]
    frames += [both(left=1, arm=CHARGE)] * 5 + [both(left=2, arm=THRUST)] * 5 + [both()] * 3
    frames[0].save("assets/players.gif", save_all=True, append_images=frames[1:], duration=90, loop=0)

if __name__ == "__main__":
    assert len(HEAD_MASK) == 44 and all(len(r) == 12 for r in HEAD_MASK)
    assert len(set(COLORS)) == len(COLORS) and not set(COLORS) & set('."\\')
    for outline, tones in MATERIALS.values():
        assert all(c in COLORS for c in tones + (outline or ''))
    built = {name: layers(p) for name, p in PLAYERS.items()}
    emit_inc("player_sprites.inc", built)
    previews(built)
    print("wrote player_sprites.inc and assets/{agassi,nadal,players,agassi_frames}.png, assets/players.gif")
