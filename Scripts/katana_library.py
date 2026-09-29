"""Katana move library: retarget selected Grruzam Katana (UE4 mannequin) clips onto
the Dark Knight in one pass, grouped by gameplay module.

Output:  /Game/Combat/Player/Animations/KatanaLib/<Module>/<Clip>_DKM
Report:  output/katana_library.json  (every clip: length, root travel, skeleton)

Re-running is safe: clips that already exist are skipped (set FORCE = True to redo).
Uses the retargeter created by katana5_setup.py (RTG_UE4Mannequin_To_DKM).
The report also holds per-frame hand/foot speeds used to pick contact frames.

Run:  py D:/Udev/HearBlade/Scripts/katana_library.py
Add a clip to a module: append its source name to MANIFEST and run again.
"""
import json
import unreal
from pathlib import Path

SRC = '/Game/GrruzamSwordPack/Animations/Katana_Blade(SK_Mannequin)/'
OUT = '/Game/Combat/Player/Animations/KatanaLib/'
RTG = '/Game/Combat/Player/Retarget/RTG_UE4Mannequin_To_DKM'
SRC_MESH = '/Game/GrruzamSwordPack/Character/SK_Mannequin/Mesh/SK_Mannequin'
SRC_SKEL = '/Game/GrruzamSwordPack/Character/SK_Mannequin/Mesh/UE4_Mannequin_Skeleton'
DST_MESH = '/Game/Characters/Dark_Knight/Dark_Knight_Male/Meshes/SKM_DKM_Full'
SUFFIX = '_DKM'
FORCE = False

# module -> (source sub-folder, [clip names without the 'Katana_Blade_' prefix], root motion)
MANIFEST = {
    # Attack combos only (heavy route = 4-hit combo, mixed branches = 3-hit combo).
    'Combo3': ('2_Attacks/0__3Combos', ['Attack_3Combo_ALL'], True),
    'Combo4': ('2_Attacks/1__4Combos', ['Attack_4Combo_ALL'], True),
    # Candidates for a real heavy charge (wind-up that keeps moving, then a big slash).
    'Skills': ('3_Skills', ['Skill_A', 'Skill_B', 'Skill_C', 'Skill_D', 'Skill_E', 'Skill_F',
                            'Skill_G_V1', 'Skill_G_V2', 'Skill_H_V1', 'Skill_H_V2', 'Skill_I'], True),
    'Dash': ('2_Attacks/3__Dash_Attack', ['Dash_Attack_ver_A', 'Dash_Attack_ver_B'], True),
}
# Clips whose per-frame limb speeds are dumped so contact frames can be chosen.
SAMPLE_BONES = ('hand_r', 'hand_l', 'ball_r', 'ball_l')

EAL = unreal.EditorAssetLibrary
AL = unreal.AnimationLibrary


def log(m):
    unreal.log_warning('KATANA_LIB ' + m)


def src_path(folder, name):
    sub, _, clip = name.rpartition('/')
    return SRC + folder + ('/' + sub if sub else '') + '/Katana_Blade_' + clip


def package_of(asset):
    for n in ('get_package', 'get_outermost'):
        fn = getattr(asset, n, None)
        if fn:
            return fn()


def restore(clips, skel):
    wrong = [c for c in clips if c.get_skeleton() != skel]
    if wrong:
        log('reloading %d clips from disk (unsaved skeleton swap)' % len(wrong))
        pkgs = [package_of(c) for c in wrong]
        try:
            unreal.EditorLoadingAndSavingUtils.reload_packages(pkgs, unreal.ReloadPackagesInteractionMode.ASSUME_POSITIVE)
        except Exception:
            unreal.EditorLoadingAndSavingUtils.reload_packages(pkgs)


def root_travel(clip):
    try:
        a = AL.get_bone_pose_for_time(clip, 'root', 0., False).translation
        b = AL.get_bone_pose_for_time(clip, 'root', clip.get_play_length(), False).translation
        return round(((b.x - a.x) ** 2 + (b.y - a.y) ** 2) ** .5, 1), round(b.z - a.z, 1)
    except Exception:
        return None, None


def limb_speeds(clip, fps=30.):
    """cm/s per frame for SAMPLE_BONES (component space, root locked)."""
    frames = int(round(clip.get_play_length() * fps))
    opts = unreal.AnimPoseEvaluationOptions()
    prev, out = None, {b: [] for b in SAMPLE_BONES}
    for f in range(frames + 1):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, min(f / fps, clip.get_play_length()), opts)
        cur = {}
        for b in SAMPLE_BONES:
            t = unreal.AnimPoseExtensions.get_bone_pose(pose, b, unreal.AnimPoseSpaces.WORLD).translation
            cur[b] = (t.x, t.y, t.z)
            out[b].append(0 if prev is None else round(sum((x - y) ** 2 for x, y in zip(cur[b], prev[b])) ** .5 * fps))
        prev = cur
    return out


def run():
    rtg = unreal.load_asset(RTG)
    src_mesh, dst_mesh = unreal.load_asset(SRC_MESH), unreal.load_asset(DST_MESH)
    skel = unreal.load_asset(SRC_SKEL)
    assert rtg and src_mesh and dst_mesh and skel, 'run katana5_setup.py first (it builds the retargeter)'
    dst_skel = dst_mesh.get_editor_property('skeleton')
    report = {}
    for module, (folder, names, root_motion) in MANIFEST.items():
        out_dir = OUT + module
        EAL.make_directory(out_dir)
        todo = []
        for name in names:
            clip_name = name.rpartition('/')[2]
            dest = out_dir + '/Katana_Blade_' + clip_name + SUFFIX
            if EAL.does_asset_exist(dest) and not FORCE:
                continue
            if EAL.does_asset_exist(dest):
                EAL.delete_asset(dest)
            path = src_path(folder, name)
            src = unreal.load_asset(path)
            if not src:
                log('MISSING source ' + path)
                continue
            todo.append(src)
        if todo:
            restore(todo, skel)
            todo = [unreal.load_asset(t.get_path_name().split('.')[0]) for t in todo]
            datas = [EAL.find_asset_data(t.get_path_name()) for t in todo]
            unreal.IKRetargetBatchOperation.duplicate_and_retarget(
                datas, src_mesh, dst_mesh, rtg, '', '', '', SUFFIX, out_dir)
        report[module] = []
        for name in names:
            dest = out_dir + '/Katana_Blade_' + name.rpartition('/')[2] + SUFFIX
            clip = unreal.load_asset(dest) if EAL.does_asset_exist(dest) else None
            if not clip:
                log('NOT CREATED ' + dest)
                continue
            if clip.get_skeleton() == dst_skel:
                clip.set_editor_property('enable_root_motion', root_motion)
                clip.set_editor_property('force_root_lock', True)
                unreal.BossBlendSpaceTools.normalize_player_attack_bone_lengths(clip)
                EAL.save_loaded_asset(clip, False)
            travel, rise = root_travel(clip)
            report[module].append(dict(clip=dest, length=round(clip.get_play_length(), 3),
                                       travel_cm=travel, rise_cm=rise,
                                       ok=clip.get_skeleton() == dst_skel, speeds=limb_speeds(clip)))
        log('%s: %d clips' % (module, len(report[module])))
    p = Path(unreal.Paths.project_dir(), 'output', 'katana_library.json')
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(report, indent=1), encoding='utf-8')
    log('DONE modules=%d clips=%d' % (len(report), sum(len(v) for v in report.values())))


run()
