import unreal

for name in ("roll_front", "roll_back", "roll_left", "roll_right"):
    asset = unreal.load_asset(f"/Game/SwordAnim/Animation/UE5/{name}")
    if not asset:
        raise RuntimeError(f"Missing roll animation: {name}")
    asset.set_editor_property("enable_root_motion", True)
    asset.set_editor_property("force_root_lock", True)
    asset.set_editor_property("root_motion_root_lock", unreal.RootMotionRootLock.ANIM_FIRST_FRAME)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    unreal.log(f"RhythmDodge: enabled root motion for {name}")
