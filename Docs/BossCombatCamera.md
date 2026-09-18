# Boss 战斗相机

玩家靠近测试关卡的 `BossBeatDirector` 且行为树正在运行时，相机自动锁定 Boss。鼠标中键或手柄右摇杆按下可切换锁定。手动解除后，离开战斗距离再返回才会自动锁回。

在内容浏览器打开 `/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_BossCamera` 调整锁定距离、追踪速度、视野和闪现反馈。Boss 的火焰预警会提前引导镜头；只有瞬移成功才在落拍时推镜。相机震动由独立修饰器叠加，不改变玩家输入旋转。

近距离构图可调 `Combat Pivot Height`（镜头支点高度）、`Combat Shoulder Offset`（肩侧偏移）、`Combat Arm Length At Close Range`、`Close Range FOV`、`Boss Focus Weight` 和 `Max Close Range Upward Pitch`。默认在贴身时抬高镜头、露出玩家侧肩并限制仰拍。解除锁定后这些偏移会平滑恢复。

`UBeatSyncedComboComponent::NotifyConfirmedHit()` 留给真实伤害确认调用。现有动画攻击点并不代表伤害已命中，因此不会触发命中专用震动。

Boss 的所有碰撞组件在运行时忽略 `Camera` 通道，但仍阻挡 `Pawn`。锁定瞄准使用弹簧臂未碰撞时的参考镜头位置；Boss 绕到玩家身后时不会再让弹簧臂反复收缩，墙壁仍正常遮挡镜头。
