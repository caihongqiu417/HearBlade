import unreal

BP_PATH = "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"
MESH_PATH = "/Game/Characters/Dark_Knight/Dark_Knight_Male/Meshes/SKM_DKM_Full"
IDLE_PATH = "/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Anim_DKM_Idle_Alert"
OLD_SKELETON_PATH = "/Game/Characters/Mannequins/Meshes/SK_Mannequin"
NEW_SKELETON_PATH = "/Game/Characters/Dark_Knight/Dark_Knight_Male/Meshes/SK_DKM_Full"

bp = unreal.load_asset(BP_PATH)
mesh_asset = unreal.load_asset(MESH_PATH)
idle = unreal.load_asset(IDLE_PATH)
old_skeleton = unreal.load_asset(OLD_SKELETON_PATH)
new_skeleton = unreal.load_asset(NEW_SKELETON_PATH)
if not all((bp, mesh_asset, idle, old_skeleton, new_skeleton)):
    raise RuntimeError("Dark Knight animation setup assets are incomplete")

# These Skeleton assets have different reference poses. Clear the temporary
# compatibility links so mannequin animations cannot deform the knight.
old_skeleton.set_editor_property("compatible_skeletons", [])
new_skeleton.set_editor_property("compatible_skeletons", [])

cdo = unreal.get_default_object(bp.generated_class())
mesh = cdo.get_editor_property("mesh")
mesh.modify()
try:
    mesh.set_editor_property("skeletal_mesh_asset", mesh_asset)
except Exception:
    mesh.set_editor_property("skeletal_mesh", mesh_asset)
mesh.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
animation_data = unreal.SingleAnimationPlayData()
animation_data.set_editor_property("anim_to_play", idle)
animation_data.set_editor_property("saved_looping", True)
animation_data.set_editor_property("saved_playing", True)
animation_data.set_editor_property("saved_position", 0.0)
animation_data.set_editor_property("saved_play_rate", 1.0)
mesh.set_editor_property("animation_data", animation_data)
mesh.set_editor_property("global_anim_rate_scale", 1.0)
mesh.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, -89.0))
mesh.set_editor_property("relative_rotation", unreal.Rotator(pitch=0.0, yaw=-90.0, roll=0.0))

bp.modify()
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
for asset in (old_skeleton, new_skeleton, bp):
    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)

unreal.log_warning("PLAYER_DARK_KNIGHT_ANIMATION mesh=" + mesh_asset.get_path_name())
unreal.log_warning("PLAYER_DARK_KNIGHT_ANIMATION idle=" + idle.get_path_name())
