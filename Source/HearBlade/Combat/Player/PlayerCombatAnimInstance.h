// 管理移动动画与全身动作混合，输入判定由战斗组件维护
#pragma once

#include "Animation/AnimInstance.h"
#include "PlayerCombatAnimInstance.generated.h"

// Persistent locomotion + one full-body slot. Input timing remains owned by
// PlayerRhythmCombatComponent; montage blending is presentation only.
UCLASS(Transient)

class HEARBLADE_API UPlayerCombatAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
  public:
	UAnimMontage* GetDefenseMontage() const
	{
		return VisualAction;
	}

	UPlayerCombatAnimInstance();
	UPROPERTY(Transient) TObjectPtr<class UBlendSpace> Locomotion;
	UPROPERTY(Transient) TObjectPtr<class UBlendSpace> DirectionalHit;
	FVector LocomotionPosition = FVector::ZeroVector;
	// Runtime values consumed by the native animation graph. The beat pose is
	// deliberately presentation-only: it never moves the capsule or changes
	// CharacterMovement speed.
	float LocomotionPlayRate = 1.0f;
	// Set by the character each frame: actual speed / authored speed of the pose.
	float SpeedMatchPlayRate = 1.0f;
	float BeatPoseAlpha = 0.0f;
	FVector BeatPelvisTranslation = FVector::ZeroVector;
	FRotator BeatSpineRotation = FRotator::ZeroRotator;
	void PlayCombatAction(class UAnimSequence* Sequence, bool bLooping, float Duration,
	                      float ChainGrace = .10f);
	class UAnimMontage* BeginBeatAttack(class UAnimSequence* Sequence, float StartPosition,
	                                    float BlendInSeconds = .10f);
	void EndBeatAttack(class UAnimMontage* Montage, float BlendOutSeconds = .08f);
	// Hand a defense/dodge clip back to locomotion early (e.g. dodge cancelled into a run).
	void ReleaseCombatAction(float BlendOutSeconds);
	void StartDirectionalHit(FVector2D ReactionDirection, float Duration);
	void EndDirectionalHit();

	bool IsDirectionalHitActive() const
	{
		return bDirectionalHitActive;
	}

	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

  protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

  private:
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> VisualAction;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> BeatAttackMontage;
	double ReleaseAtWorldSeconds = -1.0;
	FVector HitBlendPosition = FVector(1.0, 0.0, 0.0);
	float HitBlendAlpha = 0.0f;
	int32 HitReactionSerial = 0;
	bool bDirectionalHitActive = false;
	double HitReactionEndWorldSeconds = -1.0;
	float SmoothedBeatPoseAlpha = 0.0f;
	float SmoothedLocomotionPlayRate = 1.0f;
	friend struct FPlayerCombatAnimProxy;
};
