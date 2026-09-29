"""Hi-Fi-Rush style combo table on the Katana clips.

Light  L-LL-LLL-LLLL : 5-hit clip, swings 1-4 (LLL is the kick). beats = hit->next hit on
                       the half-beat grid: .5/1/.5 -> 'da-da . da-da' (LL->LLL is a long
                       swing; at .5 it would play 2.4x speed).
Heavy  H-HH-HHH-HHHH : 4-hit clip, swings 1-4. Hi-Fi-Rush charge: beats = press->hit
                       (press on a beat, hold the chamber, fast strike half a beat later, on the 'and';
                       next press on the next beat;
                       the next heavy is pressed ON the hit). Half-beat steps: 1 / 1.5 / 2.
Mixed  L/LL + H      : 3-hit clip swing 3 (big finisher, half-beat charge).
       H-L-L         : 3-hit clip swings 1-2 (1 beat each), then H -> 4-hit swing 4.
Resonance-only nodes (LLLLH, LHL, LHLH) keep their authored clips.

Contact frames come from the per-frame hand/foot speed report
(output/katana_library.json): impact = fastest frame of the swing, boundaries
= the slowest frame between swings, strike start = 3 frames before impact.

Run after katana_library.py:  py D:/Udev/HearBlade/Scripts/wire_katana_combos.py
"""
import unreal

DATA = '/Game/Combat/Player/DA_PlayerAttackSet'
C5 = '/Game/Combat/Player/Animations/Katana5/Katana_Blade_Attack_5Combo_ALL_DKM'
C4 = '/Game/Combat/Player/Animations/KatanaLib/Combo4/Katana_Blade_Attack_4Combo_ALL_DKM'
C3 = '/Game/Combat/Player/Animations/KatanaLib/Combo3/Katana_Blade_Attack_3Combo_ALL_DKM'
FPS = 30.

# id: (clip, start, impact, end, input, beats, finisher, next_light, next_heavy, kick, damage, poise)
TABLE = {
    'L':    (C5, 4, 9, 15,   'LIGHT', .5, False, 'LL', 'LH', False, 16, 10),
    'LL':   (C5, 15, 24, 30, 'LIGHT', 1, False, 'LLL', 'LH', False, 18, 12),
    'LLL':  (C5, 30, 45, 49, 'LIGHT', .5, False, 'LLLL', 'LH', True, 22, 16),
    # 4th light = the clip's 5th attack (frames 76-103). Frames 49-76 are still the
    # kick landing + two small follow-up cuts, which made the old 4th hit read as a kick.
    'LLLL': (C5, 76, 103, 130, 'LIGHT', 1, True, '', 'LLLLH', False, 28, 22),
    'H':    (C4, 3, 10, 14,  'HEAVY', 1, False, 'HL', 'HH', False, 28, 26),
    'HH':   (C4, 14, 22, 29, 'HEAVY', 1, False, '', 'HHH', False, 32, 30),
    'HHH':  (C4, 29, 37, 46, 'HEAVY', 1, False, '', 'HHHH', False, 36, 34),
    'HHHH': (C4, 46, 49, 70, 'HEAVY', 1, True, '', '', False, 48, 46),
    'LH':   (C3, 70, 103, 110, 'HEAVY', 1, True, 'LHL', '', False, 38, 35),
    'HL':   (C3, 0, 11, 28,  'LIGHT', 1, False, 'HLL', '', False, 22, 18),
    'HLL':  (C3, 28, 58, 70, 'LIGHT', .5, False, '', 'HLLH', False, 24, 20),
    'HLLH': (C4, 46, 49, 70, 'HEAVY', 1, True, '', '', False, 55, 60),
}


def set_kick(node, value):
    for name in ('kick', 'b_kick'):
        try:
            node.set_editor_property(name, value); return
        except Exception:
            pass


def run():
    data = unreal.load_asset(DATA)
    clips = {p: unreal.load_asset(p) for p in (C5, C4, C3)}
    missing = [p for p, c in clips.items() if not c]
    assert not missing, 'run katana_library.py first; missing %s' % missing
    for c in clips.values():
        c.set_editor_property('enable_root_motion', True)
        c.set_editor_property('force_root_lock', True)
        unreal.EditorAssetLibrary.save_loaded_asset(c, False)
    nodes = list(data.get_editor_property('nodes'))
    by_id = {str(n.get_editor_property('id')): n for n in nodes}
    for ident, (clip, s, i, e, kind, beats, fin, nl, nh, kick, dmg, poise) in TABLE.items():
        node = by_id.get(ident)
        if node is None:
            node = unreal.PlayerAttackNode(); nodes.append(node); by_id[ident] = node
        length = clips[clip].get_play_length()
        end = min(e / FPS, length)
        for key, value in dict(id=ident, input=getattr(unreal.PlayerAttackInput, kind), animation=clips[clip],
                               clip_start=s / FPS, strike_start=max(s + 1, i - 3) / FPS, impact_time=i / FPS,
                               link_time=end, clip_end=end, duration_beats=float(beats),
                               strike_seconds=.16 if kind == 'HEAVY' else .12, finisher=fin,
                               resonance_only=False, next_light=nl, next_heavy=nh,
                               damage=float(dmg), poise_damage=float(poise)).items():
            node.set_editor_property(key, value)
        set_kick(node, kick)
        unreal.log_warning('KATANA_COMBO %s frames %d/%d/%d beats=%.1f next=%s/%s' % (ident, s, i, e, beats, nl or '-', nh or '-'))
    data.set_editor_property('nodes', nodes)
    assert unreal.EditorAssetLibrary.save_loaded_asset(data, False)
    unreal.log_warning('KATANA_COMBO DONE nodes=%d' % len(nodes))


run()
