# Harmonic Court / 共鸣庭院

## 场景与源文件

- 当前游戏地图：`/Game/ThirdPerson/Lvl_ThirdPerson`，原有 Boss、Director、音乐和玩家配置保留。
- 当前 Blender 源文件：`SourceArt/Environment/HarmonicCourtV2/HarmonicCourtV2.blend`。
- 模块 FBX、部件位置清单和实模渲染：同目录。
- 当前 UE 模型、材质、纹理：`/Game/Environment/HarmonicCourtV2`。
- 使用本机 Blender MCP，通过 Blender 原生建模操作、倒角修改器及 FBX 导出制作。无外部模型素材，无运行时网格生成。
- 视觉方向：明亮的象牙白石材、蓝色珐琅、香槟金镶边；Boss 的火焰保持主视觉。第二版增加了弧形复合浮环、凹陷扬声器面板、分层振膜、尖冠、金属徽记、蓝金地面拼花、石砖缝、连续栏杆、外围拱廊、尖塔、浮空岩柱、树冠与花坛。
- 三套 Blender 烘焙表面颜色纹理（石材、珐琅、金属）使用 UE 世界三向投射，避免大型建筑纹理拉伸；对应粗糙度烘焙图保留在源文件目录，运行材质按各材质独立配置粗糙度。
- 建模、烘焙、细化操作脚本位于 `Scripts/Art/*_mcp.py`，通过 Blender MCP 执行原生操作；`import_harmonic_court_v2.py` 仅用于 Unreal 资产导入与关卡装配。
- 按后续反馈调整为更成熟沉稳的幻想建筑：降低白蓝饱和度，采用暖灰石材、深蓝珐琅和旧金；树冠、岩体、尖顶细分并修正平滑法线。塔身增加曲面金属包边、侧面卷草纹、珠饰、细线脚，栏板增加金属菱形纹，远塔增加尖拱装饰。
- `T_HC_Limestone_Detail` 为新生成的细矿物石材底色，来源记录在源文件目录的 `TextureProvenance.md`。蓝色与金属表面保留 Blender 烘焙纹理。
- Blender 中单独制作 40 个 UCX 碰撞体；角色行走使用平整地面与栏杆碰撞，视觉拼花、珠饰和线脚不再造成移动阻挡。UE 5.8 的 Interchange 转换会用 `auto_generate_collision` 控制整个碰撞导入开关，因此导入主体时必须开启该项才能读到 UCX。

## 节拍运动

关卡中的 `Harmonic Court - Beat Motion` 读取现有 `UBeatClockSubsystem`。独立的 52 个装饰部件使用同一音乐时钟，绝对变换从静止姿态求值，避免累计偏移。

- `BeatCrown`：塔冠上下起伏。
- `BeatSpeaker`：扬声器振膜沿朝向场地中心方向轻推。
- `BeatHalo`：分段浮环以共同圆心展开和回落。
- `BeatShard`：晶体升降及轻微旋转。

地面与建筑主体不运动，装饰运动部件无碰撞。声音停止后回到静止状态；退出关卡恢复变换。没有加入音频可视化贴图。

在 Details 中调整 `Motion Intensity`、`Crown Bounce Cm`；`Reduced Motion` 将运动幅度降至 20%。

## 旧地图备份

修改前的地图和 World Partition 外部包备份于 `Saved/Backups/HarmonicCourt_20260927_145607/Content`。
按用户要求，第二版已从当前关卡彻底删除旧灰盒及第一版场景，共 100 个外部 Actor 包；旧模型源资产保留，避免影响其他地图引用。此次修改前完整外部包备份于 `Saved/Backups/CourtV2_20260927_152128`，删除清单为同目录 `removed_actors.json`。不要将备份与新外部包混合恢复；如需回滚，应整体恢复原地图及其外部包。

## 验证入口

编辑器目标编译通过。开发版启动参数 `-HarmonicCourtCapture` 在实际游戏运行的第 5、12 秒保存截图到 `Saved/Screenshots/HarmonicCourt_*.png`，记录部件位移，第 16 秒退出。该参数仅用于显式验收，不影响正常游戏。

2026-09-27：完成独立游戏模式运行检查；第 5 秒与第 12 秒画面确认平台落地、Boss 连招、旧灰盒隐藏及场景位置正常。日志确认 44 个部件接入 BeatClock，塔冠等部件随采样拍位改变。未使用鼠标操作。尚未做整场战斗、场地边缘遍历和多帧率性能验收。

