"""Enable root extraction on isolated ordinary-attack clips only."""
import unreal
data=unreal.load_asset('/Game/Combat/Player/DA_PlayerAttackSet')
seen=set()
for node in data.get_editor_property('nodes'):
    clip=node.get_editor_property('animation')
    path=clip.get_path_name()
    assert path.startswith('/Game/Combat/Player/Animations/')
    if path in seen: continue
    seen.add(path)
    clip.set_editor_property('enable_root_motion', True)
    clip.set_editor_property('force_root_lock', True)
    assert unreal.EditorAssetLibrary.save_loaded_asset(clip)
unreal.log_warning('PLAYER_ATTACK_ROOT_MOTION_ENABLED clips='+str(len(seen)))
