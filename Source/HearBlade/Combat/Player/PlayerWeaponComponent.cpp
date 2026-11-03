#include "Combat/Player/PlayerWeaponComponent.h"
#include "Combat/Player/PlayerDefenseComponent.h"
#include "Combat/Player/PlayerAttackComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

UPlayerWeaponComponent::UPlayerWeaponComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// 保存武器和战斗组件引用，重置握剑校准状态
void UPlayerWeaponComponent::Initialize(UStaticMeshComponent* Sword)
{
    Character = Cast<ACharacter>(GetOwner());
    EquippedSword = Sword;
    DefenseComponent = GetOwner()->FindComponentByClass<UPlayerDefenseComponent>();
    AttackComponent = GetOwner()->FindComponentByClass<UPlayerAttackComponent>();
    SwordCalibrationFrames = 0;
    bSwordGripCalibrated = false;
    SwordGripRotationOffset = FQuat::Identity;
}

// 攻击防御时跟随手部姿态，移动时平滑朝向角色前方
void UPlayerWeaponComponent::UpdateDirection(float DeltaSeconds, float SwordDirectionInterpSpeed)
{
	if (!Character || !bSwordGripCalibrated || !EquippedSword)
		return;

	const FQuat CurrentRotation = EquippedSword->GetComponentQuat();
	FQuat TargetRotation = CurrentRotation;
	float InterpSpeed = SwordDirectionInterpSpeed;
	if (((DefenseComponent && DefenseComponent->IsInDefensePose()) ||
	     (AttackComponent && AttackComponent->IsPlayingAttackMotion())) &&
	    Character->GetMesh())
	{
		// 攻击和防御时按右手动画更新握剑角度，保留校准得到的偏移
		const FQuat HandRotation = Character->GetMesh()->GetSocketTransform(TEXT("hand_r"), RTS_World).GetRotation();
		TargetRotation = (HandRotation * SwordGripRotationOffset).GetNormalized();
		InterpSpeed *= 1.65f;
		if (AttackComponent && AttackComponent->IsPlayingAttackMotion())
		{
			EquippedSword->SetWorldRotation(TargetRotation);
			return;
		}
	}
	else
	{
		// 移动和翻滚时保持剑指向角色前方，避免不同动画的手腕角度导致剑突然转向
		const FVector CurrentDirection = EquippedSword->GetComponentTransform()
		                                     .TransformVectorNoScale(FVector::YAxisVector)
		                                     .GetSafeNormal();
		const FVector DesiredDirection = Character->GetActorForwardVector().GetSafeNormal();
		if (CurrentDirection.IsNearlyZero() || DesiredDirection.IsNearlyZero())
			return;
		TargetRotation =
		    (FQuat::FindBetweenNormals(CurrentDirection, DesiredDirection) * CurrentRotation).GetNormalized();
	}
	const float Alpha = 1.0f - FMath::Exp(-InterpSpeed * DeltaSeconds);
	// 按帧间隔计算旋转比例，减少帧率变化对转向速度的影响
	EquippedSword->SetWorldRotation(FQuat::Slerp(CurrentRotation, TargetRotation, Alpha).GetNormalized());
}

// 等待骨骼更新后对齐刀刃，并保存剑相对右手的旋转
void UPlayerWeaponComponent::Calibrate(float SwordGripVerticalOffsetCm)
{
	// 等待前两次骨骼更新，再校准一次握持角度
	if (!Character || bSwordGripCalibrated || !EquippedSword || ++SwordCalibrationFrames < 3)
		return;

	const FVector CurrentBladeDirection =
	    EquippedSword->GetComponentTransform().TransformVectorNoScale(FVector::YAxisVector).GetSafeNormal();
	const FVector DesiredBladeDirection = Character->GetActorForwardVector().GetSafeNormal();
	if (CurrentBladeDirection.IsNearlyZero() || DesiredBladeDirection.IsNearlyZero())
		return;

	const FQuat WorldCorrection = FQuat::FindBetweenNormals(CurrentBladeDirection, DesiredBladeDirection);
	EquippedSword->SetWorldRotation(WorldCorrection * EquippedSword->GetComponentQuat());
	EquippedSword->SetWorldLocation(EquippedSword->GetComponentLocation() +
	                                FVector::UpVector * SwordGripVerticalOffsetCm);
	const FQuat HandRotation = Character->GetMesh()->GetSocketTransform(TEXT("hand_r"), RTS_World).GetRotation();
	// 保存剑相对右手的角度，后续换动画时仍保持这个握持关系
	SwordGripRotationOffset = (HandRotation.Inverse() * EquippedSword->GetComponentQuat()).GetNormalized();
	bSwordGripCalibrated = true;

	const FVector CorrectedDirection =
	    EquippedSword->GetComponentTransform().TransformVectorNoScale(FVector::YAxisVector).GetSafeNormal();
	UE_LOG(LogTemp, Display, TEXT("SwordGrip: forward alignment dot=%.4f"),
	       FVector::DotProduct(CorrectedDirection, DesiredBladeDirection));
}
