"""Synthesize Dead-as-Disco-style PUNCH impacts (original sounds, not ripped).

A fist-hit is built from four short layers, all starting at sample 0 so the
contact lands exactly on the beat:
  snap   - 1-3 ms broadband crack (knuckle on skin)
  slap   - band-passed noise 0.7-5 kHz, ~10 ms (leather / skin slap)
  meat   - low-passed noise 120-700 Hz, ~25 ms, saturated (body 'thud')
  thump  - pitch-dropping sine (chest punch), saturated
plus a short cloth/air rustle.  Light = tighter/higher, Heavy = deeper + crunch,
Finisher = heavy punch + sub drop + gritty tail, Kick = boot thump,
OffBeat = dull muffled punch.  48 kHz 16-bit mono.

Run from a normal Python (numpy+scipy), then import:
  python Scripts/generate_hitrush_sfx.py
  (editor) py D:/Udev/HearBlade/Scripts/import_hitrush_sfx.py
"""
import numpy as np, os
from scipy.io import wavfile
from scipy.signal import butter, sosfilt

SR = 48000
rng = np.random.default_rng(11)


def t(sec): return np.arange(int(SR * sec)) / SR
def env(sec, decay, attack=.0004):
    x = t(sec); return np.clip(x / attack, 0, 1) * np.exp(-x / decay)
def filt(sig, f, kind): return sosfilt(butter(4, f, btype=kind, fs=SR, output='sos'), sig)
def noise(sec): return rng.standard_normal(int(SR * sec))
def pad(a, n): return np.pad(a, (0, max(0, n - len(a))))[:n]
def mix(*layers):
    n = max(len(l) for l in layers); return sum(pad(l, n) for l in layers)


def snap(g=1.):
    return filt(noise(.004), 2500, 'high') * env(.004, .0009, .0001) * g
def slap(p=1., g=1., decay=.009):
    return filt(noise(.06), [700 * p, 5000 * p], 'band') * env(.06, decay) * g
def meat(p=1., g=1., decay=.025, drive=4.):
    return np.tanh(filt(noise(.14), [120 * p, 700 * p], 'band') * drive) * env(.14, decay, .0008) * g
def thump(p=1., f0=200., f1=55., sweep=.03, decay=.07, sec=.3, g=1.):
    x = t(sec); f = (f1 + (f0 - f1) * np.exp(-x / sweep)) * p
    return np.tanh(np.sin(2 * np.pi * np.cumsum(f) / SR) * 2.2) * env(sec, decay, .0006) * g
def crunch(p=1., g=1.):
    """Driven low-mid noise: the dull 'crack' of a heavy body blow (no bit-crush,
    which read as metallic)."""
    n = filt(noise(.09), [180 * p, 1400 * p], 'band')
    return filt(np.tanh(n * 5), 1600, 'low') * env(.09, .02, .0006) * g
def rustle(g=1., decay=.035):
    return filt(noise(.12), [2000, 7000], 'band') * env(.12, decay, .002) * g
def sub(p=1., sec=.55, g=1.):
    x = t(sec); f = (38 + 50 * np.exp(-x / .06)) * p
    return np.sin(2 * np.pi * np.cumsum(f) / SR) * env(sec, .2, .002) * g
def grit_tail(sec=.6, g=1.):
    return filt(np.tanh(filt(noise(sec), 900, 'low') * 3), 2200, 'low') * env(sec, .15, .004) * g


def finish(x, name, peak=.95):
    x = np.tanh(x * 1.6)                                   # glue: fat, compressed punch
    x = filt(x, 30, 'high')                                # remove DC / sub rumble
    x = x / np.max(np.abs(x)) * peak
    fade = int(SR * .008); x[-fade:] *= np.linspace(1, 0, fade)
    wavfile.write(name, SR, (x * 32767).astype(np.int16))
    rms = 20 * np.log10(np.sqrt(np.mean(x[:int(SR * .1)] ** 2)))
    print(os.path.basename(name), round(len(x) / SR, 3), 's  rms(100ms)=%.1f dB' % rms)


def punch_light(p):
    return mix(snap(1.4), slap(p, 1.5), meat(p, 1.), thump(p, 230, 80, .02, .045, .22, .9), rustle(.25))
def punch_heavy(p):
    return mix(snap(1.8), slap(p * .85, 2., .012), meat(p * .8, 1.5, .035, 6.), crunch(p, 1.5),
               thump(p, 190, 48, .035, .09, .4, 1.25), rustle(.3, .05))


OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'SourceArt', 'Audio', 'HitRush')
os.makedirs(OUT, exist_ok=True)
# Slight pitch rise through each chain so a combo reads as a phrase (jab-jab-cross-hook).
for i, p in enumerate([1., 1.06, .95, 1.12], 1):
    finish(punch_light(p), f'{OUT}/SW_HitRush_Light{i}.wav')
for i, p in enumerate([1., 1.05, .94, 1.1], 1):
    finish(punch_heavy(p), f'{OUT}/SW_HitRush_Heavy{i}.wav')
