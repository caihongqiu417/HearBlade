import unreal,json
from pathlib import Path
source=unreal.load_asset('/Game/SwordAnim/Animation/UE5/Taunt')
target=unreal.load_asset('/Game/Enemies/DF_DRAGON_KNIGHT/MESHES/UE5/SK_Mannequin')
src=source.get_skeleton()
ref=src.get_reference_pose(); dst=target.get_reference_pose()
names=list(ref.get_bone_names()); dstnames=list(dst.get_bone_names())
assert names==dstnames, 'Bone hierarchy differs; needs actual retargeting'
out={'bones':len(names),'mismatches':[]}
for n in names:
    a=ref.get_bone_pose(n,unreal.AnimPoseSpaces.LOCAL)
    b=dst.get_bone_pose(n,unreal.AnimPoseSpaces.LOCAL)
    if (a.translation-b.translation).length()>.01 or min(sum((getattr(a.rotation,k)-getattr(b.rotation,k))**2 for k in ('x','y','z','w')),sum((getattr(a.rotation,k)+getattr(b.rotation,k))**2 for k in ('x','y','z','w')))>.0001:
        out['mismatches'].append(str(n))
assert not out['mismatches'], str(out)
path='/Game/Enemies/DF_DRAGON_KNIGHT/Anim/Ascension_Taunt'
clip=unreal.load_asset(path) or unreal.EditorAssetLibrary.duplicate_asset(source.get_path_name(),path)
assert unreal.BossBlendSpaceTools.assign_identical_skeleton(clip,target), 'Reference skeleton verification failed'
clip.set_editor_property('enable_root_motion',True)
clip.set_editor_property('force_root_lock',True)
unreal.EditorAssetLibrary.save_loaded_asset(clip)
out['result']=clip.get_path_name()
Path('D:/Udev/HearBlade/output/arena/ascension_animation_verified.json').write_text(json.dumps(out,indent=2))
unreal.log_warning('ASCENSION_ANIMATION_VERIFIED '+str(out))
