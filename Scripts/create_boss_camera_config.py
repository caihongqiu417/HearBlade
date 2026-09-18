import unreal

asset_path = "/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_BossCamera"
asset = unreal.load_asset(asset_path)
if not asset:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.BossCameraConfig)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "DA_BossCamera", "/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS",
        unreal.BossCameraConfig, factory)
if not asset:
    raise RuntimeError("Failed to create Boss camera settings asset")
asset.set_editor_property("combat_arm_length_at_close_range", 440.0)
asset.set_editor_property("close_range_fov", 90.0)
unreal.EditorAssetLibrary.save_loaded_asset(asset)
unreal.log("Boss camera settings: " + asset_path)
