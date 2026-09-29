"""Create normalized playback copies of the authored full combo timelines."""
import unreal

source='/Game/InsaneAirCombo/Animation/'
dest='/Game/Combat/Player/Animations/'
for name in ('combo01','combo02'):
    src=unreal.load_asset(source+name)
    assert src
    path=dest+name+'_continuous'
    clip=unreal.load_asset(path) or unreal.EditorAssetLibrary.duplicate_asset(src.get_path_name(),path)
    assert clip and unreal.BossBlendSpaceTools.normalize_player_attack_bone_lengths(clip)
    clip.set_editor_property('enable_root_motion',True)
    clip.set_editor_property('force_root_lock',True)
    assert unreal.EditorAssetLibrary.save_loaded_asset(clip)
unreal.log_warning('CONTINUOUS_PLAYER_COMBOS_CREATED')
