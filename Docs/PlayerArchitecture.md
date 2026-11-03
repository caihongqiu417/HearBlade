# HearBlade 主角、输入与界面

主角的入口是 `AHearBladeCharacter`。它创建身体、镜头和玩法组件，保留蓝图能够调用的移动、视角与闪避函数。按键绑定、握剑校准和移动动画计算分别由独立组件处理。先看角色创建了哪些组件，再沿着一次输入进入战斗组件，比从头阅读所有动画计算更容易理解整个项目。

## 主角的组成

| 类 | 负责的内容 | 主要函数 |
| --- | --- | --- |
| AHearBladeCharacter | 创建组件、提供移动入口、安排身体更新顺序 | SetupPlayerInputComponent、DoMove、Tick |
| UHearBladeInputComponent | 绑定输入、选择鼠标的攻击或防御操作、失去控制时清理输入 | Bind、MousePrimaryCombatPressed、UpdateHeldActions、Unbind |
| UPlayerAttackComponent | 轻重攻击、连招缓冲、攻击位移和命中结算 | RequestAttack、RequestResonance |
| UPlayerDefenseComponent | 防御窗口、格挡、弹反、闪避与受击状态 | IsDefenseInputWindowOpen、RequestBlock、RequestParry、RequestDodge |
| UPlayerWeaponComponent | 校准握持角度，更新剑的朝向 | Initialize、Calibrate、UpdateDirection |
| UPlayerLocomotionComponent | 根据实际速度选择步行、跑步和追击姿态 | Initialize、MeasureSpeeds、Update |
| UBossCombatCameraComponent | 锁定目标与战斗镜头 | ToggleLockOn、GetCombatTarget |

这些组件属于同一个角色，各自保存完成职责所需的状态。输入组件保存按住了哪个键，武器组件保存校准后的握持角度，移动动画组件保存平滑后的采样位置。攻击是否允许、是否进入连招窗口，仍由战斗组件决定。

## 攻击、防御与输入窗口

`UPlayerAttackComponent` 负责出招，`UPlayerDefenseComponent` 负责防御和受击恢复。角色、输入和武器组件分别通过 `AttackComponent`、`DefenseComponent` 保存引用。蓝图中原有组件实例名保留为 `BeatAttack`、`RhythmCombat`，已保存的参数仍挂在原来的实例上。

输入入口统一使用 `Request`，例如 `RequestAttack`、`RequestBlock`、`RequestParry`、`RequestDodge`。返回 `true` 表示请求已经接收，可能立即执行，也可能等待缓存；不表示已经命中或防御成功。`RequestResonance` 接收后等待下一小节启动共鸣。

| 机制 | 代码位置 | 处理规则 |
| --- | --- | --- |
| 普通攻击缓存 | `RequestAttack`、`TickComponent` | 空闲但尚未恢复时，保留第一次输入，最多等待 1 秒游戏时间，重复按键不延长期限 |
| 连招输入缓存 | `RequestAttack`、`TickAttack` | 当前攻击中保留第一次轻重选择，按原始音乐时间评级，到下一击起手时刻才执行 |
| 接招截止 | `LatestCueAudio + ChainLateSeconds()` | 截止后拒绝连招输入，收招动画结束不等于连招窗口关闭 |
| 攻击命中窗口 | `SweepBlade` | 在目标音乐时刻前后各 40 毫秒内检查武器轨迹，经过窗口还要实际接触并通过遮挡检查 |
| 防御输入窗口 | `IsDefenseInputWindowOpen` | 根据附近敌人预警决定鼠标左右键用于攻还是防，不直接判定防御成功 |
| 防御缓存 | `BufferDefenseInput`、`ProcessDefenseBuffer` | 只保留最新操作，按游戏时间过期；受击锁定时至少保留到解锁后 0.05 秒 |
| 缓存期间松开格挡 | `ReleaseBlock`、`ProcessDefenseBuffer` | 保存原始松键时间，缓存执行后补上释放，避免一直举盾 |
| 闪避无敌时间 | `IsInvulnerable` | 从原始按键的音乐时间计算，缓存等待不会额外延长无敌 |
| 防御评级 | `ResolveDefenseResult`、`ConsumeClosestDefenseInput` | 从按键历史取出最接近理想时刻的一次，使用后移除，不能重复抵挡后续命中 |

攻击的音乐时间用于安排起手、命中和接招，游戏时间用于缓存过期和受击恢复。实际命中、输入评级、动画播放是三个独立判断，不用“动画还在播放”代替是否可以接招或是否无敌。

## 按一次鼠标左键

玩家取得角色控制权后，UE 调用角色的 `SetupPlayerInputComponent`。角色把引擎创建的输入组件交给 `PlayerInput->Bind`。`Bind` 登记 IA 的回调，并把战斗按键映射加入本地玩家的 Enhanced Input 子系统。

鼠标左键触发 `MousePrimaryCombatPressed`。它先向玩家防御组件询问当前是否处于鼠标防御窗口：窗口内调用 `RequestParry`，窗口外调用 `RequestAttack(Light)`。输入组件只选择入口，攻击组件再决定立即开始攻击、保存到连招缓冲，还是拒绝这次操作。

右键同样区分窗口内格挡和窗口外重击。`bMouseSecondaryHeld` 表示右键仍按着，`bMouseSecondaryOwnsBlock` 表示这次右键已经启动了格挡。玩家提前按住右键时，`UpdateHeldActions` 会在窗口出现后尝试格挡；松开时只结束右键自己启动的格挡。

