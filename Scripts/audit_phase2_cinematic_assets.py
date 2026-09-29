import unreal,json
from pathlib import Path
paths=[
'/Game/SwordAnim/Animation/UE5/Taunt',
'/Game/SwordAnim/Animation/UE5/Defense01',
'/Game/SwordAnim/Animation/UE5/Defense01_CounterAttack',
'/Game/SwordAnim/Animation/UE5/hit_front',
'/Game/SwordAnim/Animation/UE5/Combo03',
'/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Defense01_CounterAttack',
'/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Defense01_Loop',
'/Game/Enemies/DF_DRAGON_KNIGHT/Montage/Combo01_Montage']
out=[]
for path in paths:
    a=unreal.load_asset(path)
    row={'asset':path,'exists':bool(a)}
    if a:
        row['skeleton']=a.get_editor_property('skeleton').get_path_name()
        row['length']=a.get_editor_property('sequence_length')
        if isinstance(a,unreal.AnimSequence):
            row['frames']=unreal.AnimationLibrary.get_num_frames(a)
            row['root_motion']=a.get_editor_property('enable_root_motion')
    out.append(row)
Path('D:/Udev/HearBlade/output/arena/phase2_cinematic_assets.json').write_text(json.dumps(out,indent=2))
unreal.log_warning('PHASE2_CINEMATIC_ASSETS_AUDITED')
