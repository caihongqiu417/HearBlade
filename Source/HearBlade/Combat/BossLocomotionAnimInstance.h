#pragma once

#include "Animation/AnimInstance.h"
#include "BossLocomotionAnimInstance.generated.h"

// The Anim Blueprint reads GroundSpeed to blend the Dragon Knight's idle/walk poses.
UCLASS(Transient, Blueprintable)
class HEARBLADE_API UBossLocomotionAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	void PlayArmorFlinch(FVector2D Direction,float Strength);
	FRotator ArmorFlinchRotation=FRotator::ZeroRotator;
	double ArmorFlinchStart=-1000.;
	FVector2D ArmorFlinchDirection=FVector2D::ZeroVector;
	float ArmorFlinchStrength=0.f;
	UBossLocomotionAnimInstance();

	UPROPERTY(Transient)
	TObjectPtr<class UBlendSpace> Locomotion;

	UPROPERTY(Transient)
	TObjectPtr<class UBlendSpace> DirectionalHit;

	UPROPERTY(BlueprintReadOnly, Category="Boss Locomotion")
	float GroundSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Boss Locomotion")
	bool bIsMoving = false;

	// Continuous local-space input for the eight-way walk Blend Space.
	UPROPERTY(BlueprintReadOnly, Category="Boss Locomotion")
	float MoveForward = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Boss Locomotion")
	float MoveRight = 0.0f;

	// 0 forward, then clockwise in 45 degree steps through right, back and left.
	UPROPERTY(BlueprintReadOnly, Category="Boss Locomotion")
	int32 MoveDirectionIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category="Boss Locomotion")
	bool bDirectionBit0 = false;

	UPROPERTY(BlueprintReadOnly, Category="Boss Locomotion")
	bool bDirectionBit1 = false;

	UPROPERTY(BlueprintReadOnly, Category="Boss Locomotion")
	bool bDirectionBit2 = false;

	void SetLocomotion(float NewSpeed, const FVector& LocalDirection, float DeltaSeconds)
	{
		GroundSpeed = FMath::Max(0.0f, NewSpeed);
		bIsMoving = GroundSpeed > 5.0f;
		const FVector Desired = bIsMoving ? LocalDirection.GetSafeNormal2D() : FVector::ZeroVector;
		const FVector Current(MoveForward, MoveRight, 0.0f);
		const FVector Smoothed = bIsMoving
			? FMath::VInterpTo(Current, Desired, DeltaSeconds, 8.0f) : FVector::ZeroVector;
		MoveForward = Smoothed.X;
		MoveRight = Smoothed.Y;
		const float Angle = FMath::RadiansToDegrees(FMath::Atan2(MoveRight, MoveForward));
		MoveDirectionIndex = (FMath::RoundToInt(Angle / 45.0f) + 8) % 8;
		bDirectionBit0 = (MoveDirectionIndex & 1) != 0;
		bDirectionBit1 = (MoveDirectionIndex & 2) != 0;
		bDirectionBit2 = (MoveDirectionIndex & 4) != 0;
	}

	void StartDirectionalHit(const FVector2D& ReactionDirection, float Duration);
	void EndDirectionalHit();
	bool IsDirectionalHitActive() const { return bDirectionalHitActive; }
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	FVector HitBlendPosition = FVector(-1.0, 0.0, 0.0);
	float HitBlendAlpha = 0.0f;
	int32 HitReactionSerial = 0;
	bool bDirectionalHitActive = false;
	double HitReactionEndWorldSeconds = -1.0;
	friend struct FBossCombatAnimProxy;
};
