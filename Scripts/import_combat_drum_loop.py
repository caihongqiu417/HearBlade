import unreal
from pathlib import Path

source = Path(__file__).resolve().parents[1] / "SourceAudio" / "BossRhythm" / "SW_BossCombatDrumLoop.wav"
task = unreal.AssetImportTask()
task.set_editor_property("filename", str(source))
task.set_editor_property("destination_path", "/Game/Audio/SFX")
task.set_editor_property("destination_name", "SW_BossCombatDrumLoop")
task.set_editor_property("automated", True)
task.set_editor_property("replace_existing", True)
task.set_editor_property("save", True)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
paths = list(task.get_editor_property("imported_object_paths"))
if len(paths) != 1:
    raise RuntimeError("Combat drum import failed: " + repr(paths))
sound = unreal.load_asset(paths[0])
sound.set_editor_property("looping", True)
unreal.EditorAssetLibrary.save_loaded_asset(sound)
unreal.log(f"CombatDrumStem: imported={paths[0]} duration={sound.get_editor_property('duration'):.6f} looping=true")
