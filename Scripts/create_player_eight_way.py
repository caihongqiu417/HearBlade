import math
import unreal

root='/Game/Combat/Player'
source='/Game/InsaneAirCombo/Animation/'
idle=unreal.load_asset('/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Anim_DKM_Idle_Alert')
skeleton=idle.get_skeleton()
clips={}
for name in ['strafe_'+g+'_'+d for g in ('walk','run') for d in ('F','R45','R','R135','B','L135','L','L45')]+['run_fast']:
    src=unreal.load_asset(source+name)
    assert src and src.get_skeleton()==skeleton, name+' must already use player skeleton'
    path=root+'/Animations/Locomotion/'+name
    clip=unreal.load_asset(path) or unreal.EditorAssetLibrary.duplicate_asset(src.get_path_name(),path)
    clip.set_editor_property('enable_root_motion',False)
    clip.set_editor_property('force_root_lock',True)
    assert unreal.BossBlendSpaceTools.normalize_player_attack_bone_lengths(clip)
    unreal.EditorAssetLibrary.save_loaded_asset(clip)
    clips[name]=clip
asset=unreal.load_asset(root+'/BS_PlayerEightWay')
if not asset:
    factory=unreal.BlendSpaceFactoryNew()
    factory.set_editor_property('target_skeleton',skeleton)
    asset=unreal.AssetToolsHelpers.get_asset_tools().create_asset('BS_PlayerEightWay',root,unreal.BlendSpace,factory)
params=[]
for label in ('Forward','Right'):
    p=unreal.BlendParameter()
    for k,v in dict(display_name=label,min=-1.5,max=1.5,grid_num=12).items(): p.set_editor_property(k,v)
    params.append(p)
asset.set_editor_property('blend_parameters',params+[unreal.BlendParameter()])
asset.set_editor_property('target_weight_interpolation_speed_per_sec',8.0)
samples=[]
def add(clip,x,y):
    s=unreal.BlendSample()
    s.set_editor_property('animation',clip)
    s.set_editor_property('sample_value',unreal.Vector(x,y,0))
    samples.append(s)
add(idle,0,0)
for i,d in enumerate(('F','R45','R','R135','B','L135','L','L45')):
    x,y=math.cos(i*math.pi/4),math.sin(i*math.pi/4)
    add(clips['strafe_walk_'+d],x*.4,y*.4)
    add(clips['strafe_run_'+d],x,y)
    add(clips['run_fast' if d=='F' else 'strafe_run_'+d],x*1.3,y*1.3)
asset.set_editor_property('sample_data',samples)
assert unreal.BossBlendSpaceTools.rebuild_blend_space(asset)>0
assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
unreal.log_warning('PLAYER_EIGHT_WAY_OK samples='+str(len(samples)))
