#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

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

	double GetViewStart() const { return ViewStart; }
	double GetViewDuration() const { return ViewDuration; }
	double GetBeatPeriod() const { return 60.0 / FMath::Max(BPM, 1.0); }
	double GetFirstBeatSourceSeconds() const;
	double GetFirstImpactSourceSeconds() const;
	double GetMontageStartSourceSeconds() const;
	double GetPreviewElapsedAtAnimationTime(double AnimationTime) const;
	const FBeatWaveform& GetWaveform() const { return Waveform; }
	const UAttackDataAsset* GetAttackData() const { return AttackData.Get(); }
	float GetPreviewRate() const { return PreviewRate; }
	int32 GetSelectedBeat() const;
	double GetPreviewRequestSourceSeconds() const { return PreviewRequestSourceSeconds; }
	void PickRequestAtSourceSeconds(double SourceSeconds);

private:
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
	void OnAssetPropertiesChanged(const FPropertyChangedEvent& Event);

	TWeakObjectPtr<UAttackDataAsset> AttackData;
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<SEditableTextBox> WavPathBox;
	TSharedPtr<class SBeatComboTimeline> Timeline;
	TSharedPtr<SVerticalBox> SegmentsBox;
	FBeatWaveform Waveform;
	FString WavPath;
	FString Status;
	double BPM = 104.0;
	double BeatPhase = 0.204;
	double SourceStart = 4.0;
	double ViewStart = 35.0;
	double ViewDuration = 35.0;
	float MinimumRate = 1.0f;
	float MaximumRate = 1.15f;
	float PreviewRate = 1.0f;
	TArray<float> PreviewSegmentRates;
	bool bUseSegmentRates = false;
	double PreviewRequestSourceSeconds = 49.0;
};
