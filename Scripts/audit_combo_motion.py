import unreal, json
from pathlib import Path
data=unreal.load_asset('/Game/Combat/Player/DA_PlayerAttackSet')
result={}
for node in data.get_editor_property('nodes'):
    clip=node.get_editor_property('animation')
    name=clip.get_name()
    if name in result: continue
    length=clip.get_play_length()
    poses=[]
    for frame in range(int(length*30)+1):
        pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(clip,frame/30.,unreal.AnimPoseEvaluationOptions())
        bones={}
        for bone in ('root','pelvis','spine_03','hand_r','hand_l','foot_r','foot_l'):
            tr=unreal.AnimPoseExtensions.get_bone_pose(pose,bone,unreal.AnimPoseSpaces.WORLD)
            p=tr.translation
            bones[bone]=[p.x,p.y,p.z]
        poses.append(bones)
    result[name]={'length':length,'poses':poses}
Path(unreal.Paths.project_dir(),'output/arena/combo_motion.json').write_text(json.dumps(result),encoding='utf-8')
unreal.log_warning('COMBO_MOTION_AUDIT_OK')
