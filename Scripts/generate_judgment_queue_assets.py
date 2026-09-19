from pathlib import Path
import math
import random
from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "SourceArt" / "UI" / "RhythmCombat"
OUT.mkdir(parents=True, exist_ok=True)


def glow_line(base, points, color, width, glow=18):
    layer = Image.new("RGBA", base.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)
    draw.line(points, fill=color, width=width, joint="curve")
    blurred = layer.filter(ImageFilter.GaussianBlur(glow))
    base.alpha_composite(blurred)
    base.alpha_composite(layer)


# Rail backbone: only the conveyor spine, nodes and end ornaments. Cards are independent assets.
rail = Image.new("RGBA", (778, 2022), (0, 0, 0, 0))
rd = ImageDraw.Draw(rail)
spine_x = 650
# Restricted palette: deep navy shadow, cyan body, white electric core.
glow_line(rail, [(spine_x+15, 95), (spine_x+15, 1880)], (8, 42, 68, 175), 48, 34)
glow_line(rail, [(spine_x, 95), (spine_x, 1880)], (0, 194, 255, 220), 30, 22)
glow_line(rail, [(spine_x, 95), (spine_x, 1880)], (235, 255, 255, 255), 9, 6)
# Broken gold timing ticks keep the rhythm rail from reading as a plain progress bar.
for y in range(150, 1810, 92):
    rd.polygon([(spine_x-9, y), (spine_x+20, y+11), (spine_x+20, y+20), (spine_x-9, y+9)],
               fill=(255, 184, 38, 155))

slot_y = [275, 560, 845, 1130, 1415]
for index, y in enumerate(slot_y):
    # Dark comic plate behind the moving card. All slots share the same cyan language.
    plate = [(365, y-83), (535, y-83), (590, y-52), (590, y+52), (535, y+83), (392, y+83), (350, y+52)]
    rd.polygon(plate, fill=(7, 10, 24, 92))
    notch = (0, 205, 255, 155 if index % 2 == 0 else 105)
    rd.line(plate + [plate[0]], fill=notch, width=5, joint="curve")
    # Halftone splash behind each stop keeps the existing comic language.
    for row in range(-3, 4):
        for col in range(-5, 5):
            if (row + col + index) % 2:
                continue
            radius = max(3, 8 - abs(row))
            x = 500 + col * 18 + (row & 1) * 7
            yy = y + row * 17
            dot = (0, 185, 235, 82 if index % 2 == 0 else 52)
            rd.ellipse((x-radius, yy-radius, x+radius, yy+radius), fill=dot)
    glow_line(rail, [(spine_x-30, y), (spine_x+30, y)], (0, 210, 255, 230), 20, 16)
    rd.regular_polygon((spine_x, y, 42), 6, rotation=30, fill=(9, 17, 39, 255),
                       outline=(232, 250, 255, 255))
    rd.ellipse((spine_x-18, y-18, spine_x+18, y+18), fill=(0, 205, 255, 255),
               outline=(255, 255, 255, 255), width=5)

# Top lightning marker.
rd.polygon([(618, 18), (695, 94), (661, 99), (714, 184), (602, 93), (641, 90)],
           fill=(248, 250, 255, 255), outline=(0, 154, 205, 255))
rd.polygon([(630, 38), (680, 92), (657, 93), (690, 142), (622, 88), (646, 84)], fill=(0, 213, 255, 255))

# Bottom sunburst behind the combo node.
cx, cy = spine_x, 1900
for i in range(24):
    a = math.tau * i / 24.0
    inner = 62 if i % 2 else 70
    outer = 160 if i % 2 else 205
    da = 0.035 if i % 2 else 0.055
    pts = [
        (cx + math.cos(a-da)*inner, cy + math.sin(a-da)*inner),
        (cx + math.cos(a)*outer, cy + math.sin(a)*outer),
        (cx + math.cos(a+da)*inner, cy + math.sin(a+da)*inner),
    ]
    rd.polygon(pts, fill=(255, 174, 25, 205))
glow_line(rail, [(cx-50, cy), (cx+50, cy)], (255, 190, 35, 240), 26, 25)
rd.ellipse((cx-66, cy-66, cx+66, cy+66), fill=(255, 255, 255, 255),
           outline=(255, 174, 25, 255), width=16)
rail.save(OUT / "T_HUD_JudgmentRail_Backbone.png")


# Neutral card art. Runtime tint supplies PERFECT/GOOD/BAD/MISS identity.
card = Image.new("RGBA", (680, 250), (0, 0, 0, 0))
cd = ImageDraw.Draw(card)
outer = [(26, 80), (98, 22), (620, 30), (658, 172), (590, 226), (38, 214)]
inner = [(58, 91), (113, 52), (594, 58), (621, 161), (571, 194), (67, 185)]
glow = Image.new("RGBA", card.size, (0, 0, 0, 0))
gd = ImageDraw.Draw(glow)
gd.polygon(outer, fill=(255, 255, 255, 150))
card.alpha_composite(glow.filter(ImageFilter.GaussianBlur(18)))
cd.polygon(outer, fill=(245, 248, 255, 255))
cd.polygon(inner, fill=(12, 13, 18, 250))
cd.line(inner + [inner[0]], fill=(145, 155, 170, 255), width=6, joint="curve")
# Graphite chevrons and halftone remain subtle after runtime grade tinting.
for x in range(118, 570, 34):
    cd.line([(x, 61), (x+54, 185)], fill=(65, 71, 86, 120), width=5)
