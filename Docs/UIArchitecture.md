# HearBlade 界面：玩法数据、控制器与蓝图

## 1. 从哪个资产开始

在内容浏览器打开 /Game/UI/MVC/WBP_RhythmCombatHUD。它继承 URhythmCombatWidget，真正的控件树保存在资产中，可以在 Designer 修改位置、尺寸、颜色、字体和贴图。运行时不会重新创建控件覆盖你的布局。

Event Graph 已连接 OnViewModelUpdated → RefreshViewFromModel。后者是 C++ 提供的显示辅助函数，只设置控件，不读取战斗组件。也可以保留事件，拆开 ViewModel，用蓝图的 SetText、SetPercent、SetVisibility 自行实现表现。

当前不是把全部表现节点都铺到 Event Graph：布局在蓝图，公共刷新在原生基类。

## 2. 三层各自做什么

| 层 | 对应对象 | 职责 |
| --- | --- | --- |
| 玩法数据 | 玩家战斗、玩家攻击、敌人连击、音乐时钟、教程组件 | 决定生命、判定、连招、时间和教程阶段 |
| 界面控制器 | RhythmWidgetController | 读取玩法，计算文字、透明度、轨道位置和显示快照 |
| 界面 | RhythmCombatWidget + WBP_RhythmCombatHUD | 收到快照后设置控件，决定视觉布局 |

RhythmCombatHUD 负责创建、连接、逐帧驱动与退出清理。旧 Canvas 绘制留作兼容退路；蓝图加载成功时 DrawHUD 提前返回，不会叠画旧界面。

## 3. 控制器的六个阅读入口

| 函数 | 内容 | 设计要点 |
| --- | --- | --- |
| UpdatePlayerPanel | 生命、共鸣、攻击状态、命中文字 | 只读真实数值，淡入淡出不改变玩法 |
| UpdateJudgments | 最近判定与四条历史 | 以判定时间识别新记录，避免每帧重复加入 |
| UpdateBossPanel | 血条、延迟条、危险提示 | 延迟条是表现，不是第二份真实生命 |
| UpdateRhythmTrack | 敌人轨道、主角拍点、反击提示、频谱 | 共用音乐时间，频谱仅用于视觉 |
| UpdateTutorial | 教程卡片与短时提醒 | 教程组件决定阶段 |
| UpdateLockMarker | 目标头部的屏幕位置 | 投影之后还要处理 UMG 缩放 |

Update 先核对 Pawn 与数据源，再依次更新六部分，最后统一广播。角色变化时清空旧判定与敌人引用，避免复活后遗留上个角色的数据。

## 4. 蓝图控件阅读图

~~~text
Root
├ GameplayRoot
│ ├ PlayerPanel：PlayerHealth、Resonance、HealthText、ModeText、AttackText
│ ├ BossPanel：BossHealth、BossTrail、BossName、BossHint
│ ├ JudgmentText、ComboText、History0…History3
│ ├ RhythmPanel：JudgeGate、RailEnd、Note0…Note7、Hold0…Hold7
│ ├ AttackPulsePanel：AttackCore、两侧拍点
│ ├ Spectrum0…Spectrum17
│ ├ TutorialPanel：标题、说明、进度、SkipRegion、Glyph0…Glyph3
│ ├ CalloutText
│ └ LockMarker、CounterMarker、ImpactText、ImpactFlash
└ DeathPanel
~~~

上图按职责概括，实际层级以 Designer 为准。数字后缀对应固定容量。改变容量要同步修改数据生成、刷新代码和控件树。

**保留现有刷新辅助函数时，不要随意重命名控件。** 它按名字查找控件。若要换名称，应同步修改 RefreshViewFromModel，或在蓝图事件中自行设置新控件。

## 5. 四种常见修改

**改血条位置：** 在 Designer 移动 PlayerPanel 或 PlayerHealth，检查锚点与 Alignment。先改容器可整体移动关联文字，避免错位。

**改节奏轨道长度：** 移动 JudgeGate 与 RailEnd。标记位置由两者位置与比例计算，判定线为 0，远端为 1。不要用屏幕像素修改攻击数据中的时间。

**加显示数值：** 在 FRhythmHUDViewModel 加字段 → 控制器从玩法读取 → 蓝图加控件 → 刷新函数或蓝图节点设置控件。真实数值仍由玩法组件拥有。

**改教程跳过区域：** 调整 SkipRegion。NativeTick 将实际控件区域转换为像素，再交给原教程组件的输入检测。当前仍是按住 ALT 点击、或手柄长按 View 的路径，不是 UButton 的 OnClicked 事件。

## 6. 生命周期

创建时：准备控制器 → 创建控件并加入视口 → SetWidgetController → 主动广播。SetWidgetController 也立即接收当前快照，后来加入的界面不会一直等待下一次变化。

替换时：先 RemoveDynamic 取消旧订阅，再 AddUniqueDynamic 连接新对象。退出时 NativeDestruct 与 HUD EndPlay 解除连接，Shutdown 停止频谱分析并清理引用。

每帧广播是当前实现选择，节拍标记、投影位置和淡出需要连续更新。以后优化应区分低频状态和连续表现，而不是笼统删除逐帧更新。

## 7. 生成工具与资源

HearBladeBeatEditor 插件新增 HearBladeBuildUICommandlet，仅用于首次创建控件树与事件图。已有资产时拒绝覆盖，防止 Designer 修改丢失。正常游戏和修改布局不需要重跑。

首次生成命令：

~~~powershell
& 'D:\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\Udev\HearBlade\HearBlade.uproject' -run=HearBladeBuildUI -unattended -nop4 -NullRHI -nosound
~~~

已有资产时报拒绝覆盖是预期行为，不应为重新生成而删除自己的设计。字体引用可保存的引擎资源，避免仅引用编辑器临时字体而出现方框字。现有打包配置包含 /Game/UI，新目录在其范围内；本次没有完整打包验证。

## 8. 检查与排错

开发版带 -MVCUIVerify 启动，约十二秒后执行显示层检查并截图，约十五秒后自动退出。日志应包含 Binding verification PASS。检查的是显示数据到真实控件的链路，不是战斗自动通关。-LegacyHUD 可临时对照旧绘制。

- 无新界面：查资产加载日志，并确认 GameMode 使用 RhythmCombatHUD。
- 数据变了但界面没动：查蓝图事件是否调用刷新，控件名字是否一致。
- 世界提示偏移：查像素到界面坐标的缩放，不要改敌人世界位置弥补。
- 旧对象仍影响界面：查替换数据源时是否解除旧委托。
- 方框字：查 Font 资源能否保存和加载，再查字体覆盖的字形。

新界面沿用主要贴图和战斗信息，部分旧 Canvas 装饰动画简化，不是逐像素复刻。完整战斗、教程跳过操作、分辨率适配与打包版仍需在演示环境中走一遍。


## 主角组件与界面职责

主角输入、武器姿态和移动动画分别由 HearBladeInputComponent、PlayerWeaponComponent、PlayerLocomotionComponent 处理。角色保留原蓝图参数并安排调用顺序，HUD 创建触控界面，死亡重开由 RhythmWidgetController 处理。完整说明见 [主角、输入与界面](PlayerArchitecture.md)。
