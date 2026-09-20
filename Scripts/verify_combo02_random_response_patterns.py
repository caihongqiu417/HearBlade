import unreal


combo = unreal.load_asset("/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_DragonKnight_Combo02_Beat")
if not combo:
    raise RuntimeError("Combo02 data asset is missing")

patterns = list(combo.get_editor_property("response_patterns"))
if len(patterns) != 3:
    raise RuntimeError(f"Expected 3 Combo02 response patterns, got {len(patterns)}")

expected = {
    "HoldChain": [unreal.RhythmResponseType.HOLD_BLOCK] * 3,
    "ParryParryCounter": [unreal.RhythmResponseType.BLOCK, unreal.RhythmResponseType.BLOCK, unreal.RhythmResponseType.COUNTER],
    "ParryDodgeCounter": [unreal.RhythmResponseType.BLOCK, unreal.RhythmResponseType.DODGE, unreal.RhythmResponseType.COUNTER],
}

for value in patterns:
    name = str(value.get_editor_property("pattern_name"))
    instructions = list(value.get_editor_property("instructions"))
    responses = [item.get_editor_property("required_response") for item in instructions]
    if value.get_editor_property("first_impact_index") != 1:
        raise RuntimeError(f"{name}: expected first impact index 1")
    if responses != expected.get(name):
        raise RuntimeError(f"{name}: unexpected responses {responses}")

unreal.log("Combo02RandomResponsesVerification: PASS")
