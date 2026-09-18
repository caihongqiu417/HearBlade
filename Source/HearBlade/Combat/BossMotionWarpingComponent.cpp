#include "Combat/BossMotionWarpingComponent.h"

#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

void UBossMotionWarpingAdapter::SetBoss(AActor* InBoss, USkeletalMeshComponent* InMesh)
{
	Boss = InBoss;
	Mesh = InMesh;
}

AActor* UBossMotionWarpingAdapter::GetActor() const
{
	return Boss.Get();
}

USkeletalMeshComponent* UBossMotionWarpingAdapter::GetMesh() const
{
	return Mesh.Get();
}

FVector UBossMotionWarpingAdapter::GetVisualRootLocation() const
{
	return IsValid(Boss.Get()) ? Boss->GetActorLocation() : FVector::ZeroVector;
}

FVector UBossMotionWarpingAdapter::GetBaseVisualTranslationOffset() const
{
	return IsValid(Boss.Get()) && IsValid(Mesh.Get())
		? Mesh->GetComponentTransform().GetRelativeTransform(Boss->GetActorTransform()).GetLocation()
		: FVector::ZeroVector;
}

FQuat UBossMotionWarpingAdapter::GetBaseVisualRotationOffset() const
{
	return IsValid(Boss.Get()) && IsValid(Mesh.Get())
		? Mesh->GetComponentTransform().GetRelativeTransform(Boss->GetActorTransform()).GetRotation()
		: FQuat::Identity;
}

FTransform UBossMotionWarpingComponent::WarpRootMotion(const FTransform& LocalDelta, float DeltaSeconds,
	const UAnimMontage* Montage, float PreviousPosition, float CurrentPosition, float PlayRate)
{
	FMotionWarpingUpdateContext Context;
	Context.Animation = Montage;
	Context.PreviousPosition = PreviousPosition;
	Context.CurrentPosition = CurrentPosition;
	Context.Weight = 1.0f;
	Context.PlayRate = PlayRate;
	Context.DeltaSeconds = DeltaSeconds;
	return ProcessRootMotionPreConvertToWorld(LocalDelta, DeltaSeconds, &Context);
}
