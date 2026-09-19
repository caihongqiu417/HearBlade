import unreal

asset = unreal.load_asset("/Game/Combat/Feedback/DA_PerfectDodgeFeedback")
if not asset:
    raise RuntimeError("DA_PerfectDodgeFeedback is missing")

values = {
    "duration_seconds": 0.15,
    "ghost_lifetime_seconds": 0.22,
    "ghost_count": 3,
    "strong_beat_multiplier": 1.35,
    "fov_expansion_degrees": 2.0,
    "camera_shake_degrees": 0.18,
    "camera_lateral_shift_cm": 14.0,
    "camera_dolly_shift_cm": 7.0,
    "camera_yaw_kick_degrees": 0.32,
    "camera_pitch_kick_degrees": -0.12,
    "camera_rebound_strength": 1.5,
    "obstructed_strength": 0.25,
    "tier1_combo": 4,
    "tier2_combo": 8,
    "tier3_combo": 16,
    "tier_strengths": unreal.Vector4(1.0, 1.15, 1.30, 1.45),
    "impact_frame_seconds": 0.035,
    "impact_frame_cooldown": 0.5,
    "hold_shake_degrees": 0.05,
    "hold_impact_shake_degrees": 0.15,
    "hold_stop_seconds": 0.06,
    "regular_beat_ui_scale": 0.005,
    "downbeat_ui_scale": 0.009,
}
for name, value in values.items():
    asset.set_editor_property(name, value)

asset.set_editor_property("good_sound", unreal.load_asset("/Game/Audio/SFX/SW_RhythmGood"))
asset.set_editor_property("perfect_sound", unreal.load_asset("/Game/Audio/SFX/SW_RhythmPerfect"))
asset.set_editor_property("tier_up_sound", unreal.load_asset("/Game/Audio/SFX/SW_ComboTierUp"))

unreal.EditorAssetLibrary.save_loaded_asset(asset)
unreal.log("HighComboFeedback: configured DA_PerfectDodgeFeedback")
