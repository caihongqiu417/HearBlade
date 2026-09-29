import unreal,json
from pathlib import Path
report=json.loads(Path(unreal.Paths.project_dir()+'SourceArt/Audio/MusicEmphasis/AintItFun_emphasis.json').read_text(encoding='utf-8'))
path='/Game/Audio/Music/DA_AintItFun_Emphasis'
data=unreal.load_asset(path)
if not data:
    factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.MusicEmphasisData)
    data=unreal.AssetToolsHelpers.get_asset_tools().create_asset('DA_AintItFun_Emphasis','/Game/Audio/Music',unreal.MusicEmphasisData,factory)
data.set_editor_property('music',unreal.load_asset('/Game/Audio/Music/SW_AintItFun'))
points=[]
for row in report['points']:
    p=unreal.MusicEmphasisPoint()
    for key in ['source_seconds','strength','reason']:p.set_editor_property(key,row[key])
    points.append(p)
data.set_editor_property('points',points)
assert unreal.EditorAssetLibrary.save_loaded_asset(data)
unreal.log_warning('MUSIC_EMPHASIS_SAVED candidates=%d'%len(points))
