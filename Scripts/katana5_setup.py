"""Put the Grruzam Katana 5-combo on the Dark Knight player, properly retargeted.

Why: the Grruzam clips were authored on the UE4 mannequin (3 spine bones). The
Dark Knight uses the UE5 Manny layout (5 spine bones, longer neck chain).
Re-assigning the clips straight onto SK_DKM_Full applies UE4 spine offsets on
top of spine_04/05, which stretches the torso. This script instead:

  1. Reloads the six source clips from disk (drops any unsaved in-memory
     skeleton swap to SK_DKM_Full) and checks they are on the UE4 skeleton.
  2. Builds IK Rigs + an IK Retargeter UE4 mannequin -> SKM_DKM_Full.
  3. Duplicates-and-retargets the clips into /Game/Combat/Player/Animations/Katana5.
  4. Restores Dark Knight bone lengths, enables locked root motion.
  5. Samples the continuous _ALL clip, finds the 5 swings (contact = fastest
     hand_r, boundaries = slowest hand_r between swings).
  6. Wires DA_PlayerAttackSet: L/LL/LLL = swings 1-3, H/HH = swings 4-5,
     LH = swing 4. Other (resonance / HL) routes are left untouched.

Run in the editor:  py "D:/Udev/HearBlade/Scripts/katana5_setup.py"
Results: Output Log lines starting with KATANA5, and output/katana5_landmarks.json
"""
import json
import unreal
from pathlib import Path

SRC_DIR = '/Game/GrruzamSwordPack/Animations/Katana_Blade(SK_Mannequin)/2_Attacks/2__5Combos/'
ALL_NAME = 'Katana_Blade_Attack_5Combo_ALL'
NAMES = [ALL_NAME] + ['Katana_Blade_Attack_5Combo_%d' % i for i in range(1, 6)]
SRC_MESH = '/Game/GrruzamSwordPack/Character/SK_Mannequin/Mesh/SK_Mannequin'
SRC_SKEL = '/Game/GrruzamSwordPack/Character/SK_Mannequin/Mesh/UE4_Mannequin_Skeleton'
DST_MESH = '/Game/Characters/Dark_Knight/Dark_Knight_Male/Meshes/SKM_DKM_Full'
RIG_DIR = '/Game/Combat/Player/Retarget'
OUT_DIR = '/Game/Combat/Player/Animations/Katana5'
DATA = '/Game/Combat/Player/DA_PlayerAttackSet'
SUFFIX = '_DKM'
FPS = 30.0

EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
S = unreal.RetargetSourceOrTarget.SOURCE
T = unreal.RetargetSourceOrTarget.TARGET


def log(msg):
    unreal.log_warning('KATANA5 ' + msg)


def try_call(obj, name, *args):
    fn = getattr(obj, name, None)
    if fn is None:
        return False
    try:
        fn(*args)
        return True
    except Exception as e:  # API differs a little between engine versions
        log('note: %s failed: %s' % (name, e))
        return False


# ---------------------------------------------------------------- 1. sources
def package_of(asset):
    for name in ('get_package', 'get_outermost'):
        fn = getattr(asset, name, None)
        if fn:
            return fn()
    return unreal.load_package(asset.get_path_name().split('.')[0])


def restore_sources():
    skel = unreal.load_asset(SRC_SKEL)
    assert skel, 'UE4 mannequin skeleton missing: ' + SRC_SKEL
    wrong = [unreal.load_asset(SRC_DIR + n) for n in NAMES]
    wrong = [a for a in wrong if a.get_skeleton() != skel]
    if wrong:
        log('reloading %d source clips from disk (discarding unsaved skeleton swap)' % len(wrong))
        pkgs = [package_of(a) for a in wrong]
        try:
            unreal.EditorLoadingAndSavingUtils.reload_packages(
                pkgs, unreal.ReloadPackagesInteractionMode.ASSUME_POSITIVE)
        except Exception:
            unreal.EditorLoadingAndSavingUtils.reload_packages(pkgs)
    clips = [unreal.load_asset(SRC_DIR + n) for n in NAMES]
    still = [c.get_name() for c in clips if c.get_skeleton() != skel]
    assert not still, ('These clips are still not on UE4_Mannequin_Skeleton: %s. '
                       'Close the editor WITHOUT saving them (or revert them), then rerun.' % still)
    return clips


