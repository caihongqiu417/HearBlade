"""Update combo gameplay data without touching retargeted animation assets."""
import unreal
data = unreal.load_asset('/Game/Combat/Player/DA_PlayerAttackSet')
assert data
nodes = list(data.get_editor_property('nodes'))
full1=unreal.load_asset('/Game/Combat/Player/Animations/combo01_continuous')
full2=unreal.load_asset('/Game/Combat/Player/Animations/combo02_continuous')
assert full1 and full2, 'Run create_continuous_player_combos.py first'
# Source-frame landmarks from the sampled motion: include anticipation and the
# deceleration after the strike; linking no longer cuts through the fast sweep.
# start, strike-start, contact, link, settle-end, strike duration (seconds)
landmarks={
    'L':(0,6,8,14,20,.12), 'LL':(3,7,12,17,24,.14),
    'LLL':(10,15,17,23,30,.13), 'H':(0,9,12,17,24,.16),
    'HH':(10,16,18,24,31,.16), 'LH':(2,6,9,16,23,.15),
    'LLLL':(10,16,18,24,31,.14), 'LLLLH':(3,8,11,19,27,.17),
    'LHL':(3,7,12,17,24,.14), 'LHLH':(13,20,23,30,36,.17),
    'HL':(3,6,8,14,20,.12), 'HLL':(3,7,12,17,24,.14),
    'HLLH':(8,16,18,25,34,.17),
}
for node in nodes:
    node.set_editor_property('duration_beats', 1.0)
    ident=str(node.get_editor_property('id'))
    if ident in ('L','LL','LLL'):
        node.set_editor_property('animation',full1)
        continuous={'L':(3,6,8,42),'LL':(42,48,51,92),'LLL':(92,101,106,132)}[ident]
        start,strike,impact,end=continuous
        for key,value in dict(clip_start=start/30.,strike_start=strike/30.,impact_time=impact/30.,link_time=end/30.,clip_end=end/30.).items():
            node.set_editor_property(key,value)
        continue
    if ident in ('H','HH'):
        node.set_editor_property('animation',full2)
        continuous={'H':(3,9,12,33),'HH':(33,41,44,79)}[ident]
        start,strike,impact,end=continuous
        for key,value in dict(clip_start=start/30.,strike_start=strike/30.,impact_time=impact/30.,link_time=end/30.,clip_end=end/30.).items():
            node.set_editor_property(key,value)
        continue
    start,strike,impact,link,end,seconds=landmarks[ident]
    for key,value in dict(clip_start=start/30.,strike_start=strike/30.,impact_time=impact/30.,link_time=link/30.,clip_end=end/30.,strike_seconds=seconds).items():
        node.set_editor_property(key,value)
    if str(node.get_editor_property('id')) in ('HL', 'HLL', 'HLLH'):
        node.set_editor_property('resonance_only', False)
data.set_editor_property('nodes', nodes)
assert unreal.EditorAssetLibrary.save_loaded_asset(data)
unreal.log_warning('PLAYER_COMBO_RULES_UPDATED')
