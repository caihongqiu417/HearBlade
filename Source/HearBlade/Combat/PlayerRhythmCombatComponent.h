#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/AttackData.h"
#include "PlayerRhythmCombatComponent.generated.h"

UENUM(BlueprintType)
enum class ERhythmDodgeJudgment : uint8
{
	None,
	Perfect,
	Good,
	Bad,
	Miss,
	HoldBreak,
	GoodHold,
	PerfectHold
};

USTRUCT(BlueprintType)
struct FPerfectDodgeFeedbackData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FVector BladeWorldLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) FVector AttackDirection = FVector::ForwardVector;
	UPROPERTY(BlueprintReadOnly) float SignedErrorMs = 0.0f;
	UPROPERTY(BlueprintReadOnly) int32 ComboCount = 0;
	UPROPERTY(BlueprintReadOnly) float ResonanceFraction = 0.0f;
	UPROPERTY(BlueprintReadOnly) bool bStrongBeat = false;
};

USTRUCT(BlueprintType)
struct FRhythmSuccessFeedbackData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) ERhythmResponseType Response = ERhythmResponseType::Dodge;
	UPROPERTY(BlueprintReadOnly) ERhythmHoldMarkerType HoldMarker = ERhythmHoldMarkerType::None;
	UPROPERTY(BlueprintReadOnly) ERhythmDodgeJudgment Grade = ERhythmDodgeJudgment::None;
	UPROPERTY(BlueprintReadOnly) int32 PreviousCombo = 0;
	UPROPERTY(BlueprintReadOnly) int32 ComboCount = 0;
	UPROPERTY(BlueprintReadOnly) int32 FeedbackTier = 0;
	UPROPERTY(BlueprintReadOnly) bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly) bool bPerfect = false;
	UPROPERTY(BlueprintReadOnly) bool bKeyPerfect = false;
	UPROPERTY(BlueprintReadOnly) bool bStrongBeat = false;
	UPROPERTY(BlueprintReadOnly) FVector WorldLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) FVector AttackDirection = FVector::ForwardVector;
	UPROPERTY(BlueprintReadOnly) int64 EventId = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerHealthChanged, float, Health, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDodgeJudged, ERhythmDodgeJudgment, Grade, float, SignedErrorMs, int32, ComboCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPerfectDodge, const FPerfectDodgeFeedbackData&, Feedback);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRhythmSuccessFeedback, const FRhythmSuccessFeedbackData&, Feedback);

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class HEARBLADE_API UPlayerRhythmCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerRhythmCombatComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="生命", meta=(ClampMin="1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="生命")
	float Health = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="节奏闪避")
	float Resonance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节奏闪避", meta=(ClampMin="1.0"))
	float MaxResonance = 100.0f;

	// Resonance starts falling after the player stops landing successful rhythm
	// responses. The short grace period keeps individual gains readable.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="节奏闪避|共鸣衰减", meta=(ClampMin="0.0"))
	float ResonanceDecayDelaySeconds = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="节奏闪避|共鸣衰减", meta=(ClampMin="0.0"))
	float ResonanceDecayPerSecond = 6.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="节奏闪避")
	int32 ComboCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="节奏闪避")
	ERhythmDodgeJudgment LastJudgment = ERhythmDodgeJudgment::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="节奏防御")
	ERhythmResponseType LastResponse = ERhythmResponseType::Dodge;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="节奏闪避")
	float LastJudgmentErrorMs = 0.0f;

	// Hidden combat resource. The HUD intentionally does not draw this value.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="韧性", meta=(ClampMin="1.0"))
	float MaxPoise = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="韧性")
	float Poise = 100.0f;

	UPROPERTY(BlueprintAssignable, Category="生命")
	FOnPlayerHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category="节奏闪避")
	FOnDodgeJudged OnDodgeJudged;

	UPROPERTY(BlueprintAssignable, Category="节奏闪避")
	FOnPerfectDodge OnPerfectDodge;
	UPROPERTY(BlueprintAssignable, Category="节奏反馈")
	FOnRhythmSuccessFeedback OnRhythmSuccessFeedback;

	UFUNCTION(BlueprintCallable, Category="生命")
	void ApplyCombatDamage(float Damage);

	UFUNCTION(BlueprintCallable, Category="生命")
	void RestoreHealth(float Amount);

	UFUNCTION(BlueprintCallable, Category="节奏闪避")
	void SubmitDodgeJudgment(ERhythmDodgeJudgment Grade, float SignedErrorMs, bool bStrongBeat = false);

	UFUNCTION(BlueprintCallable, Category="节奏闪避")
	void ResetCombatState();

	UFUNCTION(BlueprintCallable, Category="节奏闪避")
	bool TryDodge(FVector WorldDirection);

	UFUNCTION(BlueprintCallable, Category="节奏防御") bool TryBlockStart();
	UFUNCTION(BlueprintCallable, Category="节奏防御") void EndBlock();
	UFUNCTION(BlueprintCallable, Category="节奏防御") bool TryCounter();
	UFUNCTION(BlueprintPure, Category="节奏防御") bool IsBlocking() const { return bBlocking; }
	// The visual starts after a very short sustained press, then remains active
	// through authored HoldBlock markers. A tap block therefore stays clean while
	// a player who deliberately holds guard gets immediate readable feedback.
	UFUNCTION(BlueprintPure, Category="节奏防御") bool IsSustainedHoldActive() const
	{
		return bBlocking && !bActiveHoldBroken && GetWorld() &&
			GetWorld()->GetTimeSeconds() - BlockStartWorldSeconds >= HoldArmorActivationSeconds;
	}
	UFUNCTION(BlueprintPure, Category="节奏防御") bool IsInDefensePose() const { return bBlocking || bBlockRecovering || bCountering; }
	UFUNCTION(BlueprintPure, Category="节奏防御") bool IsPerformingAction() const { return bDodging || bBlocking || bCountering || bBlockRecovering || bPoiseBroken || bHitReacting; }
	UFUNCTION(BlueprintPure, Category="韧性") bool IsPoiseBroken() const { return bPoiseBroken; }
	UFUNCTION(BlueprintPure, Category="受击") bool IsHitReacting() const { return bHitReacting || bPoiseBroken; }
	UFUNCTION(BlueprintPure, Category="韧性") float GetPoiseFraction() const { return MaxPoise > 0.0f ? Poise / MaxPoise : 0.0f; }

	// Called only when the real blade volume reaches the player capsule.
	bool ResolveBossContact(double ContactAudioSeconds, double TargetBeat, float Damage,
		float IdealDodgeLeadSeconds, bool bStrongBeat, FVector BladeWorldLocation, FVector AttackDirection,
		ERhythmResponseType RequiredResponse, ERhythmHoldMarkerType HoldMarker, FName HoldGroup,
		FVector AttackerWorldLocation);

	// Called when a real blade window ends without touching the player. This awards one timing
	// judgment when the player actually displaced out of the sweep with a dodge.
	bool ResolveBossAvoidance(double ImpactAudioSeconds, double TargetBeat,
		float IdealDodgeLeadSeconds, bool bStrongBeat, FVector BladeWorldLocation, FVector AttackDirection,
		ERhythmResponseType RequiredResponse, ERhythmHoldMarkerType HoldMarker, FName HoldGroup);

	UFUNCTION(BlueprintPure, Category="节奏闪避")
	bool IsDodging() const { return bDodging; }

	UFUNCTION(BlueprintPure, Category="节奏闪避")
	bool IsInvulnerable() const;

	UFUNCTION(BlueprintPure, Category="生命")
	float GetHealthFraction() const { return MaxHealth > 0.0f ? Health / MaxHealth : 0.0f; }

	UFUNCTION(BlueprintPure, Category="节奏闪避")
	float GetResonanceFraction() const { return MaxResonance > 0.0f ? Resonance / MaxResonance : 0.0f; }

	double GetLastJudgmentWorldSeconds() const { return LastJudgmentWorldSeconds; }
	FVector GetDodgeStartLocation() const { return DodgeStartLocation; }
	FVector GetDodgeDirection() const { return DodgeDirection; }
	float GetPerfectWindowSeconds() const { return PerfectWindowMs / 1000.0f; }
	float GetGoodWindowSeconds() const { return GoodWindowMs / 1000.0f; }
	UFUNCTION(BlueprintPure, Category="节奏反馈") static int32 GetFeedbackTierForCombo(int32 Combo);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	double LastJudgmentWorldSeconds = -1000.0;
	double LastResonanceGainWorldSeconds = -1000.0;
	double LastDodgeInputAudioSeconds = -1000.0;
	double DodgeStartWorldSeconds = -1000.0;
	FVector DodgeDirection = FVector::ForwardVector;
	FVector DodgeStartLocation = FVector::ZeroVector;
	bool bDodging = false;
	bool bBlocking = false;
	bool bBlockRecovering = false;
	bool bBlockReleasePending = false;
	bool bBufferedBlockReleased = false;
	double BufferedBlockReleaseAudioSeconds = -1000.0;
	TArray<double> BlockInputHistory;
	TArray<double> CounterInputHistory;
	bool bCountering = false;
	bool bPoiseBroken = false;
	bool bHitReacting = false;
	bool bHitTurnPending = false;
	FRotator HitFacingRotation = FRotator::ZeroRotator;
	double HitTurnEndWorldSeconds = -1000.0;
	double HitReactionEndWorldSeconds = -1000.0;
	double PoiseBreakStartWorldSeconds = -1000.0;
	double LastPoiseDamageWorldSeconds = -1000.0;
	double LastBlockStartAudioSeconds = -1000.0;
	double BlockStartWorldSeconds = -1000.0;
	double LastBlockReleaseAudioSeconds = -1000.0;
	double LastCounterInputAudioSeconds = -1000.0;
	double DefenseAnimationStartWorldSeconds = -1000.0;
	float DefenseAnimationDurationSeconds = 0.0f;
	double CounterStartWorldSeconds = -1000.0;
	FName ActiveHoldGroup;
	bool bActiveHoldBroken = false;
	TSet<FName> ReportedHoldBreakGroups;
	TSet<int64> ResolvedImpactKeys;
	uint32 DodgeSequenceId = 0;
	uint32 LastResolvedDodgeSequenceId = MAX_uint32;
	friend class FRhythmInputTransitionsTest;
	bool bAutomationTestMode = false;
	bool bAutomationFailureTestMode = false;
	bool bHUDPreviewMode = false;
	bool bPerfectFeedbackPreviewMode = false;
	int32 PerfectFeedbackPreviewStep = 0;
	double NextPerfectFeedbackPreviewWorldSeconds = 0.0;
	int32 HUDPreviewStep = 0;
	bool bSubmittingHUDPreviewJudgment = false;
	double NextHUDPreviewWorldSeconds = 0.0;
	double LastAutomatedImpactAudioSeconds = -1000.0;
	double AutomationReleaseAudioSeconds = -1000.0;
	bool bAutomationWrongActionIssued = false;
	int64 FeedbackEventSerial = 0;
	enum class EBufferedRhythmAction : uint8 { None, Dodge, Block, Counter };
	EBufferedRhythmAction BufferedAction = EBufferedRhythmAction::None;
	FVector BufferedDodgeDirection = FVector::ForwardVector;
	double BufferedInputAudioSeconds = -1000.0;
	double BufferedInputWorldSeconds = -1000.0;

	UPROPERTY(EditAnywhere, Category="节奏闪避|动作", meta=(ClampMin="0.1"))
	float DodgeDurationSeconds = 0.62f;

	// A new dodge may replace the ending of the current roll. This keeps one-beat
	// attack strings playable even when the authored roll is longer than one beat.
	UPROPERTY(EditAnywhere, Category="节奏闪避|动作", meta=(ClampMin="0.0"))
	float DodgeChainStartSeconds = 0.12f;

	// Commands pressed near the end of another action are remembered. Judgment
	// uses the original button timestamp rather than the later animation switch.
	UPROPERTY(EditAnywhere, Category="节奏输入", meta=(ClampMin="0.05", ClampMax="0.4"))
	float InputBufferSeconds = 0.26f;

	UPROPERTY(EditAnywhere, Category="节奏输入", meta=(ClampMin="0.0", ClampMax="0.5"))
	float CounterCancelStartSeconds = 0.12f;

	// Scales authored roll root motion. The capsule follows the animation, so the mesh never snaps back.
	UPROPERTY(EditAnywhere, Category="节奏闪避|动作", meta=(ClampMin="0.1", ClampMax="1.0"))
	float DodgeRootMotionTranslationScale = 0.32f;

	UPROPERTY(EditAnywhere, Category="节奏闪避|动作", meta=(ClampMin="0.0", ClampMax="400.0"))
	float DodgeDistanceCm = 185.0f;

	UPROPERTY(EditAnywhere, Category="节奏闪避|无敌帧", meta=(ClampMin="0.0"))
	float InvulnerabilityStartSeconds = 0.04f;

	UPROPERTY(EditAnywhere, Category="节奏闪避|无敌帧", meta=(ClampMin="0.0"))
	float InvulnerabilityEndSeconds = 0.42f;

	UPROPERTY(EditAnywhere, Category="节奏闪避|判定", meta=(ClampMin="1.0"))
	float PerfectWindowMs = 80.0f;

	UPROPERTY(EditAnywhere, Category="节奏闪避|判定", meta=(ClampMin="1.0"))
	float GoodWindowMs = 160.0f;

	UPROPERTY(EditAnywhere, Category="节奏防御|动作", meta=(ClampMin="0.25", ClampMax="1.5"))
	float CounterDurationSeconds = 0.62f;

	UPROPERTY(EditAnywhere, Category="节奏防御|霸体视觉", meta=(ClampMin="0.0", ClampMax="0.5"))
	float HoldArmorActivationSeconds = 0.14f;

	UPROPERTY(EditAnywhere, Category="韧性", meta=(ClampMin="1.0"))
	float BadPoiseDamage = 38.0f;

	UPROPERTY(EditAnywhere, Category="韧性", meta=(ClampMin="1.0"))
	float DirectHitPoiseDamage = 55.0f;

	UPROPERTY(EditAnywhere, Category="韧性", meta=(ClampMin="0.05", ClampMax="1.0"))
	float PoiseBreakInputLockSeconds = 0.28f;

	UPROPERTY(EditAnywhere, Category="韧性", meta=(ClampMin="0.1", ClampMax="1.5"))
	float PoiseBreakDurationSeconds = 0.62f;

	UPROPERTY(EditAnywhere, Category="受击", meta=(ClampMin="0.05", ClampMax="0.5"))
	float LightHitDurationSeconds = 0.34f;
	UPROPERTY(EditAnywhere, Category="受击", meta=(ClampMin="0.0", ClampMax="0.2"))
	float HitTurnSeconds = 0.07f;
	UPROPERTY(EditAnywhere, Category="受击", meta=(ClampMin="0.0", ClampMax="0.5"))
	float LightHitInputLockSeconds = 0.22f;

	UPROPERTY(EditAnywhere, Category="韧性", meta=(ClampMin="0.0", ClampMax="5.0"))
	float PoiseRecoveryDelaySeconds = 1.10f;

	UPROPERTY(EditAnywhere, Category="韧性", meta=(ClampMin="1.0", ClampMax="200.0"))
	float PoiseRecoveryPerSecond = 40.0f;

	UPROPERTY(EditAnywhere, Category="受击|血液特效")
	TObjectPtr<class UNiagaraSystem> BloodHitEffect;

	UPROPERTY(EditAnywhere, Category="受击|血液特效")
	TObjectPtr<class UNiagaraSystem> BloodPoiseBreakEffect;

	UPROPERTY(EditAnywhere, Category="受击|血液特效", meta=(ClampMin="0.1", ClampMax="3.0"))
	float BloodHitScale = 0.65f;

	UPROPERTY(EditAnywhere, Category="受击|血液特效", meta=(ClampMin="0.1", ClampMax="3.0"))
	float BloodPoiseBreakScale = 0.9f;

	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> DodgeForwardAnimation;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> DodgeBackwardAnimation;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> DodgeLeftAnimation;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> DodgeRightAnimation;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> BlockStartAnimation;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> BlockLoopAnimation;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> BlockEndAnimation;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> CounterAnimation;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> BlockHitReactAnimation;
	UPROPERTY(Transient) TObjectPtr<class UParticleSystem> BlockImpactEffect;
	UPROPERTY(Transient) TObjectPtr<class UParticleSystem> CounterImpactEffect;
	UPROPERTY(Transient) TObjectPtr<class UNiagaraSystem> PerfectHoldShockEffect;
	float DodgeDistanceApplied = 0.0f;
	double GetCurrentInputAudioSeconds() const;
	void BufferInput(EBufferedRhythmAction Action, double InputAudioSeconds, FVector Direction = FVector::ZeroVector);
	void ProcessBufferedInput();
	double ConsumeClosestInput(TArray<double>& History, double IdealAudioSeconds);
	void CancelDodgeForTransition();
	void ApplyPoiseDamage(float Amount, FVector ImpactLocation, FVector AttackDirection,
		FVector AttackerWorldLocation = FVector::ZeroVector);
	void StartPoiseBreak(FVector ImpactLocation, FVector AttackDirection,
		FVector AttackerWorldLocation = FVector::ZeroVector);
	void EndPoiseBreak();
	void StartHitReaction(FVector ImpactLocation, FVector AttackDirection, bool bHeavy,
		FVector AttackerWorldLocation = FVector::ZeroVector);
	void FinishHitReaction();
	void SpawnBloodFeedback(FVector ImpactLocation, FVector AttackDirection, bool bPoiseBreak) const;
	bool BufferWhilePoiseLocked(EBufferedRhythmAction Action, double InputAudioSeconds,
		FVector Direction = FVector::ZeroVector);
	bool StartDodge(FVector WorldDirection, double InputAudioSeconds);
	bool StartBlock(double InputAudioSeconds);
	bool StartCounter(double InputAudioSeconds);
	ERhythmDodgeJudgment JudgeTimedInput(double InputAudioSeconds, double IdealAudioSeconds) const;
	ERhythmDodgeJudgment JudgeCounterInput(double InputAudioSeconds, double IdealAudioSeconds) const;
	bool ResolveResponse(double ImpactAudioSeconds, double TargetBeat, float Damage, float IdealLeadSeconds,
		bool bStrongBeat, FVector BladeWorldLocation, FVector AttackDirection, ERhythmResponseType RequiredResponse,
		ERhythmHoldMarkerType HoldMarker, FName HoldGroup, bool bPhysicalContact,
		FVector AttackerWorldLocation = FVector::ZeroVector);
	void PlayDefenseAnimation(class UAnimSequence* Animation, bool bLooping, float DesiredDuration = 0.0f);
	void PlayResponseFeedback(ERhythmResponseType Response, ERhythmDodgeJudgment Grade,
		FVector WorldLocation, FVector AttackDirection, bool bStrongBeat);
	void BroadcastPerfectFeedback(float ErrorMs, bool bStrongBeat, FVector BladeWorldLocation, FVector AttackDirection);
};
