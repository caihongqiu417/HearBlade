import unreal
from pathlib import Path

name = 'T_HUD_PhaseTwo_NoteAtlas'
task = unreal.AssetImportTask()
task.filename = str(Path(unreal.Paths.project_dir()) / 'SourceArt/UI/RhythmCombat/PhaseTwo' / (name + '.png'))
task.destination_path = '/Game/UI/RhythmCombat'
task.destination_name = name
task.automated = True
task.replace_existing = True
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
assert len(task.imported_object_paths) == 1
texture = unreal.load_asset(task.imported_object_paths[0])
texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
texture.set_editor_property('srgb', True)
unreal.EditorAssetLibrary.save_loaded_asset(texture)
unreal.log('PhaseTwoNoteUI: imported generated atlas with alpha')
