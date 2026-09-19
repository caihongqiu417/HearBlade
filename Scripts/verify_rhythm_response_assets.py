import unreal


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


combo01 = unreal.load_asset("/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_DragonKnight_Combo01_Beat")
combo02 = unreal.load_asset("/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_DragonKnight_Combo02_Beat")
visuals = unreal.load_asset("/Game/UI/RhythmCombat/Responses/DA_RhythmResponseVisuals")
require(combo01 and combo02 and visuals, "Required rhythm response assets are missing")

m1 = list(combo01.get_editor_property("impact_markers"))
m2 = list(combo02.get_editor_property("impact_markers"))
require(len(m1) == 3, f"Combo01 marker count is {len(m1)}, expected 3")
require(len(m2) == 5, f"Combo02 marker count is {len(m2)}, expected 5")
require([m.get_editor_property("required_response") for m in m1] == [
    unreal.RhythmResponseType.DODGE,
    unreal.RhythmResponseType.BLOCK,
    unreal.RhythmResponseType.COUNTER,
], "Combo01 response sequence is invalid")
require([m.get_editor_property("required_response") for m in m2] == [
    unreal.RhythmResponseType.DODGE,
    unreal.RhythmResponseType.HOLD_BLOCK,
    unreal.RhythmResponseType.HOLD_BLOCK,
    unreal.RhythmResponseType.HOLD_BLOCK,
    unreal.RhythmResponseType.COUNTER,
], "Combo02 response sequence is invalid")
require([m.get_editor_property("hold_marker") for m in m2[1:4]] == [
    unreal.RhythmHoldMarkerType.START,
    unreal.RhythmHoldMarkerType.CONTINUE,
    unreal.RhythmHoldMarkerType.END,
], "Combo02 hold start/continue/end sequence is invalid")
require(len({str(m.get_editor_property("hold_group")) for m in m2[1:4]}) == 1,
        "Combo02 hold markers do not share one group")

for prop in ("dodge", "block", "counter", "hold_block"):
    response = visuals.get_editor_property(prop)
    require(response.get_editor_property("icon") is not None, f"{prop} default icon is missing")
require(visuals.get_editor_property("hold_start_icon") is not None, "Hold start icon is missing")
require(visuals.get_editor_property("hold_track_texture") is not None, "Hold track texture is missing")
require(visuals.get_editor_property("hold_end_icon") is not None, "Hold end icon is missing")
require(visuals.get_editor_property("approach_sound") is not None, "Approach sound is missing")
require(visuals.get_editor_property("strong_approach_sound") is not None, "Strong approach sound is missing")

for path in (
    "/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Defense01_Start",
    "/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Defense01_Loop",
    "/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Defense01_End",
    "/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Defense01_CounterAttack",
    "/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Anim_DKM_Hit_Alert_Fwd",
):
    require(unreal.load_asset(path) is not None, f"Defense animation is missing: {path}")

unreal.log("RhythmResponseVerification: PASS Combo01=Dodge/Block/Counter Combo02=Dodge/HoldStart/Hold/HoldEnd/Counter visuals=complete animations=complete")
