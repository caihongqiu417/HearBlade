#include "Combat/Player/PlayerLocomotionComponent.h"
#include "Combat/Player/PlayerCombatAnimInstance.h"
#include "Combat/Player/PlayerRhythmCombatComponent.h"
#include "Combat/Player/PlayerBeatAttackComponent.h"
#include "Combat/Camera/BossCombatCameraComponent.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HearBlade.h"

UPlayerLocomotionComponent::UPlayerLocomotionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerLocomotionComponent::Initialize(UBlendSpace* Locomotion, UBlendSpace* Pursuit)
{
    Character = Cast<ACharacter>(GetOwner());
    KnightLocomotionBlendSpace = Locomotion;
    PursuitBlendSpace = Pursuit;
    CombatCamera = GetOwner()->FindComponentByClass<UBossCombatCameraComponent>();
    RhythmCombat = GetOwner()->FindComponentByClass<UPlayerRhythmCombatComponent>();
    BeatAttack = GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>();
    MeasureSpeeds();
}

void UPlayerLocomotionComponent::MeasureSpeeds()
{
	if (!KnightLocomotionBlendSpace)
		return;
	// 从追击混合空间的最外圈前进样本读取快跑速度
	if (PursuitBlendSpace)
	{
		float Best = 0.f;
		for (const FBlendSample& Sample : PursuitBlendSpace->GetBlendSamples())
		{
			const FVector2D P(Sample.SampleValue.X, Sample.SampleValue.Y);
			const UAnimSequence* Clip = Cast<UAnimSequence>(Sample.Animation);
			if (P.X <= Best || FMath::Abs(P.Y) > .1f || !Clip || Clip->GetPlayLength() <= .05f)
				continue;
			// 用根节点的水平位移除以时长，估算这段动画对应的移动速度
			const FTransform A = Clip->ExtractRootTrackTransform(FAnimExtractContext(0.0), nullptr);
			const FTransform B =
			    Clip->ExtractRootTrackTransform(FAnimExtractContext(double(Clip->GetPlayLength())), nullptr);
			const float Speed = FVector::Dist2D(A.GetLocation(), B.GetLocation()) / Clip->GetPlayLength();
			if (Speed < 20.f)
				continue;
			Best = P.X;
			PursuitBlendPosition = P;
			PursuitAnimSpeed = Speed;
		}
	}
	// 按采样点到中心的距离区分步行、跑步和快跑
	TArray<float> Rings;
	for (const FBlendSample& Sample : KnightLocomotionBlendSpace->GetBlendSamples())
	{
		const float R = FVector2D(Sample.SampleValue.X, Sample.SampleValue.Y).Size();
		if (R < .1f)
			continue;
		bool bKnown = false;
		for (float& Ring : Rings)
			if (FMath::Abs(Ring - R) < .12f * FMath::Max(1.f, R))
			{
				bKnown = true;
				break;
			}
		if (!bKnown)
			Rings.Add(R);
	}
	// 从内到外排列采样圈，最内圈是步行，其次是跑步和快跑
	Rings.Sort();
	if (Rings.Num() > 0)
		WalkRadius = Rings[0];
	RunRadius = Rings.Num() > 1 ? Rings[1] : WalkRadius * 2.f;
	FastRadius = Rings.Num() > 2 ? Rings[2] : RunRadius;
	// 分别累计三个采样圈的有效步速，最后取平均值
	float Sum[3] = {0.f, 0.f, 0.f};
	int32 Count[3] = {0, 0, 0};
	for (const FBlendSample& Sample : KnightLocomotionBlendSpace->GetBlendSamples())
	{
		const float Radius = FVector2D(Sample.SampleValue.X, Sample.SampleValue.Y).Size();
		const UAnimSequence* Clip = Cast<UAnimSequence>(Sample.Animation);
		if (Radius < .1f || !Clip || Clip->GetPlayLength() <= .05f)
			continue;
		const FTransform A = Clip->ExtractRootTrackTransform(FAnimExtractContext(0.0), nullptr);
		const FTransform B =
		    Clip->ExtractRootTrackTransform(FAnimExtractContext(double(Clip->GetPlayLength())), nullptr);
		const float Speed = FVector::Dist2D(A.GetLocation(), B.GetLocation()) / Clip->GetPlayLength();
		if (Speed < 20.f)
			continue; // 原地动画没有有效位移，保留默认速度
		// 以相邻圈的中点为分界，确定这段动画属于哪一种步态
		const int32 Ring = Radius < (WalkRadius + RunRadius) * .5f                               ? 0
		                   : (FastRadius > RunRadius && Radius > (RunRadius + FastRadius) * .5f) ? 2
		                                                                                         : 1;
		Sum[Ring] += Speed;
		++Count[Ring];
	}
	if (Count[0])
		WalkAnimSpeed = Sum[0] / Count[0];
	if (Count[1])
		RunAnimSpeed = Sum[1] / Count[1];
	if (Count[2])
		FastAnimSpeed = Sum[2] / Count[2];
	// 保证三档速度依次增大，后续换算采样半径时才有有效区间
	RunAnimSpeed = FMath::Max(RunAnimSpeed, WalkAnimSpeed + 50.f);
	FastAnimSpeed = FMath::Max(FastAnimSpeed, RunAnimSpeed + 50.f);
	UE_LOG(LogHearBlade, Display, TEXT("LocomotionSpeeds: walk=%.0f run=%.0f fast=%.0f (measured %d/%d/%d)"),
	       WalkAnimSpeed, RunAnimSpeed, FastAnimSpeed, Count[0], Count[1], Count[2]);
}

