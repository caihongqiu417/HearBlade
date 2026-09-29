import unreal, json
from pathlib import Path

fields = ['attack_montage','clip_start_seconds','clip_end_seconds','authored_impact_time','play_rate','segment_play_rates','reference_bpm','reference_beat_phase_seconds','reference_music_start_seconds','first_impact_beat_in_bar','charge_hold_seconds','charge_pose_animation_seconds']
marker_fields = ['animation_time_seconds','beat_offset','damage','ideal_dodge_lead_seconds','damage_window_before_seconds','damage_window_after_seconds','required_response','hold_marker','hold_group','prompt_lead_beats']
def value(v):
    if isinstance(v,(bool,int,float,str)): return v
    if isinstance(v,unreal.Object): return v.get_path_name()
    if isinstance(v,unreal.Array): return [value(x) for x in v]
    return str(v)
def snapshot(d):
    result={p:value(d.get_editor_property(p)) for p in fields}
    result['markers']=[{p:value(m.get_editor_property(p)) for p in marker_fields} for m in d.get_editor_property('impact_markers')]
    result['patterns']=[p.export_text() for p in d.get_editor_property('response_patterns')]
    return result
report={}
for i in (1,2):
    name='DA_DragonKnight_Combo0%d_Beat'%i
    current=snapshot(unreal.load_asset('/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/'+name))
    baseline=snapshot(unreal.load_asset('/Game/__TimingAudit/'+name))
    diff={key:{'baseline':baseline[key],'current':current[key]} for key in current if current[key]!=baseline[key]}
    report[name]={'baseline':baseline,'current':current,'differences':diff}
    unreal.log_warning('COMBO_TIMING_AUDIT %s differences=%s'%(name,json.dumps(diff,ensure_ascii=False)))
Path(unreal.Paths.project_dir()+'output/arena/combo12_timing_audit.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
