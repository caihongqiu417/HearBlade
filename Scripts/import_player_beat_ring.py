from pathlib import Path
import unreal
task = unreal.AssetImportTask()
task.filename = str(Path(unreal.Paths.project_dir(), 'SourceArt/UI/RhythmCombat/T_HUD_PlayerBeatRing.png'))
task.destination_path = '/Game/UI/RhythmCombat'
task.automated = True
task.replace_existing = True
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture = unreal.load_asset('/Game/UI/RhythmCombat/T_HUD_PlayerBeatRing')
assert texture
texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
unreal.EditorAssetLibrary.save_loaded_asset(texture)
unreal.log_warning('PLAYER_BEAT_RING_IMPORTED')
