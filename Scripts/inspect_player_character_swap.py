import unreal

BP_PATH = "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"
MESH_PATH = "/Game/Characters/Dark_Knight/Dark_Knight_Male/Meshes/SKM_DKM_Full_With_Sword"

bp = unreal.load_asset(BP_PATH)
mesh_asset = unreal.load_asset(MESH_PATH)
if not bp or not mesh_asset:
    raise RuntimeError("Required player assets are missing")

generated_class = bp.generated_class()
cdo = unreal.get_default_object(generated_class)
mesh_component = cdo.get_editor_property("mesh")

def asset_path(value):
    return value.get_path_name() if value else "None"

try:
    current_mesh = mesh_component.get_editor_property("skeletal_mesh_asset")
except Exception:
    current_mesh = mesh_component.get_editor_property("skeletal_mesh")

unreal.log_warning("PLAYER_SWAP current_mesh=" + asset_path(current_mesh))
unreal.log_warning("PLAYER_SWAP new_mesh=" + asset_path(mesh_asset))
unreal.log_warning("PLAYER_SWAP current_anim_class=" + asset_path(mesh_component.get_editor_property("anim_class")))
unreal.log_warning("PLAYER_SWAP animation_mode=" + str(mesh_component.get_editor_property("animation_mode")))
try:
    unreal.log_warning("PLAYER_SWAP animation_data=" + str(mesh_component.get_editor_property("animation_data")))
except Exception as error:
    unreal.log_warning("PLAYER_SWAP animation_data unavailable=" + str(error))
unreal.log_warning("PLAYER_SWAP location=" + str(mesh_component.get_editor_property("relative_location")))
unreal.log_warning("PLAYER_SWAP rotation=" + str(mesh_component.get_editor_property("relative_rotation")))
unreal.log_warning("PLAYER_SWAP scale=" + str(mesh_component.get_editor_property("relative_scale3d")))

new_skeleton = mesh_asset.get_editor_property("skeleton")
old_skeleton = current_mesh.get_editor_property("skeleton") if current_mesh else None
unreal.log_warning("PLAYER_SWAP old_skeleton=" + asset_path(old_skeleton))
unreal.log_warning("PLAYER_SWAP new_skeleton=" + asset_path(new_skeleton))

for label, skeleton in (("old", old_skeleton), ("new", new_skeleton)):
    if not skeleton:
        continue
    unreal.log_warning("PLAYER_SWAP {} skeleton properties={}".format(
        label, ",".join(sorted([p for p in dir(skeleton) if "compat" in p.lower()]))))