finish(mix(punch_heavy(.9), sub(1., .6, .7), grit_tail(.6, .4), crunch(.8, 1.)), f'{OUT}/SW_HitRush_Finisher.wav')
finish(mix(snap(1.3), slap(.7, 1.8, .014), meat(.7, 1.5, .04, 5.), thump(1., 170, 45, .04, .1, .4, 1.3), rustle(.4, .05)),
       f'{OUT}/SW_HitRush_Kick.wav')
def whoosh(sec, lo, hi, g):
    """Air swish: band-passed noise whose band sweeps up and swells into the hit."""
    x = t(sec); n = noise(sec); out = np.zeros_like(n); k = 6; seg = len(n) // k
    for j in range(k):
        f = lo + (hi - lo) * j / (k - 1)
        out[j * seg:(j + 1) * seg] = filt(n, [f * .6, f * 1.6], 'band')[j * seg:(j + 1) * seg]
    shape = np.sin(np.pi * np.clip(x / sec, 0, 1)) ** 2
    return out * shape * g
def clap():
    """808-style hand clap: three quick noise flams then a short body. Charge-beat cue."""
    out = np.zeros(int(SR * .2))
    for k, d in enumerate((0., .009, .018)):
        b = filt(noise(.03), [900, 2600], 'band') * env(.03, .004, .0003)
        i = int(SR * d); out[i:i + len(b)] += b[:len(out) - i] * (1. if k < 2 else 1.2)
    tail = filt(noise(.2), [800, 2200], 'band') * env(.2, .045, .001)
    return mix(out, pad(np.zeros(int(SR * .018)), 0), np.concatenate([np.zeros(int(SR * .018)), tail])[:len(out)] * .7)
finish(clap(), f'{OUT}/SW_HitRush_Charge.wav', .6)
finish(whoosh(.14, 500, 1800, 1.), f'{OUT}/SW_HitRush_WhooshLight.wav', .5)
finish(whoosh(.2, 300, 1200, 1.), f'{OUT}/SW_HitRush_WhooshHeavy.wav', .55)
finish(filt(mix(slap(.8, .6), meat(.9, .9), thump(1., 160, 70, .02, .04, .18, .6)), 1800, 'low'),
       f'{OUT}/SW_HitRush_OffBeat.wav', .55)

# ---- Defense responses (same punch family as the attacks, all hits at sample 0) ----
def air_snap(sec, lo, hi, g):
    """Whoosh with its peak AT the start (lands on the beat), falling in pitch."""
    n = noise(sec); out = np.zeros_like(n); k = 6; seg = len(n) // k
    for j in range(k):
        f = hi + (lo - hi) * j / (k - 1)
        out[j * seg:(j + 1) * seg] = filt(n, [f * .6, f * 1.6], 'band')[j * seg:(j + 1) * seg]
    return out * env(sec, sec * .3, .003) * g
# Dodge v2: a clean, smooth "fwoosh" (no crack/slap - those sounded harsh).
# Band-passed noise whose centre glides 3.2k -> 500 Hz by crossfading bands
# (no hard segment switches), a soft pitch-dropping body and a faint tick so
# the dodge still marks the beat.
def glide_air(sec, centres, peak, g):
    n = noise(sec); x = t(sec)
    pos = np.clip(x / (sec * .8), 0, 1) * (len(centres) - 1)
    out = np.zeros_like(n)
    for i, c in enumerate(centres):
        w = np.clip(1. - np.abs(pos - i), 0, 1)          # triangular crossfade
        out += filt(n, [c * .7, c * 1.4], 'band') * w
    shape = (x / peak) * np.exp(1. - x / peak)           # smooth swell, ~peak s
    return out * shape * g
dodge = mix(glide_air(.3, [3200, 2200, 1500, 1000, 700, 500], .045, 1.5),
            np.sin(2 * np.pi * np.cumsum(120 + 200 * np.exp(-t(.16) / .04)) / SR) * env(.16, .045, .006) * .35,
            filt(noise(.004), 4000, 'high') * env(.004, .0008, .0001) * .25)
finish(filt(dodge, 7000, 'low'), f'{OUT}/SW_HitRush_Dodge.wav', .8)
# Block / parry: gloved forearm block - dense low thud, leather slap, no ring.
finish(mix(snap(1.3), slap(.65, 1.9, .012), meat(.75, 1.6, .03, 6.), crunch(.7, .8),
           thump(1., 150, 55, .03, .07, .3, 1.2), rustle(.3, .04)), f'{OUT}/SW_HitRush_Block.wav')
# Counter: the heaviest punch in the set (heavy + sub + extra crack).
finish(mix(punch_heavy(1.02), snap(1.6), slap(1.1, 1.2, .008), sub(1.1, .45, .5), crunch(1.1, .9)),
       f'{OUT}/SW_HitRush_Counter.wav')
# Hold-block repeat hits: a shorter, darker block thud.
finish(filt(mix(slap(.6, 1.2, .01), meat(.7, 1.3, .025, 5.), thump(1., 140, 60, .025, .05, .2, 1.)), 2400, 'low'),
       f'{OUT}/SW_HitRush_HoldHit.wav', .8)
