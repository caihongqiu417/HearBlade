import unreal

for name in ("roll_front", "roll_back", "roll_left", "roll_right"):
    asset = unreal.load_asset(f"/Game/SwordAnim/Animation/UE5/{name}")
    if not asset:
        unreal.log_warning(f"RollInspect: missing {name}")
        continue
    values = []
    for prop in ("enable_root_motion", "force_root_lock", "root_motion_root_lock"):
        try:
            values.append(f"{prop}={asset.get_editor_property(prop)}")
        except Exception as error:
            values.append(f"{prop}=<{error}>")
    unreal.log(f"RollInspect: {name} " + " ".join(values))
