# Katana attack routes

Outside a Boss defense window, LMB is light (L) and RMB is heavy (H). Inside
the defense window, LMB remains counter and RMB remains block. Gamepad light
and heavy actions enter the same attack graph.

| Input | Motion |
| --- | --- |
| L → L → L | Existing Katana5 light sequence; third hit is the kick. |
| H → H → H → H | Four adjacent sections of `Katana_Blade_Attack_4Combo_ALL_DKM`; third hit is an uppercut and fourth hit finishes. |
| L → H or L → L → H | Final section of `Katana_Blade_Attack_3Combo_ALL_DKM`. |
| H → L → L | Second and third sections of that 3-combo clip. |
| H → L → L → H | Fourth section of the 4-combo clip finishes the mixed route. |

Light and heavy nodes both advance one beat; this follows the four-hit clip's
natural contact spacing and prevents the heavy follow-through from playing in
slow motion. The first press during a
swing is buffered for the next contact cue. Further presses cannot accelerate
the attack; they mark the next hit off-beat. Attack grades use the actual press
timestamp. The 3/4-combo assets have root motion enabled and root lock on, so
the existing capsule-root extraction and hit feedback apply to every new node.

The sections and graph links are installed by `Scripts/install_katana_combo_branches.py`
after `Scripts/katana_library.py`. `-PlayerComboProbe` checks five routes and a
20 Hz mashing interval; `HearBlade.Combat.PlayerAttackAssets` checks the asset
boundaries and reachability.

HHH only: a confirmed sword hit interrupts the Boss and raises its skeletal
meshes briefly while the collision capsule remains grounded. The camera tilts
up, punches in, and settles before the next beat. A mixed light/heavy branch
starts its crossfade earlier when buffered; a late press gets a short minimum
blend instead of snapping into the new source pose.
