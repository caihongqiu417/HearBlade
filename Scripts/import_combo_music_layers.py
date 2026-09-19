import unreal
from pathlib import Path

project = Path(unreal.Paths.project_dir())
source = project / "SourceArt" / "Audio" / "ComboLayers"
destination = "/Game/Audio/SFX"

tasks = []
for stem in ("SW_ComboTier1Low", "SW_ComboTier2Snare", "SW_ComboTier3High",
             "SW_RhythmGood", "SW_RhythmPerfect", "SW_ComboTierUp"):
    task = unreal.AssetImportTask()
    task.filename = str(source / f"{stem}.wav")
    task.destination_path = destination
    task.destination_name = stem
    task.automated = True
    task.replace_existing = True
    task.save = True
    tasks.append(task)

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for task in tasks:
    for object_path in task.imported_object_paths:
        asset = unreal.load_asset(object_path)
        if asset:
            if "ComboTier1" in object_path or "ComboTier2" in object_path or "ComboTier3" in object_path:
                try:
                    asset.set_editor_property("looping", True)
                except Exception as exc:
                    unreal.log_warning(f"Could not set looping on {object_path}: {exc}")
            unreal.EditorAssetLibrary.save_loaded_asset(asset)
            unreal.log(f"ComboMusic imported: {object_path}")

unreal.EditorAssetLibrary.save_directory(destination, only_if_is_dirty=False, recursive=True)
