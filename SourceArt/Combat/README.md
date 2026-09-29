# Ground burst artwork

`T_GroundBurstSigil.png` is original AI-generated artwork produced with the built-in image generation tool on 2026-09-27 for HearBlade. No external stock source was used. The original transparent PNG is preserved here; Unreal imports it into `/Game/Combat/Readability`.

The painted ring is decorative. `M_GroundBurstWarning` draws the actual radius mathematically, keeps the center transparent, and animates charge and the short impact wave independently. Gameplay collision never uses texture alpha. `AttackReadabilityComponent` spawns the project's existing `P_Spark` only after a resolved burst and retires these instances within 0.41 seconds.

Reimport/material authoring: `Scripts/create_attack_readability_assets.py`. Visual-only test command-line flag: `-PhaseTwoVisualTest` (non-shipping builds). It starts in phase two and captures warning/charge/impact frames; normal gameplay never enables this flag.

## Blade cue refinement

`Scripts/refine_blade_readability.py` authors the original procedural `M_AttackRecognition`: tapered brush cut, dark silhouette, ivory hot core, offset fine stroke and four small fragments. No stock art is used. The blade cue fades in/out and contracts gently while charging. `CueColor`, `Progress`, `UseIcon`, `Icon` and `RenderOpacity` remain supported. Ground-circle material and gameplay timestamps are unaffected. The full regeneration script applies this refinement last.
