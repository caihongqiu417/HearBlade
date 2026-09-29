"""Create isolated gameplay clips from already-retargeted InsaneAirCombo sources."""
import unreal, json
from pathlib import Path
ROOT='/Game/Combat/Player'
SOURCE='/Game/InsaneAirCombo/Animation/'
tools=unreal.AssetToolsHelpers.get_asset_tools()
target=unreal.load_asset('/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Anim_DKM_Attack_01').get_skeleton()
exec(Path(unreal.Paths.project_dir(),'Scripts/create_continuous_player_combos.py').read_text(encoding='utf-8'))
# Source frames preserve the real windup / sweep / recovery instead of scaling entire clips.
# id,source,start,impact,end,input,damage,poise,advance,finisher,resonance,nextL,nextH
specs=[
 ('L','combo01_1',3,8,12,'LIGHT',16,10,35,False,False,'LL','LH'),
 ('LL','combo01_2',7,12,16,'LIGHT',18,12,30,False,False,'LLL','LH'),
 ('LLL','combo01_3',12,17,21,'LIGHT',24,18,35,True,False,'LLLL',''),
 ('H','combo02_1',3,12,17,'HEAVY',28,26,55,False,False,'HL','HH'),
 ('HH','combo02_2',8,18,23,'HEAVY',42,40,50,True,False,'',''),
 ('LH','Upper_Swing',2,9,14,'HEAVY',38,35,45,True,False,'LHL',''),
 ('LLLL','combo02_2',13,18,22,'LIGHT',24,20,35,False,True,'','LLLLH'),
 ('LLLLH','Attack_Shockwave_2',3,11,16,'HEAVY',55,60,60,True,True,'',''),
 ('LHL','combo01_2',7,12,16,'LIGHT',26,20,50,False,True,'','LHLH'),
 ('LHLH','combo02_3',15,23,28,'HEAVY',60,65,65,True,True,'',''),
 ('HL','combo01_1',3,8,12,'LIGHT',22,18,40,False,False,'HLL',''),
 ('HLL','combo01_2',7,12,16,'LIGHT',24,20,40,False,False,'','HLLH'),
 ('HLLH','Attack_Shockwave_1',8,18,23,'HEAVY',65,70,50,True,False,'','')]
nodes=[]
for ident,source,start,impact,end,kind,damage,poise,advance,finish,resonance,nl,nh in specs:
    src=unreal.load_asset(SOURCE+source)
    assert src and src.get_skeleton()==target, 'Expected already-retargeted player skeleton: '+source
    path=ROOT+'/Animations/'+source
    clip=unreal.load_asset(path) or unreal.EditorAssetLibrary.duplicate_asset(src.get_path_name(),path)
    clip.set_editor_property('force_root_lock',True)
    clip.set_editor_property('enable_root_motion',True)
    assert unreal.BossBlendSpaceTools.normalize_player_attack_bone_lengths(clip)
    unreal.EditorAssetLibrary.save_loaded_asset(clip)
    n=unreal.PlayerAttackNode()
    for key,value in dict(id=ident,input=getattr(unreal.PlayerAttackInput,kind),animation=clip,clip_start=start/30.,impact_time=impact/30.,clip_end=end/30.,duration_beats=.5 if kind=='LIGHT' else 1.,damage=float(damage),poise_damage=float(poise),advance_cm=float(advance),finisher=finish,resonance_only=resonance,next_light=nl,next_heavy=nh).items():
        n.set_editor_property(key,value)
    nodes.append(n)
data=unreal.load_asset(ROOT+'/DA_PlayerAttackSet')
if not data:
    factory=unreal.DataAssetFactory(); factory.set_editor_property('data_asset_class',unreal.PlayerAttackSet)
    data=tools.create_asset('DA_PlayerAttackSet',ROOT,unreal.DataAsset,factory)
data.set_editor_property('nodes',nodes)
assert unreal.EditorAssetLibrary.save_loaded_asset(data)
# Apply the calibrated phase landmarks after recreating the base graph.
exec(Path(unreal.Paths.project_dir(),'Scripts/update_player_combo_rules.py').read_text(encoding='utf-8'))
wave=Path(unreal.Paths.project_dir(),'SourceArt/Audio/ComboLayers/SW_ResonanceCrowd.wav')
if wave.exists():
    task=unreal.AssetImportTask();task.set_editor_property('filename',str(wave));task.set_editor_property('destination_path','/Game/Audio/SFX');task.set_editor_property('automated',True);task.set_editor_property('replace_existing',True);task.set_editor_property('save',True)
    tools.import_asset_tasks([task])
unreal.log_warning('PLAYER_ATTACK_ASSETS_OK nodes='+str(len(nodes)))