失去角色控制权时，`UnPossessed` 调用 `Unbind`。它结束按住操作，清掉自己的回调和临时映射。回调使用登记时保存的句柄逐个删除，因此不会清空其他组件或蓝图添加的输入。再次控制角色时重新绑定，原有临时 IA 对象继续使用。

基础移动和视角动作仍保存在角色蓝图的原字段中，避免丢失已经配置的资源引用。战斗 IA 沿用原来的运行时创建方式，按键读取 `URhythmCombatInputSettings`；这次没有把它们全部改成内容浏览器中的资源。

## 移动、朝向和握剑

`DoMove` 先保存玩家希望移动的方向，再检查当前动作是否限制移动。允许移动时，以镜头的水平朝向计算前方和右方，交给 CharacterMovement。攻击期间的速度比例仍从攻击组件读取。

每帧身体更新保持以下顺序：检查持续按住的右键、检查角色是否死亡、校准握剑、更新角色朝向、更新剑的朝向、更新移动动画。新的武器和移动动画组件没有独立 Tick，而是由角色在原来的位置调用，避免拆分后改变执行先后。

攻击和防御时，剑跟随右手动画，并加上初始化时记录的握持偏移。普通移动时，剑保持朝向角色前方。`SyncAttackWeapon` 仍是攻击组件可调用的入口，在命中检测前及时同步剑的朝向。武器组件不结算伤害，也不修改攻击窗口。

移动动画组件读取实际速度，并把速度换算成混合空间的采样半径。步行、跑步和快跑的参考速度来自动画根节点的位移；原地动画无法测量时使用已有默认值。锁定目标后，只有玩家确实朝远处目标移动，才进入追击姿态。进入和退出追击使用不同阈值，避免在临界距离反复切换。

角色蓝图中的 `FreeRunSpeed`、`LockedCombatWalkSpeed`、握剑偏移和插值速度仍保留原名和原位置。移动计算也保留原规则：锁定步行速度不低于 280，追击速度是自由移动速度的 1.3 倍。仅在拆分职责时顺便改变这些数值，会让原有动画和手感发生变化。

## 蓝图界面与 MVC

`ARhythmCombatHUD` 创建 `/Game/UI/MVC/WBP_RhythmCombatHUD`，把它和 `URhythmWidgetController` 连接起来。处理器读取角色和 Boss 的状态，整理成 `FRhythmHUDViewModel`，再通知界面。UMG 蓝图继续负责布局和显示，不需要迁移到 C++ 绘制。

触控控件也由 HUD 创建和销毁。PlayerController 保留原蓝图中的触控控件类型和平台配置，HUD 读取这些配置；控制器不再亲自创建控件。死亡后按回车通过 Restart IA 进入界面处理器的 `RestartLevel`，处理器确认本机角色已经死亡后才重开当前关卡，不再由角色 Tick 轮询回车键。

节拍轨道的位置每帧都在变化，因此原来的显示更新仍然保留。MVC 的作用是让读取玩法数据和修改控件各有归属，不是把所有连续变化都强行改成事件。

## 文件与资源位置

以下源码路径均相对于 `Source/HearBlade`：

| 路径 | 内容 |
| --- | --- |
| HearBladeCharacter.h / .cpp | 角色组装、蓝图配置与更新入口 |
| Input/HearBladeInputComponent.h / .cpp | 输入绑定、鼠标攻防选择与解绑 |
| Combat/Player/PlayerWeaponComponent.h / .cpp | 握剑校准和武器朝向 |
| Combat/Player/PlayerLocomotionComponent.h / .cpp | 步速测量与移动动画 |
| Combat/Player/PlayerAttackComponent.h / .cpp | 主角攻击与连招 |
| Combat/Player/PlayerDefenseComponent.h / .cpp | 节奏防御和角色战斗状态 |
| Combat/Data/RhythmCombatInputSettings.h / .cpp | 战斗按键设置 |
| UI/Controller/RhythmWidgetController.h / .cpp | 显示数据与死亡重开 |
| UI/HUD/RhythmCombatHUD.h / .cpp | 界面创建和连接 |
| UI/Legacy/RhythmCombatHUDLegacy.cpp | 旧绘制方式与 HUD 结束清理 |

角色资源在 `/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter`，主角连招配置在 `/Game/Combat/Player/DA_PlayerAttackSet`。普通移动使用 `/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/BS_DKM_CombatLocomotion`，追击使用 `/Game/Combat/Player/BS_PlayerEightWay`。

节拍编辑器仍位于 `Plugins/HearBladeBeatEditor`。编辑器修改攻击数据资产，运行时战斗组件读取数据，界面处理器读取执行结果。这三处分别对应编辑配置、执行玩法和显示结果，不互相代替。

## 验证结果

UE 5.8 编辑器目标完整编译通过，9 项自动化检查通过。其中输入生命周期检查有一条测试世界尚未配置骨骼网格的警告，没有失败。

在 `Lvl_ThirdPerson` 中验证了蓝图界面的数据绑定、左右键的格挡与弹反、八方向移动、追击与停步、握剑校准。五条连招路线通过，20 Hz 连续输入在 10 拍内产生 9 次挥击。

连招诊断按攻击数据资产的实际分支检查节点，并在诊断期间暂停教程陪练，避免敌人打断连招干扰结果。移动诊断读取当前角色速度配置。正常游戏中的教程、伤害与连招规则未为这些诊断修改。

这些检查没有覆盖实体手机的触控操作、完整 Boss 全阶段通关或所有关卡组合。启动日志中还存在引擎实验性 Python 工具插件的注册错误，不属于本次角色代码修改。
