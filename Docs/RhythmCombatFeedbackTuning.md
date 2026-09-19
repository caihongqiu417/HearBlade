# 节拍战斗反馈调参

## 资产入口

- `DA_PerfectDodgeFeedback`：`/Game/Combat/Feedback/DA_PerfectDodgeFeedback`
- 简化指令槽：`/Game/UI/RhythmCombat/T_HUD_RhythmRail_Clean`
- Boss 血条：`/Game/UI/RhythmCombat/T_HUD_BossHealthFrame`
- 连击音乐层：`/Game/Audio/SFX/SW_ComboTier1Low`、`SW_ComboTier2Snare`、`SW_ComboTier3High`
- 判定提示音：`SW_RhythmGood`、`SW_RhythmPerfect`、`SW_ComboTierUp`

## 当前规则

| Combo | 反馈档位 | 视觉倍率 | 音乐层 |
|---:|---:|---:|---|
| 0–3 | 0 | 1.00 | 基础鼓 |
| 4–7 | 1 | 1.15 | + 低频鼓身 |
| 8–15 | 2 | 1.30 | + 暗色军鼓 |
| 16+ | 3 | 1.45 | + 克制高频律动 |

音乐层从战斗开始就静音同步播放，在下一小节首拍改变目标音量，用半小节淡入。断连按同样方式退回基础层。

## 推荐调参顺序

1. 先调 `TierStrengths`，确认 4、8、16 连的差异清楚。
2. 再调 `FOVExpansionDegrees` 和 `CameraReboundStrength`。前者控制 35ms 左右的推近，后者控制随后向外回弹；总偏移由代码限制，靠墙时自动降为 25%。
3. 用 `HoldShakeDegrees` 调持续格挡底噪，用 `HoldImpactShakeDegrees` 调每次挡刀的短冲击。
4. 最后调三层音乐音量。建议保持低频层最大、高频层最小，避免盖住人声、刀声和指令提示。

## 无障碍选项

- `Full`：完整黑白撞色、裂环和镜头反馈。
- `ReducedFlash`：取消黑白冲击帧，保留较弱的金色轮廓和裂环。
- `NoFullscreen`：关闭全屏后处理，保留世界特效。
- `NoCameraShake`：关闭成功镜头震动和持续格挡震动。

## 音频来源

本轮新增的三层音乐和三个提示音由 `Scripts/generate_combo_music_layers.py` 程序化生成，不含外部采样。来源声明位于 `SourceArt/Audio/ComboLayers/LICENSE.txt`。
