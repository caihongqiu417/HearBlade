import unreal

BP_PATH = "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"
NEW_MESH_PATH = "/Game/Characters/Dark_Knight/Dark_Knight_Male/Meshes/SKM_DKM_Full_With_Sword"
OLD_SKELETON_PATH = "/Game/Characters/Mannequins/Meshes/SK_Mannequin"
NEW_SKELETON_PATH = "/Game/Characters/Dark_Knight/Dark_Knight_Male/Meshes/SK_DKM_Full"

bp = unreal.load_asset(BP_PATH)
new_mesh = unreal.load_asset(NEW_MESH_PATH)
old_skeleton = unreal.load_asset(OLD_SKELETON_PATH)
new_skeleton = unreal.load_asset(NEW_SKELETON_PATH)
if not all((bp, new_mesh, old_skeleton, new_skeleton)):
    raise RuntimeError("Player mesh swap assets are incomplete")

# Both meshes contain the same 89-bone UE mannequin hierarchy but were imported
# into separate Skeleton assets. Mutual compatibility keeps the existing player
# Anim Blueprint and dodge montages usable on the Dark Knight mesh.
new_skeleton.add_compatible_skeleton(old_skeleton)
old_skeleton.add_compatible_skeleton(new_skeleton)

cdo = unreal.get_default_object(bp.generated_class())
mesh_component = cdo.get_editor_property("mesh")
mesh_component.modify()
try:
    mesh_component.set_editor_property("skeletal_mesh_asset", new_mesh)
except Exception:
    mesh_component.set_editor_property("skeletal_mesh", new_mesh)

# Preserve the existing capsule alignment and mannequin orientation. The Dark
# Knight asset follows the same UE mannequin axes and proportions.
mesh_component.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, -89.0))
mesh_component.set_editor_property("relative_rotation", unreal.Rotator(pitch=0.0, yaw=-90.0, roll=0.0))
mesh_component.set_editor_property("relative_scale3d", unreal.Vector(1.0, 1.0, 1.0))

bp.modify()
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
for asset in (new_skeleton, old_skeleton, bp):
    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)

actual_mesh = cdo.get_editor_property("mesh").get_editor_property("skeletal_mesh_asset")
if actual_mesh != new_mesh:
    raise RuntimeError("Blueprint CDO did not retain the Dark Knight mesh")
unreal.log_warning("PLAYER_SWAP_APPLIED mesh=" + actual_mesh.get_path_name())
unreal.log_warning("PLAYER_SWAP_APPLIED anim_class=" + str(mesh_component.get_editor_property("anim_class")))
