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
	UPlayerCombatAnimInstance();
	UPROPERTY(Transient) TObjectPtr<class UBlendSpace> Locomotion;
	UPROPERTY(Transient) TObjectPtr<class UBlendSpace> DirectionalHit;
	FVector LocomotionPosition = FVector::ZeroVector;
	void PlayCombatAction(class UAnimSequence* Sequence, bool bLooping, float Duration, float ChainGrace = .10f);
	void StartDirectionalHit(FVector2D ReactionDirection, float Duration);
	void EndDirectionalHit();
	bool IsDirectionalHitActive() const { return bDirectionalHitActive; }
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
private:
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> VisualAction;
	double ReleaseAtWorldSeconds = -1.0;
	FVector HitBlendPosition = FVector(-1.0, 0.0, 0.0);
	float HitBlendAlpha = 0.0f;
	int32 HitReactionSerial = 0;
	bool bDirectionalHitActive = false;
	double HitReactionEndWorldSeconds = -1.0;
	friend struct FPlayerCombatAnimProxy;
};
