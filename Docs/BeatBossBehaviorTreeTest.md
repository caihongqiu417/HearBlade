# Boss 节拍行为树测试

测试关卡：`/Game/ThirdPerson/Lvl_ThirdPerson`。关卡中的 `Boss Beat BT Test Director` 运行 `/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/BeatTest/BT_DragonKnight_BeatTest`，黑板为同目录下的 `BB_DragonKnight_BeatTest`。黑板的 `BossActor` 指向已放置的火焰龙骑士。

行为树循环执行 `节拍攻击（下一可用拍）`。任务等待玩家进入 Boss 周围 1500 cm；进入后，从 `AttackPool` 随机选一套完整连招。当前攻击池有 `DA_DragonKnight_Combo01_Beat` 和 `DA_DragonKnight_Combo02_Beat`，各有约一半机会被选中。连招结束后短暂环绕，再选择下一次攻击。

Combo02 使用专用的 `Combo02_BeatMontage`，引用启用 Root Motion 的 `Combo02_BeatRootMotion`，避免动作结束时模型根骨骼回弹；原始 `Combo02` 动画和 Montage 保持不变。源动画 30 FPS；命中帧 14、54、72、93、124 对应 0.467、1.800、2.400、3.100、4.133 秒。前四击继续使用原有相对拍位 0、2、3、4 和原有逐段速度；新增第五击相对第一击落在第 6 拍，93→124 帧这一段使用约 0.8956 倍速。最后一击之后仍使用原来的收招速度，不影响此前已经对齐的四击。节拍编辑器可直接选择该数据资产继续微调。

进入范围后，任务先以 `TurnSpeedDegreesPerSecond` 的速度把 Boss 朝向玩家，进入 `FacingToleranceDegrees` 角度后才触发攻击。连招播放时，UE Motion Warping 根据每击的命中标记创建攻击前的修正窗口；目标位置与玩家保留 `WarpStopDistance` 的距离，每击的位移修正受 `MaxWarpTravelPerStrike` 限制。相关参数在攻击数据资产的 `Motion Warping` 分类中，转身速度在行为树任务的 `转向` 分类中。此功能保留节拍调度，修正的是 Root Motion 位移与朝向。

Boss 贴身时不再必须后退到固定最小攻击距离。近距离移动以横向环绕为主、只做较慢的径向退让；攻击已经排入节拍队列但动画尚未开始时，Boss 会继续面向玩家环绕。到达动画实际启动时间后才停止走路进入起手，因此等待下一小节强拍不会表现为原地发愣。

每次攻击都会尝试火焰闪现。行为树节点的 `节拍攻击|闪现` 可调落点距离（默认 250 cm）、侧向角度（55°）、预警拍数（半拍）及三个 Niagara 特效。预警时锁定玩家侧前方落点，左右交替；第一击拍点到达时重新检查碰撞并瞬移，旧位置保留 0.28 秒的姿态残影。闪现前的 Root Motion 位移不作用于 Boss，现身后其余连击继续 Motion Warping。无安全落点时取消本次攻击，行为树会重新调度。

行为树触发攻击时，`BeatSyncedComboComponent` 读取当前音乐时钟、起手至第一击的时长和 `SchedulingLeadSeconds`，先找到能赶上的拍，再按攻击数据的 `FirstImpactBeatInBar` 选小节内的拍位（`0`=任意，`1`=每小节第 1 拍）。测试连招默认设为 `1`；Boss 可能多等几拍，但拖动编辑器紫色“触发攻击”线不会改变第一击在小节中的位置。后续击的目标偏移由数据资产记录，当前为 3 和 5.5 拍；实际时间取决于保存的逐段播放速度，应在编辑器查看“最大偏差”并自行校准。此行为树没有写死音乐播放到第几秒才攻击。

在编辑器打开测试关卡并点击“播放”，走近 Boss 到约 4.5 米以内。输出日志可搜索 `BTTask_BeatAttack` 与 `BeatSyncedCombo`：等待时显示距离；进范围后显示随机选中的连招；三次 `Impact` 分别显示目标拍数与时间误差。`OnImpact` 是命中时间事件，实际伤害判定需要由 Boss 逻辑订阅。
