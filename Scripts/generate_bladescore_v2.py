"""Blade Score v2 - one lane, one judge ring, flat readable notes.

Every note is a coloured disc with ONE white glyph:
  RED disc  = boss attack (glyph = what to do: dodge / block / counter ...)
  GOLD disc = your attack beat (glyph = sword)
Glyphs for dodge / block / counter come from the project's flat "Minimal"
response icons (SourceArt/RhythmResponses/Rhythm_*_Minimal.png), reduced to a
single white silhouette so they stay readable at 30 px.

Writes SourceArt/UI/BladeScore/T_BS2_*.png (auto-imported on editor start).
Run:  python Scripts/generate_bladescore_v2.py
"""
import os
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
SA = os.path.join(ROOT, 'SourceArt')
OUT = os.path.join(SA, 'UI', 'BladeScore')
SS = 4


def save(im, name, size):
    im.resize(size, Image.LANCZOS).save(os.path.join(OUT, name + '.png')); print('wrote', name, size)


def glyph_from_minimal(name):
    """White silhouette of the bright strokes (drops the baked black shadow)."""
    im = Image.open(os.path.join(SA, 'RhythmResponses', 'Rhythm_%s_Minimal.png' % name)).convert('RGBA')
    a = np.array(im).astype(np.float32)
    bright = a[..., :3].max(axis=2) > 110
    alpha = np.where(bright, a[..., 3], 0).astype(np.uint8)
    out = Image.new('RGBA', im.size, (255, 255, 255, 0)); out.putalpha(Image.fromarray(alpha))
    out = out.crop(out.getbbox())
    # thicken slightly so strokes survive downscaling
    out.putalpha(out.getchannel('A').filter(ImageFilter.MaxFilter(9)))
    return square(out)


def square(im, pad=.08):
    s = max(im.size); c = int(s * (1 + 2 * pad))
    out = Image.new('RGBA', (c, c), (255, 255, 255, 0))
    out.alpha_composite(im, ((c - im.width) // 2, (c - im.height) // 2))
    return out


def canvas(w, h):
    im = Image.new('RGBA', (w * SS, h * SS), (0, 0, 0, 0)); return im, ImageDraw.Draw(im)


def disc(name, fill, rim=(255, 255, 255, 235)):
    im, d = canvas(256, 256); s = SS
    d.ellipse((10 * s, 10 * s, 246 * s, 246 * s), fill=(0, 0, 0, 200))            # dark contour
    d.ellipse((18 * s, 18 * s, 238 * s, 238 * s), fill=fill)                        # colour body
    d.ellipse((30 * s, 30 * s, 226 * s, 226 * s), outline=rim, width=7 * s)         # bright inner ring
    save(im, name, (256, 256))


def judge():
    im, d = canvas(256, 256); s = SS
    d.ellipse((8 * s, 8 * s, 248 * s, 248 * s), outline=(0, 0, 0, 200), width=26 * s)
    d.ellipse((14 * s, 14 * s, 242 * s, 242 * s), outline=(255, 255, 255, 255), width=14 * s)
    save(im, 'T_BS2_Judge', (256, 256))


def sword():
    im, d = canvas(256, 256); s = SS
    # simple upright sword: blade, guard, grip, pommel
    d.polygon([(128 * s, 18 * s), (146 * s, 48 * s), (146 * s, 168 * s), (110 * s, 168 * s), (110 * s, 48 * s)], fill='white')
    d.rounded_rectangle((70 * s, 166 * s, 186 * s, 186 * s), radius=8 * s, fill='white')
    d.rectangle((118 * s, 186 * s, 138 * s, 222 * s), fill='white')
    d.ellipse((111 * s, 218 * s, 145 * s, 246 * s), fill='white')
    save(im, 'T_BS2_Glyph_Sword', (256, 256))


def area():
    im, d = canvas(256, 256); s = SS
    d.ellipse((64 * s, 64 * s, 192 * s, 192 * s), outline='white', width=18 * s)
    for ang in range(4):
        import math
        a = math.radians(45 + 90 * ang); cx, cy = 128 + math.cos(a) * 92, 128 + math.sin(a) * 92
        tip = (cx + math.cos(a) * 26, cy + math.sin(a) * 26)
        n = (-math.sin(a), math.cos(a))
        d.polygon([(tip[0] * s, tip[1] * s), ((cx + n[0] * 20) * s, (cy + n[1] * 20) * s), ((cx - n[0] * 20) * s, (cy - n[1] * 20) * s)], fill='white')
    save(im, 'T_BS2_Glyph_Area', (256, 256))


def lane():
    im, d = canvas(1024, 64); s = SS
    d.rounded_rectangle((0, 12 * s, 1024 * s, 52 * s), radius=20 * s, fill=(6, 8, 14, 190))
    d.line((24 * s, 14 * s, 1000 * s, 14 * s), fill=(255, 232, 170, 150), width=2 * s)
    d.line((24 * s, 50 * s, 1000 * s, 50 * s), fill=(255, 232, 170, 150), width=2 * s)
    save(im, 'T_BS2_Lane', (1024, 64))


def holdbar():
    im, d = canvas(256, 64); s = SS
    d.rounded_rectangle((0, 8 * s, 256 * s, 56 * s), radius=24 * s, fill=(255, 255, 255, 255))
    save(im, 'T_BS2_HoldBar', (256, 64))


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    disc('T_BS2_BossNote', (206, 28, 56, 255))
    disc('T_BS2_AttackNote', (236, 160, 24, 255))
    judge(); sword(); area(); lane(); holdbar()
    for key, src in (('Dodge', 'Dodge'), ('Block', 'Block'), ('Counter', 'Counter')):
        glyph_from_minimal(src).resize((256, 256), Image.LANCZOS).save(os.path.join(OUT, 'T_BS2_Glyph_%s.png' % key))
        print('wrote T_BS2_Glyph_%s' % key)
