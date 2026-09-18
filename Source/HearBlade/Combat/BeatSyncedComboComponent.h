#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BeatSyncedComboComponent.generated.h"

class UAttackDataAsset;
class USkeletalMeshComponent;
class UBossMotionWarpingComponent;
class UNiagaraSystem;
class UAnimInstance;
class UAnimSequence;
class UNiagaraComponent;
class UBeatClockSubsystem;

USTRUCT(BlueprintType)
struct FBeatBlinkSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="闪现", meta=(ClampMin="0.0"))
	float LandingDistance = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="闪现", meta=(ClampMin="0.0", ClampMax="90.0"))
	float SideAngleDegrees = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="闪现", meta=(ClampMin="0.0"))
	float WarningBeats = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="闪现")
	TObjectPtr<UNiagaraSystem> WarningEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="闪现")
	TObjectPtr<UNiagaraSystem> OriginEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="闪现")
	TObjectPtr<UNiagaraSystem> LandingEffect;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnComboImpact, int32, ImpactIndex, double, TargetBeat, float, BeatErrorSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnComboStarted, int32, FirstImpactBeat);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnComboFinished);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBossBlinkWarning, FVector, LandingLocation, double, TargetAudioSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBossBlinkSucceeded, FVector, LandingLocation, double, TargetAudioSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBossBlinkCancelled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBossConfirmedHit);

// 行为树触发攻击后，将第一击安排在下一可用的指定小节拍位。
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class HEARBLADE_API UBeatSyncedComboComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBeatSyncedComboComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="节拍连招")
	TObjectPtr<UAttackDataAsset> AttackData;

	// 行为树触发攻击后，留给动画起手的额外时间。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="节拍连招", meta=(ClampMin="0.0"))
	float SchedulingLeadSeconds = 0.1f;

	// 空标签：播放 Owner 上所有与 Montage 骨架相同的 SkeletalMeshComponent。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="节拍连招")
	FName MeshComponentTag;

	// Optional animation graph used for idle/walk and montage blending.
	UPROPERTY(EditAnywhere, Category="节拍连招|移动")
	TSoftClassPtr<UAnimInstance> LocomotionAnimClass;

	UPROPERTY(BlueprintAssignable, Category="节拍连招")
	FOnComboStarted OnComboStarted;

	UPROPERTY(BlueprintAssignable, Category="节拍连招")
	FOnComboImpact OnImpact;

	UPROPERTY(BlueprintAssignable, Category="节拍连招")
	FOnComboFinished OnComboFinished;
	UPROPERTY(BlueprintAssignable, Category="节拍连招|相机") FOnBossBlinkWarning OnBlinkWarning;
	UPROPERTY(BlueprintAssignable, Category="节拍连招|相机") FOnBossBlinkSucceeded OnBlinkSucceeded;
	UPROPERTY(BlueprintAssignable, Category="节拍连招|相机") FOnBossBlinkCancelled OnBlinkCancelled;
	UPROPERTY(BlueprintAssignable, Category="节拍连招|相机") FOnBossConfirmedHit OnConfirmedHit;
	// Call this from actual damage confirmation, not from an animation impact marker.
	UFUNCTION(BlueprintCallable, Category="节拍连招|相机") void NotifyConfirmedHit() { OnConfirmedHit.Broadcast(); }

	UFUNCTION(BlueprintCallable, Category="节拍连招")
	bool QueueCombo();

	// 行为树或蓝图在决定攻击时调用；使用传入的独立动作数据，并对齐下一可用拍。
	UFUNCTION(BlueprintCallable, Category="节拍连招")
	bool QueueAttack(UAttackDataAsset* InAttackData);

	// Must be called before QueueAttack. The first impact and the blink then share one clock timestamp.
	void ConfigureNextBlink(const FBeatBlinkSettings& Settings);
	double GetScheduledFirstImpactAudioSeconds() const { return ScheduledFirstImpactAudioSeconds; }

	UFUNCTION(BlueprintCallable, Category="节拍连招")
	void SetAttackTarget(AActor* InTarget);

	UFUNCTION(BlueprintCallable, Category="节拍连招")
	void CancelCombo();

	UFUNCTION(BlueprintPure, Category="节拍连招")
	bool IsComboQueuedOrPlaying() const { return bQueued || bPlaying; }

	UFUNCTION(BlueprintPure, Category="节拍连招")
	bool WasLastAttackCompleted() const { return bLastAttackCompleted; }

	float GetLastLocomotionSpeed() const { return LastLocomotionSpeed; }
	bool WasLastLocomotionBlocked() const { return bLastLocomotionBlocked; }
	float GetWalkClipLengthSeconds() const;

	// The existing fire effects breathe on the music beat, including while idle.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="节拍连招|节奏感", meta=(ClampMin="0.0", ClampMax="0.3"))
	float FireBeatPulseStrength = 0.10f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void BeginPlay() override;

private:
	float GetImpactTime(int32 Index) const;
	int32 GetImpactCount() const;
	bool StartPlayback(double AudioNowSeconds);
	void UpdateWarpTarget(float AnimationPosition);
	void ClearWarping();
	bool LockBlinkDestination();
	void ClearBlink();
	void UpdateBeatFirePulse(const UBeatClockSubsystem* Clock);

	bool bQueued = false;
	bool bPlaying = false;
	bool bLastAttackCompleted = false;
	int32 FirstImpactBeat = 0;
	int32 NextImpactIndex = 0;
	double ScheduledStartAudioSeconds = 0.0;
	double ScheduledFirstImpactAudioSeconds = 0.0;
	FBeatBlinkSettings BlinkSettings;
	bool bBlinkRequested = false;
	bool bBlinkWarningShown = false;
	bool bBlinkExecuted = false;
	FVector LockedBlinkLocation = FVector::ZeroVector;
	float LockedBlinkGroundZ = 0.0f;
	FRotator LockedBlinkRotation = FRotator::ZeroRotator;
	bool bNextBlinkOnRight = false;
	float LastRootMotionPosition = 0.0f;
	TWeakObjectPtr<USkeletalMeshComponent> RootMotionMesh;
	TWeakObjectPtr<USkeletalMeshComponent> LocomotionMesh;
	float LastLocomotionSpeed = 0.0f;
	bool bLastLocomotionBlocked = false;
	double LocomotionResumeWorldSeconds = 0.0;
	double LastLocomotionLogWorldSeconds = -1.0e9;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimSequence>> DirectionalWalkAnimations;
	float LocomotionAnimationTime = 0.0f;
	TWeakObjectPtr<AActor> AttackTarget;
	struct FFirePulseTarget
	{
		TWeakObjectPtr<UNiagaraComponent> Component;
		FVector BaseScale = FVector::OneVector;
	};
	TArray<FFirePulseTarget> FirePulseTargets;
	UPROPERTY(Transient)
	TObjectPtr<UBossMotionWarpingComponent> WarpComponent;
	int32 ActiveWarpWindowIndex = INDEX_NONE;
	FVector WarpAnchorLocation = FVector::ZeroVector;
};
