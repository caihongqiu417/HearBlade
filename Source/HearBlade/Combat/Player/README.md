# Player：职责与入口

主角攻击、节奏防御和动画推进。

先读 [PlayerBeatAttackComponent.cpp](PlayerBeatAttackComponent.cpp)，再返回 [战斗总路线](../README.md)。

## 主角组件

PlayerBeatAttackComponent 处理攻击和连招，PlayerRhythmCombatComponent 处理节奏防御与受击
PlayerWeaponComponent 只计算握持姿态和剑的朝向，PlayerLocomotionComponent 处理步速与移动动画
角色 Tick 按原顺序调用武器和移动动画更新，这两个组件不自行 Tick
输入绑定位于 Input/HearBladeInputComponent，界面数据位于 UI/Controller/RhythmWidgetController
