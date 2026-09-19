import unreal
folder='/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations'
name='BS_DKM_CombatLocomotion'
path=folder+'/'+name
skeleton=unreal.load_asset('/Game/Characters/Dark_Knight/Dark_Knight_Male/Meshes/SK_DKM_Full')
if not skeleton: raise RuntimeError('Dark Knight skeleton missing')
asset=unreal.load_asset(path)
if not asset:
    factory=unreal.BlendSpaceFactoryNew(); factory.set_editor_property('target_skeleton',skeleton)
    asset=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,unreal.BlendSpace,factory)
if not asset: raise RuntimeError('Could not create '+path)

def parameter(label):
    p=unreal.BlendParameter(); p.set_editor_property('display_name',label); p.set_editor_property('min',-1.0); p.set_editor_property('max',1.0); p.set_editor_property('grid_num',8)
    try:p.set_editor_property('interpolation_time',0.12)
    except Exception:pass
    return p
asset.set_editor_property('blend_parameters',[parameter('Forward'),parameter('Right'),unreal.BlendParameter()])
try: asset.set_editor_property('target_weight_interpolation_speed_per_sec',8.0)
except Exception: pass
root=folder+'/'
def load(anim):
    a=unreal.load_asset(root+anim)
    if not a: raise RuntimeError('Missing '+anim)
    return a
# Direction coordinates: +X forward, +Y right. Walk occupies the inner ring;
# run occupies the outer ring. The source pack has forward hemisphere runs only,
# so side/back outer samples deliberately retain their matching walks.
r=.38; d=.70710678
walk=[
 ('Anim_DKM_Walk_Alert_Fwd',1,0),('Anim_DKM_Walk_Alert_FwdRight',d,d),
 ('Anim_DKM_Walk_Alert_Right',0,1),('Anim_DKM_Walk_Alert_BwdRight',-d,d),
 ('Anim_DKM_Walk_Alert_Bwd',-1,0),('Anim_DKM_Walk_Alert_BwdLeft',-d,-d),
 ('Anim_DKM_Walk_Alert_Left',0,-1),('Anim_DKM_Walk_Alert_FwdLeft',d,-d)]
outer=[
 ('Anim_DKM_Run_Alert_Fwd',1,0),('Anim_DKM_Run_Alert_FwdRight',d,d),
 ('Anim_DKM_Walk_Alert_Right',0,1),('Anim_DKM_Walk_Alert_BwdRight',-d,d),
 ('Anim_DKM_Walk_Alert_Bwd',-1,0),('Anim_DKM_Walk_Alert_BwdLeft',-d,-d),
 ('Anim_DKM_Walk_Alert_Left',0,-1),('Anim_DKM_Run_Alert_FwdLeft',d,-d)]
samples=[]
def add(anim,x,y):
    s=unreal.BlendSample(); s.set_editor_property('animation',load(anim)); s.set_editor_property('sample_value',unreal.Vector(x,y,0)); samples.append(s)
add('Anim_DKM_Idle_Alert',0,0)
for anim,x,y in walk:add(anim,x*r,y*r)
for anim,x,y in outer:add(anim,x,y)
asset.set_editor_property('sample_data',samples)
triangles=unreal.BossBlendSpaceTools.rebuild_blend_space(asset)
if triangles<=0: raise RuntimeError('Blend Space triangulation failed')
unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False)
unreal.log_warning('PLAYER_BLENDSPACE created {} samples={} triangles={}'.format(path,len(samples),triangles))

