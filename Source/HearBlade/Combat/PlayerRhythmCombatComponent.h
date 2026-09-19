#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerRhythmCombatComponent.generated.h"

UENUM(BlueprintType)
enum class ERhythmDodgeJudgment : uint8
{
	None,
	Perfect,
	Good,
	Bad,
	Miss
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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerHealthChanged, float, Health, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDodgeJudged, ERhythmDodgeJudgment, Grade, float, SignedErrorMs, int32, ComboCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPerfectDodge, const FPerfectDodgeFeedbackData&, Feedback);

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="节奏闪避")
	int32 ComboCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="节奏闪避")
	ERhythmDodgeJudgment LastJudgment = ERhythmDodgeJudgment::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="节奏闪避")
	float LastJudgmentErrorMs = 0.0f;

	UPROPERTY(BlueprintAssignable, Category="生命")
	FOnPlayerHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category="节奏闪避")
	FOnDodgeJudged OnDodgeJudged;

	UPROPERTY(BlueprintAssignable, Category="节奏闪避")
	FOnPerfectDodge OnPerfectDodge;

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

	// Called only when the real blade volume reaches the player capsule.
	bool ResolveBossContact(double ContactAudioSeconds, double TargetBeat, float Damage,
		float IdealDodgeLeadSeconds, bool bStrongBeat, FVector BladeWorldLocation, FVector AttackDirection);

	// Called when a real blade window ends without touching the player. This awards one timing
	// judgment when the player actually displaced out of the sweep with a dodge.
	bool ResolveBossAvoidance(double ImpactAudioSeconds, double TargetBeat,
		float IdealDodgeLeadSeconds, bool bStrongBeat, FVector BladeWorldLocation, FVector AttackDirection);

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

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	double LastJudgmentWorldSeconds = -1000.0;
	double LastDodgeInputAudioSeconds = -1000.0;
	double DodgeStartWorldSeconds = -1000.0;
	FVector DodgeDirection = FVector::ForwardVector;
	FVector DodgeStartLocation = FVector::ZeroVector;
	bool bDodging = false;
	uint32 DodgeSequenceId = 0;
	uint32 LastResolvedDodgeSequenceId = MAX_uint32;
	bool bAutomationTestMode = false;
	bool bHUDPreviewMode = false;
	bool bPerfectFeedbackPreviewMode = false;
	int32 PerfectFeedbackPreviewStep = 0;
	double NextPerfectFeedbackPreviewWorldSeconds = 0.0;
	int32 HUDPreviewStep = 0;
	bool bSubmittingHUDPreviewJudgment = false;
	double NextHUDPreviewWorldSeconds = 0.0;
	double LastAutomatedImpactAudioSeconds = -1000.0;

	UPROPERTY(EditAnywhere, Category="节奏闪避|动作", meta=(ClampMin="0.1"))
	float DodgeDurationSeconds = 0.62f;

	// A new dodge may replace the ending of the current roll. This keeps one-beat
	// attack strings playable even when the authored roll is longer than one beat.
	UPROPERTY(EditAnywhere, Category="节奏闪避|动作", meta=(ClampMin="0.0"))
	float DodgeChainStartSeconds = 0.40f;

	// Scales authored roll root motion. The capsule follows the animation, so the mesh never snaps back.
	UPROPERTY(EditAnywhere, Category="节奏闪避|动作", meta=(ClampMin="0.1", ClampMax="1.0"))
	float DodgeRootMotionTranslationScale = 0.32f;

	UPROPERTY(EditAnywhere, Category="节奏闪避|无敌帧", meta=(ClampMin="0.0"))
	float InvulnerabilityStartSeconds = 0.04f;

	UPROPERTY(EditAnywhere, Category="节奏闪避|无敌帧", meta=(ClampMin="0.0"))
	float InvulnerabilityEndSeconds = 0.42f;

	UPROPERTY(EditAnywhere, Category="节奏闪避|判定", meta=(ClampMin="1.0"))
	float PerfectWindowMs = 60.0f;

	UPROPERTY(EditAnywhere, Category="节奏闪避|判定", meta=(ClampMin="1.0"))
	float GoodWindowMs = 120.0f;

	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> DodgeForwardAnimation;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> DodgeBackwardAnimation;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> DodgeLeftAnimation;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> DodgeRightAnimation;
	void BroadcastPerfectFeedback(float ErrorMs, bool bStrongBeat, FVector BladeWorldLocation, FVector AttackDirection);
};
