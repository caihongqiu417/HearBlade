# 战斗目录：先找职责，再读函数

目录按职责拆分，原生类名与模块名不变，资源仍引用同一批类型。

| 文件夹 | 职责 | 先读 |
| --- | --- | --- |
| Data | 攻击、输入与表现配置。只定义数据，不推进战斗。 | [AttackData.h](Data/AttackData.h) |
| Player | 主角攻击、节奏防御和动画推进。 | [PlayerBeatAttackComponent.cpp](Player/PlayerBeatAttackComponent.cpp) |
| Boss | 敌人连击、空间修正、阶段与特殊攻击。 | [BeatSyncedComboComponent.cpp](Boss/BeatSyncedComboComponent.cpp) |
| AI | 行为树任务决定选择和等待攻击。 | [BTTask_BeatAttack.cpp](AI/BTTask_BeatAttack.cpp) |
| Camera | 战斗锁定、镜头配置与镜头修改。 | [BossCombatCameraComponent.cpp](Camera/BossCombatCameraComponent.cpp) |
| Feedback | 攻击识别、闪避残影及瞬时表现。 | [AttackReadabilityComponent.cpp](Feedback/AttackReadabilityComponent.cpp) |
| Tutorial | 教程阶段和敌人的陪练动作。 | [CombatTutorialComponent.cpp](Tutorial/CombatTutorialComponent.cpp) |
| Debug | 运行时观察、录制与诊断入口。 | [PlayerAttackRuntimeProbe.cpp](Debug/PlayerAttackRuntimeProbe.cpp) |
| Tests | 节奏与攻击回归检查。 | [PlayerAttackTests.cpp](Tests/PlayerAttackTests.cpp) |

~~~text
Data 配置 → Player 或 AI/Boss 执行 → 应对与伤害结果
                           ├→ Feedback / Camera 表现
                           └→ UI 控制器读取显示
~~~

从音乐时间到界面的完整解释：[项目导读](../../../Docs/READING_GUIDE.md)。