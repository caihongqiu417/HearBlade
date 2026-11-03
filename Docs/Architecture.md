# HearBlade：职责、资源与流程总览

## 目录怎么找

| 目录 | 职责 | 不放在这里的内容 |
| --- | --- | --- |
| Source/HearBlade/Audio/BeatClock | 统一音乐时间与拍数换算 | 界面布局 |
| Source/HearBlade/Combat | 输入应对、主角攻击、敌人连击和空间修正 | 蓝图控件位置 |
| Source/HearBlade/UI/Model | 显示快照字段 | 真实伤害结算 |
| Source/HearBlade/UI/Controller | 将玩法整理成显示数据 | 控件创建和贴图布局 |
| Source/HearBlade/UI/View | 接收快照，更新控件 | 查询敌人、改变生命 |
| Source/HearBlade/UI/HUD | 创建、连接与逐帧驱动 | 大量具体绘图 |
| Source/HearBlade/UI/Legacy | 旧 Canvas 兼容实现 | 新功能的首选入口 |
| Source/HearBlade/UI/Tests | 显示层验证 | 正常战斗流程 |
| Plugins/HearBladeBeatEditor | Slate 编辑工具与首次 UMG 生成工具 | 游戏运行时依赖 |

## 资源怎么找

| 内容浏览器路径 | 作用 | 由谁使用 |
| --- | --- | --- |
| /Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter | 主角蓝图 | 角色组件配置 |
| /Game/Combat/Player/DA_PlayerAttackSet | 主角轻重连招数据 | PlayerAttackComponent |
| /Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_DragonKnight_Combo01_Beat | 敌人节拍攻击数据 | 行为树任务与连击组件 |
| /Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/BeatTest/BT_DragonKnight_BeatTest | 敌人行为树资源 | 实际使用关系需看当前关卡配置 |
| /Game/UI/MVC/WBP_RhythmCombatHUD | 可编辑的 UMG 布局与事件图 | RhythmCombatHUD |
| /Game/UI/RhythmCombat | 血条、轨道和应对提示等贴图 | 新 UMG 与旧 HUD |
| /Game/UI/Tutorial | 教程卡片及按键图标 | 蓝图界面 |

资源存在不代表任意关卡都在使用它。新 WBP 已通过实机启动验证；敌人行为树和角色蓝图仍要核对演示关卡实例。

## 三条流程

~~~text
设计数据：节拍编辑器 → 攻击数据资产 → 运行时连击组件
战斗执行：输入 / 行为树 → 目标音乐时刻 → 动画位置 → 空间位移 → 接触与结算
界面显示：玩法结果 → 控制器 → 显示快照 → 蓝图事件 → UMG 控件
~~~

时间和空间分开：音乐时刻决定什么时候命中，根位移与目标修正决定从哪里到哪里。界面读取结果，不能反过来通过显示的 PERFECT 决定伤害。

源码内入口：[界面目录](../Source/HearBlade/UI/README.md)。进一步读 [项目导读](READING_GUIDE.md) 和 [界面操作说明](UIArchitecture.md)。

## 文件迁移说明

RhythmWidgetController 移到 UI/Controller，RhythmCombatWidget 移到 UI/View，RhythmCombatHUD 的新入口移到 UI/HUD，旧绘制独立到 UI/Legacy。显示数据从控制器头文件抽到 UI/Model。类名和模块名保持原样，因此原生类的资源身份不随文件夹变化。

## 战斗目录进一步分工

| 文件夹 | 职责 | 先读 |
| --- | --- | --- |
| Data | 攻击、输入与表现配置。只定义数据，不推进战斗。 | [AttackData.h](../Source/HearBlade/Combat/Data/AttackData.h) |
| Player | 主角攻击、节奏防御和动画推进。 | [PlayerAttackComponent.cpp](../Source/HearBlade/Combat/Player/PlayerAttackComponent.cpp) |
| Boss | 敌人连击、空间修正、阶段与特殊攻击。 | [BeatSyncedComboComponent.cpp](../Source/HearBlade/Combat/Boss/BeatSyncedComboComponent.cpp) |
| AI | 行为树任务决定选择和等待攻击。 | [BTTask_BeatAttack.cpp](../Source/HearBlade/Combat/AI/BTTask_BeatAttack.cpp) |
| Camera | 战斗锁定、镜头配置与镜头修改。 | [BossCombatCameraComponent.cpp](../Source/HearBlade/Combat/Camera/BossCombatCameraComponent.cpp) |
| Feedback | 攻击识别、闪避残影及瞬时表现。 | [AttackReadabilityComponent.cpp](../Source/HearBlade/Combat/Feedback/AttackReadabilityComponent.cpp) |
| Tutorial | 教程阶段和敌人的陪练动作。 | [CombatTutorialComponent.cpp](../Source/HearBlade/Combat/Tutorial/CombatTutorialComponent.cpp) |
| Debug | 运行时观察、录制与诊断入口。 | [PlayerAttackRuntimeProbe.cpp](../Source/HearBlade/Combat/Debug/PlayerAttackRuntimeProbe.cpp) |
| Tests | 节奏与攻击回归检查。 | [PlayerAttackTests.cpp](../Source/HearBlade/Combat/Tests/PlayerAttackTests.cpp) |

每个子目录都有 README；从 [战斗总路线](../Source/HearBlade/Combat/README.md) 进入。文件移动只调整包含路径，保留原有玩法实现和工作区修改。


## 主角组件与界面职责

主角输入、武器姿态和移动动画分别由 HearBladeInputComponent、PlayerWeaponComponent、PlayerLocomotionComponent 处理。角色保留原蓝图参数并安排调用顺序，HUD 创建触控界面，死亡重开由 RhythmWidgetController 处理。完整说明见 [主角、输入与界面](PlayerArchitecture.md)。
