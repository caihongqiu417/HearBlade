import unreal, json
from pathlib import Path
paths=['/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Anim_DKM_Attack_01','/Game/InsaneAirCombo/Animation/Idle','/Game/Combat/Player/Animations/combo01_1','/Game/Combat/Player/Animations/combo01_2','/Game/Combat/Player/Animations/combo01_3']
paths += [str(p) for p in unreal.EditorAssetLibrary.list_assets('/Game/Combat/Player/Animations', recursive=False) if str(p) not in paths]
result=[]
for path in paths:
    clip=unreal.load_asset(path)
    if not clip: continue
    row={'path':path,'enable_root_motion':clip.get_editor_property('enable_root_motion'),'force_root_lock':clip.get_editor_property('force_root_lock'),'root_motion_root_lock':str(clip.get_editor_property('root_motion_root_lock')),'frames':[]}
    for time in [0., .15, .3, .5, .7]:
        pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(clip,time,unreal.AnimPoseEvaluationOptions())
        frame={'time':time,'bones':{}}
        for bone in ['root','pelvis','spine_01','spine_02','spine_03','spine_04','spine_05','neck_01','head','upperarm_r','lowerarm_r','hand_r','thigh_r','calf_r']:
            t=unreal.AnimPoseExtensions.get_bone_pose(pose,bone,unreal.AnimPoseSpaces.LOCAL)
            frame['bones'][bone]={'pos':[t.translation.x,t.translation.y,t.translation.z],'scale':[t.scale3d.x,t.scale3d.y,t.scale3d.z]}
        row['frames'].append(frame)
        if path.startswith('/Game/Combat/Player/Animations/'):
            reference=clip.get_skeleton().get_reference_pose()
            for bone in frame['bones']:
                if bone in ('root','pelvis'): continue
                actual=pose.get_bone_pose(bone,unreal.AnimPoseSpaces.LOCAL)
                expected=reference.get_bone_pose(bone,unreal.AnimPoseSpaces.LOCAL)
                assert (actual.translation-expected.translation).length()<.02, (path,time,bone,'bone length mismatch')
    result.append(row)
Path(unreal.Paths.project_dir(),'output/arena/player_pose_audit.json').write_text(json.dumps(result,indent=2))
unreal.log_warning('PLAYER_POSE_PROPORTIONS_OK clips='+str(len(result)))
