import unreal

folder = "/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations"
name = "BS_DKM_DirectionalHit"
path = folder + "/" + name
skeleton = unreal.load_asset("/Game/Characters/Dark_Knight/Dark_Knight_Male/Meshes/SK_DKM_Full")
if not skeleton:
    raise RuntimeError("Dark Knight skeleton missing")

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
        value.set_editor_property("interpolation_time", 0.05)
    except Exception:
        pass
    return value

asset.set_editor_property("blend_parameters", [parameter("ReactionForward"), parameter("ReactionRight"), unreal.BlendParameter()])
root = folder + "/"
samples = []
for animation, x, y in (
    ("Anim_DKM_Hit_Alert_Fwd", 1.0, 0.0),
    ("Anim_DKM_Hit_Alert_Bwd", -1.0, 0.0),
    ("Anim_DKM_Hit_Alert_Right", 0.0, 1.0),
    ("Anim_DKM_Hit_Alert_Left", 0.0, -1.0),
):
    sequence = unreal.load_asset(root + animation)
    if not sequence:
        raise RuntimeError("Missing directional hit animation: " + animation)
    sample = unreal.BlendSample()
    sample.set_editor_property("animation", sequence)
    sample.set_editor_property("sample_value", unreal.Vector(x, y, 0.0))
    samples.append(sample)

asset.set_editor_property("sample_data", samples)
triangles = unreal.BossBlendSpaceTools.rebuild_blend_space(asset)
if triangles <= 0:
    raise RuntimeError("Directional hit Blend Space triangulation failed")
unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
unreal.log("PLAYER_HIT_BLENDSPACE created {} samples={} triangles={}".format(path, len(samples), triangles))
