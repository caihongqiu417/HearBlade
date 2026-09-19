"""Create the authored PERFECT dodge materials and editable feedback data asset."""
import unreal

PATH = "/Game/Combat/Feedback"
UI_PATH = "/Game/UI/RhythmCombat"
tools = unreal.AssetToolsHelpers.get_asset_tools()
edit = unreal.MaterialEditingLibrary


def material(name):
    path = f"{PATH}/{name}"
    obj = unreal.load_asset(path)
    if not obj:
        obj = tools.create_asset(name, PATH, unreal.Material, unreal.MaterialFactoryNew())
    if not obj:
        raise RuntimeError("Could not create " + path)
    for expression in list(edit.get_material_expressions(obj)):
        edit.delete_material_expression(obj, expression)
    return obj


def node(mat, cls, x, y):
    result = edit.create_material_expression(mat, cls, x, y)
    if not result:
        raise RuntimeError("Failed node " + cls.__name__)
    return result


def scalar(mat, name, default, x, y):
    result = node(mat, unreal.MaterialExpressionScalarParameter, x, y)
    result.set_editor_property("parameter_name", name)
    result.set_editor_property("default_value", default)
    return result


def color(mat, name, value, x, y):
    result = node(mat, unreal.MaterialExpressionVectorParameter, x, y)
    result.set_editor_property("parameter_name", name)
    result.set_editor_property("default_value", unreal.LinearColor(*value))
    return result


def binary(mat, cls, a, b, x, y):
    result = node(mat, cls, x, y)
    edit.connect_material_expressions(a, "", result, "A")
    edit.connect_material_expressions(b, "", result, "B")
    return result


def texture_sample(mat, texture_path, x, y):
    result = node(mat, unreal.MaterialExpressionTextureSample, x, y)
    result.set_editor_property("texture", unreal.load_asset(texture_path))
    return result


# World-space broken ring plane.
ring = material("M_PerfectDodgeRing")
ring.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
ring.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
ring.set_editor_property("two_sided", True)
ring_tex = texture_sample(ring, UI_PATH + "/T_PerfectDodge_RingMask", -900, -100)
ring_color = color(ring, "RingColor", (1.0, .48, .025, 1.0), -900, -400)
gold_mix = scalar(ring, "GoldMix", .6, -900, -600)
opacity = scalar(ring, "Opacity", 0.0, -900, 200)
bright = binary(ring, unreal.MaterialExpressionMultiply, ring_color, gold_mix, -620, -380)
masked = binary(ring, unreal.MaterialExpressionMultiply, bright, ring_tex, -360, -180)
final_ring = binary(ring, unreal.MaterialExpressionMultiply, masked, opacity, -100, -100)
edit.connect_material_property(final_ring, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
edit.recompile_material(ring)

# Cyan silhouette afterimage.
ghost = material("M_PlayerPerfectDodgeGhost")
ghost.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
ghost.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
ghost.set_editor_property("two_sided", True)
ghost.set_editor_property("used_with_skeletal_mesh", True)
fresnel = node(ghost, unreal.MaterialExpressionFresnel, -800, -100)
ghost_color = color(ghost, "GhostColor", (.01, .55, 1.0, 1.0), -800, -400)
ghost_opacity = scalar(ghost, "Opacity", .0, -800, 200)
ghost_glow = binary(ghost, unreal.MaterialExpressionMultiply, ghost_color, fresnel, -500, -250)
ghost_glow = binary(ghost, unreal.MaterialExpressionMultiply, ghost_glow, ghost_opacity, -250, -180)
edit.connect_material_property(ghost_glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
edit.connect_material_property(ghost_opacity, "", unreal.MaterialProperty.MP_OPACITY)
edit.recompile_material(ghost)

# Fullscreen Beat Rift: mild desaturation plus edge streaks; center remains readable.
post = material("M_PP_PerfectDodge_BeatRift")
post.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
scene = node(post, unreal.MaterialExpressionSceneTexture, -1100, -150)
scene.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
intensity = scalar(post, "Intensity", 0.0, -1100, 250)
gold = scalar(post, "GoldMix", .5, -1100, 430)
desat_amount = binary(post, unreal.MaterialExpressionMultiply, intensity,
                      scalar(post, "DesaturationAmount", .15, -1100, 610), -780, 340)
desat = node(post, unreal.MaterialExpressionDesaturation, -520, -80)
edit.connect_material_expressions(scene, "Color", desat, "Input")
edit.connect_material_expressions(desat_amount, "", desat, "Fraction")
streak = texture_sample(post, UI_PATH + "/T_PerfectDodge_StreakMask", -780, -450)
cyan = color(post, "CyanColor", (.0, .65, 1.0, 1.0), -760, -690)
gold_color = color(post, "GoldColor", (1.0, .34, .015, 1.0), -760, -850)
lerp_color = node(post, unreal.MaterialExpressionLinearInterpolate, -470, -690)
edit.connect_material_expressions(cyan, "", lerp_color, "A")
edit.connect_material_expressions(gold_color, "", lerp_color, "B")
edit.connect_material_expressions(gold, "", lerp_color, "Alpha")
streak_color = binary(post, unreal.MaterialExpressionMultiply, streak, lerp_color, -210, -480)
streak_color = binary(post, unreal.MaterialExpressionMultiply, streak_color, intensity, 30, -400)
combined = binary(post, unreal.MaterialExpressionAdd, desat, streak_color, 260, -100)
edit.connect_material_property(combined, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
edit.recompile_material(post)

for asset in (ring, ghost, post):
    unreal.EditorAssetLibrary.save_loaded_asset(asset)

# A Niagara copy supplies short comic sparks; the material plane remains the readable ring.
niagara_path = PATH + "/NS_PerfectDodge_Ring"
if not unreal.EditorAssetLibrary.does_asset_exist(niagara_path):
    source = "/Game/Enemies/DF_DRAGON_KNIGHT/VFX/NS_Embers_System.NS_Embers_System"
    copied = unreal.EditorAssetLibrary.duplicate_asset(source, niagara_path)
    unreal.log(f"PerfectDodgeFeedback Niagara copied={bool(copied)}")

data_path = PATH + "/DA_PerfectDodgeFeedback"
data = unreal.load_asset(data_path)
if not data:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.PerfectDodgeFeedbackConfig)
    data = tools.create_asset("DA_PerfectDodgeFeedback", PATH, unreal.PerfectDodgeFeedbackConfig, factory)
if not data:
    raise RuntimeError("Could not create " + data_path)
data.set_editor_property("post_process_material", post)
data.set_editor_property("ghost_material", ghost)
data.set_editor_property("ring_material", ring)
data.set_editor_property("ring_particles", unreal.load_asset(niagara_path))
data.set_editor_property("duration_seconds", .15)
data.set_editor_property("ghost_lifetime_seconds", .22)
data.set_editor_property("ghost_count", 3)
data.set_editor_property("fov_expansion_degrees", 3.5)
data.set_editor_property("camera_shake_degrees", .18)
unreal.EditorAssetLibrary.save_loaded_asset(data)
unreal.log("PerfectDodgeFeedback assets created under " + PATH)