for row in range(3):
    for col in range(13):
        radius = 3 + (col % 3)
        x = 132 + col * 35 + row * 8
        y = 174 + row * 12
        cd.ellipse((x-radius, y-radius, x+radius, y+radius), fill=(185, 194, 210, 100))
cd.line([(88, 105), (575, 112)], fill=(255, 255, 255, 120), width=4)
# Graphic lightning cuts.
cd.polygon([(30, 75), (98, 5), (88, 55), (170, 13), (122, 72)], fill=(255, 255, 255, 255))
cd.polygon([(585, 218), (655, 180), (632, 225), (675, 237)], fill=(255, 255, 255, 230))
card.save(OUT / "T_HUD_JudgmentCard.png")


combo = Image.new("RGBA", (700, 300), (0, 0, 0, 0))
co = ImageDraw.Draw(combo)
outer = [(30, 85), (135, 22), (630, 35), (680, 135), (592, 262), (78, 238)]
inner = [(70, 99), (151, 54), (605, 65), (640, 137), (568, 226), (96, 209)]
glow = Image.new("RGBA", combo.size, (0, 0, 0, 0))
g = ImageDraw.Draw(glow)
g.polygon(outer, fill=(255, 176, 28, 190))
combo.alpha_composite(glow.filter(ImageFilter.GaussianBlur(24)))
co.polygon(outer, fill=(255, 176, 28, 255))
co.polygon(inner, fill=(9, 11, 18, 252))
co.line(inner + [inner[0]], fill=(255, 244, 220, 255), width=9, joint="curve")
co.line([(140, 220), (575, 76)], fill=(0, 210, 255, 230), width=7)
co.line([(165, 230), (603, 84)], fill=(215, 248, 255, 135), width=4)
for x in range(125, 575, 42):
    co.polygon([(x, 202), (x+20, 195), (x+5, 214)], fill=(255, 173, 30, 120))
co.polygon([(25, 110), (72, 78), (54, 133), (103, 111), (63, 166)], fill=(255, 214, 82, 245))
co.polygon([(595, 228), (680, 135), (649, 226), (610, 270)], fill=(0, 153, 205, 170))
combo.save(OUT / "T_HUD_ComboPanel.png")

# Broken, transparent-center ring used by the PERFECT world plane material.
ring = Image.new("RGBA", (512, 512), (0, 0, 0, 0))
ri = ImageDraw.Draw(ring)
center = (256, 256)
arcs = [(6, 61), (72, 128), (141, 205), (222, 281), (296, 352)]
for index, (start, end) in enumerate(arcs):
    box = (58 + index % 2 * 10, 58 + index % 2 * 10,
           454 - index % 2 * 10, 454 - index % 2 * 10)
    ri.arc(box, start=start, end=end, fill=(255, 224, 95, 255), width=18 if index % 2 == 0 else 11)
for angle in range(0, 360, 30):
    if angle in (60, 150, 240, 330):
        continue
    a = math.radians(angle)
    r0, r1 = 198, 245 + (angle % 60) // 3
    p0 = (center[0] + math.cos(a) * r0, center[1] + math.sin(a) * r0)
    p1 = (center[0] + math.cos(a - .035) * r1, center[1] + math.sin(a - .035) * r1)
    p2 = (center[0] + math.cos(a + .035) * r1, center[1] + math.sin(a + .035) * r1)
    ri.polygon([p0, p1, p2], fill=(255, 182, 30, 235))
ring.save(OUT / "T_PerfectDodge_RingMask.png")

# Fullscreen comic speed lines. The middle stays clear so the dodge remains readable.
streak = Image.new("RGBA", (1024, 512), (0, 0, 0, 0))
sd = ImageDraw.Draw(streak)
rng = random.Random(4217)
vanish = (512, 280)
for side in (-1, 1):
    edge_x = 0 if side < 0 else 1023
    for i in range(34):
        edge_y = rng.randint(12, 500)
        inner_x = rng.randint(255, 430) if side < 0 else rng.randint(594, 769)
        inner_y = int(vanish[1] + (edge_y - vanish[1]) * rng.uniform(.18, .55))
        width = rng.randint(2, 7)
        color = (20, 226, 255, rng.randint(95, 210)) if i % 4 else (255, 211, 64, rng.randint(120, 235))
        sd.line([(edge_x, edge_y), (inner_x, inner_y)], fill=color, width=width)
streak.save(OUT / "T_PerfectDodge_StreakMask.png")

print("Generated judgment queue UI assets in", OUT)
