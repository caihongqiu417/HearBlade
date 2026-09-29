# Blade Score (刃谱) art

Current textures are composed from the project's existing painted art by
`Scripts/compose_bladescore_art.py`:

| Texture | Source |
|---|---|
| T_BS_Blade | PhaseTwo blade rail (painted) + hilt |
| T_BS_Icon_Dodge / Block / Counter / Hold | RhythmResponses/Rhythm_*.png (painted) |
| T_BS_Icon_Either / Release | composites of the painted icons above |
| T_BS_Icon_Area | Combat/T_GroundBurstSigil.png (painted) |
| T_BS_Tag_Gap / Tag_Tail / Icon_Attack | PhaseTwo slash (painted) + Bangers font |
| T_BS_Word_Fill / Word_Attack / Word_Defend | Bangers font, ink outline |
| T_BS_Tsuba | old guard ring + painted shield + painted slash |
| T_BS_Hamon / Danger / Spark / Tick / TsubaGlow* / NotePlate | FX sprites (generate_bladescore_art.py) |

To replace any of them with a dedicated illustration, generate it with an image
tool, save as `SourceArt/UI/BladeScore/Painted/<TextureName>.png` (transparent
PNG, any size with the same aspect ratio), then run
`python Scripts/compose_bladescore_art.py`. The editor re-imports on next start.

Colour law for every prompt: RED/CRIMSON = the boss (things you must answer).
GOLD/CYAN = the player (your attack beats, windows, hits).

## Prompts (same house style as the PhaseTwo blade atlas)

**T_BS_Tsuba (square)**
Production transparent RGBA game HUD icon for HearBlade, a dark armored knight
rhythm action game. A round katana guard (tsuba) seen straight on, black lacquered
iron rim with fine ivory edge and antique gold inlay, split horizontally into two
halves: TOP half deep crimson enamel with a small bold white-and-steel shield
emblem (means DEFEND), BOTTOM half dark amber-gold enamel with a small bold ivory
sword-slash emblem (means ATTACK). A thin gold line divides the halves. Hand-inked
comic weapon UI, strong black contour, readable at 64 px, centered, generous
transparent margin, no text, no background, no glow cloud.

**T_BS_NotePlate (square)**
Transparent RGBA HUD plate for a boss attack note: a sharp diamond-shaped crimson
enamel plate with black ink contour and a thin bright red inner edge, darker centre
so a colourful icon placed on top stays readable. Comic ink style, flat, centered,
no icon inside, no text, no background.

**T_BS_Tick (32x64)**
Tiny transparent RGBA beat notch engraved on a blade spine: a short vertical ivory
bar with rounded ends and a thin black contour, flat, white so it can be tinted.
