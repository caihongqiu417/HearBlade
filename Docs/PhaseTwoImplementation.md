# Boss 二阶段交付核对

## 2026-09-27 用户确认更新

用户确认现有卡点听感可接受，保留当前连招速度和拍数。已删除偏拍超50ms的保存拦截，改为摘要实时提醒“仅供参考，不阻止保存”。这替代下方等待选择和禁止保存的旧结论。当前编辑器进程54004占用DLL，待关闭后编译插件。

更新：2026-09-27。目标仍在进行中。历史过程保留在 PhaseTwoImplementationHistory.md；本页为当前状态。

## 当前待处理问题

用户反馈 Combo1/2 卡点感下降。逐字段比较 Git HEAD 49cdcc4 与当前资产：命中时间、拍偏移、倍率、音乐参数和随机指令均相同，报告 output/arena/combo12_timing_audit.json。
但 Combo1 原配置本身不一致：0/3/5.5拍与倍率不匹配，使第2击提前0.5382秒、第3击提前0.8718秒；运行日志也证实。正在等待用户选择保留拍数还是较快动作，不覆盖其配置。Combo2计算间隔与拍数吻合，不等于所有音画感知延迟均已排除。

## 实现与证据

所有日志位于 output/arena，截图位于 Saved/Screenshots。

| 要求 | 实现与证据 |
|---|---|
| 二阶段切换 | 60%血量，当前连招结束后下一小节开始4拍无伤害过渡；不重启音乐、不回血；BossPhaseTransition.cpp及phase2_pie_audit.log |
| 二阶段提示 | 只显示中性圆圈，隐藏操作图标和长按条；实际截图和录像 |
| Combo03 | 33/70/94/175帧四击，172仅蓄力姿态；0/2/3/8拍，保持约0.185秒；姿态截图和phase2_timing_tests.log |
| 保留原连招 | C01/02仅新增识别元数据，对比原始Git资产战斗字段相同；C01原有偏拍待处理 |
| 每击识别 | DA支持名称、描述、提前量、形状、颜色、图标和可选音效；Auto跟随实际随机响应；武器叠层辅助读招 |
| 编辑器 | 源帧、提示开始、范围锁定/爆发、172无伤害节点、二阶段圆圈；BeatEditorPhaseTwo.png及phase2_editor_audit.log |
| 配置防护 | 超过50ms偏拍显示提前/延后量并禁止保存；编辑器编译成功，C03校验通过 |
| 范围规则 | 94后锁中心，450cm/25伤害/至少4拍；停止追踪和根位移；需要整个胶囊退出，防御和无敌不能抵消 |
| 安全出口 | 16方向可达地面/坡度/碰撞/时间检查；phase2_scenario_Blocked.log封路取消 |
| 脱离奖励 | 提前离圈无MISS；末120ms完整离圈12共鸣一次，无Combo；phase2_scenario_Early.log及phase2_escape_eligibility.log |
| 防刷奖励 | 初始在内、只跨界一次、爆发时安全；phase2_scenario_Reenter_verified.log两次跨界返回圈内，受伤一次无奖励 |
| 轮换 | 先两套近战，之后随机2–3套再范围；最终反击结束也计数；phase2_rotation_fixed.log及两轮PIE |
| 警示效果 | 原创透明金红刻纹、准确数学边界、收束环、短扩散波和受控火星；phase2_readability_final.log和实际截图 |
| 清理 | phase2_scenario_Death_verified.log死亡取消；phase2_pie_audit.log真正编辑器PIE两轮开始/结束，每轮奖励一次 |
| 编译 | build_phase2_editor.log和build_phase2_game.log均Result: Succeeded |

## 验证边界

- 限帧30/60/120的单次范围落点误差28.9/1.7/7.9ms；限帧不代表稳定实测FPS，不是普遍误差保证。
- 注入250ms主线程卡顿后只结算一次伤害，迟177.9ms，不能声称卡顿仍零延迟。
- 两轮PIE均转阶段、两套近战、范围招、踩拍逃离；奖励各一次12，逃离误差-87.5/-74.8ms。自动输入不代替手动听感验收。
- 最终材质录像已完成：output/arena/HearBlade_PhaseTwo_Final.mp4，1280×720，约64.6秒，包含引擎主混音，使用显式测试输入。已检查原始帧和导出范围预警画面，完整HUD可见。首次背缓冲采集出现黑块，因此改用游戏视口截图回调后重录；旧损坏采集不能作为验收依据。
- 可选识别音效未填新声音，保留原刀声。警示图为原创AI素材，来源见SourceArt/Combat/README.md。

## 配置与待完成

资产：/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_DragonKnight_Combo03_Beat。使用说明见BossPhaseTwoGuide.md。
测试参数均显式开启，普通游戏不会自动输入、扣血或截图。

1. 等待用户确认Combo1修正方向并复测。
2. 最终录像已导出并查看关键画面，主混音已录入；尚未人工听感验收。
3. 不能将单次测试外推为所有情况保证，全部目标尚未完成。
