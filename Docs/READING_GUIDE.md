# HearBlade 项目实现

## 角色与战斗组件

HearBlade 的战斗由音乐时间、攻击数据、动作执行和接触判定共同组成。攻击需要在指定拍点到达命中姿势，同时根据目标位置调整朝向与位移。玩家的防御输入再与攻击要求、命中时间和实际接触共同决定结果。

HearBladeCharacter 创建角色与相关组件，并将输入分配给攻击、防御等功能。PlayerAttackComponent 管理轻重攻击、连招和动画推进；PlayerDefenseComponent 管理生命、防御输入与应对结果。攻击过程和防御结算各自保存状态，避免所有逻辑集中在角色类中。

Combat 目录按职责划分。Data 定义攻击配置，Player 实现主角战斗，Boss 执行敌人连击与阶段变化，AI 组织行为选择，Camera、Feedback 和 Tutorial 分别处理镜头、反馈及教程。Audio 提供音乐时间，UI 展示玩法结果，编辑工具位于 Plugins/HearBladeBeatEditor。

对应文件：

- [Source/HearBlade/HearBladeCharacter.cpp](../Source/HearBlade/HearBladeCharacter.cpp)
- [Source/HearBlade/Combat/Player/PlayerAttackComponent.cpp](../Source/HearBlade/Combat/Player/PlayerAttackComponent.cpp)
- [Source/HearBlade/Combat/Player/PlayerDefenseComponent.cpp](../Source/HearBlade/Combat/Player/PlayerDefenseComponent.cpp)

## 音乐时间与攻击数据

BeatClockSubsystem 为主角、敌人和界面提供统一的音乐时间。每拍时长由六十除以每分钟拍数得到；当前时间减去共同起点后，可以换算为带小数的拍数。小数部分用于表示拍间位置，支持提前输入、动画推进和提示标记的连续移动。

运行期间，GetAudioNowSeconds 返回考虑播放速度后的 MusicalAudioSeconds，未运行时使用引擎音频时间。该实现没有直接读取声卡采样位置。攻击、判定和提示使用同一时间依据，才能避免分别计时造成的偏差。

AttackData 定义攻击片段、命中标记和应对类型，PlayerAttackSet 保存主角轻重攻击节点及其衔接。头文件定义字段，数据资产保存具体动画和数值。Validate 检查片段起止、命中位置和分支引用，减少无效配置进入运行流程。

对应文件：

- [Source/HearBlade/Audio/BeatClock/BeatClockSubsystem.cpp](../Source/HearBlade/Audio/BeatClock/BeatClockSubsystem.cpp)
- [Source/HearBlade/Combat/Data/AttackData.h](../Source/HearBlade/Combat/Data/AttackData.h)
- [Source/HearBlade/Combat/Data/PlayerAttackSet.h](../Source/HearBlade/Combat/Data/PlayerAttackSet.h)

## 主角攻击

阅读主角攻击时，可以从 RequestAttack 开始，再看 StartAttack 和 TickAttack。RequestAttack 先检查角色能否行动、音乐是否运行，以及当前配置是否允许重攻击，再根据攻击状态判断立即执行还是进入等待。StartAttack 选择相应的连招节点，确定目标命中时间，并准备动画片段。玩家真实按键时间与动作执行时间分别保存，延迟执行不会覆盖原始输入时刻。出招期间按下下一次攻击，会保存第一次输入及其时间；继续按键只增加次数，不把时间改成最后一次按下的时刻。这样不能靠反复按键，把原先偏早的输入改成一次准时输入。收招尚未结束时，输入会暂存，等待能够开始攻击时重试。

攻击开始后，组件根据音乐时间计算动画位置，使动作在目标时刻到达配置的命中姿势。准备段和收招段可以采用不同的时间安排，连招推进还需要保持命中附近的播放速度连续，避免两段衔接时出现突变。这里要区分重攻击和蓄力：bHeavyAttacksEnabled 的代码默认值为 true，但 WindupBeats 中的蓄力开关当前关闭。重攻击有自己的攻击节点，并不表示当前一定会进入蓄力流程。实际角色是否允许重攻击，还需要查看蓝图对默认值的覆盖。

空间移动使用动画中的根位移，也就是动画根部在一段时间内产生的移动量。组件提取前后两次动画位置之间的位移，转换到世界空间，再通过角色移动组件处理碰撞。受阻后仍更新已处理的动画位置，避免未完成的位移积累到后续帧突然释放。

