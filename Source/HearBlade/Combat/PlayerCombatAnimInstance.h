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
	FVector LocomotionPosition = FVector::ZeroVector;
	void PlayCombatAction(class UAnimSequence* Sequence, bool bLooping, float Duration, float ChainGrace = .10f);
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
private:
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> VisualAction;
	double ReleaseAtWorldSeconds = -1.0;
};
