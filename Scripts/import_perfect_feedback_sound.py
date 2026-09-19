import unreal
from pathlib import Path

project = Path(unreal.Paths.project_dir())
source = project / "SourceArt" / "Audio" / "ComboLayers" / "SW_RhythmPerfect.wav"

task = unreal.AssetImportTask()
task.filename = str(source)
task.destination_path = "/Game/Audio/SFX"
task.destination_name = "SW_RhythmPerfect"
task.automated = True
task.replace_existing = True
task.save = True

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
asset = unreal.load_asset("/Game/Audio/SFX/SW_RhythmPerfect")
if not asset:
    raise RuntimeError("Failed to import SW_RhythmPerfect")
unreal.EditorAssetLibrary.save_loaded_asset(asset)
unreal.log(f"Perfect feedback imported from {source}: {asset.get_path_name()}")
