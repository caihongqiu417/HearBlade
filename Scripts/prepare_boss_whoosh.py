"""Convert two CC0 Swishes Sound Pack samples to Unreal-friendly PCM WAVs.

Source: https://opengameart.org/content/swishes-sound-pack (artisticdude, CC0).
The source ZIP is kept under SourceAudio/BossRhythm/ThirdParty for provenance.
"""

import io
import math
import struct
import wave
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "SourceAudio" / "BossRhythm"
ZIP = ROOT / "ThirdParty" / "swishes.zip"
SAMPLES = {
    # Use the shortest recordings without time stretching. These have a fast,
    # dry "shhk" transient instead of the heavy low-mid tail of the old mix.
    "SW_BossBladeStrong.wav": (("swishes/swish-11.wav", 0.0, 1.0, 1.0),
                               ("swishes/swish-13.wav", 0.0, 1.0, 0.28)),
    "SW_BossBladeWeak.wav": (("swishes/swish-13.wav", 0.0, 1.0, 1.0),),
}

with zipfile.ZipFile(ZIP) as archive:
    for target, layers in SAMPLES.items():
        rate = 44100
        decoded = []
        for source, offset, stretch, weight in layers:
            with wave.open(io.BytesIO(archive.read(source)), "rb") as src:
                assert src.getsampwidth() == 3 and src.getnchannels() == 2
                assert src.getframerate() == rate
                raw = src.readframes(src.getnframes())
            samples = [int.from_bytes(raw[i:i+3], "little", signed=True) / 8388608.0
                       for i in range(0, len(raw), 3)]
            decoded.append((samples, offset, stretch, weight))
        frame_count = int(max(offset * rate + len(samples) / 2 * stretch
                              for samples, offset, stretch, _ in decoded) + rate * 0.012)
        mixed = [0.0] * (frame_count * 2)
        for samples, offset, stretch, weight in decoded:
            input_frames = len(samples) // 2
            output_frames = int(input_frames * stretch)
            start = int(offset * rate)
            for frame in range(output_frames):
                source_position = frame / stretch
                a = min(int(source_position), input_frames - 1)
                b = min(a + 1, input_frames - 1)
                fraction = source_position - a
                for channel in range(2):
                    value = samples[a*2+channel] * (1-fraction) + samples[b*2+channel] * fraction
                    mixed[(start+frame)*2+channel] += value * weight
        # Remove rumble and emphasize the upper-mid air movement. A one-pole
        # high-pass keeps this audible over guitars without adding a metal ring.
        filtered = [0.0] * len(mixed)
        alpha = 1.0 / (1.0 + 2.0 * math.pi * 520.0 / rate)
        for channel in range(2):
            previous_input = 0.0
            previous_output = 0.0
            for frame in range(frame_count):
                index = frame * 2 + channel
                current = mixed[index]
                high = alpha * (previous_output + current - previous_input)
                filtered[index] = high + 0.32 * (high - previous_output)
                previous_input = current
                previous_output = high
        mixed = filtered
        gain = 0.94 / max(abs(v) for v in mixed)
        fade_frames = int(rate * 0.012)
        packed = bytearray()
        for frame in range(frame_count):
            fade = min(1.0, (frame_count - frame) / max(1, fade_frames))
            for channel in range(2):
                value = max(-0.999, min(0.999, mixed[frame * 2 + channel] * gain * fade))
                packed.extend(struct.pack("<h", round(value * 32767)))
        with wave.open(str(ROOT / target), "wb") as dst:
            dst.setnchannels(2)
            dst.setsampwidth(2)
            dst.setframerate(rate)
            dst.writeframes(packed)
        rms = math.sqrt(sum(v*v for v in mixed) / len(mixed)) * gain
        print(f"{target}: {frame_count/rate:.3f}s, RMS {rms:.3f}")