# ---------------------------------------------------------------- 2. rigs
def make_rig(name, mesh):
    path = RIG_DIR + '/' + name
    rig = unreal.load_asset(path) if EAL.does_asset_exist(path) else None
    if not rig:
        rig = TOOLS.create_asset(name, RIG_DIR, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    c = unreal.IKRigController.get_controller(rig)
    c.set_skeletal_mesh(mesh)
    assert c.apply_auto_generated_retarget_definition() is not False, 'auto characterize failed for ' + name
    try_call(c, 'apply_auto_fbik')
    EAL.save_loaded_asset(rig, False)
    return rig


def make_retargeter(src_rig, dst_rig, src_mesh, dst_mesh):
    name = 'RTG_UE4Mannequin_To_DKM'
    path = RIG_DIR + '/' + name
    rtg = unreal.load_asset(path) if EAL.does_asset_exist(path) else None
    if not rtg:
        rtg = TOOLS.create_asset(name, RIG_DIR, unreal.IKRetargeter, unreal.IKRetargetFactory())
    rc = unreal.IKRetargeterController.get_controller(rtg)
    rc.set_ik_rig(S, src_rig)
    rc.set_ik_rig(T, dst_rig)
    # 5.6+ op stack; harmless no-ops on older versions.
    if hasattr(rc, 'get_num_retarget_ops') and rc.get_num_retarget_ops() == 0:
        try_call(rc, 'add_default_ops')
    try_call(rc, 'assign_ik_rig_to_all_ops', S, src_rig)
    try_call(rc, 'assign_ik_rig_to_all_ops', T, dst_rig)
    try_call(rc, 'set_preview_mesh', S, src_mesh)
    try_call(rc, 'set_preview_mesh', T, dst_mesh)
    if not try_call(rc, 'auto_map_chains', unreal.AutoMapChainType.FUZZY, True):
        try_call(rc, 'auto_map_chains', unreal.AutoMapChainType.FUZZY, True, '')
    # Align target retarget pose with the UE4 A-pose so arm angles match.
    try_call(rc, 'auto_align_all_bones', T)
    EAL.save_loaded_asset(rtg, False)
    return rtg


# ---------------------------------------------------------------- 3. retarget
def retarget(clips, src_mesh, dst_mesh, rtg):
    EAL.make_directory(OUT_DIR)
    for n in NAMES:  # clean rerun
        for p in (OUT_DIR + '/' + n + SUFFIX, SRC_DIR + n + SUFFIX):
            if EAL.does_asset_exist(p):
                EAL.delete_asset(p)
    datas = [EAL.find_asset_data(c.get_path_name()) for c in clips]
    try:  # UE 5.8: (..., prefix, suffix, target_path, ...)
        made = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
            datas, src_mesh, dst_mesh, rtg, '', '', '', SUFFIX, OUT_DIR)
    except TypeError as e:
        log('note: 5.8 signature failed (%s), trying legacy' % e)
        made = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
            datas, src_mesh, dst_mesh, rtg, '', '', '', SUFFIX, False)
    out = {}
    for d in made:
        path = str(d.package_name)
        name = path.split('/')[-1]
        dest = OUT_DIR + '/' + name
        if path != dest:
            assert EAL.rename_asset(path, dest), 'move failed ' + path
        out[name[:-len(SUFFIX)] if name.endswith(SUFFIX) else name] = unreal.load_asset(dest)
    missing = [n for n in NAMES if n not in out]
    assert not missing, 'retarget produced no output for %s' % missing
    return out


def finish_clip(clip, dst_skel):
    assert clip.get_skeleton() == dst_skel, clip.get_name() + ' not on Dark Knight skeleton'
    clip.set_editor_property('enable_root_motion', True)
    clip.set_editor_property('force_root_lock', True)
    # Keep Dark Knight proportions exactly (rotations from the retarget only).
    assert unreal.BossBlendSpaceTools.normalize_player_attack_bone_lengths(clip)
    assert EAL.save_loaded_asset(clip, False)


