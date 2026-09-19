"""Generate original short percussion for the Boss prototype.

The blade WAVs come from the CC0 Swishes Sound Pack. Regenerate them with
prepare_boss_whoosh.py so this script cannot overwrite the chosen wind sound.
"""

import math
import random
import wave
from pathlib import Path

RATE = 48000
OUT = Path(__file__).resolve().parents[1] / "SourceAudio" / "BossRhythm"
OUT.mkdir(parents=True, exist_ok=True)
random.seed(4061)


def save(name, duration, sample):
    values = []
    energy = 0.0
    peak = 0.0
    for i in range(int(duration * RATE)):
        t = i / RATE
        value = max(-0.98, min(0.98, sample(t)))
        energy += value * value
        peak = max(peak, abs(value))
        values.append(int(value * 32767).to_bytes(2, "little", signed=True))
    with wave.open(str(OUT / name), "wb") as handle:
        handle.setnchannels(1)
        handle.setsampwidth(2)
        handle.setframerate(RATE)
        handle.writeframes(b"".join(values))
    print("{}: peak={:.2f}, RMS={:.2f}".format(name, peak, math.sqrt(energy / len(values))))


def kick(t):
    phase = 2 * math.pi * (52 * t + 108 * (1 - math.exp(-t * 55)) / 55)
    body = math.sin(phase) * math.exp(-t * 20)
    click = random.uniform(-1, 1) * math.exp(-t * 190)
    return 0.82 * body + 0.18 * click


def tick(t):
    noise = random.uniform(-1, 1)
    tone = math.sin(2 * math.pi * 2700 * t)
    return (0.52 * noise + 0.22 * tone) * math.exp(-t * 75)


save("SW_BossDrumKick.wav", 0.24, kick)
save("SW_BossDrumTick.wav", 0.09, tick)
print(OUT)
