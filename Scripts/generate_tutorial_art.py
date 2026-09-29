"""Tutorial input glyphs + card panel (transparent PNGs, drawn at 4x, downsampled).

Keyboard/mouse: T_TUT_K_<Name>   Gamepad (Xbox layout): T_TUT_P_<Name>
Card panel: T_TUT_Card.  Output: SourceArt/UI/Tutorial -> /Game/UI/Tutorial
(auto-imported on editor start by Content/Python/init_unreal.py).
Run:  python Scripts/generate_tutorial_art.py
"""
import os
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
OUT = os.path.join(ROOT, 'SourceArt', 'UI', 'Tutorial')
FONT = os.path.join(ROOT, 'SourceArt', 'UI', 'RhythmCombat', 'Fonts', 'Bangers-Regular.ttf')
SS = 4
INK = (8, 9, 14, 255); IVORY = (246, 238, 220, 255); GOLD = (255, 190, 64, 255); DARK = (28, 30, 40, 255)


def canvas(w, h):
    im = Image.new('RGBA', (w * SS, h * SS), (0, 0, 0, 0)); return im, ImageDraw.Draw(im)


def save(im, name, w, h):
    im.resize((w, h), Image.LANCZOS).save(os.path.join(OUT, name + '.png')); print('wrote', name)


def font(px):
    return ImageFont.truetype(FONT, px * SS)


def keycap(name, label, w=128, size=54):
    """Chunky keycap: ink contour, ivory face, darker base lip."""
    h = 128
    im, d = canvas(w, h); s = SS
    d.rounded_rectangle((6 * s, 10 * s, (w - 6) * s, 122 * s), radius=20 * s, fill=INK)
    d.rounded_rectangle((12 * s, 16 * s, (w - 12) * s, 116 * s), radius=16 * s, fill=(170, 160, 142, 255))
    d.rounded_rectangle((12 * s, 14 * s, (w - 12) * s, 102 * s), radius=16 * s, fill=IVORY)
    f = font(size)
    d.text((w / 2 * s, 58 * s), label, font=f, fill=INK, anchor='mm')
    save(im, 'T_TUT_K_' + name, w, h)


def mouse(name, button):
    """Mouse outline, the pressed button filled gold."""
    im, d = canvas(128, 128); s = SS
    body = (30 * s, 8 * s, 98 * s, 120 * s)
    d.rounded_rectangle(body, radius=34 * s, fill=INK)
    d.rounded_rectangle((36 * s, 14 * s, 92 * s, 114 * s), radius=28 * s, fill=IVORY)
    if button == 'L':
        d.pieslice((36 * s, 14 * s, 92 * s, 70 * s), 180, 270, fill=GOLD); d.rectangle((36 * s, 42 * s, 64 * s, 58 * s), fill=GOLD)
    if button == 'R':
        d.pieslice((36 * s, 14 * s, 92 * s, 70 * s), 270, 360, fill=GOLD); d.rectangle((64 * s, 42 * s, 92 * s, 58 * s), fill=GOLD)
    d.line((36 * s, 58 * s, 92 * s, 58 * s), fill=INK, width=4 * s)
    d.line((64 * s, 14 * s, 64 * s, 58 * s), fill=INK, width=4 * s)
    d.rounded_rectangle((58 * s, 24 * s, 70 * s, 46 * s), radius=6 * s, fill=GOLD if button == 'M' else INK)
    if button == 'Move':  # mouse movement arrows
        for dx, dy in ((0, -1), (0, 1), (-1, 0), (1, 0)):
            pass
    save(im, 'T_TUT_K_' + name, 128, 128)


def face(name, letter, color):
    im, d = canvas(128, 128); s = SS
    d.ellipse((6 * s, 6 * s, 122 * s, 122 * s), fill=INK)
    d.ellipse((14 * s, 14 * s, 114 * s, 114 * s), fill=color)
    d.ellipse((20 * s, 18 * s, 108 * s, 70 * s), fill=tuple(min(255, c + 40) for c in color[:3]) + (120,))
    d.text((64 * s, 66 * s), letter, font=font(64), fill=(255, 255, 255, 255), anchor='mm', stroke_width=3 * s, stroke_fill=INK)
    save(im, 'T_TUT_P_' + name, 128, 128)


