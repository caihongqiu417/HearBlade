"""Restore pre-phase-two authored timing only, retaining recognition and response data."""
import unreal,json,datetime,shutil
from pathlib import Path
root=Path(unreal.Paths.project_dir())
backup=root/'Saved/Backups'/('ComboTimingRestore_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S'))
backup.mkdir(parents=True)
fields=['attack_montage','clip_start_seconds','clip_end_seconds','authored_impact_time','play_rate','segment_play_rates','reference_bpm','reference_beat_phase_seconds','reference_music_start_seconds','first_impact_beat_in_bar','minimum_play_rate','maximum_play_rate']
report={}
for i in [1,2]:
    name=f'DA_DragonKnight_Combo0{i}_Beat'
    target='/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/'+name
    src=unreal.load_asset('/Game/__TimingHistory/9a85720/'+name)
    dst=unreal.load_asset(target)
    assert src and dst
    old=list(src.get_editor_property('impact_markers'));markers=list(dst.get_editor_property('impact_markers'))
    assert len(old)==len(markers), 'Marker count changed; review before replacing'
    shutil.copy2(root/('Content/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/'+name+'.uasset'),backup/(name+'.uasset'))
    before=list(dst.get_editor_property('segment_play_rates'))
    for field in fields:dst.set_editor_property(field,src.get_editor_property(field))
    for a,b in zip(markers,old):
        for field in ['animation_time_seconds','beat_offset']:a.set_editor_property(field,b.get_editor_property(field))
    dst.set_editor_property('impact_markers',markers)
    assert unreal.EditorAssetLibrary.save_loaded_asset(dst)
    report[name]={'before_rates':before,'restored_rates':list(dst.get_editor_property('segment_play_rates')),'backup':str(backup),'reference':'9a85720'}
(root/'output/arena/restored_combo12_timing.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log_warning('COMBO12_TIMING_RESTORED '+json.dumps(report))
