# 界面目录：从入口到数据，再到蓝图

~~~text
UI/
├ HUD/          创建并连接界面；从 UpdateBlueprintHUD 开始读
├ Model/        只供显示的数据结构，没有玩法判断
├ Controller/   从玩法取数，整理六个区域并广播
├ View/         接收数据，调用蓝图事件并更新控件
├ Legacy/       旧 Canvas 绘制与兼容入口，不是默认显示实现
└ Tests/        显示链路验证，只在指定测试参数下运行
~~~

| 先读 | 输入 | 输出 |
| --- | --- | --- |
| HUD/RhythmCombatHUD.cpp | 本机 PlayerController | 控制器与 WBP 实例 |
| Model/RhythmHUDViewModel.h | 无对象依赖的数据字段 | 蓝图可读的显示快照 |
| Controller/RhythmWidgetController.cpp | 玩家、敌人、音乐和教程状态 | OnViewModelChanged |
| View/RhythmCombatWidget.cpp | 显示快照 | OnViewModelUpdated 蓝图事件与控件变化 |
| Tests/RhythmUIVerification.cpp | 临时显示样本 | 真实控件状态的断言与日志 |

控件布局资源在 /Game/UI/MVC/WBP_RhythmCombatHUD。模型文件是显示模型；生命等真正的玩法数据仍由 Combat 组件保存。

阅读流程：HUD 创建 → Controller 整理 → Model 快照 → View 接收 → 蓝图事件 → 控件。反向排错：控件 → 蓝图事件 → 快照字段 → 控制器数据来源。

完整操作见 [界面结构说明](../../../Docs/UIArchitecture.md)。
