import unreal

folder = "/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS"
asset_name = "BS_DragonKnight_Walk8"
asset_path = folder + "/" + asset_name
anim_folder = "/Game/Enemies/DF_DRAGON_KNIGHT/DEMO/UE5_ANIMATIONS/"
skeleton = unreal.load_asset("/Game/Enemies/DF_DRAGON_KNIGHT/MESHES/UE5/SK_Mannequin")
if not skeleton:
    raise RuntimeError("Boss skeleton is missing")

factory = unreal.BlendSpaceFactoryNew()
factory.set_editor_property("target_skeleton", skeleton)
asset = unreal.load_asset(asset_path)
if not asset:
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name, folder, unreal.BlendSpace, factory)
if not asset:
    raise RuntimeError("Could not create " + asset_path)

forward = unreal.BlendParameter()
forward.set_editor_property("display_name", "Forward")
forward.set_editor_property("min", -1.0)
forward.set_editor_property("max", 1.0)
forward.set_editor_property("grid_num", 4)
right = unreal.BlendParameter()
right.set_editor_property("display_name", "Right")
right.set_editor_property("min", -1.0)
right.set_editor_property("max", 1.0)
right.set_editor_property("grid_num", 4)
asset.set_editor_property("blend_parameters", [forward, right, unreal.BlendParameter()])

samples = [
    ("move_walk_front", 1.0, 0.0),
    ("move_walk_frontR45", 0.707107, 0.707107),
    ("move_walk_right", 0.0, 1.0),
    ("move_walk_backR45", -0.707107, 0.707107),
    ("move_walk_back", -1.0, 0.0),
    ("move_walk_backL45", -0.707107, -0.707107),
    ("move_walk_left", 0.0, -1.0),
    ("move_walk_frontL45", 0.707107, -0.707107),
    ("AS_MM_Idle", 0.0, 0.0),
]
blend_samples = []
for name, x, y in samples:
    animation = unreal.load_asset(anim_folder + name)
    if not animation:
        raise RuntimeError("Missing walk sample " + name)
    sample = unreal.BlendSample()
    sample.set_editor_property("animation", animation)
    sample.set_editor_property("sample_value", unreal.Vector(x, y, 0.0))
    blend_samples.append(sample)

asset.set_editor_property("sample_data", blend_samples)
triangle_count = unreal.BossBlendSpaceTools.rebuild_blend_space(asset)
if triangle_count <= 0:
    raise RuntimeError("Blend Space triangulation produced no triangles")
unreal.EditorAssetLibrary.save_loaded_asset(asset)
unreal.log("Created {} with {} samples and {} triangles".format(
    asset_path, len(blend_samples), triangle_count))
