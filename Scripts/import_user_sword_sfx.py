import unreal
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = root / "Content" / "Enemies" / "DF_DRAGON_KNIGHT" / "SOUND" / "dragon-studio-sword-slice-393847.mp3"
if not source.is_file():
    raise RuntimeError("Missing user sword sound: " + str(source))

task = unreal.AssetImportTask()
task.set_editor_property("filename", str(source))
task.set_editor_property("destination_path", "/Game/Enemies/DF_DRAGON_KNIGHT/SOUND")
task.set_editor_property("destination_name", "SW_DragonKnight_SwordSlice")
task.set_editor_property("automated", True)
task.set_editor_property("replace_existing", True)
task.set_editor_property("save", True)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
paths = list(task.get_editor_property("imported_object_paths"))
if len(paths) != 1:
    raise RuntimeError("Sword sound import failed: " + repr(paths))
sound = unreal.load_asset(paths[0])
unreal.log(f"UserSwordSfx: imported={paths[0]} duration={sound.get_editor_property('duration'):.3f}")
