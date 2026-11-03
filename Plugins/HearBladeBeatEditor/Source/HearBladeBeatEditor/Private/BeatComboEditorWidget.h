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

	double GetFirstBeatSourceSeconds() const;
	double GetFirstImpactSourceSeconds() const;
	double GetMontageStartSourceSeconds() const;
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

	int32 GetSelectedBeat() const;
	const UMusicEmphasisData* GetMusicEmphasis() const;

	double GetPreviewRequestSourceSeconds() const
	{
		return PreviewRequestSourceSeconds;
	}

	void PickRequestAtSourceSeconds(double SourceSeconds);
	bool IsImpactResponseValid(int32 ImpactIndex) const;

	bool IsPhaseTwoPreview() const
	{
		return bPhaseTwoPreview;
	}

	bool TryBeginHoldEndpointDrag(double SourceSeconds, double ToleranceSeconds, FName& OutGroup,
	                              bool& bOutStart) const;
	void DragHoldEndpoint(FName Group, bool bStartEndpoint, double SourceSeconds);

  private:
	// 资源选择与预览操作
	void SelectAsset(UAttackDataAsset* Asset);
	void HandleAssetChanged(const struct FAssetData& AssetData);
	FString GetAssetPath() const;
	FReply BrowseWav();
	FReply LoadWav();
	FReply AutoAlign();
	FReply AutoAlignSegments();
	FReply ResetSegments();
	FReply SaveAsset();
	FText GetSummary() const;
	void RefreshTimeline();
	void RebuildSegmentControls();
	void RebuildResponseControls();
	FReply ApplyResponseTemplate(int32 TemplateIndex);
	FReply ValidateResponses();
	FReply SelectHoldRangeEndpoint(int32 ImpactIndex);
	// 保存前检查指令间隔和持续格挡分组
	bool ValidateResponseConfiguration(FString& OutError) const;
	void OnAssetPropertiesChanged(const FPropertyChangedEvent& Event);

	// 当前编辑资源与窗口控件
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
	double PreviewRequestSourceSeconds = 49.0;
	int32 PendingHoldRangeStart = INDEX_NONE;
	bool bPhaseTwoPreview = false;
};
