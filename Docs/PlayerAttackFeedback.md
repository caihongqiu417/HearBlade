# Player attack feedback

## Controls

- Player health is 500 (five times the original 100), including existing Blueprint templates at game startup. Defeat displays an explicit Enter-to-retry prompt.

- Outside an imminent enemy strike: LMB light, RMB heavy. F remains a direct heavy shortcut.
- During the defense window (normally 300 ms before impact through 120 ms after, limited by the live cue): LMB counter, RMB block. Q remains a direct counter shortcut; Shift dodges. Ground bursts do not enable mouse block/counter mode.
- Each press chooses exactly one role. Releasing an attack press never releases guard; holding a key through a context change never synthesizes another press.
- R: arm full resonance for the next bar.
- The main ring contracts once per quarter-note beat (104 cycles/min at 104 BPM). Press at overlap. The secondary half-beat dot is a musical subdivision; ordinary and resonance attack chains currently advance on whole beats.
- PERFECT / GOOD / EARLY / LATE report input timing; +N reports resonance actually earned by a hit.
- Attack playback disables the additive idle beat bounce.

## Animation repair

The imported clips shared the player's skeleton object but retained source bone translations: spine_02 was roughly 20 cm versus the player's 6.795 cm. Player playback copies now use the target reference bone translations and scales while retaining animated rotations and root/pelvis motion. Original `/Game/InsaneAirCombo` assets are untouched. The setup script reapplies this repair through `NormalizePlayerAttackBoneLengths`.

The old spine translation pulse is disabled to avoid changing torso length. The L/LL/LLL nodes now use adjacent slices of the authored full `combo01` timeline (frames 3–42, 42–92 and 92–132); H/HH use adjacent slices of full `combo02` (frames 3–33 and 33–79). The end pose of each slice is exactly the start pose of the next. Each slice reaches contact in 180 ms for light or 240 ms for heavy, then plays its continuous recovery until the next beat. Root entry blends over 45 ms and chained entries over 55 ms. Defense/counter playback is unchanged.

Ordinary attacks own a separate montage reference, independent of the defense animation release timer. Musical positioning uses an active montage at zero play rate. One follow-up input can be queued during a swing; ordinary input during recovery is retained for 220 ms. Accepted defense inputs clear pending ordinary inputs. Attack startup clears old guard-release and defense-buffer requests. A real hit still interrupts attacks; resonance expiry itself no longer cancels an in-flight swing.

Accepted defense playback explicitly interrupts the ordinary attack component before playing the original guard/counter clip. A leftover attack montage reference must never silently reject defense playback. Defense blends in over 70 ms, and the animation regression checks actual montage advancement and hand poses after switching from an ordinary attack.

Mouse defense routing includes the most recent impact marker for its late judgment window. Advancing the Boss marker index at the nominal beat must not switch the mouse back to ordinary attacks while that strike can still make contact. The HUD also labels individual cues with their required response.

Ordinary attack playback copies now have enable_root_motion=True and force_root_lock=True. Because their montages are positioned explicitly, the attack component extracts each authored root delta between consecutive sampled positions and applies horizontal translation/yaw once through CharacterMovement with a capsule sweep. IgnoreRootMotion on the shared animation instance strips the root from the displayed pose and prevents a second engine application; it does not disable this explicit attack extraction. The old fixed-distance attack push is removed, and movement input is suppressed during the attack motion. Ground height/gravity remain owned by CharacterMovement. Defense, counter and locomotion assets are unchanged. Run -PlayerRootMotionProbe to verify capsule travel in an unobstructed swing.

## Blood

Blood uses the project's `Blood_VFX_Pack` non-Stylized Niagara systems: `P_Splash_HitWithMetal_01` and `P_BigSplash_HitWithMetal` for Boss wounds, `P_Splash_Hit_01` and `P_SplashWithBurst_Hit_01` for player wounds. Player sword contacts pass the actual intersection point to the Boss blood spawner. Emission lasts 0.12 seconds, with a 0.65-second retirement tail. The extra InsaneAirCombo hit sparks were removed to avoid stacking unrelated hit effects.

## Input regression probe

Launch a development game with `-PlayerComboProbe` (`-PlayerAttackInputProbe` is a compatibility alias). It verifies LLL, LH, LLH, HHHH and HLLH, then sends sustained 20 Hz attack input and checks the number of actual swings. The September 29 runtime test completed all five routes and produced 9 swings over 10 beats during rapid input. Mouse defense still has a separate real-input contact probe below.

## Ordinary combos

L means light (LMB), H means heavy (RMB). Normal routes include LLL, LH, LLH, HHHH and HLLH. Resonance additionally permits LLLLH and LHLH. The first press during a swing is buffered toward its next contact cue; extra presses do not accelerate the route and lose the timing reward. Both light and heavy nodes advance one beat, matching the four-hit source's natural spacing. Contact still lands on the music grid, and the original press time determines the grade. See `Docs/KatanaComboRoutes.md` for the new clip mapping and inputs.

