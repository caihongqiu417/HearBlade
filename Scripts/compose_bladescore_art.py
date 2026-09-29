"""Blade Score HUD textures built from the project's PAINTED art (no vector drawing
for anything the player has to read).

Sources (all generated illustrations already in the project):
  SourceArt/RhythmResponses/Rhythm_{Dodge,Block,Counter,HoldBlock}.png  boss-note icons
  SourceArt/Combat/T_GroundBurstSigil.png                                area-attack icon
  SourceArt/UI/RhythmCombat/PhaseTwo/T_HUD_PhaseTwo_BladeAtlas.png       blade body + slash
  SourceArt/UI/RhythmCombat/Fonts/Bangers-Regular.ttf                    HUD comic font

Writes SourceArt/UI/BladeScore/T_BS_*.png with the SAME names/sizes the HUD
already loads, so nothing in C++ changes. The editor auto-imports them on start
(Content/Python/init_unreal.py). FX-only pieces (glow, haze, spark, tick) are
kept from generate_bladescore_art.py.

If you later generate dedicated art (see SourceArt/UI/BladeScore/PROMPTS.md),
drop the file into SourceArt/UI/BladeScore/Painted/<same name>.png: it wins
over everything here.

Run:  python Scripts/compose_bladescore_art.py
"""
import os
from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageEnhance

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
SA = os.path.join(ROOT, 'SourceArt')
OUT = os.path.join(SA, 'UI', 'BladeScore')
PAINTED = os.path.join(OUT, 'Painted')
FONT = os.path.join(SA, 'UI', 'RhythmCombat', 'Fonts', 'Bangers-Regular.ttf')
RED = (232, 44, 72); GOLD = (255, 184, 48); CYAN = (120, 220, 255); INK = (6, 8, 14)


def load(*p):
    return Image.open(os.path.join(SA, *p)).convert('RGBA')


