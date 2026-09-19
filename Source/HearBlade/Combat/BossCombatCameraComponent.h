#pragma once

#include "Components/ActorComponent.h"
#include "BossCombatCameraComponent.generated.h"

class UBossCameraConfig;
class UBossCameraModifier;
class UBeatSyncedComboComponent;
class ABossBeatDirector;

UCLASS(ClassGroup=(Camera), meta=(BlueprintSpawnableComponent))
class HEARBLADE_API UBossCombatCameraComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UBossCombatCameraComponent();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat Camera") TObjectPtr<UBossCameraConfig> Config;
	UFUNCTION(BlueprintCallable, Category="Combat Camera") void SetCombatTarget(AActor* NewTarget);
	UFUNCTION(BlueprintCallable, Category="Combat Camera") void ClearCombatTarget();
	UFUNCTION(BlueprintCallable, Category="Combat Camera") void ToggleLockOn();
	UFUNCTION(BlueprintPure, Category="Combat Camera") bool IsLockedOn() const { return Target.IsValid(); }
	UFUNCTION(BlueprintPure, Category="Combat Camera") AActor* GetCombatTarget() const { return Target.Get(); }
	void AddLookInput(float Yaw, float Pitch);
	void TriggerPerfectDodgeFeedback(float FOVExpansionDegrees, float ShakeDegrees, float Strength,
		FVector BladeWorldLocation, FVector AttackDirection, bool bStrongBeat,
		float LateralShiftCm, float DollyShiftCm, float YawKickDegrees, float PitchKickDegrees,
		float ReboundStrength);
	void StartHoldFeedback(float ShakeDegrees);
	void TriggerHoldImpact(float ShakeDegrees, float DurationSeconds);
	void StopHoldFeedback(float FadeSeconds);
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
	UFUNCTION() void HandleWarning(FVector Landing, double TargetAudioSeconds);
	UFUNCTION() void HandleBlink(FVector Landing, double TargetAudioSeconds);
	UFUNCTION() void HandleCancelled();
	UFUNCTION() void HandleConfirmedHit();
	void BindCombo(UBeatSyncedComboComponent* Combo);
	void CancelFeedback();
	const UBossCameraConfig* Settings() const;
	TWeakObjectPtr<AActor> Target;
	TWeakObjectPtr<ABossBeatDirector> Director;
	TWeakObjectPtr<UBeatSyncedComboComponent> BoundCombo;
	TWeakObjectPtr<UBossCameraModifier> Modifier;
	FVector WarningLanding = FVector::ZeroVector;
	bool bWarning = false;
	bool bManualSuppression = false;
	float OutOfRangeTime = 0.f;
	float OccludedTime = 0.f;
	float ManualYaw = 0.f;
	float ManualPitch = 0.f;
	float LastLookInputTime = -100.f;
};
