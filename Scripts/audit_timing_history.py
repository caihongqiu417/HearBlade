import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())
result={}
for path in (root/'Content/__TimingHistory').rglob('*.uasset'):
    asset=unreal.load_asset('/Game/'+str(path.relative_to(root/'Content').with_suffix('')).replace('\\','/'))
    if not asset:continue
    result[str(path.relative_to(root/'Content'))]={
      'rates':list(asset.get_editor_property('segment_play_rates')),
      'play_rate':asset.get_editor_property('play_rate'),
      'markers':[[m.get_editor_property('animation_time_seconds'),m.get_editor_property('beat_offset')] for m in asset.get_editor_property('impact_markers')]}
(root/'output/arena/timing_history.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
unreal.log_warning('TIMING_HISTORY '+json.dumps(result))
