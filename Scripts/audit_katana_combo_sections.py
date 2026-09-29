"""Read source section lengths and retargeted combo metadata without changing assets."""
import json
from pathlib import Path
import unreal

root = '/Game/GrruzamSwordPack/Animations/Katana_Blade(SK_Mannequin)/2_Attacks/'
paths = {
    'Combo3': (root + '0__3Combos/', 'Katana_Blade_Attack_3Combo_', 3),
    'Combo4': (root + '1__4Combos/', 'Katana_Blade_Attack_4Combo_', 4),
}
result = {}
for name, (folder, prefix, count) in paths.items():
    clips = {}
    for part in ['ALL'] + [str(index) for index in range(1, count + 1)]:
        clip = unreal.load_asset(folder + prefix + part)
        assert clip, folder + prefix + part
        clips[part] = {'length': clip.get_play_length()}
    result[name] = clips

data = unreal.load_asset('/Game/Combat/Player/DA_PlayerAttackSet')
assert data
result['nodes'] = []
for node in data.get_editor_property('nodes'):
    animation = node.get_editor_property('animation')
    result['nodes'].append({
        'id': str(node.get_editor_property('id')),
        'animation': animation.get_path_name() if animation else None,
        'start': node.get_editor_property('clip_start'),
        'strike': node.get_editor_property('strike_start'),
        'impact': node.get_editor_property('impact_time'),
        'end': node.get_editor_property('clip_end'),
        'beats': node.get_editor_property('duration_beats'),
        'next_light': str(node.get_editor_property('next_light')),
        'next_heavy': str(node.get_editor_property('next_heavy')),
    })

output = Path(unreal.Paths.project_dir()) / 'output' / 'katana_combo_sections.json'
output.write_text(json.dumps(result, indent=2), encoding='utf-8')
unreal.log_warning('KATANA_SECTIONS ' + str(output))
