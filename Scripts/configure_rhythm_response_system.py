import unreal
from pathlib import Path


project_dir = Path(unreal.Paths.project_dir())
source_dir = project_dir / "SourceArt" / "RhythmResponses"
destination = "/Game/UI/RhythmCombat/Responses"
sources = {
    "Dodge": source_dir / "Rhythm_Dodge.png",
    "Block": source_dir / "Rhythm_Block.png",
    "Counter": source_dir / "Rhythm_Counter.png",
    "HoldBlock": source_dir / "Rhythm_HoldBlock.png",
}

tasks = []
for name, source in sources.items():
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source))
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", f"T_Response_{name}")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

textures = {}
for name, task in zip(sources, tasks):
    imported = list(task.get_editor_property("imported_object_paths"))
    if len(imported) != 1:
        raise RuntimeError(f"Response icon import failed: {name}: {imported}")
    texture = unreal.load_asset(imported[0])
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    textures[name] = texture

visual_path = f"{destination}/DA_RhythmResponseVisuals"
visuals = unreal.load_asset(visual_path)
if not visuals:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.RhythmResponseVisualData)
    visuals = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "DA_RhythmResponseVisuals", destination, unreal.RhythmResponseVisualData, factory)
if not visuals:
    raise RuntimeError("Unable to create DA_RhythmResponseVisuals")

colors = {
    "Dodge": unreal.LinearColor(0.58, 0.26, 1.0, 1.0),
    "Block": unreal.LinearColor(0.10, 0.72, 1.0, 1.0),
    "Counter": unreal.LinearColor(1.0, 0.28, 0.06, 1.0),
    "HoldBlock": unreal.LinearColor(0.10, 0.92, 0.82, 1.0),
}
for prop, name in (("dodge", "Dodge"), ("block", "Block"), ("counter", "Counter"), ("hold_block", "HoldBlock")):
    value = visuals.get_editor_property(prop)
    value.set_editor_property("icon", textures[name])
    value.set_editor_property("color", colors[name])
    value.set_editor_property("icon_scale", 1.0)
    visuals.set_editor_property(prop, value)
visuals.set_editor_property("hold_start_icon", textures["HoldBlock"])
visuals.set_editor_property("hold_track_texture", textures["HoldBlock"])
visuals.set_editor_property("hold_end_icon", textures["HoldBlock"])
prompt_sound = unreal.load_asset("/Game/Audio/SFX/SW_BossDrumTick")
strong_prompt_sound = unreal.load_asset("/Game/Audio/SFX/SW_BossDrumKick")
visuals.set_editor_property("approach_sound", prompt_sound)
visuals.set_editor_property("strong_approach_sound", strong_prompt_sound)
unreal.EditorAssetLibrary.save_loaded_asset(visuals)


def set_response(marker, response, hold_marker=unreal.RhythmHoldMarkerType.NONE):
    marker.set_editor_property("required_response", response)
    marker.set_editor_property("prompt_lead_beats", 3.0)
    marker.set_editor_property("prompt_icon_override", None)
    marker.set_editor_property("prompt_icon_scale", 1.0)
    marker.set_editor_property("prompt_icon_color", unreal.LinearColor.WHITE)
    marker.set_editor_property("hold_group", "HoldA")
    marker.set_editor_property("hold_marker", hold_marker)


combo01 = unreal.load_asset("/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_DragonKnight_Combo01_Beat")
combo02 = unreal.load_asset("/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_DragonKnight_Combo02_Beat")
if not combo01 or not combo02:
    raise RuntimeError("Combo01 or Combo02 data asset is missing")

markers = list(combo01.get_editor_property("impact_markers"))
if len(markers) != 3:
    raise RuntimeError(f"Combo01 expected 3 markers, got {len(markers)}")
set_response(markers[0], unreal.RhythmResponseType.DODGE)
set_response(markers[1], unreal.RhythmResponseType.BLOCK)
set_response(markers[2], unreal.RhythmResponseType.COUNTER)
combo01.set_editor_property("impact_markers", markers)
unreal.EditorAssetLibrary.save_loaded_asset(combo01)

markers = list(combo02.get_editor_property("impact_markers"))
if len(markers) != 5:
    raise RuntimeError(f"Combo02 expected 5 markers, got {len(markers)}")
set_response(markers[0], unreal.RhythmResponseType.DODGE)
set_response(markers[1], unreal.RhythmResponseType.HOLD_BLOCK, unreal.RhythmHoldMarkerType.START)
set_response(markers[2], unreal.RhythmResponseType.HOLD_BLOCK, unreal.RhythmHoldMarkerType.CONTINUE)
set_response(markers[3], unreal.RhythmResponseType.HOLD_BLOCK, unreal.RhythmHoldMarkerType.END)
set_response(markers[4], unreal.RhythmResponseType.COUNTER)
combo02.set_editor_property("impact_markers", markers)
unreal.EditorAssetLibrary.save_loaded_asset(combo02)

unreal.log("RhythmResponseSystem: imported 4 icons, created visual data, configured Combo01 and Combo02")