主角朝目标靠近的效果由目标朝向、动画位移和碰撞处理共同完成。SweepBlade 检查刀刃运动中的接触，并记录本次已经命中的对象，避免连续多帧重复结算。时间安排决定命中时刻，位移与接触检测决定攻击是否真正碰到目标。

对应文件：

- [Source/HearBlade/Combat/Player/PlayerAttackComponent.cpp](../Source/HearBlade/Combat/Player/PlayerAttackComponent.cpp)
- [Source/HearBlade/Combat/Data/PlayerAttackSet.h](../Source/HearBlade/Combat/Data/PlayerAttackSet.h)
- [Source/HearBlade/Combat/Player/PlayerDefenseComponent.cpp](../Source/HearBlade/Combat/Player/PlayerDefenseComponent.cpp)

## 敌人连击与目标修正

BTTask_BeatAttack 处理接近、转向、绕行、选择攻击和等待。任务确定执行攻击后，交给 BeatSyncedComboComponent 排队和播放。行为选择与逐帧动作执行分开，使行为树能够组织战术，而连击组件专门处理动画、命中时刻和伤害窗口。

BeatSyncedComboComponent 根据攻击数据推进动画，并为每一击更新目标修正。UpdateWarpTarget 在当前攻击窗口内记录起点，根据玩家位置、停止距离和单击最大移动距离确定落点。临近命中时可按配置锁定目标，限制出手后的持续追踪。

BossMotionWarpingComponent 将本帧根位移、动画区间和播放速度交给引擎处理，返回修正后的位移。连击组件负责应用结果。音乐调度确定何时命中，目标修正调整角色移动的位置和朝向，两部分使用各自的输入与约束。

出手前允许修正，可以降低敌人因目标轻微移动而连续挥空的情况；出手后限制追踪，则为闪避保留有效空间。停止距离、单击移动上限和目标锁定时机共同控制这一行为。

对应文件：

- [Source/HearBlade/Combat/AI/BTTask_BeatAttack.cpp](../Source/HearBlade/Combat/AI/BTTask_BeatAttack.cpp)
- [Source/HearBlade/Combat/Boss/BeatSyncedComboComponent.cpp](../Source/HearBlade/Combat/Boss/BeatSyncedComboComponent.cpp)
- [Source/HearBlade/Combat/Boss/BossMotionWarpingComponent.cpp](../Source/HearBlade/Combat/Boss/BossMotionWarpingComponent.cpp)

## 防御判定与战斗反馈

PlayerDefenseComponent 接收闪避、格挡、反击及持续格挡等输入。ResolveDefenseResult 根据本次攻击要求选择对应规则，并使用命中标识防止重复处理。持续格挡还包含开始、保持和结束状态，不能将每次接触都视为新的独立按键。

节奏评价比较真实输入时间与理想输入时间。理想时间由命中时刻及提前量确定，输入偏差用于评级；实际接触和防御状态则参与伤害处理。评价文字是结算后的结果，界面显示不参与反向决定防御是否成功。

Feedback 处理攻击提示、闪避残影和瞬时效果，Camera 处理锁定与战斗镜头。它们读取已经发生的战斗事件或状态，更新相应表现。共鸣等特殊状态由玩法组件管理，再向界面和反馈模块提供结果。

CombatTutorialComponent 保存教程阶段，依据玩家完成的动作推进教学。提示卡片显示标题、说明和进度，敌人陪练逻辑配合当前教学目标。教程规则与卡片布局分开维护，调整界面不会改变阶段完成条件。

对应文件：

- [Source/HearBlade/Combat/Player/PlayerDefenseComponent.cpp](../Source/HearBlade/Combat/Player/PlayerDefenseComponent.cpp)
- [Source/HearBlade/Combat/Feedback/AttackReadabilityComponent.cpp](../Source/HearBlade/Combat/Feedback/AttackReadabilityComponent.cpp)
- [Source/HearBlade/Combat/Camera/BossCombatCameraComponent.cpp](../Source/HearBlade/Combat/Camera/BossCombatCameraComponent.cpp)
- [Source/HearBlade/Combat/Tutorial/CombatTutorialComponent.cpp](../Source/HearBlade/Combat/Tutorial/CombatTutorialComponent.cpp)

## 界面结构

