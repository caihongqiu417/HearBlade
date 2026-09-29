"""Import the synthesized Hi-Fi-Rush-style hit SFX (SourceArt/Audio/HitRush/*.wav,
made by Scripts/generate_hitrush_sfx.py) into /Game/Audio/SFX/HitRush as one-shots.

Run:  py D:/Udev/HearBlade/Scripts/import_hitrush_sfx.py
"""
import unreal
from pathlib import Path

SRC = Path(unreal.Paths.project_dir()) / 'SourceArt/Audio/HitRush'
DEST = '/Game/Audio/SFX/HitRush'
tasks = []
for wav in sorted(SRC.glob('*.wav')):
    t = unreal.AssetImportTask()
    t.filename = str(wav); t.destination_path = DEST; t.destination_name = wav.stem
    t.automated = True; t.replace_existing = True; t.save = True
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
    for path in t.imported_object_paths:
        sound = unreal.load_asset(path)
        sound.set_editor_property('looping', False)   # never a loop (see music bug)
        unreal.EditorAssetLibrary.save_loaded_asset(sound, False)
        unreal.log_warning('HITRUSH imported ' + path)
unreal.log_warning('HITRUSH DONE count=%d' % len(tasks))
