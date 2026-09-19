import unreal
from pathlib import Path

source = Path(__file__).resolve().parents[1] / "SourceAudio" / "BossRhythm"
destination = "/Game/Audio/SFX"
unreal.EditorAssetLibrary.make_directory(destination)
tasks = []
for name in ("SW_BossBladeStrong", "SW_BossBladeWeak"):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source / (name + ".wav")))
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for task in tasks:
    paths = list(task.get_editor_property("imported_object_paths"))
    if len(paths) != 1:
        raise RuntimeError("Audio import failed: " + str(task.get_editor_property("destination_name")))
    unreal.log("BossRhythmAudio: imported " + paths[0])
