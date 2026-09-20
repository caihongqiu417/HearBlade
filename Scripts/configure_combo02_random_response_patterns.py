import unreal


ASSET_PATH = "/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_DragonKnight_Combo02_Beat"


def instruction(response, hold_marker=unreal.RhythmHoldMarkerType.NONE, hold_group=""):
    value = unreal.AttackResponseInstruction()
    value.set_editor_property("required_response", response)
    value.set_editor_property("hold_marker", hold_marker)
    value.set_editor_property("hold_group", hold_group)
    return value


def pattern(name, instructions):
    value = unreal.AttackResponsePattern()
    value.set_editor_property("pattern_name", name)
    value.set_editor_property("weight", 1.0)
    # Only impacts 2, 3 and 4 are replaced. Impact 1 and 5 remain authored.
    value.set_editor_property("first_impact_index", 1)
    value.set_editor_property("instructions", instructions)
    return value


combo = unreal.load_asset(ASSET_PATH)
if not combo:
    raise RuntimeError(f"Missing asset: {ASSET_PATH}")

markers = list(combo.get_editor_property("impact_markers"))
if len(markers) != 5:
    raise RuntimeError(f"Combo02 expected 5 impacts, got {len(markers)}")

patterns = [
    pattern("HoldChain", [
        instruction(unreal.RhythmResponseType.HOLD_BLOCK, unreal.RhythmHoldMarkerType.START, "Combo02Hold"),
        instruction(unreal.RhythmResponseType.HOLD_BLOCK, unreal.RhythmHoldMarkerType.CONTINUE, "Combo02Hold"),
        instruction(unreal.RhythmResponseType.HOLD_BLOCK, unreal.RhythmHoldMarkerType.END, "Combo02Hold"),
    ]),
    pattern("ParryParryCounter", [
        instruction(unreal.RhythmResponseType.BLOCK),
        instruction(unreal.RhythmResponseType.BLOCK),
        instruction(unreal.RhythmResponseType.COUNTER),
    ]),
    pattern("ParryDodgeCounter", [
        instruction(unreal.RhythmResponseType.BLOCK),
        instruction(unreal.RhythmResponseType.DODGE),
        instruction(unreal.RhythmResponseType.COUNTER),
    ]),
]

combo.set_editor_property("response_patterns", patterns)
unreal.EditorAssetLibrary.save_loaded_asset(combo)

saved = list(combo.get_editor_property("response_patterns"))
if len(saved) != 3:
    raise RuntimeError(f"Failed to save response patterns, count={len(saved)}")
unreal.log("Combo02RandomResponses: PASS patterns=HoldChain/ParryParryCounter/ParryDodgeCounter weights=1/1/1")