def shoulder(name, label, trigger=False):
    w, h = 176, 128
    im, d = canvas(w, h); s = SS
    if trigger:
        d.rounded_rectangle((20 * s, 8 * s, 156 * s, 120 * s), radius=40 * s, fill=INK)
        d.rounded_rectangle((28 * s, 16 * s, 148 * s, 112 * s), radius=34 * s, fill=(60, 64, 78, 255))
    else:
        d.rounded_rectangle((6 * s, 26 * s, 170 * s, 102 * s), radius=30 * s, fill=INK)
        d.rounded_rectangle((14 * s, 34 * s, 162 * s, 94 * s), radius=24 * s, fill=(60, 64, 78, 255))
    d.text((w / 2 * s, 64 * s), label, font=font(56), fill=IVORY, anchor='mm')
    save(im, 'T_TUT_P_' + name, w, h)


def stick(name, label, press=False):
    im, d = canvas(128, 128); s = SS
    d.ellipse((6 * s, 6 * s, 122 * s, 122 * s), fill=INK)
    d.ellipse((14 * s, 14 * s, 114 * s, 114 * s), fill=(60, 64, 78, 255))
    d.ellipse((30 * s, 30 * s, 98 * s, 98 * s), fill=(90, 96, 112, 255), outline=GOLD if press else INK, width=5 * s)
    d.text((64 * s, 66 * s), label, font=font(46), fill=IVORY, anchor='mm')
    save(im, 'T_TUT_P_' + name, 128, 128)


def view_button():
    im, d = canvas(128, 128); s = SS
    d.ellipse((6 * s, 6 * s, 122 * s, 122 * s), fill=INK)
    d.ellipse((14 * s, 14 * s, 114 * s, 114 * s), fill=(60, 64, 78, 255))
    d.rounded_rectangle((36 * s, 40 * s, 84 * s, 76 * s), radius=6 * s, outline=IVORY, width=6 * s)
    d.rounded_rectangle((48 * s, 54 * s, 96 * s, 90 * s), radius=6 * s, fill=(60, 64, 78, 255), outline=IVORY, width=6 * s)
    save(im, 'T_TUT_P_View', 128, 128)


def card():
    w, h = 1024, 256
    im, d = canvas(w, h); s = SS
    d.rounded_rectangle((8 * s, 8 * s, (w - 8) * s, (h - 8) * s), radius=26 * s, fill=(0, 0, 0, 150))
    d.rounded_rectangle((14 * s, 14 * s, (w - 14) * s, (h - 14) * s), radius=22 * s, fill=(12, 13, 20, 235), outline=GOLD, width=4 * s)
    d.rounded_rectangle((24 * s, 24 * s, (w - 24) * s, (h - 24) * s), radius=16 * s, outline=(255, 190, 64, 90), width=2 * s)
    save(im, 'T_TUT_Card', w, h)


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    for n, l, w, sz in (('Shift', 'SHIFT', 176, 50), ('Space', 'SPACE', 176, 50), ('Tab', 'TAB', 150, 54), ('WASD', 'WASD', 176, 50),
                        ('Q', 'Q', 128, 64), ('F', 'F', 128, 64), ('R', 'R', 128, 64), ('Enter', 'ENTER', 176, 50)):
        keycap(n, l, w, sz)
    mouse('LMB', 'L'); mouse('RMB', 'R'); mouse('MMB', 'M'); mouse('Mouse', 'None')
    face('A', 'A', (60, 170, 60, 255)); face('B', 'B', (210, 50, 50, 255))
    face('X', 'X', (40, 110, 220, 255)); face('Y', 'Y', (230, 180, 30, 255))
    shoulder('LB', 'LB'); shoulder('RB', 'RB'); shoulder('LT', 'LT', True); shoulder('RT', 'RT', True)
    stick('LS', 'L'); stick('RS', 'R'); stick('R3', 'R3', True)
    view_button(); card()
