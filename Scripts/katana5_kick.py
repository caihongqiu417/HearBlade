"""Mark the 3rd light hit (LLL) as a kick and time it from the FOOT, not the hand.

Samples calf/foot/ball of both legs over the LLL slice of the retargeted _ALL
clip, takes the fastest-moving foot's peak as the contact frame, and writes
ImpactTime / StrikeStart / bKick into DA_PlayerAttackSet.

Run after compiling (needs the new bKick field):
    py D:/Udev/HearBlade/Scripts/katana5_kick.py
"""
import json
import unreal
from pathlib import Path

DATA = '/Game/Combat/Player/DA_PlayerAttackSet'
NODE = 'LLL'
FPS = 30.0


def log(m):
    unreal.log_warning('KATANA5_KICK ' + m)


def world(clip, t, bone):
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, t, unreal.AnimPoseEvaluationOptions())
    p = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD).translation
    return (p.x, p.y, p.z)


def dist(a, b):
    return sum((x - y) ** 2 for x, y in zip(a, b)) ** .5


def run():
    data = unreal.load_asset(DATA)
    nodes = list(data.get_editor_property('nodes'))
    node = next(n for n in nodes if str(n.get_editor_property('id')) == NODE)
    clip = node.get_editor_property('animation')
    start = node.get_editor_property('clip_start')
    end = node.get_editor_property('clip_end')
    f0, f1 = int(round(start * FPS)), int(round(end * FPS))
    report = {}
    best = (0.0, None, None)
    for bone in ('ball_l', 'ball_r', 'foot_l', 'foot_r'):
        pts = [world(clip, f / FPS, bone) for f in range(f0, f1 + 1)]
        speed = [0.0] + [dist(a, b) * FPS for a, b in zip(pts, pts[1:])]
        report[bone] = [round(v) for v in speed]
        i = max(range(len(speed)), key=lambda k: speed[k])
        if speed[i] > best[0]:
            best = (speed[i], bone, f0 + i)
    hand = [world(clip, f / FPS, 'hand_r') for f in range(f0, f1 + 1)]
    report['hand_r'] = [0] + [round(dist(a, b) * FPS) for a, b in zip(hand, hand[1:])]
    peak_speed, bone, impact = best
    assert bone, 'no foot motion found'
    impact = min(max(impact, f0 + 2), f1 - 2)
    strike = max(f0 + 1, impact - 3)
    log('fastest foot=%s speed=%.0fcm/s impact frame=%d (was %d) strike frame=%d' % (
        bone, peak_speed, impact, round(node.get_editor_property('impact_time') * FPS), strike))
    node.set_editor_property('impact_time', impact / FPS)
    node.set_editor_property('strike_start', strike / FPS)
    for name in ('kick', 'b_kick'):
        try:
            node.set_editor_property(name, True)
            break
        except Exception:
            continue
    else:
        raise RuntimeError('bKick field missing - compile the C++ first')
    data.set_editor_property('nodes', nodes)
    assert unreal.EditorAssetLibrary.save_loaded_asset(data, False)
    report['result'] = dict(bone=bone, impact=impact, strike=strike, frames=[f0, f1])
    Path(unreal.Paths.project_dir(), 'output', 'katana5_kick.json').write_text(json.dumps(report, indent=1), encoding='utf-8')
    log('DONE')


run()
