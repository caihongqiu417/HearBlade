"""Read-only source animation audit, including candidate contact samples."""
import unreal, json
from pathlib import Path
root='/Game/InsaneAirCombo/Animation/'
names=['combo01_1','combo01_2','combo01_3','combo02_1','combo02_2','combo02_3','Upper_Swing','Attack_Shockwave','Attack_Shockwave_1','Attack_Shockwave_2','Air_Spin_Attack_Z0','Air_Dash_Attack_Z0']
target=unreal.load_asset('/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Anim_DKM_Attack_01').get_skeleton()
out={'target':target.get_path_name(),'animations':[]}
unreal.log_warning('POSE_API '+str(unreal.AnimPoseExtensions.get_anim_pose_at_time.__doc__))
unreal.log_warning('POSE_OPTIONS '+str(unreal.AnimPoseEvaluationOptions.__doc__))
for name in names:
    clip=unreal.load_asset(root+name)
    if not clip: raise RuntimeError(name)
    row={'name':name,'skeleton':clip.get_skeleton().get_path_name(),'length':clip.get_play_length(),'frames':unreal.AnimationLibrary.get_num_frames(clip),'root_motion':clip.get_editor_property('enable_root_motion')}
    row['same_skeleton']=clip.get_skeleton()==target
    row['compatible']=str(clip.get_skeleton().get_editor_property('compatible_skeletons'))
    samples=[]
    for frame in range(row['frames']+1):
        pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(clip,frame/30.,unreal.AnimPoseEvaluationOptions())
        hand=unreal.AnimPoseExtensions.get_bone_pose(pose,'hand_r',unreal.AnimPoseSpaces.WORLD)
        samples.append({'frame':frame,'hand':[hand.translation.x,hand.translation.y,hand.translation.z]})
    row['samples']=samples
    out['animations'].append(row)
Path(unreal.Paths.project_dir(),'output/arena/player_attack_audit.json').write_text(json.dumps(out,indent=2))
unreal.log_warning('PLAYER_ATTACK_AUDIT '+json.dumps(out))
