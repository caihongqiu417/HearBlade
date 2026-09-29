#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/PlayerAttackSet.h"
#include "Combat/AttackData.h"
#include "Combat/PlayerRhythmCombatComponent.h"
#include "CombatTutorialComponent.generated.h"

class UBeatSyncedComboComponent;
class UPlayerRhythmCombatComponent;
class UPlayerBeatAttackComponent;
struct FRhythmSuccessFeedbackData;

UENUM()
enum class ECombatTutorialStep : uint8
{
	None,
	// Boss sparring (behaviour tree paused): defence with single practice notes,
	// then the two basic combos; afterwards the fight restarts from the top.
	Dodge, Block, Counter, LightCombo, HeavyCombo, Openings, Resonance, Done
};

// Teach-by-doing tutorial (Hi-Fi Rush style): a short card says what to press,
// the boss spars with single notes, and each step advances only when the player
// actually does it. Runs once per save (GameUserSettings.ini), skippable by
// holding TAB / View. Added at runtime by the HUD; the HUD draws the card.
UCLASS(ClassGroup=(Combat))
class HEARBLADE_API UCombatTutorialComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCombatTutorialComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---- read by the HUD --------------------------------------------------
	bool IsCardVisible() const { return Step != ECombatTutorialStep::None; }
	ECombatTutorialStep Step = ECombatTutorialStep::None;
	FString Title, Body;
	// Input glyph texture names for the current device (T_TUT_K_* or T_TUT_P_*), shown left to right.
	TArray<FName> Glyphs;
	int32 Progress = 0, Goal = 0;
	bool bGamepad = false;
	float SkipHold = 0.f;            // 0..1 while the gamepad skip button is held
	// SKIP button (screen pixels, written by the HUD). Hold ALT to show the cursor and click it.
	FBox2D SkipButton = FBox2D(ForceInit);
	bool bCursorMode = false, bSkipHover = false;
	double StepStartWorld = 0.;
	double SuccessFlashWorld = -1000.; // a step's goal counter just went up
	double MissWorld = -1000.;         // the last practice note was missed
	// Non-blocking one-time tips during the real fight (resonance, hold notes...).
	FString Callout; TArray<FName> CalloutGlyphs; double CalloutWorld = -1000.;

private:
	UFUNCTION() void HandleRhythmFeedback(const FRhythmSuccessFeedbackData& Feedback);
	UFUNCTION() void HandleAttackHit(AActor* Target, float Damage, EPlayerAttackGrade Grade);
	UFUNCTION() void HandleBossPhase(int32 Phase);
	void Bind();
	void Begin(ECombatTutorialStep NewStep);
	// bRestartFight: reload the level so the real boss fight starts fresh.
	void Finish(bool bRestartFight);
	void CountCombo(TCHAR Letter);
	void SetBossHold(bool bHold);
	void Count(int32 Amount = 1);
	void TickPractice(double WorldNow);
	void TickDevice();
	void TickCallouts(double WorldNow);
	// The behaviour tree (which normally chases) is paused while sparring: walk the boss to the player here.
	void TickBossApproach(float DeltaTime, bool bActive);
	void RefreshGlyphs();
	ERhythmResponseType PracticeResponse() const;
	// Once per game session (per GameInstance): every launch teaches again, the fight restart does not.
	bool IsDone() const;
	void MarkDone() const;
	void SetCursorMode(bool bOn);

	TWeakObjectPtr<UPlayerRhythmCombatComponent> Combat;
	TWeakObjectPtr<UPlayerBeatAttackComponent> Attacks;
	TWeakObjectPtr<UBeatSyncedComboComponent> Boss;
	bool bBound = false, bBossHeld = false, bPracticePending = false;
	double NextPracticeWorld = 0., AdvanceAtWorld = -1.;
	TWeakObjectPtr<class ABossBeatDirector> Director;
	FVector MoveStart = FVector::ZeroVector;
	int32 OpeningMarksAtStart = 0;
	double LastOpeningCounted = -1000.;
	bool bCalloutResonance = false, bCalloutHold = false, bCalloutArea = false;
};
