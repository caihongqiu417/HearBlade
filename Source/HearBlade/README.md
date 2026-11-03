# HearBlade 源码阅读入口

| 目录 | 负责什么 | 第一入口 |
| --- | --- | --- |
| Audio/BeatClock | 音乐时间与拍数 | BeatClockSubsystem.cpp |
| Combat | 主角、敌人、数据与表现，已经分子目录 | [战斗目录](Combat/README.md) |
| UI | 数据、控制器、视图与兼容绘制 | [界面目录](UI/README.md) |
| 根目录角色文件 | 组件组装与输入转发 | HearBladeCharacter.cpp |

先看 [职责、资源与流程](../../Docs/Architecture.md)，再读 [完整导读](../../Docs/READING_GUIDE.md)。编辑器工具在工程的 Plugins/HearBladeBeatEditor。
