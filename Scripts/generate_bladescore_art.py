"""Blade Score HUD art (PNG textures, transparent, drawn at 4x and downsampled).

Colour law (must stay true everywhere):
  RED / CRIMSON  = the boss  (incoming notes you must answer: dodge / block / counter)
  GOLD / CYAN    = you       (your attack beats, attack windows, your hits)

Outputs SourceArt/UI/BladeScore/*.png; the editor auto-imports them into
/Game/UI/RhythmCombat/BladeScore (Content/Python/init_unreal.py).
Run:  python Scripts/generate_bladescore_art.py
"""
import math, os
from PIL import Image, ImageDraw, ImageFilter, ImageFont

SS = 4
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'SourceArt', 'UI', 'BladeScore')
os.makedirs(OUT, exist_ok=True)
FONT = None
for f in ('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',
          '/usr/share/fonts/opentype/noto/NotoSansCJK-Black.ttc'):
    if os.path.exists(f):
        FONT = f; break

RED = (232, 44, 72); CRIMSON = (120, 10, 34); GOLD = (255, 184, 48); CYAN = (120, 220, 255)
INK = (6, 8, 14)


class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.im = Image.new('RGBA', (w * SS, h * SS), (0, 0, 0, 0))

    def layer(self, fn, blur=0):
        l = Image.new('RGBA', self.im.size, (0, 0, 0, 0))
        fn(ImageDraw.Draw(l), SS)
        if blur: l = l.filter(ImageFilter.GaussianBlur(blur * SS))
        self.im.alpha_composite(l)

    def save(self, name):
        self.im.resize((self.w, self.h), Image.LANCZOS).save(os.path.join(OUT, name + '.png'))
        print(name)


def P(pts, s): return [(x * s, y * s) for x, y in pts]


# ---------------------------------------------------------------- blade rail
# 2048 x 256; the tsuba (judge) sits at x = 512 (25 %), centre line y = 128.
def blade():
    c = Canvas(2048, 256)
    top, bot, tip = 100, 156, 2040
    # handle: dark wrap with gold diamonds, pommel cap
    c.layer(lambda d, s: d.rounded_rectangle([40*s, 108*s, 470*s, 148*s], radius=12*s, fill=(20, 16, 26, 255),
                                             outline=(150, 112, 50, 255), width=3*s))
    for x in range(56, 450, 26):
        c.layer(lambda d, s, x=x: d.polygon(P([(x, 128), (x+13, 111), (x+26, 128), (x+13, 145)], s), fill=(64, 44, 28, 255),
                                            outline=(120, 88, 40, 255), width=s))
    c.layer(lambda d, s: d.rounded_rectangle([22*s, 104*s, 52*s, 152*s], radius=8*s, fill=(150, 112, 50, 255)))
    # blade body: soft shadow, steel body, bright edge, darker spine
    body = [(540, top), (tip-110, top), (tip, 126), (tip-70, bot), (540, bot)]
    c.layer(lambda d, s: d.polygon(P([(x+4, y+6) for x, y in body], s), fill=(0, 0, 0, 150)), blur=6)
    c.layer(lambda d, s: d.polygon(P(body, s), fill=(30, 34, 46, 240)))
    for i in range(10):  # vertical steel gradient
        y0 = top + (bot - top) * i / 10; y1 = top + (bot - top) * (i + 1) / 10
        v = int(22 + 26 * math.sin(math.pi * (i + .5) / 10))
        c.layer(lambda d, s, y0=y0, y1=y1, v=v: d.rectangle([540*s, y0*s, (tip-110)*s, y1*s], fill=(v, v+4, v+14, 120)))
    c.layer(lambda d, s: d.polygon(P(body, s), outline=(12, 14, 20, 255), width=3*s))
    c.layer(lambda d, s: d.line(P([(540, top+2), (tip-110, top+2), (tip-2, 126)], s), fill=(235, 240, 250, 255), width=4*s))
    c.layer(lambda d, s: d.line(P([(540, bot-2), (tip-70, bot-2)], s), fill=(80, 88, 110, 255), width=3*s))
    # habaki (blade collar) next to the guard
    c.layer(lambda d, s: d.rectangle([540*s, 98*s, 580*s, 158*s], fill=(170, 128, 56, 255), outline=(90, 64, 24, 255), width=2*s))
    c.save('T_BS_Blade')


# ---------------------------------------------------------------- icons
def shield(d, s, cx, cy, r, fill, outline=None, w=0):
    pts = [(cx - r*.8, cy - r*.85), (cx + r*.8, cy - r*.85), (cx + r*.8, cy - r*.1),
           (cx, cy + r), (cx - r*.8, cy - r*.1)]
    d.polygon(P(pts, s), fill=fill, outline=outline, width=w)


