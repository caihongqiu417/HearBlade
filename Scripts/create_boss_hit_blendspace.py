import unreal

folder = "/Game/Enemies/DF_DRAGON_KNIGHT/Anim"
name = "BS_DragonKnight_DirectionalHit"
path = folder + "/" + name
skeleton = unreal.load_asset("/Game/Enemies/DF_DRAGON_KNIGHT/MESHES/UE5/SK_Mannequin")
if not skeleton:
    raise RuntimeError("Dragon Knight skeleton missing")

asset = unreal.load_asset(path)
if not asset:
    factory = unreal.BlendSpaceFactoryNew()
    factory.set_editor_property("target_skeleton", skeleton)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.BlendSpace, factory)
if not asset:
    raise RuntimeError("Could not create " + path)

def parameter(label):
    value = unreal.BlendParameter()
    value.set_editor_property("display_name", label)
    value.set_editor_property("min", -1.0)
    value.set_editor_property("max", 1.0)
    value.set_editor_property("grid_num", 4)
    try:
        value.set_editor_property("interpolation_time", 0.04)
    except Exception:
        pass
    return value

asset.set_editor_property("blend_parameters", [
    parameter("ReactionForward"), parameter("ReactionRight"), unreal.BlendParameter()
])
samples = []
for animation, x, y in (
    ("hit_front", 1.0, 0.0),
    ("hit_back", -1.0, 0.0),
    ("hit_right", 0.0, 1.0),
    ("hit_left", 0.0, -1.0),
):
    sequence = unreal.load_asset(folder + "/" + animation)
    if not sequence:
        raise RuntimeError("Missing boss directional hit animation: " + animation)
    if sequence.get_editor_property("skeleton") != skeleton:
        raise RuntimeError(animation + " uses a different skeleton")
    sample = unreal.BlendSample()
    sample.set_editor_property("animation", sequence)
    sample.set_editor_property("sample_value", unreal.Vector(x, y, 0.0))
    samples.append(sample)

asset.set_editor_property("sample_data", samples)
triangles = unreal.BossBlendSpaceTools.rebuild_blend_space(asset)
if triangles <= 0:
    raise RuntimeError("Boss hit Blend Space triangulation failed")
unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
unreal.log("BOSS_HIT_BLENDSPACE created {} samples={} triangles={}".format(path, len(samples), triangles))
