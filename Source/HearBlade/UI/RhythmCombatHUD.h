#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Combat/PlayerRhythmCombatComponent.h"
#include "RhythmCombatHUD.generated.h"

class UTexture2D;
class UBeatSyncedComboComponent;

UCLASS()
class HEARBLADE_API ARhythmCombatHUD : public AHUD
{
	GENERATED_BODY()

public:
	ARhythmCombatHUD();
	virtual void DrawHUD() override;

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
	void DrawAttackTimingCue(const UPlayerRhythmCombatComponent* State, float Scale);
	void DrawLockMarker();
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
	TArray<FJudgmentFeedEntry> JudgmentFeed;
	FJudgmentFeedEntry OutgoingJudgment;
	bool bHasOutgoingJudgment = false;
	double QueueAnimationStartSeconds = -1000.0;
	double ComboPunchStartSeconds = -1000.0;
	int32 LastComboCount = INDEX_NONE;
	double LastCapturedJudgmentSeconds = -1000.0;
	TWeakObjectPtr<UBeatSyncedComboComponent> CachedCueCombo;
};