def sword(d, s, cx, cy, r, ang, fill):
    dx, dy = math.cos(ang), math.sin(ang); nx, ny = -dy, dx
    L = r; W = r * .13
    blade_pts = [(cx + dx*L, cy + dy*L), (cx + dx*(L*.75) + nx*W, cy + dy*(L*.75) + ny*W),
                 (cx - dx*L*.35 + nx*W, cy - dy*L*.35 + ny*W), (cx - dx*L*.35 - nx*W, cy - dy*L*.35 - ny*W),
                 (cx + dx*(L*.75) - nx*W, cy + dy*(L*.75) - ny*W)]
    d.polygon(P(blade_pts, s), fill=fill)
    g = r * .38
    gx, gy = cx - dx*L*.35, cy - dy*L*.35
    d.line(P([(gx + nx*g, gy + ny*g), (gx - nx*g, gy - ny*g)], s), fill=fill, width=int(r*.16*s))
    d.line(P([(gx, gy), (gx - dx*L*.45, gy - dy*L*.45)], s), fill=fill, width=int(r*.17*s))


def chevrons(d, s, cx, cy, r, fill):
    for k in (-.45, .2):
        x = cx + k*r
        d.polygon(P([(x - r*.25, cy - r*.7), (x + r*.35, cy), (x - r*.25, cy + r*.7),
                     (x - r*.55, cy + r*.7), (x + .05*r, cy), (x - r*.55, cy - r*.7)], s), fill=fill)


def icon(name, draw, size=256):
    """White silhouette with a thick dark outline (legible over any scene)."""
    c = Canvas(size, size)
    m = Image.new('L', (size*SS, size*SS), 0)
    draw(ImageDraw.Draw(m), SS, size)
    out = m.filter(ImageFilter.MaxFilter(29))  # ~3.5 px dark outline after downsampling
    ink = Image.new('RGBA', m.size, INK + (255,)); ink.putalpha(out)
    white = Image.new('RGBA', m.size, (255, 255, 255, 255)); white.putalpha(m)
    c.im.alpha_composite(ink); c.im.alpha_composite(white)
    c.save(name)


def icons():
    icon('T_BS_Icon_Dodge', lambda d, s, n: (chevrons(d, s, n*.5, n*.5, n*.36, 255),
         [d.line(P([(n*.1, n*(.36 + .14*k)), (n*.26, n*(.36 + .14*k))], s), fill=255, width=int(n*.045*s)) for k in range(3)]))
    icon('T_BS_Icon_Block', lambda d, s, n: shield(d, s, n*.5, n*.5, n*.36, 255))
    def counter(d, s, n):
        # shield on the left, a separate sword thrusting out of it to the right
        shield(d, s, n*.3, n*.52, n*.24, 255)
        sword(d, s, n*.66, n*.48, n*.3, -math.pi/5, 255)
        d.arc(P([(n*.18, n*.12), (n*.82, n*.62)], s), 200, 330, fill=255, width=int(n*.05*s))
    icon('T_BS_Icon_Counter', counter)
    def hold(d, s, n):
        shield(d, s, n*.36, n*.5, n*.28, 255)
        d.rectangle(P([(n*.58, n*.44), (n*.92, n*.56)], s), fill=255)
    icon('T_BS_Icon_Hold', hold)
    def release(d, s, n):
        shield(d, s, n*.5, n*.6, n*.26, 255)
        d.polygon(P([(n*.5, n*.06), (n*.7, n*.28), (n*.56, n*.28), (n*.56, n*.36), (n*.44, n*.36), (n*.44, n*.28), (n*.3, n*.28)], s), fill=255)
    icon('T_BS_Icon_Release', release)
    def either(d, s, n):
        shield(d, s, n*.32, n*.5, n*.24, 255)
        d.line(P([(n*.5, n*.18), (n*.5, n*.82)], s), fill=255, width=int(n*.03*s))
        chevrons(d, s, n*.74, n*.5, n*.2, 255)
    icon('T_BS_Icon_Either', either)
    def area(d, s, n):
        d.ellipse(P([(n*.3, n*.3), (n*.7, n*.7)], s), outline=255, width=int(n*.07*s))
        for a in range(4):
            ang = a*math.pi/2 + math.pi/4; dx, dy = math.cos(ang), math.sin(ang)
            cx, cy = n*.5 + dx*n*.34, n*.5 + dy*n*.34; nx, ny = -dy, dx
            d.polygon(P([(cx + dx*n*.12, cy + dy*n*.12), (cx + nx*n*.09, cy + ny*n*.09), (cx - nx*n*.09, cy - ny*n*.09)], s), fill=255)
    icon('T_BS_Icon_Area', area)
    icon('T_BS_Icon_Attack', lambda d, s, n: sword(d, s, n*.5, n*.5, n*.42, -math.pi/4, 255))


# ---------------------------------------------------------------- note plate (boss)
def plate():
    c = Canvas(256, 256)
    dia = lambda r: [(128, 128 - r), (128 + r, 128), (128, 128 + r), (128 - r, 128)]
    c.layer(lambda d, s: d.polygon(P(dia(120), s), fill=RED + (120,)), blur=8)
    c.layer(lambda d, s: d.polygon(P(dia(112), s), fill=INK + (255,)))
    c.layer(lambda d, s: d.polygon(P(dia(104), s), fill=CRIMSON + (255,)))
    c.layer(lambda d, s: d.polygon(P(dia(104), s), outline=RED + (255,), width=6*s))
    c.layer(lambda d, s: d.polygon(P(dia(86), s), outline=(255, 120, 140, 110), width=2*s))
    c.save('T_BS_NotePlate')


