import unreal, json
from pathlib import Path
out={}
for n in ('Buff','Taunt','Defense01_Start','Defense01_Loop','hit_front'):
    a=unreal.load_asset('/Game/SwordAnim/Animation/UE5/'+n)
    out[n]={'skeleton':a.get_editor_property('skeleton').get_path_name(),'length':a.get_editor_property('sequence_length')}
out['animation_api']=[x for x in dir(unreal.AnimationLibrary) if any(v in x for v in ('bone','skeleton'))]
out['skeleton_api']=[x for x in dir(unreal.Skeleton) if any(v in x for v in ('bone','reference','compatible'))]
out['seq_api']=[x for x in dir(unreal.AnimSequence) if any(v in x for v in ('skeleton','bone'))]
for n in ('SM_HC2_BeatHalo_029','SM_HC2_BeatShard_004'):
    a=unreal.load_asset('/Game/Environment/HarmonicCourtV2/Meshes/'+n)
    b=a.get_bounds()
    out[n]={'origin':str(b.origin),'extent':str(b.box_extent)}
a=unreal.load_asset('/Game/Enemies/DF_DRAGON_KNIGHT/MESHES/SWORD/SM_Dragon_knight_sword')
b=a.get_bounds();out['Sword']={'origin':str(b.origin),'extent':str(b.box_extent)}
Path('D:/Udev/HearBlade/output/arena/ascension_inputs.json').write_text(json.dumps(out,indent=2))
