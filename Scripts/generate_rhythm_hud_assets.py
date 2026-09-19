from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "SourceArt" / "UI" / "RhythmCombat"
OUT.mkdir(parents=True, exist_ok=True)


def save_health_frame():
    w, h = 1024, 256
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    glow = Image.new("RGBA", im.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.rounded_rectangle((72, 66, 982, 188), 42, outline=(255, 81, 26, 170), width=12)
    gd.ellipse((8, 8, 242, 242), outline=(255, 189, 46, 210), width=18)
    glow = glow.filter(ImageFilter.GaussianBlur(14))
    im.alpha_composite(glow)
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((72, 66, 982, 188), 42, fill=(9, 12, 23, 225), outline=(255, 205, 72, 255), width=5)
    d.rounded_rectangle((104, 88, 956, 136), 18, fill=(18, 22, 36, 255), outline=(101, 42, 28, 255), width=3)
    d.rounded_rectangle((104, 148, 724, 171), 9, fill=(14, 25, 39, 255), outline=(32, 125, 154, 220), width=2)
    d.ellipse((8, 8, 242, 242), fill=(8, 11, 22, 246), outline=(255, 203, 68, 255), width=8)
    d.ellipse((28, 28, 222, 222), outline=(255, 78, 26, 230), width=5)
    # Rhythmic teeth. Kept separate from the bars so they can remain readable at low health.
    for i in range(12):
        x = 106 + i * 54
        top = 185 + (i % 2) * 9
        d.polygon([(x, 185), (x + 22, top + 31), (x + 44, 185)], fill=(255, 188, 51, 225))
    im.save(OUT / "T_HUD_PlayerFrame.png")


def save_judgment_burst():
    size = 512
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    glow = Image.new("RGBA", im.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    c = size // 2
    for r, a, width in [(180, 120, 18), (128, 180, 12), (78, 230, 8)]:
        gd.ellipse((c-r, c-r, c+r, c+r), outline=(255, 170, 38, a), width=width)
    glow = glow.filter(ImageFilter.GaussianBlur(15))
    im.alpha_composite(glow)
    d = ImageDraw.Draw(im)
    for r, a, width in [(180, 190, 5), (128, 230, 4), (78, 255, 4)]:
        d.ellipse((c-r, c-r, c+r, c+r), outline=(255, 216, 102, a), width=width)
    for i in range(16):
        import math
        a = math.tau * i / 16
        r0 = 193 if i % 2 else 168
        r1 = 244 if i % 2 else 226
        x0, y0 = c + math.cos(a) * r0, c + math.sin(a) * r0
        x1, y1 = c + math.cos(a) * r1, c + math.sin(a) * r1
        d.line((x0, y0, x1, y1), fill=(255, 102, 30, 210), width=6)
    im.save(OUT / "T_HUD_JudgmentBurst.png")


def save_combo_slash():
    w, h = 512, 256
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    glow = Image.new("RGBA", im.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.polygon([(30, 202), (94, 45), (482, 80), (404, 224)], fill=(255, 73, 25, 130))
    glow = glow.filter(ImageFilter.GaussianBlur(18))
    im.alpha_composite(glow)
    d = ImageDraw.Draw(im)
    d.polygon([(24, 202), (91, 52), (486, 80), (400, 226)], fill=(10, 13, 26, 225), outline=(255, 192, 54, 255))
    d.line((54, 191, 463, 93), fill=(255, 78, 25, 245), width=8)
    d.line((82, 213, 431, 130), fill=(54, 196, 225, 210), width=4)
    im.save(OUT / "T_HUD_ComboSlash.png")


if __name__ == "__main__":
    save_health_frame()
    save_judgment_burst()
    save_combo_slash()
    for item in sorted(OUT.glob("*.png")):
        print(item)
