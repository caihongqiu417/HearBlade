"""Combo02: keep only the LAST of its consecutive counter notes; the ones before it
become ordinary blade notes (DodgeOrBlock). Edits the data asset itself, including
every random response pattern, then saves it.

Run in the Unreal editor (Output Log, Python):
    py D:/Udev/HearBlade/Scripts/fix_combo02_counters.py
"""
import unreal

PATH = '/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_DragonKnight_Combo02_Beat'
COUNTER = unreal.RhythmResponseType.COUNTER
ORDINARY = unreal.RhythmResponseType.DODGE_OR_BLOCK

asset = unreal.EditorAssetLibrary.load_asset(PATH)
if not asset:
    raise RuntimeError('missing ' + PATH)

markers = list(asset.get_editor_property('impact_markers'))
patterns = list(asset.get_editor_property('response_patterns'))


def name(r):
    return str(r).split('.')[-1]


def fix_sequence(get, set_ordinary, count, label):
    seq = [get(i) for i in range(count)]
    changed = []
    for i in range(count - 1):
        if seq[i] == COUNTER and seq[i + 1] == COUNTER:
            set_ordinary(i)
            changed.append(i + 1)
    unreal.log('Combo02 %s: %s -> hits %s now ordinary' % (label, [name(r) for r in seq], changed or 'none'))


# 1) The authored markers.
def m_get(i):
    return markers[i].get_editor_property('required_response')


def m_set(i):
    markers[i].set_editor_property('required_response', ORDINARY)


fix_sequence(m_get, m_set, len(markers), 'markers')

# 2) Each random pattern: its instructions replace the markers from FirstImpactIndex.
for p in patterns:
    first = p.get_editor_property('first_impact_index')
    instr = list(p.get_editor_property('instructions'))

    def p_get(i, first=first, instr=instr):
        k = i - first
        if 0 <= k < len(instr):
            return instr[k].get_editor_property('required_response')
        return markers[i].get_editor_property('required_response')

    def p_set(i, first=first, instr=instr):
        k = i - first
        if 0 <= k < len(instr):
            instr[k].set_editor_property('required_response', ORDINARY)
        else:
            markers[i].set_editor_property('required_response', ORDINARY)

    fix_sequence(p_get, p_set, len(markers), 'pattern %s' % p.get_editor_property('pattern_name'))
    p.set_editor_property('instructions', instr)

asset.set_editor_property('impact_markers', markers)
asset.set_editor_property('response_patterns', patterns)
unreal.EditorAssetLibrary.save_loaded_asset(asset)
unreal.log('Combo02: saved ' + PATH)