## Eight-direction locomotion

`/Game/Combat/Player/BS_PlayerEightWay` contains idle, eight walk samples, eight run samples and an outer pursuit ring. It uses playback copies of the already-retargeted InsaneAirCombo strafe clips plus `run_fast`; source animations are untouched. Local actual velocity drives the Forward/Right axes. Root motion is disabled and root lock enabled on the copies.

Free running is 500 cm/s by default; lock-on movement can reach 360 cm/s. Moving toward a locked target more than 450 cm away enters pursuit at 650 cm/s, returning to the normal gait within 300 cm or when movement intent changes. The distance hysteresis prevents rapid gait switching. Attacks, defense and hit reactions retain full-body animation priority. `HearBlade.Combat.PlayerEightWay` checks all eight run samples and their evaluated moving foot poses; `-PlayerLocomotionProbe` checks runtime movement, pursuit and stopping.

`HearBlade.Combat.PlayerAttackTiming` covers both defense-window edges, expiration, beat-grid scheduling and contextual default keys. Ordinary input never updates the counter timing history; accepted defense requests clear pending ordinary actions. A release follows the press's role even if the music advances across a window edge.

`-PlayerDefenseContactProbe` injects real LMB/RMB events against a transient Boss combo with alternating block/counter requirements. It intentionally fails the first strike, then verifies physical sword contacts, successful RMB block and LMB counter judgments and no ordinary attacks. The September 28 run passed both Perfect block and Perfect counter with zero damage. `HearBlade.Combat.AnimationBlend` also passed the ordinary-to-guard and ordinary-to-counter montage/pose regression.

## Hit response

Hit input recovery uses its own deadline: 220 ms for a light hit and 280 ms for a poise break, independent of the longer visible recoil. Inputs queued during this lock survive through the unlock frame and retain their original rhythm timestamp. A new damaging hit clears obsolete queued actions. Releasing a looping guard uses the actual block-start time instead of its animation sentinel, preventing an indefinite guard recovery.

Unarmored Boss hits retrigger the directional reaction. Hits during an armored attack add a short spine flinch without stopping the Boss attack; poise breaks still play the full interrupt reaction. Ordinary sword contact no longer freezes the pose and jumps forward to catch up with music. This removes the discontinuity after contact while retaining camera, sound and Boss reaction feedback. Camera impact is stronger, while the global clock is unchanged. The runtime attack probe exercised normal reactions, armored flinches, a poise break and natural resonance accumulation to 100.

## Audio

Existing project SoundWaves are reused, without modifying the Boss assets:

- `/Game/Audio/SFX/SW_BossBladeWeak`: light swing.
- `/Game/Audio/SFX/SW_BossBladeStrong`: heavy swing.
- `/Game/Enemies/DF_DRAGON_KNIGHT/SOUND/SW_DragonKnight_SwordSlice`: contact layer, faded out over 0.28 seconds.
- `/Game/Audio/SFX/SW_RhythmPerfect`: additional on-beat hit feedback.

Swing is triggered once per attack at 70 ms before its musical impact. Contact requires a real weapon intersection and is deduplicated per target per attack. An interrupted attack cannot trigger a later scheduled swing. Off-beat attacks retain physical sound feedback.

The swish source provenance is documented in `Scripts/prepare_boss_whoosh.py`. `Scripts/audit_player_audio.py` enumerates actual registered audio assets and durations.

## Beat ring artwork

Generated with the built-in imagegen tool; source PNG: `SourceArt/UI/RhythmCombat/T_HUD_PlayerBeatRing.png`. Imported texture: `/Game/UI/RhythmCombat/T_HUD_PlayerBeatRing`. Transparent center/background, UI compression, no mipmaps. The HUD drives scale, tint and beat flash from the music clock.

Generation prompt:

> Create ONE game HUD beat timing ring texture, square 1024x1024. Transparent background AND transparent hollow center. The ring is centered precisely, circular, with radius about 390 pixels. Premium rhythm-action comic/manga game UI: crisp warm ivory-white luminous main ring with a thin charcoal ink edge, tiny angular segmented accents at cardinal directions, subtle pale golden highlight. Mostly neutral white so the game can tint it gold or cyan. A single ring only, no other circles, no text, letters, numbers, icons, scenery, panels, shadows outside the ring or filled background. High contrast readable silhouette when displayed at 48 to 100 pixels. Ring thickness about 24 pixels, restrained glow no more than 12 pixels. Clean polished game-ready transparent PNG asset. The inner 340 pixel radius must remain fully transparent.

## Continuous combo slicing

The split source clips each contained their own standing entry and recovery, which made every node visibly reset. Playback copies `combo01_continuous` and `combo02_continuous` preserve the full authored timelines while data nodes select only the required frame ranges. Music grades input and schedules adjacent slice starts; the motion advances monotonically from slice start through contact to the next boundary without a standing frame. Asset tests require shared source animations and exact matching boundaries. Boss default health is 1200.
