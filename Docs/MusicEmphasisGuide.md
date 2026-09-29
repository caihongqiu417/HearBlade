# 局部音乐强调点优先级

## 当前规则

- 候选来自实际游戏音频 Music/Paramore_AintItFun_UE.wav，时间单位为原音频文件秒数。
- 分析局部能量反差、频谱瞬态、中央声道中频突增，不把整个副歌自动视为高潮。
- 当前为混合音频启发式分析。中频突增只是人声突出程度的代理，吉他也可能触发；不是分离人声后的高音识别，也没有人工听感确认。
- 行为树在可用招式中优先选能让最后一击接近高强度候选的招式。阶段、距离、范围轮换和安全出口要求优先。
- 倒推最后一击，搜索现有规则允许的起手拍，额外最多等待4拍。排队期间继续原有走位，不中断正在播放的连招，不改动画分段倍率。
- 候选时间不强行吸附到第一拍；当现有起手限制/连招节奏不能在120ms内接近候选时回退普通调度。小节第一拍起手限制仍有效，因此不是所有候选都可命中。
- 使用真实音乐起播偏移换算时间；临时音乐速度变化继续沿用同一音乐时钟。换成其他Sound资产时不使用本曲强调表。

## 调整

资产：/Game/Audio/Music/DA_AintItFun_Emphasis

- Points：逐点修改SourceSeconds、Strength、bEnabled。Reason是自动分析理由。
- MinimumStrength：提高会只选择更明显的强调点。
- MaxExtraWaitBeats：降低会减少等待机会。
- MatchToleranceSeconds：匹配误差上限；不会因此改变判定窗口。
- bEnabled：关闭整套强调优先级。

节拍编辑器中的橙色短线为强调点，旁边数字表示强度，不是命中帧。
编辑器使用其预览请求与准备时间；实际行为树还会检查闪现预警、距离和安全性，所以不同现场状态可能选择下一个可行时机。

## 重分析

python Scripts/analyze_music_emphasis.py

结果：SourceArt/Audio/MusicEmphasis/AintItFun_emphasis.json（含源文件SHA256与方法说明）。
导入脚本：Scripts/import_music_emphasis.py。导入会替换Points，手动微调后不要随意重新导入。

只分析本地音频，没有上传歌曲或下载第三方音频素材。

## 当前验证状态

- 编辑器目标首次编译及73个候选点导入成功。
- music_emphasis_runtime.log 实际选择到 Combo01，最后一击原曲时间61.934秒，最近候选62.054秒，优先分数0.423。匹配容差不是Perfect输入窗口。
- 同次二阶段日志证实闪避与格挡均有Perfect、defended=1；一阶段保持原指令。
- 后续补充强引用保留资产、范围预选使用真实预警时间，编辑器最终编译通过（build_dodge_input_editor.log）。600厘米范围的补充复测尚未完成，不把旧运行结果当成新范围的验证。
- 按用户要求，Combo01/02 的时间与速度已恢复至二阶段改动之前的 9a85720 配置，保留当前识别信息和防御配置；具体倍率与备份见 output/arena/restored_combo12_timing.json。Combo01 原配置的偏拍仍只提醒，不自动修正。
