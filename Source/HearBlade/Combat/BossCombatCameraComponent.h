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
	void SetPhasePresentation(bool bEnabled) { bPhasePresentation=bEnabled; CancelFeedback(); }
	bool bPhasePresentation=false;
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
	// Kick finisher: punch-in, a Dutch-angle roll toward the kick side and a pitch
	// dip, held through the hit-stop and released with a small overshoot.
	void TriggerKickAccent(float FOVKickDegrees, float RollDegrees, float PitchDipDegrees,
		float ShakeDegrees, FVector ContactWorld, float HoldSeconds);
	// Short upward camera response for the player's third heavy strike.
	void TriggerUppercutAccent(float Strength);
	// Launcher: a smooth side close-up of player + launched boss (blend in, slow
	// dolly-in with the rising body, blend back to gameplay framing).
	void TriggerLaunchShot(AActor* Subject, float Seconds);
	void TriggerFailureFeedback(bool bMiss);
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
	UFUNCTION() void HandleCinematicStarted(float Strength, bool bStrongBeat);
	UFUNCTION() void HandleCinematicImpact(float Strength, bool bStrongBeat, FVector ClashLocation);
	UFUNCTION() void HandleCinematicEnded();
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
	void TickLaunchShot(float DeltaTime);
	TWeakObjectPtr<AActor> ShotSubject;
	double ShotStartWorld = -1000.0;
	float ShotSeconds = 0.f, ShotSide = 1.f;
	bool bShotInit = false;
	FVector ShotSmoothedLoc = FVector::ZeroVector;
	FRotator ShotSmoothedRot = FRotator::ZeroRotator;
	FVector CinematicFocusLocation = FVector::ZeroVector;
	double CinematicFocusEndWorldSeconds = -1000.0;
};
