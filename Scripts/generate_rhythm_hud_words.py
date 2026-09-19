from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[1]
font_path = root / "SourceArt" / "UI" / "RhythmCombat" / "Fonts" / "Bangers-Regular.ttf"
out_dir = root / "SourceArt" / "UI" / "RhythmCombat"
font = ImageFont.truetype(str(font_path), 112)

words = {
    # Gold is reserved for the best result so PERFECT reads immediately.
    "PERFECT": (255, 196, 38, 255),
    "GOOD": (205, 245, 255, 255),
    "BAD": (132, 158, 174, 255),
    "MISS": (194, 70, 62, 255),
    "COMBO": (255, 184, 38, 255),
}

for word, color in words.items():
    image = Image.new("RGBA", (640, 180), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    bounds = draw.textbbox((0, 0), word, font=font, stroke_width=0)
    x = (image.width - (bounds[2] - bounds[0])) / 2 - bounds[0]
    y = (image.height - (bounds[3] - bounds[1])) / 2 - bounds[1] - 5
    draw.text((x, y), word, font=font, fill=color, stroke_width=15, stroke_fill=(0, 0, 0, 255))
    draw.text((x, y), word, font=font, fill=color, stroke_width=6, stroke_fill=(255, 255, 255, 255))
    image.save(out_dir / f"T_HUD_Word_{word}.png")
