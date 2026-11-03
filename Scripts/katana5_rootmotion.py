"""Make sure the retargeted Katana 5-combo carries real root motion.

The player attack code (PlayerAttackComponent::TickAttack) already consumes
root motion: it extracts the root delta of each node's slice and moves the
capsule with CharacterMovement (root is locked on the mesh). So "root motion"
only needs the data: the retargeted clip's root bone must travel like the
source's. If the IK retarget dropped it, copy the source root track across,
scaled by the pelvis-height ratio of the two characters.

Run:  py D:/Udev/HearBlade/Scripts/katana5_rootmotion.py
"""
import json
import unreal
from pathlib import Path

SRC_DIR = '/Game/GrruzamSwordPack/Animations/Katana_Blade(SK_Mannequin)/2_Attacks/2__5Combos/'
DST_DIR = '/Game/Combat/Player/Animations/Katana5/'
NAMES = ['Katana_Blade_Attack_5Combo_ALL'] + ['Katana_Blade_Attack_5Combo_%d' % i for i in range(1, 6)]
SRC_MESH = '/Game/GrruzamSwordPack/Character/SK_Mannequin/Mesh/SK_Mannequin'
DST_MESH = '/Game/Characters/Dark_Knight/Dark_Knight_Male/Meshes/SKM_DKM_Full'
DATA = '/Game/Combat/Player/DA_PlayerAttackSet'
EAL = unreal.EditorAssetLibrary
AL = unreal.AnimationLibrary


def log(m):
    unreal.log_warning('KATANA5_RM ' + m)


def raw_root(clip, t):
    """Root bone translation from the raw track (ignores root lock)."""
    if hasattr(AL, 'get_bone_pose_for_time'):
        return AL.get_bone_pose_for_time(clip, 'root', t, False).translation
    flags = [(k, clip.get_editor_property(k)) for k in ('force_root_lock', 'enable_root_motion')]
    for k, _ in flags:
        clip.set_editor_property(k, False)
    try:
        return root_local(clip, t).translation
    finally:
        for k, v in flags:
            clip.set_editor_property(k, v)


def travel(clip, a, b):
    p, q = raw_root(clip, a), raw_root(clip, b)
    return (q.x - p.x, q.y - p.y, q.z - p.z)


def size2d(v):
    return (v[0] ** 2 + v[1] ** 2) ** .5


def root_local(clip, t):
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, t, unreal.AnimPoseEvaluationOptions())
    return unreal.AnimPoseExtensions.get_bone_pose(pose, 'root', unreal.AnimPoseSpaces.LOCAL)


def ref_height(mesh):
    pose = unreal.AnimPoseExtensions.get_reference_pose(mesh.get_editor_property('skeleton'))
    return unreal.AnimPoseExtensions.get_ref_bone_pose(pose, 'pelvis', unreal.AnimPoseSpaces.WORLD).translation.z


def copy_root(src, dst, scale):
    """Key the source root track (translation scaled, rotation kept) onto dst."""
    length = dst.get_play_length()
    try:
        frames = int(AL.get_num_keys(dst))
    except Exception:
        frames = int(round(length * 30)) + 1
    pos, rot, scl = [], [], []
    for f in range(frames):
        t = min(length, length * f / max(1, frames - 1))
        # Evaluate the source without root lock so the root actually moves.
        tr = root_local(src, min(t, src.get_play_length()))
        p = tr.translation
        pos.append(unreal.Vector(p.x * scale, p.y * scale, p.z * scale))
        rot.append(tr.rotation)
        scl.append(unreal.Vector(1, 1, 1))
    ctrl = dst.get_controller() if hasattr(dst, 'get_controller') else dst.get_editor_property('controller')
    ctrl.open_bracket(unreal.Text('Copy root motion'), False) if hasattr(ctrl, 'open_bracket') else None
    ctrl.set_bone_track_keys('root', pos, rot, scl, False)
    ctrl.close_bracket(False) if hasattr(ctrl, 'close_bracket') else None


def run():
    scale = ref_height(unreal.load_asset(DST_MESH)) / ref_height(unreal.load_asset(SRC_MESH))
    log('pelvis height ratio DKM/UE4 = %.3f' % scale)
    report = {}
    for name in NAMES:
        src = unreal.load_asset(SRC_DIR + name)
        dst = unreal.load_asset(DST_DIR + name + '_DKM')
        L = dst.get_play_length()
        s, d = travel(src, 0., src.get_play_length()), travel(dst, 0., L)
        fixed = False
        if size2d(s) > 5. and size2d(d) < .5 * size2d(s) * scale:
            copy_root(src, dst, scale)
            dst.set_editor_property('enable_root_motion', True)
            dst.set_editor_property('force_root_lock', True)
            assert EAL.save_loaded_asset(dst, False)
            d, fixed = travel(dst, 0., L), True
        report[name] = dict(source=s, retargeted=d, copied=fixed)
        log('%s source=%.1fcm retargeted=%.1fcm copied=%s' % (name, size2d(s), size2d(d), fixed))
    # Per-node slice travel on the combo actually used by the player.
    data = unreal.load_asset(DATA)
    for node in data.get_editor_property('nodes'):
        clip = node.get_editor_property('animation')
        if not clip.get_path_name().startswith(DST_DIR):
            continue
        v = travel(clip, node.get_editor_property('clip_start'), node.get_editor_property('clip_end'))
        report['node_' + str(node.get_editor_property('id'))] = v
        log('node %s travel=%.1fcm (x=%.1f y=%.1f)' % (node.get_editor_property('id'), size2d(v), v[0], v[1]))
    Path(unreal.Paths.project_dir(), 'output', 'katana5_rootmotion.json').write_text(json.dumps(report, indent=1), encoding='utf-8')
    log('DONE')


run()
