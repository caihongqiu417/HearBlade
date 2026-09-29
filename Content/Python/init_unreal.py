"""Runs automatically when the editor starts (Python plugin convention).

Keeps generated source art in sync: every file below that has no asset yet, or
is newer than its asset, is (re)imported. Nothing happens when all are current.
  SourceArt/Audio/HitRush/*.wav   -> /Game/Audio/SFX/HitRush          (one-shot sounds)
  SourceArt/UI/BladeScore/*.png   -> /Game/UI/RhythmCombat/BladeScore (UI textures)
"""
import unreal
from pathlib import Path

SYNC = (
    ('SourceArt/Audio/HitRush', '*.wav', '/Game/Audio/SFX/HitRush'),
    ('SourceArt/UI/BladeScore', '*.png', '/Game/UI/RhythmCombat/BladeScore'),
    ('SourceArt/UI/Tutorial', '*.png', '/Game/UI/Tutorial'),
)


def _set(asset, prop, value):
    try:
        asset.set_editor_property(prop, value)
    except Exception as e:
        unreal.log_warning('SOURCEART %s.%s: %s' % (asset.get_name(), prop, e))


def _fix(asset):
    if isinstance(asset, unreal.SoundWave):
        _set(asset, 'looping', False)
    elif isinstance(asset, unreal.Texture2D):
        _set(asset, 'compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        _set(asset, 'lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
        _set(asset, 'mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        _set(asset, 'never_stream', True)
        _set(asset, 'srgb', True)


def _sync():
    project = Path(unreal.Paths.project_dir())
    total = 0
    for src_rel, pattern, dest in SYNC:
        src = project / src_rel
        if not src.is_dir():
            continue
        content = project / 'Content' / dest[len('/Game/'):]
        tasks = []
        for f in sorted(src.glob(pattern)):
            uasset = content / (f.stem + '.uasset')
            if uasset.exists() and uasset.stat().st_mtime >= f.stat().st_mtime:
                continue
            t = unreal.AssetImportTask()
            t.filename = str(f); t.destination_path = dest; t.destination_name = f.stem
            t.automated = True; t.replace_existing = True; t.save = True
            tasks.append(t)
        if not tasks:
            continue
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
        for t in tasks:
            for path in t.imported_object_paths:
                asset = unreal.load_asset(path)
                if asset:
                    _fix(asset)
                    unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        total += len(tasks)
    if total:
        unreal.log_warning('SOURCEART auto-import: %d asset(s) updated' % total)


try:
    _sync()
except Exception as e:  # never block editor startup
    unreal.log_warning('SOURCEART auto-import skipped: %s' % e)
