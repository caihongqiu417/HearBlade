import unreal
from pathlib import Path


source_dir = Path(__file__).resolve().parents[1] / "SourceArt" / "UI" / "RhythmCombat"
tasks = []
for source in sorted(source_dir.glob("*.png")):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source))
    task.set_editor_property("destination_path", "/Game/UI/RhythmCombat")
    task.set_editor_property("destination_name", source.stem)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    tasks.append(task)

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for task in tasks:
    paths = list(task.get_editor_property("imported_object_paths"))
    if len(paths) != 1:
        raise RuntimeError(f"HUD texture import failed: {task.filename} -> {paths!r}")
    texture = unreal.load_asset(paths[0])
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    unreal.log(f"RhythmHUD: imported={paths[0]} size={texture.blueprint_get_size_x()}x{texture.blueprint_get_size_y()}")
