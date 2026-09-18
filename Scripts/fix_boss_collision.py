import unreal

BLUEPRINT = "/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/BP_Dragon_Knight_UE5_Fire"
CAPSULE_TEMPLATE = (
    BLUEPRINT + ".BP_Dragon_Knight_UE5_Fire_C:BossCollisionCapsule_GEN_VARIABLE"
)
MESH_NAMES = ("UE5_body", "UE5_greaves", "UE5_head", "UE5_gauntlets", "UE5_body_cape")


def configure_capsule(capsule):
    if not isinstance(capsule, unreal.CapsuleComponent):
        raise RuntimeError("BossCollisionCapsule is not a CapsuleComponent")
    capsule.set_capsule_size(60.0, 110.0, True)
    capsule.set_collision_profile_name("Pawn")
    pawn_response = capsule.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN)
    if "block" not in str(pawn_response).lower():
        raise RuntimeError("Boss capsule still does not block Pawn")
    unreal.log("BossCollision: configured " + capsule.get_path_name())


blueprint = unreal.load_asset(BLUEPRINT)
if not blueprint:
    raise RuntimeError("Boss blueprint missing")
template = unreal.load_object(None, CAPSULE_TEMPLATE)
if not template:
    raise RuntimeError("Boss capsule template missing")
configure_capsule(template)
for name in MESH_NAMES:
    mesh = unreal.load_object(None, BLUEPRINT + ".BP_Dragon_Knight_UE5_Fire_C:" + name + "_GEN_VARIABLE")
    if not mesh:
        raise RuntimeError("Missing Boss mesh template: " + name)
    mesh.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, -110.0))
if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False):
    raise RuntimeError("Failed to save Boss blueprint")

unreal.log("BossCollision: Blueprint templates saved; runtime normalizes older placed instances")
