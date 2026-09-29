"""Author Combo03 without changing Combo01/02 timing or response patterns."""
import unreal, math, json
from pathlib import Path

ROOT='/Game/Enemies/DF_DRAGON_KNIGHT'
tools=unreal.AssetToolsHelpers.get_asset_tools()
source=unreal.load_asset('/Game/SwordAnim/Animation/UE5/Combo03')
assert source
frames=unreal.AnimationLibrary.get_num_frames(source)
length=unreal.AnimationLibrary.get_sequence_length(source)
fps=frames/length
assert abs(fps-30)<.01 and frames>=175, (fps,frames)
motion_path=ROOT+'/Montage/Combo03_BeatRootMotion'
motion=unreal.load_asset(motion_path) or unreal.EditorAssetLibrary.duplicate_asset(source.get_path_name(),motion_path)
motion.set_editor_property('enable_root_motion',True)
motion.set_editor_property('force_root_lock',True)
unreal.EditorAssetLibrary.save_loaded_asset(motion)
montage=unreal.load_asset(ROOT+'/Montage/Combo03_BeatMontage')
if not montage:
    factory=unreal.AnimMontageFactory();factory.set_editor_property('target_skeleton',motion.get_editor_property('skeleton'));factory.set_editor_property('source_animation',motion)
    montage=tools.create_asset('Combo03_BeatMontage',ROOT+'/Montage',unreal.AnimMontage,factory)
assert montage
unreal.EditorAssetLibrary.save_loaded_asset(montage)

data=unreal.load_asset(ROOT+'/BLUEPRINTS/DA_DragonKnight_Combo03_Beat')
if not data:
    factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.AttackDataAsset)
    data=tools.create_asset('DA_DragonKnight_Combo03_Beat',ROOT+'/BLUEPRINTS',unreal.AttackDataAsset,factory)
data.set_editor_property('attack_montage',montage)
data.set_editor_property('skill_name','焚场四连')
data.set_editor_property('skill_description','普通斩、普通斩、大横斩、蓄力落地范围爆发。')
data.set_editor_property('minimum_boss_phase',2)
data.set_editor_property('source_frame_rate',fps)
data.set_editor_property('charge_pose_source_frame',172)
data.set_editor_property('charge_pose_animation_seconds',172/fps)
data.set_editor_property('minimum_play_rate',1.)
data.set_editor_property('maximum_play_rate',1.5)
beat=60/104
times=[f/fps for f in [33,70,94,175]]
offsets=[0.,2.,3.,8.]
rates=[1.,(times[1]-times[0])/(2*beat),(times[2]-times[1])/beat,1.,1.]
data.set_editor_property('segment_play_rates',rates)
data.set_editor_property('charge_hold_seconds',max(0,5*beat-(times[3]-times[2])))
markers=[]
for i,(frame,name,response,shape) in enumerate([
 (33,'普通斩一',unreal.RhythmResponseType.BLOCK,unreal.AttackRecognitionShape.SHORT_GLINT),
 (70,'普通斩二',unreal.RhythmResponseType.BLOCK,unreal.AttackRecognitionShape.SHORT_GLINT),
 (94,'大横斩',unreal.RhythmResponseType.DODGE,unreal.AttackRecognitionShape.WIDE_ARC),
 (175,'焚场落地',unreal.RhythmResponseType.DODGE,unreal.AttackRecognitionShape.GROUND_WARNING)]):
    m=unreal.AttackImpactMarker();m.set_editor_property('animation_time_seconds',frame/fps);m.set_editor_property('source_frame',frame)
    m.set_editor_property('beat_offset',offsets[i]);m.set_editor_property('required_response',response)
    recognition=unreal.AttackRecognitionInfo();recognition.set_editor_property('action_name',name);recognition.set_editor_property('shape',shape)
    recognition.set_editor_property('lead_beats',1.);m.set_editor_property('recognition',recognition)
    if i==3:
        m.set_editor_property('damage_shape',unreal.BossAttackDamageShape.GROUND_BURST);m.set_editor_property('damage',25.)
        m.set_editor_property('ideal_dodge_lead_seconds',0.)
        area=m.get_editor_property('ground_burst');area.set_editor_property('radius_cm',600.)
        m.set_editor_property('ground_burst',area)
    markers.append(m)
data.set_editor_property('impact_markers',markers)
data.set_editor_property('authored_impact_time',times[0])
unreal.EditorAssetLibrary.save_loaded_asset(data)

def original_signature(d):
    return {'rates':list(d.get_editor_property('segment_play_rates')),'markers':[
        [m.get_editor_property(p) if p in ['animation_time_seconds','beat_offset'] else str(m.get_editor_property(p)) for p in ['animation_time_seconds','beat_offset','required_response','hold_marker','hold_group']]
        for m in d.get_editor_property('impact_markers')], 'patterns':str(d.get_editor_property('response_patterns'))}
for name in ['Combo01','Combo02']:
    d=unreal.load_asset(ROOT+'/BLUEPRINTS/DA_DragonKnight_'+name+'_Beat');assert d
    before=original_signature(d)
    items=list(d.get_editor_property('impact_markers'))
    for i,m in enumerate(items):
        info=unreal.AttackRecognitionInfo();info.set_editor_property('action_name',name+' 第'+str(i+1)+'击')
        info.set_editor_property('description','提示跟随本次实际指令，保留已有随机变化。')
        m.set_editor_property('recognition',info)
    d.set_editor_property('impact_markers',items);d.set_editor_property('skill_name',name)
    assert original_signature(d)==before, 'Existing combat config changed!'
    unreal.EditorAssetLibrary.save_loaded_asset(d)
unreal.log_warning('COMBO03_AUTHORED fps=%.3f impacts=33/70/94/175 pose=172 offsets=0/2/3/8 hold=%.3f'%(fps,data.get_editor_property('charge_hold_seconds')))
