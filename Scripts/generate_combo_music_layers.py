"""Generate original, phase-aligned one-bar percussion layers for HearBlade.

These sounds are synthesized from equations and seeded noise. They contain no
third-party samples and are intended to loop at 104 BPM in 4/4.
"""
from __future__ import annotations

import math
import random
import struct
import wave
from pathlib import Path

RATE = 48_000
BPM = 104.0
BEAT = 60.0 / BPM
BAR = BEAT * 4.0
COUNT = round(RATE * BAR)
OUT = Path(__file__).resolve().parents[1] / "SourceArt" / "Audio" / "ComboLayers"


def env(t: float, start: float, decay: float, attack: float = 0.004) -> float:
    x = t - start
    if x < 0.0 or x > decay * 7.0:
        return 0.0
    return min(1.0, x / attack) * math.exp(-x / decay)


def softclip(x: float) -> float:
    return math.tanh(x * 1.25) / math.tanh(1.25)


def write(name: str, samples: list[float]) -> None:
    peak = max(max(abs(v) for v in samples), 1e-8)
    gain = min(0.88 / peak, 1.0)
    pcm = b"".join(struct.pack("<h", int(max(-1, min(1, softclip(v * gain))) * 32767)) for v in samples)
    OUT.mkdir(parents=True, exist_ok=True)
    with wave.open(str(OUT / name), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(RATE)
        wav.writeframes(pcm)


def low_layer() -> list[float]:
    hits = [(0.0, 1.0), (2 * BEAT, 0.78)]
    result = []
    for i in range(COUNT):
        t = i / RATE
        value = 0.0
        for start, strength in hits:
            x = t - start
            e = env(t, start, 0.18, 0.006)
            if e:
                phase = 2 * math.pi * (62 * x + 38 * (1 - math.exp(-x / 0.025)) * 0.025)
                value += strength * e * (0.82 * math.sin(phase) + 0.18 * math.sin(phase * 0.5))
        result.append(value * 0.78)
    return result


def snare_layer() -> list[float]:
    rng = random.Random(0x48454152)
    noise_state = 0.0
    result = []
    hits = [(BEAT, 1.0), (3 * BEAT, 1.0)]
    for i in range(COUNT):
        t = i / RATE
        raw = rng.uniform(-1.0, 1.0)
        noise_state += 0.09 * (raw - noise_state)  # soft, dark noise body
        value = 0.0
        for start, strength in hits:
            x = t - start
            e = env(t, start, 0.09, 0.003)
            if e:
                body = math.sin(2 * math.pi * 176 * x) + 0.35 * math.sin(2 * math.pi * 244 * x)
                value += strength * e * (0.44 * body + 0.40 * noise_state)
        result.append(value * 0.54)
    return result


def high_layer() -> list[float]:
    result = []
    hits = [(n * BEAT / 2, 0.72 if n % 2 else 1.0) for n in range(8)]
    for i in range(COUNT):
        t = i / RATE
        value = 0.0
        for start, strength in hits:
            x = t - start
            e = env(t, start, 0.028, 0.002)
            if e:
                # Muted tonal wood click: clear rhythm without a sharp hiss.
                value += strength * e * (math.sin(2 * math.pi * 540 * x) + 0.28 * math.sin(2 * math.pi * 810 * x))
        result.append(value * 0.25)
    return result


def tonal_snap(duration: float, frequencies: tuple[float, ...], rise: float = 0.0) -> list[float]:
    count = round(RATE * duration)
    result = []
    for i in range(count):
        t = i / RATE
        attack = min(1.0, t / 0.004)
        release = math.exp(-t / (duration * 0.28))
        value = 0.0
        for index, frequency in enumerate(frequencies):
            phase = 2 * math.pi * (frequency * t + rise * t * t * 0.5)
            value += math.sin(phase) / (1.0 + index * 0.35)
        result.append(value * attack * release * 0.34)
    return result


def perfect_blade_clash() -> list[float]:
    """A rewarding blade deflection: contact first, a compact success lift second."""
    duration = 0.165
    count = round(RATE * duration)
    rng = random.Random(0x50455246)
    low_noise = 0.0
    previous_low = 0.0
    result = []
    for i in range(count):
        t = i / RATE
        # 2 ms transient: filtered noise gives the blade contact a crisp edge,
        # while the difference removes boxy low-frequency noise.
        raw = rng.uniform(-1.0, 1.0)
        low_noise += 0.24 * (raw - low_noise)
        high_pass = low_noise - previous_low
        previous_low = low_noise
        transient = high_pass * min(1.0, t / 0.0015) * math.exp(-t / 0.010) * 1.9

        # Inharmonic steel modes. Their slight downward drift reads as impact,
        # and the fast decay avoids the old bell-like tail.
        metal_env = min(1.0, t / 0.0018) * math.exp(-t / 0.047)
        metal = 0.0
        for index, (frequency, weight) in enumerate(((1180.0, 0.58), (1765.0, 0.40), (2470.0, 0.22))):
            phase = 2 * math.pi * (frequency * t - (35.0 + index * 18.0) * t * t)
            metal += math.sin(phase + index * 0.73) * weight

        # Short low body makes PERFECT land on the beat without turning boomy.
        body_env = min(1.0, t / 0.003) * math.exp(-t / 0.035)
        body_phase = 2 * math.pi * (118.0 * t - 95.0 * t * t)
        body = math.sin(body_phase) * body_env * 0.34

        # A delayed open-fifth lift rewards the player after the physical hit.
        # It is deliberately short and upper-mid focused, so it reads as
        # affirmation rather than a UI bell or a long musical chord.
        lift_t = t - 0.024
        lift = 0.0
        if lift_t >= 0.0:
            lift_env = min(1.0, lift_t / 0.003) * math.exp(-lift_t / 0.045)
            sweep = 95.0 * lift_t * lift_t
            lift = lift_env * (
                0.30 * math.sin(2 * math.pi * (820.0 * lift_t + sweep))
                + 0.21 * math.sin(2 * math.pi * (1230.0 * lift_t + sweep * 1.35))
            )
        tail = clamp01((duration - t) / 0.018)
        result.append((transient * 0.30 + metal * metal_env * 0.48 + body + lift) * tail)
    return result


def clamp01(value: float) -> float:
    return max(0.0, min(1.0, value))


if __name__ == "__main__":
    write("SW_ComboTier1Low.wav", low_layer())
    write("SW_ComboTier2Snare.wav", snare_layer())
    write("SW_ComboTier3High.wav", high_layer())
    write("SW_RhythmGood.wav", tonal_snap(0.11, (330.0, 495.0), 90.0))
    write("SW_RhythmPerfect.wav", perfect_blade_clash())
    write("SW_ComboTierUp.wav", tonal_snap(0.32, (260.0, 390.0, 520.0), 520.0))
    (OUT / "LICENSE.txt").write_text(
        "Original procedural audio generated for HearBlade by Scripts/generate_combo_music_layers.py.\n"
        "No external samples or third-party source material are used.\n",
        encoding="utf-8",
    )
    print(f"Generated 3 synchronized {BAR:.6f}s loops at {OUT}")
