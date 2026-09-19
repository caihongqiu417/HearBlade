import unreal

source = "/Game/Enemies/DF_DRAGON_KNIGHT/VFX/NS_Embers_System"
destination = "/Game/Combat/Feedback/NS_PerfectDodge_Ring"
if not unreal.EditorAssetLibrary.does_asset_exist(destination):
    result = unreal.EditorAssetLibrary.duplicate_asset(source, destination)
    if not result:
        raise RuntimeError("Failed to duplicate PERFECT ring Niagara from " + source)
data = unreal.load_asset("/Game/Combat/Feedback/DA_PerfectDodgeFeedback")
effect = unreal.load_asset(destination)
if not data or not effect:
    raise RuntimeError("Feedback data or Niagara asset was not found")
data.set_editor_property("ring_particles", effect)
unreal.EditorAssetLibrary.save_loaded_asset(data)
unreal.EditorAssetLibrary.save_loaded_asset(effect)
unreal.log("PerfectDodgeFeedback Niagara ready: " + destination)
