"""Find how split combo clips map onto the authored continuous combo timelines."""
import json
from pathlib import Path
import unreal

ROOT='/Game/InsaneAirCombo/Animation/'
NAMES=['combo01','combo01_1','combo01_2','combo01_3','combo02','combo02_1','combo02_2','combo02_3']
BONES=('pelvis','spine_01','spine_03','hand_r','hand_l','foot_r','foot_l')
opts=unreal.AnimPoseEvaluationOptions()

def sample(clip, frame):
    t=min(frame/30.0,clip.get_play_length())
    pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(clip,t,opts)
    root=unreal.AnimPoseExtensions.get_bone_pose(pose,'root',unreal.AnimPoseSpaces.WORLD)
    result=[]
    for bone in BONES:
        tr=unreal.AnimPoseExtensions.get_bone_pose(pose,bone,unreal.AnimPoseSpaces.WORLD)
        p=root.inverse_transform_location(tr.translation)
        result.extend((p.x,p.y,p.z))
    return result

clips={n:unreal.load_asset(ROOT+n) for n in NAMES}
assert all(clips.values())
result={}
for n,c in clips.items():
    count=round(c.get_play_length()*30)+1
    result[n]={'length':c.get_play_length(),'frames':count,
               'root_motion':c.get_editor_property('enable_root_motion'),
               'samples':[sample(c,f) for f in range(count)]}

for piece in ('combo01_1','combo01_2','combo01_3'):
    whole=result['combo01']['samples']; part=result[piece]['samples']
    matches=[]
    for pf,v in enumerate(part):
        best=min(range(len(whole)),key=lambda wf:sum((a-b)*(a-b) for a,b in zip(v,whole[wf])))
        err=(sum((a-b)*(a-b) for a,b in zip(v,whole[best]))/len(v))**.5
        matches.append([pf,best,err])
    result[piece]['whole_matches']=matches
for piece in ('combo02_1','combo02_2','combo02_3'):
    whole=result['combo02']['samples']; part=result[piece]['samples']
    matches=[]
    for pf,v in enumerate(part):
        best=min(range(len(whole)),key=lambda wf:sum((a-b)*(a-b) for a,b in zip(v,whole[wf])))
        err=(sum((a-b)*(a-b) for a,b in zip(v,whole[best]))/len(v))**.5
        matches.append([pf,best,err])
    result[piece]['whole_matches']=matches

out=Path(unreal.Paths.project_dir(),'output/arena/full_combo_audit.json')
out.write_text(json.dumps(result),encoding='utf-8')
unreal.log_warning('FULL_COMBO_AUDIT_OK '+str(out))