# ---------------------------------------------------------------- 5. landmarks
def sample_hand(clip):
    frames = int(round(clip.get_play_length() * FPS))
    opts = unreal.AnimPoseEvaluationOptions()
    pts = []
    for f in range(frames + 1):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, min(f / FPS, clip.get_play_length()), opts)
        p = unreal.AnimPoseExtensions.get_bone_pose(pose, 'hand_r', unreal.AnimPoseSpaces.WORLD).translation
        pts.append((p.x, p.y, p.z))
    speed = [0.0]
    for a, b in zip(pts, pts[1:]):
        speed.append(((b[0] - a[0]) ** 2 + (b[1] - a[1]) ** 2 + (b[2] - a[2]) ** 2) ** .5 * FPS)
    smooth = [sum(speed[max(0, i - 1):i + 2]) / len(speed[max(0, i - 1):i + 2]) for i in range(len(speed))]
    return smooth


def find_swings(speed, count=5, min_gap=8):
    peaks = [i for i in range(1, len(speed) - 1) if speed[i] >= speed[i - 1] and speed[i] >= speed[i + 1]]
    peaks.sort(key=lambda i: -speed[i])
    chosen = []
    for p in peaks:
        if all(abs(p - q) >= min_gap for q in chosen):
            chosen.append(p)
        if len(chosen) == count:
            break
    assert len(chosen) == count, 'found only %d swings' % len(chosen)
    chosen.sort()
    bounds = [min(range(0, chosen[0] - 2), key=lambda i: speed[i]) if chosen[0] > 3 else 0]
    for a, b in zip(chosen, chosen[1:]):
        mid = (a + b) / 2.0  # among equally slow frames prefer the middle of the lull
        bounds.append(min(range(a + 2, b - 1), key=lambda i: (round(speed[i] / 10.0), abs(i - mid))))
    bounds.append(len(speed) - 1)
    segs = []
    for i, impact in enumerate(chosen):
        start, end = bounds[i], bounds[i + 1]
        strike = max(start + 1, impact - 3)
        segs.append(dict(start=start, strike=strike, impact=impact, end=end))
    return segs


# ---------------------------------------------------------------- 6. data
ROUTE = {'L': 0, 'LL': 1, 'LLL': 2, 'H': 3, 'HH': 4, 'LH': 3}
HEAVY = {'H', 'HH', 'LH'}


def wire(all_clip, segs):
    data = unreal.load_asset(DATA)
    assert data
    nodes = list(data.get_editor_property('nodes'))
    for node in nodes:
        ident = str(node.get_editor_property('id'))
        if ident not in ROUTE:
            continue
        s = segs[ROUTE[ident]]
        start, strike, impact, end = (s[k] / FPS for k in ('start', 'strike', 'impact', 'end'))
        end = min(end, all_clip.get_play_length())
        assert start < strike < impact < end, (ident, s)
        for key, value in dict(animation=all_clip, clip_start=start, strike_start=strike,
                               impact_time=impact, link_time=end, clip_end=end,
                               strike_seconds=.16 if ident in HEAVY else .12,
                               duration_beats=2.0 if ident in HEAVY else 1.0).items():
            node.set_editor_property(key, value)
    data.set_editor_property('nodes', nodes)
    assert EAL.save_loaded_asset(data, False)


def run():
    src_mesh = unreal.load_asset(SRC_MESH)
    dst_mesh = unreal.load_asset(DST_MESH)
    assert src_mesh and dst_mesh
    clips = restore_sources()
    EAL.make_directory(RIG_DIR)
    src_rig = make_rig('IK_UE4Mannequin_Grruzam', src_mesh)
    dst_rig = make_rig('IK_DarkKnight', dst_mesh)
    rtg = make_retargeter(src_rig, dst_rig, src_mesh, dst_mesh)
    out = retarget(clips, src_mesh, dst_mesh, rtg)
    dst_skel = dst_mesh.get_editor_property('skeleton')
    for clip in out.values():
        finish_clip(clip, dst_skel)
    all_clip = out[ALL_NAME]
    speed = sample_hand(all_clip)
    segs = find_swings(speed)
    wire(all_clip, segs)
    report = dict(clip=all_clip.get_path_name(), length=all_clip.get_play_length(),
                  segments=segs, hand_speed=[round(v, 1) for v in speed])
    path = Path(unreal.Paths.project_dir(), 'output', 'katana5_landmarks.json')
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, indent=1), encoding='utf-8')
    for i, s in enumerate(segs):
        log('swing%d frames start=%d strike=%d impact=%d end=%d' % (i + 1, s['start'], s['strike'], s['impact'], s['end']))
    log('DONE retargeted=%d data=%s' % (len(out), DATA))


run()
