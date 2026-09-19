"""Generate a rounded one-bar 104 BPM combat drum stem for seamless looping."""

import math
import random
import wave
from array import array
from pathlib import Path

RATE = 48000
BPM = 104.0
BEAT = 60.0 / BPM
BAR = BEAT * 4.0
FRAMES = round(BAR * RATE)
OUT = Path(__file__).resolve().parents[1] / "SourceAudio" / "BossRhythm" / "SW_BossCombatDrumLoop.wav"
random.seed(104)
mix = array("f", [0.0]) * FRAMES


def add(start_seconds, duration, generator, gain=1.0):
    start = round(start_seconds * RATE)
    count = min(round(duration * RATE), FRAMES - start)
    for i in range(max(0, count)):
        mix[start + i] += generator(i / RATE) * gain


def kick(t):
    # Fast click + pitch-dropping body. Short enough to leave room for the song.
    phase = 2.0 * math.pi * (43.0 * t + 92.0 * (1.0 - math.exp(-t * 35.0)) / 35.0)
    body = math.sin(phase) * math.exp(-t * 17.0)
    click = random.uniform(-1.0, 1.0) * math.exp(-t * 150.0)
    return 0.92 * body + 0.20 * click


def snare(t):
    # Keep the backbeat dark and drum-like.  A small noise layer suggests the
    # wires without turning into the scratchy foreground of the old version.
    noise = random.uniform(-1.0, 1.0)
    body = (
        math.sin(2.0 * math.pi * 155.0 * t)
        + 0.42 * math.sin(2.0 * math.pi * 205.0 * t)
    ) * math.exp(-t * 20.0)
    return 0.22 * noise * math.exp(-t * 30.0) + 0.78 * body


def tom(t):
    phase = 2.0 * math.pi * (92.0 * t + 28.0 * (1.0 - math.exp(-t * 24.0)) / 24.0)
    return math.sin(phase) * math.exp(-t * 16.0)


# Kick anchors beats 1 and 3; a dark snare marks 2 and 4.  Quiet toms replace
# the old hats and crash so the guide layer reads as drums rather than cymbals.
for beat in range(4):
    time = beat * BEAT
    add(time, 0.30, kick, 1.0 if beat == 0 else (0.82 if beat == 2 else 0.18))
    if beat in (1, 3):
        add(time, 0.24, snare, 0.72)
    add(time + 0.5 * BEAT, 0.18, tom, 0.13)

# Soft limiting produces a loud stem without hard digital clipping.
peak = max(abs(value) for value in mix)
drive = 1.25 / max(peak, 1e-6)
pcm = array("h")
energy = 0.0
for value in mix:
    shaped = math.tanh(value * drive * 1.1) / math.tanh(1.1)
    shaped *= 0.80
    energy += shaped * shaped
    pcm.append(round(max(-1.0, min(1.0, shaped)) * 32767))

with wave.open(str(OUT), "wb") as handle:
    handle.setnchannels(1)
    handle.setsampwidth(2)
    handle.setframerate(RATE)
    handle.writeframes(pcm.tobytes())
print(
    f"{OUT}: duration={FRAMES/RATE:.6f}s "
    f"peak={max(abs(value) for value in pcm)/32767.0:.3f} "
    f"rms={math.sqrt(energy/FRAMES):.3f}"
)