// 将实际移动速度换算为混合空间中的采样半径
float UPlayerLocomotionComponent::RadiusForSpeed(float Speed) const
{
	if (Speed <= WalkAnimSpeed)
		return WalkRadius * Speed / FMath::Max(WalkAnimSpeed, 1.f);
	if (Speed <= RunAnimSpeed)
		return WalkRadius +
		       (RunRadius - WalkRadius) * (Speed - WalkAnimSpeed) / (RunAnimSpeed - WalkAnimSpeed);
	if (FastRadius <= RunRadius)
		return RunRadius;
	return FMath::Min(FastRadius, RunRadius + (FastRadius - RunRadius) * (Speed - RunAnimSpeed) /
	                                              (FastAnimSpeed - RunAnimSpeed));
}

float UPlayerLocomotionComponent::SpeedForRadius(float Radius) const
{
	if (Radius <= WalkRadius)
		return WalkAnimSpeed * Radius / FMath::Max(WalkRadius, .01f);
	if (Radius <= RunRadius)
		return FMath::Lerp(WalkAnimSpeed, RunAnimSpeed,
		                   (Radius - WalkRadius) / FMath::Max(RunRadius - WalkRadius, .01f));
	if (FastRadius <= RunRadius)
		return RunAnimSpeed;
	return FMath::Lerp(RunAnimSpeed, FastAnimSpeed,
	                   (FMath::Min(Radius, FastRadius) - RunRadius) / (FastRadius - RunRadius));
}

void UPlayerLocomotionComponent::Update(float DeltaSeconds, float LockedCombatWalkSpeed, float FreeRunSpeed)
{
	if (!Character || !Character->GetMesh() || !KnightLocomotionBlendSpace)
		return;

	const bool bCombatLocked = CombatCamera && CombatCamera->IsLockedOn();
	const AActor* Target = CombatCamera ? CombatCamera->GetCombatTarget() : nullptr;
	const bool bBusy = (RhythmCombat && RhythmCombat->IsPerformingAction()) ||
	                   (BeatAttack && BeatAttack->IsPlayingAttackMotion());
	const FVector ToTarget = Target ? Target->GetActorLocation() - Character->GetActorLocation() : FVector::ZeroVector;
	// 进入追击和退出追击使用不同的距离、方向阈值，避免在边界反复切换
	const FVector2D Input2D(Character->GetLastMovementInputVector().X, Character->GetLastMovementInputVector().Y);
	// 输入方向与目标方向越接近，点积越接近 1，侧移或后退不进入追击
	const float Aim = Input2D.SizeSquared() > .04f
	                      ? FVector2D::DotProduct(Input2D.GetSafeNormal(),
	                                              FVector2D(ToTarget.X, ToTarget.Y).GetSafeNormal())
	                      : -1.f;
	bPursuitGait = bCombatLocked && Target && !bBusy && Aim > (bPursuitGait ? .35f : .6f) &&
	               ToTarget.Size2D() > (bPursuitGait ? 220.f : 320.f);
	// 追击、锁定步行、自由移动分别使用原来的速度规则
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		Movement->MaxWalkSpeed = bPursuitGait    ? FreeRunSpeed * 1.3f
		                         : bCombatLocked ? FMath::Max(280.f, LockedCombatWalkSpeed)
		                                         : FreeRunSpeed;

	// 用实际速度匹配动画的步幅，避免角色慢走时仍播放完整跑步循环
	const FVector LocalVelocity = Character->GetActorTransform().InverseTransformVectorNoScale(Character->GetVelocity());
	const float Speed2D = FVector2D(LocalVelocity.X, LocalVelocity.Y).Size();
	FVector2D TargetInput =
	    FVector2D(LocalVelocity.X, LocalVelocity.Y).GetSafeNormal() * RadiusForSpeed(Speed2D);
	if (Character->GetVelocity().SizeSquared2D() < 9.0f)
		TargetInput = FVector2D::ZeroVector;
	SmoothedLocomotionInput =
	    FMath::Vector2DInterpTo(SmoothedLocomotionInput, TargetInput, DeltaSeconds, 9.0f);

	if (auto* Instance = Cast<UPlayerCombatAnimInstance>(Character->GetMesh()->GetAnimInstance()))
	{
		if (bPursuitGait && PursuitBlendSpace)
		{
			// 追击使用原来的快跑动画，并按实际速度调整播放速率
			Instance->Locomotion = PursuitBlendSpace;
			Instance->LocomotionPosition = FVector(PursuitBlendPosition.X, PursuitBlendPosition.Y, 0.0f);
			Instance->SpeedMatchPlayRate =
			    Speed2D > 30.f ? FMath::Clamp(Speed2D / FMath::Max(PursuitAnimSpeed, 1.f), .75f, 1.3f) : 1.f;
			return;
		}
		Instance->Locomotion = KnightLocomotionBlendSpace;
		Instance->LocomotionPosition = FVector(SmoothedLocomotionInput.X, SmoothedLocomotionInput.Y, 0.0f);
		// 混合位置平滑变化时同步调整播放速率，减少脚步滑动
		const float PoseSpeed = SpeedForRadius(SmoothedLocomotionInput.Size());
		Instance->SpeedMatchPlayRate =
		    (Speed2D > 30.f && PoseSpeed > 30.f) ? FMath::Clamp(Speed2D / PoseSpeed, .75f, 1.3f) : 1.f;
	}
}
