import unreal
from pathlib import Path

source = Path(unreal.Paths.project_dir()) / "SourceArt" / "RhythmResponses" / "T_HUD_AttackTimingRail_Minimal.png"
task = unreal.AssetImportTask()
task.set_editor_property("filename", str(source))
task.set_editor_property("destination_path", "/Game/UI/RhythmCombat")
task.set_editor_property("destination_name", "T_HUD_AttackTimingRail_Minimal")
task.set_editor_property("automated", True)
task.set_editor_property("replace_existing", True)
task.set_editor_property("save", True)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
paths = list(task.get_editor_property("imported_object_paths"))
if len(paths) != 1:
    raise RuntimeError(f"Minimal attack rail import failed: {paths}")
texture = unreal.load_asset(paths[0])
texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
texture.set_editor_property("srgb", True)
unreal.EditorAssetLibrary.save_loaded_asset(texture)
unreal.log(f"MinimalAttackRail: imported {paths[0]}")
