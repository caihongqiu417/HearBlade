import unreal, json
from pathlib import Path
d=unreal.load_asset('/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_DragonKnight_Combo03_Beat')
r={k:d.get_editor_property(k) for k in ['play_rate','charge_hold_seconds','charge_pose_animation_seconds']}
r['rates']=list(d.get_editor_property('segment_play_rates'))
r['markers']=[{'time':m.get_editor_property('animation_time_seconds'),'beat':m.get_editor_property('beat_offset'),'radius':m.get_editor_property('ground_burst').get_editor_property('radius_cm'),'warning':m.get_editor_property('ground_burst').get_editor_property('warning_beats')} for m in d.get_editor_property('impact_markers')]
Path('D:/Udev/HearBlade/output/arena/combo03_current.json').write_text(json.dumps(r,indent=2))
