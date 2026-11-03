// 构建节拍攻击编辑窗口，处理标记、预览与资产保存
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "Combat/Data/MusicEmphasisData.h"

class UAttackDataAsset;
class IDetailsView;
class SEditableTextBox;
class SVerticalBox;
struct FPropertyChangedEvent;

struct FBeatWaveform
{
	TArray<float> Peaks;
	double DurationSeconds = 0.0;
	FString Error;
	bool Load(const FString& Path);
};

class SBeatComboEditorWidget : public SCompoundWidget
{
  public:
	SLATE_BEGIN_ARGS(SBeatComboEditorWidget) {}
	SLATE_END_ARGS()
	// 创建资源选择、波形时间轴、调速控件和属性面板
	void Construct(const FArguments& InArgs);

	double GetViewStart() const
	{
		return ViewStart;
	}

	double GetViewDuration() const
	{
		return ViewDuration;
	}

	double GetBeatPeriod() const
	{
		return 60.0 / FMath::Max(BPM, 1.0);
	}

	// 结合音乐起点和偏移，计算波形中的首个拍点
	double GetFirstBeatSourceSeconds() const;
	// 返回预览中第一击的音乐时间
	double GetFirstImpactSourceSeconds() const;
	// 从首击时间倒推动画开始时间
	double GetMontageStartSourceSeconds() const;
	// 按各段播放速度累加时长，并计入蓄力停顿
	double GetPreviewElapsedAtAnimationTime(double AnimationTime) const;

	const FBeatWaveform& GetWaveform() const
	{
		return Waveform;
	}

	const UAttackDataAsset* GetAttackData() const
	{
		return AttackData.Get();
	}

	float GetPreviewRate() const
	{
		return PreviewRate;
	}

	// 预留动作前摇，再按小节要求与音乐重音选择首击拍点
	int32 GetSelectedBeat() const;
	// 取得音乐重音配置，供首击拍点预览使用
	const UMusicEmphasisData* GetMusicEmphasis() const;

	double GetPreviewRequestSourceSeconds() const
	{
		return PreviewRequestSourceSeconds;
	}

	// 修改临时攻击请求时间并刷新预览，不写入攻击资源
	void PickRequestAtSourceSeconds(double SourceSeconds);
	// 检查指定命中的防御指令是否有效
	bool IsImpactResponseValid(int32 ImpactIndex) const;

	bool IsPhaseTwoPreview() const
	{
		return bPhaseTwoPreview;
	}

	// 查找鼠标附近的持续格挡端点
	bool TryBeginHoldEndpointDrag(double SourceSeconds, double ToleranceSeconds, FName& OutGroup,
	                              bool& bOutStart) const;
	// 把拖动位置转换为对应的格挡起止配置
	void DragHoldEndpoint(FName Group, bool bStartEndpoint, double SourceSeconds);

  private:
	// 资源选择与预览操作
	void SelectAsset(UAttackDataAsset* Asset);
	// 将资源选择器的结果交给 SelectAsset
	void HandleAssetChanged(const struct FAssetData& AssetData);
	// 返回当前选择的攻击资源路径
	FString GetAssetPath() const;
	// 打开文件选择窗口，选择波形使用的 WAV
	FReply BrowseWav();
	// 加载当前音频路径并刷新波形
	FReply LoadWav();
	// 尝试允许范围内的整体速度，选择各次命中拍点误差最小的结果
	FReply AutoAlign();
	// 分别计算相邻命中间的速度，蓄力停顿先从可用时间中扣除
	FReply AutoAlignSegments();
	// 恢复整段同速的预览配置
	FReply ResetSegments();
	FReply SaveAsset();
	// 汇总当前资源、拍点与播放速度的预览信息
	FText GetSummary() const;
	// 通知时间轴重新绘制
	void RefreshTimeline();
	// 按命中数量重建各段播放速度输入框
	void RebuildSegmentControls();
	// 根据命中标记重建防御指令编辑控件
	void RebuildResponseControls();
	// 将选中的防御指令模板写入攻击标记
	FReply ApplyResponseTemplate(int32 TemplateIndex);
	// 检查防御指令并把结果显示在窗口中
	FReply ValidateResponses();
	// 用两次选择确定持续格挡的起止标记
	FReply SelectHoldRangeEndpoint(int32 ImpactIndex);
	// 保存前检查指令间隔和持续格挡分组
	bool ValidateResponseConfiguration(FString& OutError) const;
	// 资源属性变化后重建相关控件并更新预览
	void OnAssetPropertiesChanged(const FPropertyChangedEvent& Event);

	// 当前编辑资源与窗口控件
	// 属性面板编辑的攻击资源，与下方临时预览速度分开保存
	TWeakObjectPtr<UAttackDataAsset> AttackData;
	TStrongObjectPtr<UMusicEmphasisData> EmphasisData;
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<SEditableTextBox> WavPathBox;
	TSharedPtr<class SBeatComboTimeline> Timeline;
	TSharedPtr<SVerticalBox> SegmentsBox;
	TSharedPtr<SVerticalBox> ResponsesBox;
	FBeatWaveform Waveform;
	FString WavPath;
	FString Status;
	// 音乐时间与视图范围
	double BPM = 104.0;
	double BeatPhase = 0.204;
	double SourceStart = 4.0;
	double ViewStart = 35.0;
	double ViewDuration = 35.0;
	// 预览速度，点击保存后才写入攻击资源
	float MinimumRate = 1.0f;
	float MaximumRate = 1.15f;
	float PreviewRate = 1.0f;
	TArray<float> PreviewSegmentRates;
	bool bUseSegmentRates = false;
	// 模拟 Boss 何时请求攻击，仅用于预览，不保存为正式攻击时间
	double PreviewRequestSourceSeconds = 49.0;
	// 第一次选择的持续格挡起点，第二次选择后形成完整范围
	int32 PendingHoldRangeStart = INDEX_NONE;
	bool bPhaseTwoPreview = false;
};