RhythmCombatHUD 创建界面与控制器，并建立连接。RhythmWidgetController 读取玩家、敌人、音乐时钟和教程状态，将其整理为 FRhythmHUDViewModel。快照包含血量比例、判定记录、节拍标记和提示文字等显示数据，实际战斗状态仍由玩法组件保存。不过，当前 Rebind 在角色没有教程组件时会创建该组件，所以这个控制器还承担了一项教程初始化工作，并非所有函数都只读取数据。

RhythmCombatWidget 接收控制器的数据，通过 OnViewModelUpdated 交给蓝图事件。WBP_RhythmCombatHUD 保存 UMG 控件树，布局、锚点、字体和贴图在设计器中配置。当前事件图调用 RefreshViewFromModel，由原生辅助函数设置文字、进度、位置和可见性。

创建或更换界面控制器时，先建立订阅，再主动发送当前显示数据，保证界面首次出现就能显示有效状态。具体顺序是 UpdateBlueprintHUD 创建控制器和蓝图控件，SetWidgetController 建立通知绑定，然后 BroadcastInitialValues 调用 Update，生成并发送当前数据。替换控制器前解除旧订阅，退出时清理连接。这样可以避免旧对象继续更新已经更换的界面。

控制器当前逐帧更新，满足节拍标记移动、透明度变化和世界坐标投影的需要。控件查找使用约定名称，修改名称需要同步更新刷新逻辑。UI/Legacy 保存旧 Canvas 实现，UI/Tests 保存显示层检查，二者与新界面的正常显示流程分开。UpdateBlueprintHUD 只尝试加载一次蓝图资源；资源缺失或启用 LegacyHUD 参数时返回 false，由调用方继续绘制旧版界面。

对应文件：

- [Source/HearBlade/UI/HUD/RhythmCombatHUD.cpp](../Source/HearBlade/UI/HUD/RhythmCombatHUD.cpp)
- [Source/HearBlade/UI/Controller/RhythmWidgetController.cpp](../Source/HearBlade/UI/Controller/RhythmWidgetController.cpp)
- [Source/HearBlade/UI/Model/RhythmHUDViewModel.h](../Source/HearBlade/UI/Model/RhythmHUDViewModel.h)
- [Source/HearBlade/UI/View/RhythmCombatWidget.cpp](../Source/HearBlade/UI/View/RhythmCombatWidget.cpp)

## 节拍编辑器

BeatComboEditorWidget 使用 Slate 构建动画、音乐波形和命中标记的编辑界面。开发者在动画中标记接触时刻，再根据音乐拍点调整分段播放关系。标记由人工编辑，工具不自动识别动作画面中的命中。

一段连击可能有多个间隔不同的命中点，统一调整整段播放速度通常无法同时对齐所有命中。因此编辑器保存分段速度及相关时间信息，使准备段、两次命中之间和收招段能够分别调整。

SaveAsset 在写入前检查动画、片段范围、命中标记和分段数据是否一致，检查还包括防御指令是否有效。分段速度的数量必须等于命中标记数量加一，因为第一次命中前、相邻两次命中之间，以及最后一次命中后，都各有一段动画。通过后，函数将播放速度、分段速度和音乐参考信息写入攻击资产，再保存资源包。预览触发时间属于临时编辑状态，不写入正式攻击规则。运行时组件只读取保存后的数据，不依赖编辑窗口。

主角由 BP_ThirdPersonCharacter 配置，DA_PlayerAttackSet 保存主角连招，敌人攻击资产与行为树配置敌人行为，WBP_RhythmCombatHUD 配置游戏界面。编辑器负责制作攻击数据，战斗组件负责执行，界面负责显示执行结果。

对应文件：

- [Plugins/HearBladeBeatEditor/Source/HearBladeBeatEditor/Private/BeatComboEditorWidget.cpp](../Plugins/HearBladeBeatEditor/Source/HearBladeBeatEditor/Private/BeatComboEditorWidget.cpp)
- [Source/HearBlade/Combat/Data/AttackData.h](../Source/HearBlade/Combat/Data/AttackData.h)
- [Source/HearBlade/Combat/Boss/BeatSyncedComboComponent.cpp](../Source/HearBlade/Combat/Boss/BeatSyncedComboComponent.cpp)


## 主角组件与界面职责

主角输入、武器姿态和移动动画分别由 HearBladeInputComponent、PlayerWeaponComponent、PlayerLocomotionComponent 处理。角色保留原蓝图参数并安排调用顺序，HUD 创建触控界面，死亡重开由 RhythmWidgetController 处理。完整说明见 [主角、输入与界面](PlayerArchitecture.md)。
