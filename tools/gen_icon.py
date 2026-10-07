# Launcher icon (assets/icon0.png): "FATAL" in dripping blood over "PONG", a fireball crossing the court.
# Drawn at 128x128 with the game's 5x7 font, saved scaled 4x. Run from the project root.
import random

from PIL import Image, ImageDraw

# Same 5x7 glyphs as text.c
FONT = {
 'A': [0x0E,0x11,0x11,0x1F,0x11,0x11,0x11], 'D': [0x1C,0x12,0x11,0x11,0x11,0x12,0x1C],
 'E': [0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F], 'G': [0x0E,0x11,0x10,0x17,0x11,0x11,0x0F],
 'H': [0x11,0x11,0x11,0x1F,0x11,0x11,0x11], 'K': [0x11,0x12,0x14,0x18,0x14,0x12,0x11],
 'N': [0x11,0x11,0x19,0x15,0x13,0x11,0x11], 'O': [0x0E,0x11,0x11,0x11,0x11,0x11,0x0E],
 'P': [0x1E,0x11,0x11,0x1E,0x10,0x10,0x10], 'U': [0x11,0x11,0x11,0x11,0x11,0x11,0x0E],
 'F': [0x1F,0x10,0x10,0x1E,0x10,0x10,0x10], 'I': [0x0E,0x04,0x04,0x04,0x04,0x04,0x0E],
 'T': [0x1F,0x04,0x04,0x04,0x04,0x04,0x04], 'L': [0x10,0x10,0x10,0x10,0x10,0x10,0x1F],
}
N = 128
img = Image.new('RGB', (N, N), (14, 14, 18))
d = ImageDraw.Draw(img)
def rect(x, y, w, h, c): d.rectangle([x, y, x + w - 1, y + h - 1], fill=c)
def text(s, x, y, scale, c, shadow=None):
    for i, ch in enumerate(s):
        for r, bits in enumerate(FONT[ch]):
            for col in range(5):
                if (bits >> (4 - col)) & 1:
                    px, py = x + (i * 6 + col) * scale, y + r * scale
                    if shadow: rect(px + scale // 2, py + scale // 2, scale, scale, shadow)
                    rect(px, py, scale, scale, c)
def centered(s, y, scale, c, shadow=None):
    text(s, (N - (len(s) * 6 - 1) * scale) // 2, y, scale, c, shadow)

ORANGE, YELLOW, RED = (255, 140, 0), (255, 240, 60), (255, 50, 0)

BLOOD, DARK_BLOOD, SHINE = (200, 12, 10), (110, 0, 0), (255, 90, 80)

def blood_text(s, y, scale):
    """Like draw_blood_text in hud.c: black outline, red fill darkening downward, drips from the lowest blocks"""
    x0 = (N - (len(s) * 6 - 1) * scale) // 2
    blocks = []
    for i, ch in enumerate(s):
        rows = FONT[ch]
        for r, bits in enumerate(rows):
            for col in range(5):
                if not (bits >> (4 - col)) & 1: continue
                lowest = not any((rows[k] >> (4 - col)) & 1 for k in range(r + 1, 7))
                above = r > 0 and (rows[r - 1] >> (4 - col)) & 1
                blocks.append((x0 + (i * 6 + col) * scale, y + r * scale, r, lowest, above))
    drips = [(x, yy + scale, random.randint(3, 9)) for x, yy, r, lowest, _ in blocks if lowest and random.random() < 0.5]
    for x, yy, *_ in blocks: rect(x - 1, yy - 1, scale + 2, scale + 2, (8, 0, 0))
    for x, yy, n in drips: rect(x, yy, 3, n + 1, (8, 0, 0)); rect(x + 1, yy + n + 1, 2, 2, (8, 0, 0))
    for x, yy, n in drips: rect(x + 1, yy, 1, n, DARK_BLOOD); rect(x + 1, yy + n, 1, 1, BLOOD)
    for x, yy, r, _, above in blocks:
        rect(x, yy, scale, scale, (205 - r * 14, 10, 10))
        if not above: rect(x, yy, scale, 1, SHINE)

random.seed(3)
# Frame and title
rect(0, 0, N, 2, DARK_BLOOD); rect(0, N - 2, N, 2, DARK_BLOOD); rect(0, 0, 2, N, DARK_BLOOD); rect(N - 2, 0, 2, N, DARK_BLOOD)
centered("PONG", 52, 4, (235, 235, 240), shadow=(70, 70, 80))
blood_text("FATAL", 8, 4)

# Court
for y in range(86, 126, 8): rect(63, y, 2, 4, (50, 50, 60))
rect(10, 94, 4, 24, (240, 240, 240))       # P1
rect(114, 88, 4, 24, (70, 110, 255))       # P2, stunned blue

# Fireball trail, then the fireball itself (hot core toward the leading edge)
random.seed(7)
for _ in range(40):
    x = random.randint(16, 46); y = 104 + random.randint(-6, 6) * (x - 12) // 30
    s = random.choice([2, 2, 3])
    rect(x, y, s, s, (255, random.randint(60, 180), 0))
rect(46, 99, 18, 12, RED)
rect(55, 102, 8, 6, YELLOW)
rect(86, 116, 5, 5, (255, 255, 255))        # Ball

img.resize((512, 512), Image.NEAREST).save('assets/icon0.png')