第二版最终检查：重新加载地图确认旧灰盒/第一版场景 Actor 为 0，新版模型 Actor 为 53；主体读到 40 个 UCX 碰撞体。实际游戏运行确认 Boss 连招、52 个节拍部件和材质正常；中心及四个外围采样点均能命中场景。最终游戏截图为 `output/arena/HarmonicCourt_Mature_Game.png`。尚未进行完整边界遍历及长时间性能验收。

## 场地节拍装置与天空（2026-09-27）

- 新增 12 组共 48 个模型 Actor：石金底座、双层调音转环、悬挂摆锤、低矮共振盘。布置在距场地中心 16～17.28 米处，不占用中央交战空间，装饰无碰撞。
- 四套源模型在 Blender MCP 中使用圆柱、圆环、倒角等原生建模操作完成；源文件 `SourceArt/Environment/CourtResonators/CourtResonators.blend`，导入资产 `/Game/Environment/CourtResonators/Meshes`。
- 新增 36 个运动部件，连同原来的 52 个共 88 个部件读取同一 BeatClock。摆锤每拍经过中心、双拍往返；转环四拍往返；共振盘按拍轻微升降和扩张。
- 新天空 `M_Court_TwilightSky` 使用蓝紫天顶、暖灰地平线及低对比云层，主光保持稳定。天空亮度每四小节缓慢呼吸，最大变化 2.5%，云层相位随音乐缓慢移动，无音频贴图或频谱柱。
- `Harmonic Court - Beat Motion` Details 中 `Pendulum Angle Degrees` 默认 14°，`Sky Pulse Strength` 默认 0.025，`Reduced Motion` 将运动与天空呼吸降至 20%。
- 导入/装配脚本：`Scripts/Art/install_court_rhythm_sky.py`。更新前外部 Actor 包备份位于 `Saved/Backups/CourtRhythmSky_*`。

## 临时战斗特效回收

- 连续自动交互复现白色团块；修复前 25/50/80 秒仍活动的 `NS_PerfectDodge_Ring` 数量分别为 16/35/48。该资产由持续余烬复制，AutoDestroy 不会主动结束循环发射。
- `TransientCombatFX.h` 对一次性反馈设置明确发射和尾迹期限：完美余烬 0.12 + 0.45 秒，格挡/反击粒子 0.15 + 0.45 秒；闪现按原预警时长停止，再给 0.6 秒尾迹并兜底回收。
- 仅处理新生成的临时实例，不修改 Boss 身体/刀刃上持续燃烧的特效资产，不改变判定、连招或伤害。
- `-HarmonicCourtLongCapture -RhythmCombatAutoTest` 为显式测试参数，运行 85 秒，在 5/25/50/80 秒截取实际游戏画面并审计 Niagara/Cascade。正常启动不自动输入、不自动退出。
- 修复后编辑器目标编译通过；85 秒实际游戏自动交互完成（48 Combo），25/50/80 秒完美余烬存活数均为 0，80 秒画面无此前白色团块与空中余烬堆积。新天空材质无编译失败，88 个部件已绑定。日志 `output/arena/particles_auto_after.log`；对比图 `Particles_Before_80.png` / `Particles_After_80.png`，同目录。此验证覆盖连续交互，尚未覆盖所有战斗分支及长时间压力测试。

### 白色喷泉补修

用户随后反馈固定位置仍有小喷泉，说明上述稀疏采样不能证明瞬间特效正确。资产文本审计确认 `NS_Embers_System` 及其复制品 `NS_PerfectDodge_Ring` 依赖 `User.Spawn skeletal mesh`、`User.Spawn and color texture`；直接 SpawnSystemAtLocation 未绑定网格与遮罩，不能作为独立落点/判定粒子。持续时间限制只解决累积，不解决该资源误用。

- 独立闪现落点将这两个已知骨骼采样资产替换为已有 `NS_Fire_local_System` 火焰。
- 完美判定/完美松开将已知复制余烬替换为短促 `P_Spark`，原金环、角色残影与屏幕表现保留。自定义的其他 Niagara 引用继续受明确时限管理。
- Boss 身体上已经绑定网格的余烬不替换。
- `-CourtFXProbe` 在前两次闪现后的 0.1/0.5/1.2 秒截图，供检查瞬时喷泉；正常启动不截图。
- 补修验证：游戏目标及编辑器目标编译通过。85 秒独立游戏连续交互完成，检查前两次闪现瞬时截图及第 80 秒画面，未见白色喷泉。日志 `output/arena/standalone_fx_after.log` 确认落点实际使用火焰；第 80 秒无 WorldSettings 下遗留 Niagara，保留的两个动画拖尾均 inactive。截图 `output/arena/BlinkFX_Fixed.png`。这属于本次场景与操作序列的验证，不代表全部特效分支均已穷举。
