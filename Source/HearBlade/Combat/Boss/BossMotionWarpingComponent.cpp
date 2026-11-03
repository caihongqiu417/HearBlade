// 让敌人的手动动画推进使用引擎的根位移修正
#include "Combat/Boss/BossMotionWarpingComponent.h"

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
                                                       const UAnimMontage* Montage, float PreviousPosition,
                                                       float CurrentPosition, float PlayRate)
{
	// 告诉引擎这一帧播放了动画中的哪一段
	FMotionWarpingUpdateContext Context;
	Context.Animation = Montage;
	Context.PreviousPosition = PreviousPosition;
	Context.CurrentPosition = CurrentPosition;
	Context.Weight = 1.0f;
	Context.PlayRate = PlayRate;
	Context.DeltaSeconds = DeltaSeconds;
	// 修正这一帧的动画位移，实际移动由调用方执行
	return ProcessRootMotionPreConvertToWorld(LocalDelta, DeltaSeconds, &Context);
}
