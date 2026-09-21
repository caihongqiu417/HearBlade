#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Combat/PlayerRhythmCombatComponent.h"
#include "RhythmCombatHUD.generated.h"

class UTexture2D;
class UBeatSyncedComboComponent;
class URhythmResponseVisualData;

UCLASS()
class HEARBLADE_API ARhythmCombatHUD : public AHUD
{
	GENERATED_BODY()

public:
	ARhythmCombatHUD();
	virtual void DrawHUD() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FJudgmentFeedEntry
	{
		ERhythmDodgeJudgment Grade = ERhythmDodgeJudgment::None;
		float ErrorMs = 0.0f;
		double TimeSeconds = 0.0;
		int32 Combo = 0;
		float StartSlot = 5.0f;
		float TargetSlot = 4.0f;
	};

	void DrawPlayerPanel(const UPlayerRhythmCombatComponent* State, float Scale, double Now);
	void DrawJudgment(const UPlayerRhythmCombatComponent* State, float Scale, double Now);
	void DrawFailureFeedback(float Scale, double Now);
	void DrawAttackTimingCue(const UPlayerRhythmCombatComponent* State, float Scale);
	void DrawLockMarker();
	void DrawBossHealth(float Scale);
	void DrawAudioVisualizer(float Scale);
	UPROPERTY() TObjectPtr<UTexture2D> BossHealthFrameTexture;
	float BossHealthTrail = 1.f;
	TWeakObjectPtr<UBeatSyncedComboComponent> HealthBarTarget;
	UBeatSyncedComboComponent* FindCueCombo();
	static FLinearColor GetJudgmentColor(ERhythmDodgeJudgment Grade);
	static FString GetJudgmentText(ERhythmDodgeJudgment Grade);
	UTexture2D* GetJudgmentWordTexture(ERhythmDodgeJudgment Grade) const;

	UPROPERTY() TObjectPtr<UTexture2D> PlayerFrameTexture;
	UPROPERTY() TObjectPtr<UTexture2D> JudgmentBurstTexture;
	UPROPERTY() TObjectPtr<UTexture2D> ComboSlashTexture;
	UPROPERTY() TObjectPtr<UTexture2D> JudgmentRailTexture;
	UPROPERTY() TObjectPtr<UTexture2D> JudgmentRailBackboneTexture;
	UPROPERTY() TObjectPtr<UTexture2D> JudgmentCardTexture;
	UPROPERTY() TObjectPtr<UTexture2D> ComboPanelTexture;
	UPROPERTY() TObjectPtr<UTexture2D> PerfectWordTexture;
	UPROPERTY() TObjectPtr<UTexture2D> GoodWordTexture;
	UPROPERTY() TObjectPtr<UTexture2D> BadWordTexture;
	UPROPERTY() TObjectPtr<UTexture2D> MissWordTexture;
	UPROPERTY() TObjectPtr<UTexture2D> ComboWordTexture;
	UPROPERTY() TObjectPtr<UTexture2D> AttackTimingRailTexture;
	UPROPERTY() TObjectPtr<UTexture2D> AttackSlashTexture;
	UPROPERTY() TObjectPtr<UTexture2D> DodgeResponseTexture;
	UPROPERTY() TObjectPtr<UTexture2D> BlockResponseTexture;
	UPROPERTY() TObjectPtr<UTexture2D> CounterResponseTexture;
	UPROPERTY() TObjectPtr<UTexture2D> HoldResponseTexture;
	UPROPERTY() TObjectPtr<URhythmResponseVisualData> ResponseVisuals;
	TArray<FJudgmentFeedEntry> JudgmentFeed;
	FJudgmentFeedEntry OutgoingJudgment;
	bool bHasOutgoingJudgment = false;
	double QueueAnimationStartSeconds = -1000.0;
	double ComboPunchStartSeconds = -1000.0;
	int32 LastComboCount = INDEX_NONE;
	int32 LastComboTier = 0;
	double ComboTierUpWorldSeconds = -1000.0;
	double LastCapturedJudgmentSeconds = -1000.0;
	double FailureFeedbackStartSeconds = -1000.0;
	ERhythmDodgeJudgment FailureFeedbackGrade = ERhythmDodgeJudgment::None;
	float FailureFeedbackErrorMs = 0.0f;
	double LastPromptedImpactAudioSeconds = -1000.0;
	TWeakObjectPtr<UBeatSyncedComboComponent> CachedCueCombo;
	bool bSpectrumAnalysisStarted = false;
	TArray<float> SmoothedSpectrum;
	float SpectrumAutoPeak = 0.01f;
};
