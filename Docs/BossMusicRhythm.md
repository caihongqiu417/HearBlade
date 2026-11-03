# Boss 与音乐节奏

战斗关卡运行时，Boss 使用同一个 `BeatClockSubsystem` 驱动攻击、漫步和火焰。行为树随机选择连招，第一击与闪现对齐目标拍点；漫步不等待固定歌曲秒数。

在行为树的 **节拍攻击（下一可用拍）** 节点中调整：

- `Walk Cycle Beats`：一轮行走动画占多少拍，默认 2 拍。
- `Walk Beat Accent Strength`：每拍的步速与步行动画速度起伏幅度，默认 0.16；设为 0 可关闭步伐重音。

Boss 蓝图的 `BeatSyncedComboComponent` 上的 `Fire Beat Pulse Strength` 控制常驻火焰在拍点的放大幅度，默认 0.10；设为 0 可关闭。行走动画的基础播放速度会随 BPM 调整，在拍点附近会轻微加速，根运动提取使用相同的播放倍率，避免动画和实际移动速度分离。这个效果让漫步有音乐律动，但不会把每次脚掌落地硬锁到拍点。

当前连招数据保持原值。若日志出现“第 2/3 击与目标拍点相差”警告，需要在节拍编辑器内调对应连招的攻击间隔或动画倍率；漫步节奏设置不会替连招消除这些误差。

## 强拍闪现与声音

连招第一击若落在小节第 1 拍，Boss 从前一拍开始蓄势：脚下火焰逐渐收拢，剑身火焰材质发亮，持剑处的暖色光增强。最后半拍在锁定落点显示收缩火焰；第一击拍点闪现后，火焰从原地和落点爆开。身体网格上另有独立叠加材质 `M_BossBeatOverlay`：强拍形成明显的橙金色能量轮廓与移动光带，弱拍产生较短的青蓝色扫光。每次出刀拍位都会分别触发该材质，原有护甲材质保持不变。落点被障碍挡住而取消攻击时不会提前触发强拍轮廓。

`BeatSyncedComboComponent` 的 **强拍** 分类可调 `Strong Beat Charge Beats` 和 `Strong Beat Sword Glow Multiplier`；**强弱拍** 分类可调叠加材质、强弱拍持续时间及刀声增益。刀声在每击的动画攻击标记触发；小节第 1 拍使用强刀声，其余拍位较轻。这个标记代表出刀时刻，不代表实际伤害已命中。

关卡中的 `BeatClockTestActor` 的 **战斗鼓点** 分类控制叠加在歌曲上的专用战斗鼓组。默认鼓组是严格按 104 BPM 制作的一小节循环：第 1、3 拍由低频底鼓定锚，第 2、4 拍由偏暗的鼓皮声强调，八分音符用很轻的通鼓维持律动。当前版本移除了长镲和突出的高频踩镲。默认鼓组音量为 1.35，原曲混音音量为 0.78。`Combat Drum Stem Volume` 控制鼓组响度，`Music Mix Volume` 给原曲让出空间，关闭 `Enable Combat Drums` 可以整体禁用。

鼓组与 BGM 使用两个音频组件同时播放，并根据原曲拍点相位从循环中的对应位置起播。循环本身在音频线程连续播放，因此低帧率或游戏线程卡顿不会让每一拍重新触发并逐渐漂移。`Use Looping Drum Stem` 关闭或鼓组资产缺失时，系统才回退到旧的逐拍底鼓/轻敲击方式。刀声仍由动画攻击标记提前触发，方便让剑风峰值抵达命中帧。

默认鼓组资产：`/Game/Audio/SFX/SW_BossCombatDrumLoop`。源文件在 `SourceAudio/BossRhythm/SW_BossCombatDrumLoop.wav`，可用 `Scripts/generate_combat_drum_loop.py` 重新生成。

Boss 刀声现在使用项目内 `/Game/Enemies/DF_DRAGON_KNIGHT/SOUND/SW_DragonKnight_SwordSlice`，其源文件是用户提供的 `dragon-studio-sword-slice-393847.mp3`。源音效约 2.19 秒，主要扫风峰值位于源文件约 0.264 秒；运行时从 0.15 秒开始读取，并在动画攻击标记前约 0.15 秒启动，使主要峰值落在刀锋帧前约 30 毫秒。强拍使用 0.98 音高，弱拍使用 1.05 音高，二者仍是同一个音源。声音非空间化播放，每击同时短暂降低歌曲音量。`Blade Sound Start Time Seconds`、`Blade Whoosh Lead Seconds`、`Blade Sfx Gain`、强弱音量和音乐闪避均可在 `BeatSyncedComboComponent` 中调整。

默认强拍刀声音量为 1.0，弱拍为 0.9；出刀瞬间歌曲分别降到原音量的 0.38 / 0.58，之后平滑恢复。若觉得音乐被压得太明显，先将两个 `Music Gain` 调高；若刀声本身仍不喜欢，可替换 `SW_BossBladeStrong`、`SW_BossBladeWeak`，无需改动连招时间。
# 玩家节奏闪避闭环（第一轮）

- 玩家角色自动带有 `PlayerDefenseComponent`：100 生命、100 共鸣、Combo 与最近一次闪避判定。
- 左 Shift／手柄 B 启动方向闪避，持续 0.62 秒；动画 Root Motion 以 0.32 倍位移驱动胶囊体，实测移动约 160～180 厘米且结束时不会拉回。无敌帧为动作开始后 0.08～0.42 秒。
- Boss 攻击数据的每个 `ImpactMarker` 保存伤害、理想闪避提前量及真实伤害窗口。
- Boss 刀剑网格在窗口中持续扫掠玩家胶囊体；只有实际接触才产生判定或伤害。
- PERFECT 为 ±60ms，GOOD 为 ±120ms。BAD 不取消有效无敌帧，MISS 才代表没有有效闪避并受到正常伤害。
- PERFECT/GOOD 增加 Combo 和共鸣；BAD/MISS 清空 Combo，但不额外扣除共鸣。
- HUD 由 `RhythmCombatHUD` 绘制：生命和共鸣位于左上角，右侧判定轨道显示 PERFECT／GOOD／BAD／MISS 与 Combo。文字使用导入的 Bangers 艺术字纹理；血量和共鸣使用贴合外框斜边的四边形填充，音乐脉冲只改变外框。
- 刀刃真正扫到玩家时按碰撞和无敌帧结算。若玩家通过翻滚位移使刀刃完全落空，伤害窗口结束时仍会按这次闪避输入时间结算判定；同一次翻滚覆盖多刀时只奖励一次，低帧率跨过整个短伤害窗口也不会丢失结算。
- 调试日志关键词：`RhythmDodge`、`BossDamageSweep`。
