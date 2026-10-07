# Launcher icon (assets/icon0.png, 512x512): a shaded tennis ball over a blood splatter, blood dripping off it,
# "FATAL PONG" in Anton underneath. Drawn at 4x and scaled down for smooth edges. Run from the project root;
# needs Pillow (no numpy: shading is built from blurred masks and blend modes).
import math
import random

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

S = 2048                                    # Working size, scaled to OUT at the end
OUT = 512
FONT = "tools/fonts/Anton-Regular.ttf"      # SIL Open Font License, see tools/fonts/OFL.txt
BALL_C, BALL_R = (1024, 800), 520

def radial(size, center, radius, inner, outer):
    """RGB image fading from `inner` at `center` to `outer` at `radius` and beyond"""
    img = Image.new('RGB', size, outer)
    d = ImageDraw.Draw(img)
    steps = 120
    for i in range(steps):
        k = i / steps
        r = radius * (1 - k)
        c = tuple(int(o + (n - o) * k) for o, n in zip(outer, inner))
        d.ellipse([center[0] - r, center[1] - r, center[0] + r, center[1] + r], fill=c)
    return img.filter(ImageFilter.GaussianBlur(radius / 40))

def metaball(blobs, blur, size=(S, S)):
    """Mask of circles (x, y, r) melted together: blurred, then thresholded, so they merge like liquid"""
    m = Image.new('L', size, 0)
    d = ImageDraw.Draw(m)
    for x, y, r in blobs:
        d.ellipse([x - r, y - r, x + r, y + r], fill=255)
    return m.filter(ImageFilter.GaussianBlur(blur)).point(lambda v: 255 if v > 110 else 0)

def splatter(cx, cy, radius, rays, drops, seed):
    """Blobs for a splash: a core, lobes round its edge, tapering rays and loose droplets"""
    rnd = random.Random(seed)
    blobs = [(cx, cy, radius * 0.75)]
    for _ in range(14):
        a, d = rnd.uniform(0, 2 * math.pi), radius * rnd.uniform(0.5, 0.95)
        blobs.append((cx + math.cos(a) * d, cy + math.sin(a) * d, radius * rnd.uniform(0.2, 0.4)))
    for _ in range(rays):                       # Streaks thrown outward, thinning, ending in a drop
        a, length = rnd.uniform(0, 2 * math.pi), radius * rnd.uniform(1.1, 1.9)
        w = radius * rnd.uniform(0.07, 0.13)
        for i in range(24):
            t = i / 23
            d = radius * 0.6 + (length - radius * 0.6) * t
            blobs.append((cx + math.cos(a) * d, cy + math.sin(a) * d, w * (1 - 0.65 * t)))
        blobs.append((cx + math.cos(a) * (length + w * 2), cy + math.sin(a) * (length + w * 2), w * 0.8))
    for _ in range(drops):
        a, d = rnd.uniform(0, 2 * math.pi), radius * rnd.uniform(1.2, 2.3)
        blobs.append((cx + math.cos(a) * d, cy + math.sin(a) * d, radius * rnd.uniform(0.02, 0.06)))
    return blobs

def blood(mask, base, light, gloss=True):
    """Fill `mask` with wet blood: darker toward the edges, a glossy rim lit from the upper left"""
    inner = mask.filter(ImageFilter.GaussianBlur(40))
    img = Image.composite(Image.new('RGB', mask.size, light), Image.new('RGB', mask.size, base), inner)
    if gloss:                                   # Shine: the mask minus itself shifted down-right, softened
        shifted = ImageChops.offset(mask, 10, 12)
        rim = ImageChops.subtract(mask, shifted).filter(ImageFilter.GaussianBlur(4))
        img = Image.composite(Image.new('RGB', mask.size, (255, 140, 130)), img, rim.point(lambda v: v * 0.7))
    return img

def seams(width, offset=(0, 0)):
    """The two seams as an L mask: arcs of circles centred outside the ball on either side, so they curve in
    like a pair of opposing C's, the whole pair turned a little"""
    m = Image.new('L', (S, S), 0)
    d = ImageDraw.Draw(m)
    cx, cy = BALL_C[0] + offset[0], BALL_C[1] + offset[1]
    for side in (-1, 1):
        ox, r = cx + side * BALL_R * 1.28, BALL_R * 0.98
        d.ellipse([ox - r, cy - r, ox + r, cy + r], outline=255, width=width)
    return m.rotate(-28, resample=Image.BICUBIC, center=(cx, cy))

