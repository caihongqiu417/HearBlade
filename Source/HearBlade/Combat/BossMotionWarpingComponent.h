#pragma once

#include "CoreMinimal.h"
#include "MotionWarpingAdapter.h"
#include "MotionWarpingComponent.h"
#include "BossMotionWarpingComponent.generated.h"

class USkeletalMeshComponent;

// The existing Boss is an Actor rather than a Character, so the plugin needs an adapter.
UCLASS()
class HEARBLADE_API UBossMotionWarpingAdapter : public UMotionWarpingBaseAdapter
{
	GENERATED_BODY()

public:
	void SetBoss(AActor* InBoss, USkeletalMeshComponent* InMesh);
	virtual AActor* GetActor() const override;
	virtual USkeletalMeshComponent* GetMesh() const override;
	virtual FVector GetVisualRootLocation() const override;
	virtual FVector GetBaseVisualTranslationOffset() const override;
	virtual FQuat GetBaseVisualRotationOffset() const override;

private:
	TWeakObjectPtr<AActor> Boss;
	TWeakObjectPtr<USkeletalMeshComponent> Mesh;
};

UCLASS()
class HEARBLADE_API UBossMotionWarpingComponent : public UMotionWarpingComponent
{
	GENERATED_BODY()

public:
	FTransform WarpRootMotion(const FTransform& LocalDelta, float DeltaSeconds,
		const UAnimMontage* Montage, float PreviousPosition, float CurrentPosition, float PlayRate);
};
