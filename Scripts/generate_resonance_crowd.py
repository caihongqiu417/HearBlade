"""Original synthesized, non-verbal crowd swell. No recordings or external samples."""
import math, random, wave, struct
from pathlib import Path
random.seed(41)
rate=24000; duration=3.2
voices=[(random.uniform(150,360),random.uniform(0,.6),random.random()*math.tau) for _ in range(32)]
samples=[];low=0.
for i in range(int(rate*duration)):
    t=i/rate; low=.92*low+.08*random.uniform(-1,1)
    noise=low*2.5; value=0.
    for f,delay,phase in voices:
        age=t-delay
        if age<0:continue
        env=min(1.,age/.18)*max(0.,1.-age/3.)**1.5
        pitch=f*(1.+.10*math.sin(age*3.))
        value+=env*(math.sin(math.tau*pitch*age+phase)+.25*math.sin(math.tau*pitch*2*age+phase))
    swell=min(1.,t/.16)*max(0.,1-t/duration)
    samples.append((value/32.+noise*.12)*swell)
peak=max(abs(x) for x in samples)
path=Path(__file__).resolve().parents[1]/'SourceArt/Audio/ComboLayers/SW_ResonanceCrowd.wav'
with wave.open(str(path),'wb') as out:
    out.setparams((1,2,rate,0,'NONE','not compressed'))
    out.writeframes(b''.join(struct.pack('<h',int(x/peak*24000)) for x in samples))
print(path)
