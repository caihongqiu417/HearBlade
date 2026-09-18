# HearBlade 节拍动作编辑器

在 UE 5.8 的 **工具 → 节拍连招编辑器** 打开。每份 `AttackData` 配置一个可由行为树调度的攻击 Montage；这个 Montage 可以包含多次命中。行为树决定何时调用，运行时选下一可用拍；不保存歌曲中的固定出招时间。

1. 默认打开 `DA_DragonKnight_Combo01_Beat`，它直接引用流畅的完整 `Combo01_Montage`，已经有三次命中标记和逐段速度。下方可检查 `ImpactMarkers`、`ClipStartSeconds`、`ClipEndSeconds`；时间值使用原 Montage 的时间坐标。
2. 加载 WAV，设置 BPM、原音频拍相位和播放起点。点击时间轴或填写“预览发令(原音频秒)”来模拟行为树在那个时刻请求攻击。紫线是发令，红线是排到下一可用拍的第一击，橙线是后续击，绿线是选定片段起止。
3. “自动匹配速度”让完整 Montage 同速；“自动逐击对齐”按三次命中标记分别计算击间速度，也可手动微调。检查最大打击偏差。
4. 点“保存到攻击数据”。保存动作片段、速度与音乐参考参数；**预览发令时间不保存**。选择不同的预览时刻，确认总会找当时的下一可用拍。
5. 创建 Blackboard，加入名为 `BossActor`、类型为 Object（Actor）的键；创建使用该 Blackboard 的 Behavior Tree。在树中放置“节拍攻击（下一可用拍）”任务。任务默认指向完整 Combo01 攻击数据，让 `BossActorKey` 指向该黑板键。任务会等待整套三击连招播放完才成功。以后可在行为树中按距离、状态选择其他攻击数据。
6. 现有 Boss 蓝图是 Actor，不能直接由 AIController 占有。在关卡中放置 `BossBeatDirector`，把 `BossActor` 指向场景中的 `BP_Dragon_Knight_UE5_Fire`，把 `BehaviorTreeAsset` 指向新建的树。调度器启动行为树并把 Boss 引用写进黑板。Boss 已有 `BeatSyncedComboComponent`；场景中仍需由 `BP_BeatClockTestActor` 先启动音乐时钟。

参考 WAV 仅供编辑器显示。音频波形不能自动识别动画真实命中帧；请核对 `ImpactMarkers.AnimationTimeSeconds`。测试时不要依赖旧的入场自动出招：现在攻击只由行为树任务或蓝图调用 `QueueAttack`/`QueueCombo` 发起。

额外创建的 `_1_Beat`、`_2_Beat`、`_3_Beat` 是可选的单击动作模板，**无需用它们拼接当前流畅的完整连招**。这些单击资产尚未标定命中点，不能直接作为行为树攻击任务使用。
