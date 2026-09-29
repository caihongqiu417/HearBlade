"""Encode timestamped gameplay frames plus the recorded engine master mix."""
import csv, pathlib, subprocess, sys
folder=pathlib.Path(sys.argv[1]).resolve()
output=pathlib.Path(sys.argv[2]).resolve()
rows=list(csv.reader((folder/'frames.csv').open(encoding='utf-8-sig')))
assert len(rows)>10 and (folder/'game_audio.wav').is_file()
lines=['ffconcat version 1.0']
for i,(name,time) in enumerate(rows):
    assert pathlib.Path(name).name==name and (folder/name).is_file()
    duration=float(rows[i+1][1])-float(time) if i+1<len(rows) else 1/30
    assert duration>0
    lines += ["file '"+name+"'",f'duration {duration:.6f}']
lines += ["file '"+rows[-1][0]+"'"]
manifest=folder/'video.ffconcat';manifest.write_text('\n'.join(lines)+'\n',encoding='utf-8')
output.parent.mkdir(parents=True,exist_ok=True)
subprocess.run(['ffmpeg','-y','-loglevel','warning','-f','concat','-safe','0','-i',str(manifest),
                '-ss',rows[0][1],'-i',str(folder/'game_audio.wav'),'-map','0:v:0','-map','1:a:0',
                '-c:v','libx264','-preset','fast','-crf','20','-pix_fmt','yuv420p','-r','30','-fps_mode','cfr',
                '-c:a','aac','-b:a','192k','-shortest','-movflags','+faststart',str(output)],check=True)
print(f'Encoded {len(rows)} timestamped frames: {output}')
