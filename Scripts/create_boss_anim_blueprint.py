import unreal

package = "/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS"
name = "ABP_DragonKnight_Movement"
path = package + "/" + name
skeleton_path = "/Game/Enemies/DF_DRAGON_KNIGHT/MESHES/UE5/SK_Mannequin"

if unreal.EditorAssetLibrary.does_asset_exist(path):
    unreal.log("Boss AnimBlueprint already exists: " + path)
    raise SystemExit(0)

factory = unreal.AnimBlueprintFactory()
factory.set_editor_property("target_skeleton", unreal.load_asset(skeleton_path))
factory.set_editor_property("parent_class", unreal.BossLocomotionAnimInstance)
asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, package, unreal.AnimBlueprint, factory)
if not asset:
    raise RuntimeError("Failed to create " + path)
unreal.EditorAssetLibrary.save_loaded_asset(asset)
unreal.log("Created Boss AnimBlueprint: " + path)