# ---------------------------------------------------------------- tsuba (judge)
def tsuba():
    c = Canvas(256, 256)
    box = [28, 8, 228, 248]
    c.layer(lambda d, s: d.ellipse(P([(box[0]+4, box[1]+6), (box[2]+4, box[3]+6)], s), fill=(0, 0, 0, 160)), blur=5)
    c.layer(lambda d, s: d.pieslice(P([(box[0], box[1]), (box[2], box[3])], s), 180, 360, fill=(96, 8, 28, 255)))
    c.layer(lambda d, s: d.pieslice(P([(box[0], box[1]), (box[2], box[3])], s), 0, 180, fill=(70, 50, 12, 255)))
    c.layer(lambda d, s: d.ellipse(P([(box[0], box[1]), (box[2], box[3])], s), outline=(214, 170, 80, 255), width=8*s))
    c.layer(lambda d, s: d.line(P([(box[0]+6, 128), (box[2]-6, 128)], s), fill=(214, 170, 80, 255), width=6*s))
    shield(ImageDraw.Draw(c.im), SS, 128, 74, 44, (255, 225, 230, 255))
    sword(ImageDraw.Draw(c.im), SS, 128, 184, 58, -math.pi/4, (255, 236, 190, 255))
    c.save('T_BS_Tsuba')
    for name, rng in (('T_BS_TsubaGlowTop', (180, 360)), ('T_BS_TsubaGlowBottom', (0, 180))):
        g = Canvas(256, 256)
        g.layer(lambda d, s, rng=rng: d.pieslice(P([(20, 0), (236, 256)], s), rng[0], rng[1], fill=(255, 255, 255, 255)), blur=10)
        g.save(name)


# ---------------------------------------------------------------- small sprites
def sprites():
    c = Canvas(32, 64)  # attack tick
    c.layer(lambda d, s: d.rounded_rectangle(P([(4, 2), (28, 62)], s), radius=6*s, fill=INK + (255,)))
    c.layer(lambda d, s: d.rounded_rectangle(P([(9, 7), (23, 57)], s), radius=4*s, fill=(255, 255, 255, 255)))
    c.save('T_BS_Tick')
    c = Canvas(512, 64)  # tileable hamon (temper line) glow, white
    def wave(d, s, w):
        pts = [(x, 32 + 9*math.sin(x/512*2*math.pi*3) + 3*math.sin(x/512*2*math.pi*7)) for x in range(0, 513, 4)]
        d.line(P(pts, s), fill=(255, 255, 255, 255), width=w*s)
    c.layer(lambda d, s: wave(d, s, 18), blur=9)
    c.layer(lambda d, s: wave(d, s, 7))
    c.save('T_BS_Hamon')
    c = Canvas(128, 128)  # boss danger haze
    c.layer(lambda d, s: d.ellipse(P([(34, 4), (94, 124)], s), fill=(255, 255, 255, 255)), blur=18)
    c.save('T_BS_Danger')
    c = Canvas(128, 128)  # your hit notch in a window
    def star(d, s):
        pts = []
        for i in range(16):
            a = i*math.pi/8; r = 60 if i % 2 == 0 else 16
            pts.append((64 + math.cos(a)*r*(1 if i % 4 == 0 else .55), 64 + math.sin(a)*r*(1 if i % 4 == 0 else .55)))
        d.polygon(P(pts, s), fill=(255, 255, 255, 255))
    c.layer(lambda d, s: d.ellipse(P([(34, 34), (94, 94)], s), fill=(255, 255, 255, 170)), blur=10)
    c.layer(star)
    c.save('T_BS_Spark')


def words():
    def word(name, text, color, w=640, h=200, size=150):
        c = Canvas(w, h)
        f = ImageFont.truetype(FONT, size*SS)
        c.layer(lambda d, s: d.text((w*s/2 + 8*s, h*s/2 + 8*s), text, font=f, fill=(0, 0, 0, 200), anchor='mm'), blur=3)
        c.layer(lambda d, s: d.text((w*s/2, h*s/2), text, font=f, fill=color + (255,), anchor='mm',
                                    stroke_width=10*s, stroke_fill=INK + (255,)))
        c.save(name)
    word('T_BS_Word_Fill', 'FILL!', GOLD)
    # window tags: sword icon + multiplier (language-free)
    for name, text, col in (('T_BS_Tag_Gap', 'x2', GOLD), ('T_BS_Tag_Tail', 'x1.2', CYAN)):
        c = Canvas(256, 96)
        f = ImageFont.truetype(FONT, 64*SS)
        sword(ImageDraw.Draw(c.im), SS, 48, 48, 38, -math.pi/4, col + (255,))
        c.layer(lambda d, s, text=text, col=col: d.text((96*s, 50*s), text, font=f, fill=col + (255,), anchor='lm',
                                                        stroke_width=6*s, stroke_fill=INK + (255,)))
        c.save(name)


blade(); icons(); plate(); tsuba(); sprites(); words()
