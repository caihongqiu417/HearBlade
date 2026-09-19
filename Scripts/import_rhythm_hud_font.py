import unreal
from pathlib import Path

font_source = Path(__file__).resolve().parents[1] / "SourceArt" / "UI" / "RhythmCombat" / "Fonts" / "Bangers-Regular.ttf"
task = unreal.AssetImportTask()
task.set_editor_property("filename", str(font_source))
task.set_editor_property("destination_path", "/Game/UI/RhythmCombat/Fonts")
task.set_editor_property("destination_name", "Bangers_Regular")
task.set_editor_property("automated", True)
task.set_editor_property("replace_existing", True)
task.set_editor_property("save", True)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
paths = list(task.get_editor_property("imported_object_paths"))
if not paths:
    raise RuntimeError(f"HUD font import failed: {font_source}")
unreal.log(f"RhythmHUD: imported font={paths[0]}")
