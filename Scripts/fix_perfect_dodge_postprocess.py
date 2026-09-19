import unreal

path = "/Game/Combat/Feedback/M_PP_PerfectDodge_BeatRift"
material = unreal.load_asset(path)
if not material:
    raise RuntimeError("Missing " + path)
edit = unreal.MaterialEditingLibrary
scene = None
desat = None
for expression in edit.get_material_expressions(material):
    if isinstance(expression, unreal.MaterialExpressionSceneTexture): scene = expression
    if isinstance(expression, unreal.MaterialExpressionDesaturation): desat = expression
if not scene or not desat:
    raise RuntimeError("Post process graph nodes missing")
ok = edit.connect_material_expressions(scene, "", desat, "")
unreal.log(f"BeatRift post-process scene->desaturation connected={ok}")
edit.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)
