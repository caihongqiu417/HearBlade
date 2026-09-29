"""Local accent candidates from the actual game mix. No chorus labels or fake vocal stems.

Center-channel midrange rise is only a vocal-prominence proxy: guitars can trigger it.
All detected timestamps are retained unsnapped; runtime decides beat compatibility.
"""
import json, subprocess, hashlib
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
source=ROOT/'Music/Paramore_AintItFun_UE.wav'
raw=subprocess.check_output(['ffmpeg','-v','error','-i',str(source),'-ar','16000','-ac','2','-f','f32le','pipe:1'])
stereo=np.frombuffer(raw,dtype='<f4').reshape(-1,2)
center=stereo.mean(axis=1)
sr=16000;n=2048;hop=160
frames=np.lib.stride_tricks.sliding_window_view(center,n)[::hop]
power=np.abs(np.fft.rfft(frames*np.hanning(n),axis=1)).astype(np.float32)
freq=np.fft.rfftfreq(n,1/sr)
times=(np.arange(len(frames))*hop+n/2)/sr
def smooth(x,width):return np.convolve(x,np.ones(width)/width,mode='same')
def norm(x):
    lo,hi=np.percentile(x,[20,98])
    return np.clip((x-lo)/max(hi-lo,1e-8),0,1)
energy=np.sqrt(np.mean(frames**2,axis=1))
mid=np.sqrt(np.mean(power[:,(freq>=250)&(freq<=3000)]**2,axis=1))
bass=np.sqrt(np.mean(power[:,(freq>=45)&(freq<=180)]**2,axis=1))
flux=np.r_[0,np.maximum(np.diff(np.log1p(power),axis=0),0).mean(axis=1)]
# Positive local contrast, rather than absolute loudness across the whole chorus.
rise=np.maximum(smooth(mid,7)-smooth(mid,151),0)
contrast=np.maximum(smooth(energy,7)-smooth(energy,101),0)
score=.38*norm(rise)+.32*norm(smooth(flux,3))+.20*norm(contrast)+.10*norm(bass)
score=smooth(score,3)
eligible=[i for i in range(2,len(score)-2) if score[i]>=.55 and score[i]>=max(score[i-2:i+3]) and energy[i]>.012]
selected=[]
for i in sorted(eligible,key=lambda i:score[i],reverse=True):
    if all(abs(times[i]-times[j])>=1.25 for j in selected):selected.append(i)
points=[]
for i in sorted(selected):
    points.append({'source_seconds':round(float(times[i]),3),'strength':round(float(score[i]),4),
      'reason':'center-midrange rise + attack' if norm(rise)[i]>.5 else 'mix transient + local contrast',
      'vocal_verified':False})
out=ROOT/'SourceArt/Audio/MusicEmphasis';out.mkdir(parents=True,exist_ok=True)
report={'source':str(source),'sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
 'duration_seconds':len(center)/sr,'method':'local contrast + spectral attack + center-channel midrange rise',
 'limitations':'Mixed-audio candidates, not isolated vocals or verified high-note detection. Times are source-file seconds.',
 'points':points}
(out/'AintItFun_emphasis.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('Candidates:',len(points),'strongest:',sorted(points,key=lambda p:p['strength'],reverse=True)[:8])
