import unreal
from pathlib import Path


project_dir = Path(unreal.Paths.project_dir())
source_dir = project_dir / "SourceArt" / "RhythmResponses"
destination = "/Game/UI/RhythmCombat/Responses"
sources = {
    "Dodge": source_dir / "Rhythm_Dodge_Minimal.png",
    "Block": source_dir / "Rhythm_Block_Minimal.png",
    "Counter": source_dir / "Rhythm_Counter_Minimal.png",
    "HoldBlock": source_dir / "Rhythm_HoldBlock_Minimal.png",
}

tasks = []
for name, source in sources.items():
    if not source.exists():
        raise RuntimeError(f"Missing minimal response icon: {source}")
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source))
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", f"T_Response_{name}_Minimal")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    tasks.append(task)

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

textures = {}
for name, task in zip(sources, tasks):
    imported = list(task.get_editor_property("imported_object_paths"))
    if len(imported) != 1:
        raise RuntimeError(f"Minimal response icon import failed: {name}: {imported}")
    texture = unreal.load_asset(imported[0])
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    textures[name] = texture

visuals = unreal.load_asset(f"{destination}/DA_RhythmResponseVisuals")
if not visuals:
    raise RuntimeError("DA_RhythmResponseVisuals is missing")

# Keep the existing judgment colors and sounds. Only replace the instruction art.
for prop, name in (("dodge", "Dodge"), ("block", "Block"),
                   ("counter", "Counter"), ("hold_block", "HoldBlock")):
    value = visuals.get_editor_property(prop)
    value.set_editor_property("icon", textures[name])
    value.set_editor_property("icon_scale", 0.96)
    visuals.set_editor_property(prop, value)

visuals.set_editor_property("hold_start_icon", textures["HoldBlock"])
visuals.set_editor_property("hold_track_texture", textures["HoldBlock"])
visuals.set_editor_property("hold_end_icon", textures["HoldBlock"])
unreal.EditorAssetLibrary.save_loaded_asset(visuals)

for name, texture in textures.items():
    unreal.log(f"MINIMAL_RESPONSE_ICON {name}: {texture.get_path_name()}")
unreal.log("MINIMAL_RESPONSE_ICONS_PASS")
