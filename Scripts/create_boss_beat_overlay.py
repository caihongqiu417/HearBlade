"""Create a real additive UE material for Boss heavy/light beat VFX.

Heavy beats make a molten orange silhouette with rising energy bands. Light
beats draw a shorter cyan edge streak. C++ drives StrongPulse and WeakPulse on
a dynamic overlay instance; the original armor materials stay untouched.
"""

import unreal

PATH = "/Game/Enemies/DF_DRAGON_KNIGHT/MATERIALS"
NAME = "M_BossBeatOverlay"
ASSET = PATH + "/" + NAME

if unreal.EditorAssetLibrary.does_asset_exist(ASSET):
    material = unreal.load_asset(ASSET)
else:
    factory = unreal.MaterialFactoryNew()
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        NAME, PATH, unreal.Material, factory)
if not material:
    raise RuntimeError("Could not create " + ASSET)

material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property("two_sided", True)
material.set_editor_property("used_with_skeletal_mesh", True)

edit = unreal.MaterialEditingLibrary
for expression in list(edit.get_material_expressions(material)):
    edit.delete_material_expression(material, expression)


def node(cls, x, y):
    result = edit.create_material_expression(material, cls, x, y)
    if not result:
        raise RuntimeError("Failed node " + cls.__name__)
    return result


def scalar(name, default, x, y):
    result = node(unreal.MaterialExpressionScalarParameter, x, y)
    result.set_editor_property("parameter_name", name)
    result.set_editor_property("default_value", default)
    return result


def color(name, value, x, y):
    result = node(unreal.MaterialExpressionVectorParameter, x, y)
    result.set_editor_property("parameter_name", name)
    result.set_editor_property("default_value", unreal.LinearColor(*value))
    return result


def constant(value, x, y):
    result = node(unreal.MaterialExpressionConstant, x, y)
    result.set_editor_property("r", value)
    return result


def binary(cls, a, b, x, y):
    result = node(cls, x, y)
    if not edit.connect_material_expressions(a, "", result, "A"):
        raise RuntimeError("Failed A wire " + cls.__name__)
    if not edit.connect_material_expressions(b, "", result, "B"):
        raise RuntimeError("Failed B wire " + cls.__name__)
    return result


heavy = scalar("StrongPulse", 0.0, -1200, -800)
light = scalar("WeakPulse", 0.0, -1200, -500)
hot = color("StrongColor", (1.0, 0.06, 0.003, 1.0), -1200, -1050)
cool = color("WeakColor", (0.015, 0.30, 0.9, 1.0), -1200, -250)
hot_drive = binary(unreal.MaterialExpressionMultiply, hot, heavy, -950, -900)
cool_drive = binary(unreal.MaterialExpressionMultiply, cool, light, -950, -400)
beat_color = binary(unreal.MaterialExpressionAdd, hot_drive, cool_drive, -700, -650)

# Broad camera-facing rim, plus thin moving bands crossing the whole body.
fresnel = node(unreal.MaterialExpressionFresnel, -1500, 100)
rim_strength = constant(34.0, -1500, 300)
rim = binary(unreal.MaterialExpressionMultiply, fresnel, rim_strength, -1150, 130)
position = node(unreal.MaterialExpressionWorldPosition, -1950, 600)
height = node(unreal.MaterialExpressionComponentMask, -1720, 600)
height.set_editor_property("r", False)
height.set_editor_property("g", False)
height.set_editor_property("b", True)
if not edit.connect_material_expressions(position, "", height, ""):
    raise RuntimeError("Failed height input")
height_rate = constant(0.014, -1700, 800)
height_phase = binary(unreal.MaterialExpressionMultiply, height, height_rate, -1450, 600)
time = node(unreal.MaterialExpressionTime, -1900, 1000)
travel_rate = constant(4.5, -1700, 1150)
travel = binary(unreal.MaterialExpressionMultiply, time, travel_rate, -1450, 1000)
phase = binary(unreal.MaterialExpressionSubtract, height_phase, travel, -1200, 700)
wave = node(unreal.MaterialExpressionSine, -1000, 650)
if not edit.connect_material_expressions(phase, "", wave, ""):
    raise RuntimeError("Failed sine input")
wave_squared = binary(unreal.MaterialExpressionMultiply, wave, wave, -820, 650)
sharpness = constant(9.0, -800, 850)
thin_bands = node(unreal.MaterialExpressionPower, -600, 650)
if not edit.connect_material_expressions(wave_squared, "", thin_bands, "Base"):
    raise RuntimeError("Failed Power Base wire")
if not edit.connect_material_expressions(sharpness, "", thin_bands, "Exp"):
    raise RuntimeError("Failed Power Exp wire")
band_strength = constant(26.0, -580, 860)
bands = binary(unreal.MaterialExpressionMultiply, thin_bands, band_strength, -350, 600)
base = constant(2.0, -400, 150)
shape = binary(unreal.MaterialExpressionAdd, rim, bands, -130, 250)
shape = binary(unreal.MaterialExpressionAdd, shape, base, 100, 200)
emissive = binary(unreal.MaterialExpressionMultiply, beat_color, shape, 350, -300)
if not edit.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
    raise RuntimeError("Failed emissive connection")

edit.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)
unreal.log("BossBeatOverlay created: " + ASSET)
