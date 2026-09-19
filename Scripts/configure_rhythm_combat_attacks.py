import unreal


ATTACKS = {
    "/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_DragonKnight_Combo01_Beat": [18.0, 20.0, 30.0],
    "/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_DragonKnight_Combo02_Beat": [10.0, 12.0, 14.0, 18.0, 28.0],
}


for path, damages in ATTACKS.items():
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError(f"Missing attack data: {path}")
    markers = list(asset.get_editor_property("impact_markers"))
    if len(markers) != len(damages):
        raise RuntimeError(f"{path}: expected {len(damages)} impacts, found {len(markers)}")
    for index, (marker, damage) in enumerate(zip(markers, damages)):
        marker.set_editor_property("damage", damage)
        marker.set_editor_property("ideal_dodge_lead_seconds", 0.14)
        marker.set_editor_property("damage_window_before_seconds", 0.10)
        marker.set_editor_property("damage_window_after_seconds", 0.10)
        unreal.log(
            f"RhythmCombatAttack: asset={asset.get_name()} impact={index + 1} "
            f"damage={damage:.1f} leadMs=140 windowMs=-100/+100"
        )
    asset.set_editor_property("impact_markers", markers)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)

