# Sword wind source

`swishes.zip` is the **Swishes Sound Pack** by artisticdude, downloaded from
https://opengameart.org/content/swishes-sound-pack on 2026-09-19. The author
publishes the pack under **CC0**.

`Scripts/prepare_boss_whoosh.py` uses the shortest swish-11/swish-13 recordings,
removes low-frequency rumble and emphasizes their upper-mid air transient. It converts the original
24-bit stereo files to 16-bit stereo PCM, normalizes and fades the result, and
writes the Unreal import sources `SW_BossBladeStrong.wav` and
`SW_BossBladeWeak.wav`. The original archive is retained here for provenance.
The resulting audio contains no synthesized metal chime.