def fit(im, size, pad=.06):
    """Trim to the alpha bounds and fit into a transparent square."""
    im = im.crop(im.getbbox())
    box = int(size * (1 - 2 * pad))
    im.thumbnail((box, box), Image.LANCZOS)
    out = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    out.alpha_composite(im, ((size - im.width) // 2, (size - im.height) // 2))
    return out


def ink_outline(im, px, color=INK):
    """Dark contour around the silhouette so painted icons survive any background."""
    a = im.getchannel('A').filter(ImageFilter.MaxFilter(px * 2 + 1))
    base = Image.new('RGBA', im.size, color + (0,)); base.putalpha(a)
    base.alpha_composite(im)
    return base


def save(im, name):
    override = os.path.join(PAINTED, name + '.png')
    if os.path.exists(override):
        im = Image.open(override).convert('RGBA').resize(im.size, Image.LANCZOS)
        print('painted override', name)
    im.save(os.path.join(OUT, name + '.png')); print('wrote', name, im.size)


# ---------------------------------------------------------------- boss icons
def icons():
    S = 256
    dodge = fit(load('RhythmResponses', 'Rhythm_Dodge.png'), S)
    block = fit(load('RhythmResponses', 'Rhythm_Block.png'), S)
    counter = fit(load('RhythmResponses', 'Rhythm_Counter.png'), S)
    hold = fit(load('RhythmResponses', 'Rhythm_HoldBlock.png'), S)
    for name, im in (('T_BS_Icon_Dodge', dodge), ('T_BS_Icon_Block', block),
                     ('T_BS_Icon_Counter', counter), ('T_BS_Icon_Hold', hold)):
        save(ink_outline(im, 3), name)
    # Either: boot in front-left, shield behind-right ("dodge OR block").
    e = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    e.alpha_composite(block.resize((int(S * .72),) * 2, Image.LANCZOS), (int(S * .28), 0))
    e.alpha_composite(dodge.resize((int(S * .72),) * 2, Image.LANCZOS), (0, int(S * .28)))
    save(ink_outline(e, 3), 'T_BS_Icon_Either')
    # Release: the hold shield, brightened, with the painted ring cut open at the top.
    r = ImageEnhance.Brightness(hold).enhance(1.25)
    d = ImageDraw.Draw(r)
    d.polygon([(S * .5, S * .02), (S * .62, S * .2), (S * .38, S * .2)], fill=(255, 245, 210, 255))
    save(ink_outline(r, 3), 'T_BS_Icon_Release')
    save(fit(load('Combat', 'T_GroundBurstSigil.png'), S, .02), 'T_BS_Icon_Area')


# ---------------------------------------------------------------- blade body
def atlas():
    return load('UI', 'RhythmCombat', 'PhaseTwo', 'T_HUD_PhaseTwo_BladeAtlas.png')


def blade():
    W, H = 2048, 256
    old = Image.open(os.path.join(OUT, 'T_BS_Blade.png')).convert('RGBA')
    out = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    out.alpha_composite(old.crop((0, 0, 500, H)), (0, 0))       # keep the wrapped hilt
    rail = atlas().crop((19, 55, 2013, 322))                      # painted black/ivory/gold rail
    rail = rail.resize((W - 470, int(rail.height * (W - 470) / rail.width)), Image.LANCZOS)
    out.alpha_composite(rail, (470, (H - rail.height) // 2))
    save(out, 'T_BS_Blade')


def slash(heavy=False):
    a = atlas()
    return a.crop((1016, 311, 2020, 734)) if heavy else a.crop((345, 312, 874, 724))


# ---------------------------------------------------------------- guard (tsuba)
def tsuba():
    S = 256
    base = Image.open(os.path.join(OUT, 'T_BS_Tsuba.png')).convert('RGBA').resize((S, S), Image.LANCZOS)
    # Clear the old pictograms inside both halves, keep the ring and colours.
    ring = Image.new('L', (S, S), 0); ImageDraw.Draw(ring).ellipse((30, 22, S - 30, S - 22), fill=255)
    top = Image.new('RGBA', (S, S), (92, 8, 28, 255)); bot = Image.new('RGBA', (S, S), (64, 44, 8, 255))
    half = Image.new('L', (S, S), 0); ImageDraw.Draw(half).rectangle((0, 0, S, S // 2), fill=255)
    fill = Image.composite(top, bot, half); fill.putalpha(ring)
    base.alpha_composite(fill)
    ImageDraw.Draw(base).line((34, S // 2, S - 34, S // 2), fill=(250, 214, 140, 255), width=5)
    shield = fit(load('RhythmResponses', 'Rhythm_Block.png'), 96, 0)
    base.alpha_composite(ink_outline(shield, 2), ((S - 96) // 2, 28))
    sl = slash(); sl.thumbnail((120, 88), Image.LANCZOS)
    base.alpha_composite(ink_outline(sl, 2), ((S - sl.width) // 2, S // 2 + 12))
    save(base, 'T_BS_Tsuba')


# ---------------------------------------------------------------- words / tags
def text_img(text, size, color, w, h, stroke=10):
    f = ImageFont.truetype(FONT, size)
    im = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.text((w // 2 + 6, h // 2 + 6), text, font=f, fill=(0, 0, 0, 200), anchor='mm', stroke_width=stroke, stroke_fill=(0, 0, 0, 200))
    d.text((w // 2, h // 2), text, font=f, fill=color + (255,), anchor='mm', stroke_width=stroke, stroke_fill=INK + (255,))
    return im


def tags():
    for name, mult, col, heavy in (('T_BS_Tag_Gap', 'x2', GOLD, True), ('T_BS_Tag_Tail', 'x1.2', CYAN, False)):
        W, H = 256, 96
        im = Image.new('RGBA', (W, H), (0, 0, 0, 0))
        sl = slash(heavy); sl.thumbnail((110, 84), Image.LANCZOS)
        im.alpha_composite(sl, (4, (H - sl.height) // 2))
        t = text_img(mult, 78, col, 150, 96, 7)
        im.alpha_composite(t, (104, 0))
        save(im, name)
    save(text_img('FILL!', 160, GOLD, 640, 200, 12), 'T_BS_Word_Fill')
    # What to do RIGHT NOW, shown beside the guard.
    save(text_img('ATTACK!', 150, GOLD, 640, 200, 12), 'T_BS_Word_Attack')
    save(text_img('DEFEND!', 150, RED, 640, 200, 12), 'T_BS_Word_Defend')
    a = fit(slash(), 256, .04); save(ink_outline(a, 3), 'T_BS_Icon_Attack')


if __name__ == '__main__':
    os.makedirs(PAINTED, exist_ok=True)
    # NOTE: FX sprites come from generate_bladescore_art.py; run that first if missing.
    icons(); blade(); tsuba(); tags()
