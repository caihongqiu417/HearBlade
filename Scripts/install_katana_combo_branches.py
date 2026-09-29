"""Connect the retargeted Katana 3/4-combo clips to the player attack graph.

Run in Unreal with -ExecutePythonScript after katana_library.py. Existing light
nodes and their Katana5 animation remain intact.
"""
import unreal

ROOT = '/Game/Combat/Player'
data = unreal.load_asset(ROOT + '/DA_PlayerAttackSet')
combo3 = unreal.load_asset(ROOT + '/Animations/KatanaLib/Combo3/Katana_Blade_Attack_3Combo_ALL_DKM')
combo4 = unreal.load_asset(ROOT + '/Animations/KatanaLib/Combo4/Katana_Blade_Attack_4Combo_ALL_DKM')
assert data and combo3 and combo4
assert combo3.get_skeleton() == combo4.get_skeleton()
for clip in (combo3, combo4):
    assert clip.get_editor_property('enable_root_motion')
    assert clip.get_editor_property('force_root_lock')

nodes = list(data.get_editor_property('nodes'))
by_id = {str(n.get_editor_property('id')): n for n in nodes}
assert all(key in by_id for key in ('L', 'LL', 'LLL', 'H', 'HH', 'LH', 'HL', 'HLL', 'HLLH'))

def edit(ident, clip, frames, beats, damage, poise, finisher, next_light='', next_heavy='', uppercut=False):
    # All sections come from one continuous clip. The first frame of a new
    # section equals the previous section's end frame, avoiding a standing reset.
    start, strike, impact, end = frames
    assert 0 <= start < strike < impact < end
    assert end / 30. <= clip.get_play_length() + .001
    node = by_id.get(ident) or unreal.PlayerAttackNode()
    node.set_editor_property('id', ident)
    values = dict(
        input=unreal.PlayerAttackInput.HEAVY if ident.endswith('H') else unreal.PlayerAttackInput.LIGHT,
        animation=clip, clip_start=start / 30., strike_start=strike / 30.,
        impact_time=impact / 30., link_time=end / 30., clip_end=end / 30.,
        duration_beats=float(beats), strike_seconds=.14,
        damage=float(damage), poise_damage=float(poise),
        finisher=finisher, resonance_only=False, uppercut=uppercut,
        next_light=next_light, next_heavy=next_heavy,
    )
    for key, value in values.items():
        node.set_editor_property(key, value)
    if ident not in by_id:
        nodes.append(node)
        by_id[ident] = node

# Four contacts are visible near frames 10, 22, 38 and 49. Preserve the
# authored between-strike poses; the gameplay clock decides when each hits.
edit('H', combo4, (3, 7, 10, 16), 1, 28, 26, False, 'HL', 'HH')
edit('HH', combo4, (16, 19, 22, 31), 1, 42, 40, False, '', 'HHH')
edit('HHH', combo4, (31, 34, 38, 43), 1, 50, 48, False, '', 'HHHH', uppercut=True)
edit('HHHH', combo4, (43, 46, 49, 70), 1, 65, 62, True)

# Three-combo has long anticipation and recovery. Only the short final strike
# follows an L or LL branch. HL and HLL use adjacent second/third sections.
edit('LH', combo3, (75, 98, 103, 110), 1, 38, 35, True, 'LHL')
edit('LLH', combo3, (75, 98, 103, 110), 1, 42, 38, True)
edit('HL', combo3, (34, 54, 59, 75), 1, 22, 18, False, 'HLL')
edit('HLL', combo3, (75, 98, 103, 110), 1, 28, 24, False, '', 'HLLH')
edit('HLLH', combo4, (43, 46, 49, 70), 1, 70, 70, True)

by_id['LL'].set_editor_property('next_heavy', 'LLH')
data.set_editor_property('nodes', nodes)
assert unreal.EditorAssetLibrary.save_loaded_asset(data)
unreal.log_warning('KATANA_COMBO_BRANCHES_OK nodes=%d routes=HHHH,LLH,HLLH' % len(nodes))
