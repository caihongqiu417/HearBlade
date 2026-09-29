# Phase two restrained timing notes

Generated with the built-in image_gen tool on 2026-09-28. Original RGBA pixels preserved. These replace only the phase-two notes. The HUD reuses the phase-one `T_HUD_RhythmRail_Clean` and shared timing-gate drawing, with the same frame dimensions and position. Neutral notes convey timing rather than a required button; the heavier ring indicates an area attack and retains the early charge band. No judgment, animation or DA timing values are changed.

Source: `C:/Users/26349/.codex/generated_images/01a0b42a-e9e4-7e62-b148-e5e6fda58531/exec-bb58f46f-4b9c-4ea9-aa93-0bc52f1b93d2.png`

Project PNG: `SourceArt/UI/RhythmCombat/PhaseTwo/T_HUD_PhaseTwo_NoteAtlas.png`

Runtime texture: `/Game/UI/RhythmCombat/T_HUD_PhaseTwo_NoteAtlas`

Import script: `Scripts/import_phase_two_note_ui.py`

Alpha checked: RGBA 1536 x 1024, transparent centers. HUD UV rectangles: normal (96,176)-(672,752), heavy (856,164)-(1456,764). Samples center the visible rings, rather than relying on the requested generation coordinates.

## Exact generation prompt

Use case: stylized-concept. Production transparent RGBA sprite atlas for HearBlade, a premium dark armored knight rhythm action game's HUD. This is a USABLE GAME ASSET, not a mockup, no scene or text. Canvas landscape 1536x1024. Two isolated rhythm-note icons, centers exactly at (384,512) and (1152,512), each contained within 500x500 square, generous empty transparent gutters. Left: one simple precise circular timing sigil: thin obsidian metal ring, fine ivory cutting edge, restrained antique champagne gold inset, two elegant small breaks at diagonal opposing points, four very subtle engraved notches, completely transparent hollow center. Strong clear circular silhouette at 36px game display, polished artisan detail without busy embellishment, no diamonds, no shield or sword pictogram. Right: matching heavy timing note: two concentric thin rings, outer ring slightly segmented in just four broad arcs, inner fine ivory edge, a little darker warm amber-gold inset; same hollow clear center. Readable at 48px, visibly heavier than left but just as clean. Style: precise hand-inked luxury weapon UI, black contour ensures readability on bright sky; flat front-facing orthographic icon, no perspective, no extrusion, no surrounding shards, no sparks, no flourishes, no glow cloud, no colored background, no checkerboard baked in. Entire background and both centers truly transparent. Minimal elegant refined metal and ivory contours, not shiny toy plastic, not neon. NO text, letters, labels, numbers or extra elements. Keep both perfectly centered in their separate halves.