def ball():
    """The tennis ball (RGB) and its mask: felt with fuzz, two seams, lit as a sphere from the upper left"""
    mask = Image.new('L', (S, S), 0)
    cx, cy = BALL_C
    ImageDraw.Draw(mask).ellipse([cx - BALL_R, cy - BALL_R, cx + BALL_R, cy + BALL_R], fill=255)
    felt = Image.new('RGB', (S, S), (214, 242, 58))           # Optic yellow
    fuzz = Image.effect_noise((S, S), 40).filter(ImageFilter.GaussianBlur(1.2))
    felt = Image.composite(Image.new('RGB', (S, S), (238, 255, 120)), felt, fuzz.point(lambda v: max(0, v - 128)))
    seam_shadow = seams(56, (6, 9)).filter(ImageFilter.GaussianBlur(9))
    felt = Image.composite(Image.new('RGB', (S, S), (120, 150, 20)), felt, seam_shadow)
    felt = Image.composite(Image.new('RGB', (S, S), (248, 248, 240)), felt, seams(38))
    # Sphere lighting: dark toward the lower right, a soft hotspot upper left
    shade = radial((S, S), (cx - BALL_R * 0.35, cy - BALL_R * 0.4), BALL_R * 1.6, (255, 255, 255), (96, 96, 80))
    lit = ImageChops.multiply(felt, shade)
    spot = radial((S, S), (cx - BALL_R * 0.4, cy - BALL_R * 0.45), BALL_R * 0.45, (90, 90, 70), (0, 0, 0))
    lit = ImageChops.screen(lit, spot)
    return lit, mask

def drips(seed):
    """Blobs for blood pooling along the bottom of the ball and running off it: streaks of varied width and
    length, each thinning as it runs and ending in a heavier drop"""
    rnd = random.Random(seed)
    cx, cy = BALL_C
    blobs = []
    for i in range(40):                         # The pool hugging the lower rim
        a = math.radians(48 + 84 * i / 39)
        blobs.append((cx + math.cos(a) * BALL_R * 0.94, cy + math.sin(a) * BALL_R * 0.94, rnd.uniform(26, 40)))
    for a_deg, length, w in ((58, 120, 20), (71, 250, 30), (84, 390, 36), (97, 210, 26), (110, 300, 32),
                             (122, 110, 18)):
        a = math.radians(a_deg + rnd.uniform(-2, 2))
        x0, y0 = cx + math.cos(a) * BALL_R * 0.94, cy + math.sin(a) * BALL_R * 0.94
        for i in range(36):
            t = i / 35
            blobs.append((x0 + rnd.uniform(-1.5, 1.5), y0 + length * t, w * (1 - 0.45 * t)))
        blobs.append((x0, y0 + length + w * 0.5, w * 0.95))
    return blobs

def title(img):
    """FATAL in blood red over PONG in white, Anton, dark outline and drop shadow, a few drips off FATAL"""
    font = ImageFont.truetype(FONT, 330)
    words = (("FATAL", (196, 14, 20)), ("PONG", (242, 240, 234)))
    gap = 60
    widths = [font.getbbox(w)[2] - font.getbbox(w)[0] for w, _ in words]
    x = (S - sum(widths) - gap) // 2
    y = 1500
    shadow = Image.new('L', (S, S), 0)
    sd = ImageDraw.Draw(shadow)
    pos = []
    for (word, _), w in zip(words, widths):
        pos.append((x - font.getbbox(word)[0], y))
        sd.text((pos[-1][0] + 14, y + 18), word, font=font, fill=200, stroke_width=16, stroke_fill=200)
        x += w + gap
    img.paste((0, 0, 0), (0, 0), shadow.filter(ImageFilter.GaussianBlur(18)))
    d = ImageDraw.Draw(img)
    for (word, color), p in zip(words, pos):
        d.text(p, word, font=font, fill=color, stroke_width=14, stroke_fill=(16, 2, 4))
    # Drips from FATAL's baseline
    fx = pos[0][0]
    base = y + font.getbbox("FATAL")[3]
    blobs = []
    for off, length in ((70, 60), (300, 95), (430, 45)):
        for i in range(16):
            t = i / 15
            blobs.append((fx + off, base - 10 + length * t, 13 * (1 - 0.3 * t)))
        blobs.append((fx + off, base - 10 + length + 8, 17))
    m = metaball(blobs, 6)
    img.paste(blood(m, (150, 8, 14), (196, 14, 20), gloss=False), (0, 0), m)

def main():
    img = radial((S, S), (S // 2, S * 0.42), S * 0.8, (58, 10, 14), (8, 4, 6))
    # Blood splashed on the ground behind the ball
    back = metaball(splatter(BALL_C[0] + 40, BALL_C[1] + 40, 540, 7, 26, seed=4), 14)
    img.paste(blood(back, (88, 0, 6), (128, 6, 12)), (0, 0), back)
    lit, mask = ball()
    img.paste(lit, (0, 0), mask)
    # Blood on the ball: a splash over its upper right, and drips running off its bottom
    on = metaball(splatter(BALL_C[0] + 230, BALL_C[1] - 170, 150, 6, 10, seed=11), 8)
    on_ball = ImageChops.multiply(on, mask)
    runs = metaball(drips(seed=5), 7)
    wet = ImageChops.lighter(on_ball, runs)
    img.paste(blood(wet, (150, 8, 14), (200, 18, 24)), (0, 0), wet)
    title(img)
    img.resize((OUT, OUT), Image.LANCZOS).save('assets/icon0.png')
    print("wrote assets/icon0.png")

if __name__ == "__main__":
    main()
