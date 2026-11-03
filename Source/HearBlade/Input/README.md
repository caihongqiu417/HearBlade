# 玩家输入

HearBladeInputComponent 负责动作绑定、鼠标攻防选择和失去控制时的清理

先看 Bind 的动作登记，再看 MousePrimaryCombatPressed 和 MouseSecondaryCombatPressed
右键持续按住时由 UpdateHeldActions 检查防御窗口，Unbind 清理回调和临时映射
攻击条件和输入缓冲仍由战斗组件处理，基础 IA 继续读取角色蓝图的原配置

HearBladeInputTests 验证重复绑定、按键保留、失去控制和重新绑定
完整说明见 [主角、输入与界面](../../../Docs/PlayerArchitecture.md)
