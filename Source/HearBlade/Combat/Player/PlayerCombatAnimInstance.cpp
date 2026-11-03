// 管理移动动画与全身动作混合，输入判定由战斗组件维护
#include "Combat/Player/PlayerCombatAnimInstance.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "Combat/Player/PlayerRhythmCombatComponent.h"
#include "Combat/Player/PlayerBeatAttackComponent.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "Animation/AnimNodeSpaceConversions.h"
#include "BoneControllers/AnimNode_ModifyBone.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

struct FPlayerCombatAnimProxy : FAnimInstanceProxy
{
	FAnimNode_BlendSpacePlayer_Standalone LocomotionNode;
	FAnimNode_Slot ActionSlot;
	FAnimNode_BlendSpacePlayer_Standalone HitNode;
	FAnimNode_TwoWayBlend RootBlend;
	FAnimNode_ConvertLocalToComponentSpace LocalToComponentNode;
	FAnimNode_ModifyBone PelvisBeatNode;
	FAnimNode_ModifyBone SpineBeatNode;
	FAnimNode_ConvertComponentToLocalSpace ComponentToLocalNode;
	int32 LastHitReactionSerial = INDEX_NONE;

	explicit FPlayerCombatAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance)
	{
		ActionSlot.SlotName = TEXT("DefaultSlot");
		ActionSlot.bAlwaysUpdateSourcePose = true;
		ActionSlot.Source.SetLinkNode(&LocomotionNode);
		LocomotionNode.SetLoop(true);
		HitNode.SetLoop(false);
		RootBlend.A.SetLinkNode(&ActionSlot);
		RootBlend.B.SetLinkNode(&HitNode);
		RootBlend.AlphaInputType = EAnimAlphaInputType::Float;
		RootBlend.Alpha = 0.0f;

		// 动作先与受击姿态混合，再经过身体附加姿态节点
		LocalToComponentNode.LocalPose.SetLinkNode(&RootBlend);
		PelvisBeatNode.ComponentPose.SetLinkNode(&LocalToComponentNode);
		// 保留脊柱节点，避免通过骨盆偏移抬起双脚
		PelvisBeatNode.BoneToModify.BoneName = TEXT("spine_01");
		// 当前忽略脊柱平移，保持原动画的身体比例
		PelvisBeatNode.TranslationMode = BMM_Ignore;
		// 平移值使用角色组件坐标
		PelvisBeatNode.TranslationSpace = BCS_ComponentSpace;
		SpineBeatNode.ComponentPose.SetLinkNode(&PelvisBeatNode);
		SpineBeatNode.BoneToModify.BoneName = TEXT("spine_03");
		SpineBeatNode.RotationMode = BMM_Additive;
		SpineBeatNode.RotationSpace = BCS_BoneSpace;
		ComponentToLocalNode.ComponentPose.SetLinkNode(&SpineBeatNode);
	}

	virtual FAnimNode_Base* GetCustomRootNode() override
	{
		return &ComponentToLocalNode;
	}

	virtual void Initialize(UAnimInstance* Instance) override
	{
		const auto* Combat = CastChecked<UPlayerCombatAnimInstance>(Instance);
		LocomotionNode.SetBlendSpace(Combat->Locomotion);
		HitNode.SetBlendSpace(Combat->DirectionalHit);
		FAnimInstanceProxy::Initialize(Instance);
	}

	virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
	{
		FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
		const auto* Combat = CastChecked<UPlayerCombatAnimInstance>(Instance);
		LocomotionNode.SetBlendSpace(Combat->Locomotion);
		LocomotionNode.SetPosition(Combat->LocomotionPosition);
		LocomotionNode.SetPlayRate(Combat->LocomotionPlayRate);
		HitNode.SetBlendSpace(Combat->DirectionalHit);
		HitNode.SetPosition(Combat->HitBlendPosition);
		if (LastHitReactionSerial != Combat->HitReactionSerial)
		{
			HitNode.SetAccumulatedTime(0.0f);
			LastHitReactionSerial = Combat->HitReactionSerial;
		}
		RootBlend.Alpha = Combat->HitBlendAlpha;
		PelvisBeatNode.Translation = Combat->BeatPelvisTranslation;
		PelvisBeatNode.Alpha = Combat->BeatPoseAlpha;
		SpineBeatNode.Rotation = Combat->BeatSpineRotation;
		SpineBeatNode.Alpha = Combat->BeatPoseAlpha;
	}
};

UPlayerCombatAnimInstance::UPlayerCombatAnimInstance()
{
	Locomotion =
	    LoadObject<UBlendSpace>(nullptr, TEXT("/Game/Combat/Player/BS_PlayerEightWay.BS_PlayerEightWay"));
	DirectionalHit = LoadObject<UBlendSpace>(
	    nullptr,
	    TEXT(
	        "/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/BS_DKM_DirectionalHit.BS_DKM_DirectionalHit"));
	// 攻击位移由战斗组件提取并交给移动组件执行，这里不再重复应用根运动
	SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
}

// 方向受击动画

// 记录受击方向和时间，启动方向姿态混合
void UPlayerCombatAnimInstance::StartDirectionalHit(FVector2D ReactionDirection, float Duration)
{
	if (!GetWorld())
		return;
	if (!ReactionDirection.Normalize())
		ReactionDirection = FVector2D(1.0f, 0.0f);
	HitBlendPosition = FVector(ReactionDirection.X, ReactionDirection.Y, 0.0f);
	bDirectionalHitActive = true;
	HitReactionEndWorldSeconds = GetWorld()->GetTimeSeconds() + FMath::Max(.1f, Duration);
	++HitReactionSerial;
	if (VisualAction)
		Montage_Stop(.06f, VisualAction);
	EndBeatAttack(BeatAttackMontage);
}

// 结束受击姿态，回到当前基础动作
void UPlayerCombatAnimInstance::EndDirectionalHit()
{
	bDirectionalHitActive = false;
	HitReactionEndWorldSeconds = -1.0;
}

// 攻击蒙太奇控制

// 创建并播放攻击蒙太奇，让攻击组件接管动画位置
UAnimMontage* UPlayerCombatAnimInstance::BeginBeatAttack(UAnimSequence* Sequence, float StartPosition,
                                                         float BlendInSeconds)
{
	if (!Sequence)
		return nullptr;
	EndDirectionalHit();
	HitBlendAlpha = 0.f;
	ReleaseAtWorldSeconds = -1.;
	VisualAction = nullptr;
	// 切换动作时让上一招继续收招，避免从冻结姿态开始混合
	if (BeatAttackMontage)
		Montage_SetPlayRate(BeatAttackMontage, 1.f);
	BeatAttackMontage = PlaySlotAnimationAsDynamicMontage(Sequence, TEXT("DefaultSlot"), BlendInSeconds, .14f,
	                                                      1.f, 1, -1.f, StartPosition);
	if (BeatAttackMontage)
	{
		BeatAttackMontage->bEnableAutoBlendOut = false;
		// 关闭自动推进，由战斗组件按音乐时间设置动画位置
		// 播放速率设为零后，由 TickAttack 每帧设置位置，不代表攻击永久停住
		Montage_SetPlayRate(BeatAttackMontage, 0.f);
	}
	return BeatAttackMontage;
}

// 淡出指定攻击蒙太奇，释放攻击姿态
void UPlayerCombatAnimInstance::EndBeatAttack(UAnimMontage* Montage, float BlendOutSeconds)
{
	if (Montage)
		Montage_Stop(BlendOutSeconds, Montage);
	if (BeatAttackMontage == Montage)
		BeatAttackMontage = nullptr;
}

// 提前释放防御动作，平滑回到行走
void UPlayerCombatAnimInstance::ReleaseCombatAction(float BlendOutSeconds)
{
	if (VisualAction)
		Montage_Stop(BlendOutSeconds, VisualAction);
	VisualAction = nullptr;
	ReleaseAtWorldSeconds = -1.0;
}

// 原生动画图

// 创建行走、战斗动作槽与受击混合的原生动画图
FAnimInstanceProxy* UPlayerCombatAnimInstance::CreateAnimInstanceProxy()
{
	return new FPlayerCombatAnimProxy(this);
}

// 释放原生动画图代理
void UPlayerCombatAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy)
{
	delete Proxy;
}

// 防御动画

// 播放闪避或防御动作，设置循环和衔接时间
void UPlayerCombatAnimInstance::PlayCombatAction(UAnimSequence* Sequence, bool bLooping, float Duration,
                                                 float ChainGrace)
{
	if (!Sequence || !GetWorld())
		return;
	// 防御开始时释放攻击动画，避免两个动作争用同一个全身插槽
	EndBeatAttack(BeatAttackMontage);
	EndDirectionalHit();
	HitBlendAlpha = 0.f;
	const float ActualDuration = Duration > 0.f ? Duration : Sequence->GetPlayLength();
	const float Rate = Sequence->GetPlayLength() / FMath::Max(.05f, ActualDuration);
	UAnimMontage* Next = PlaySlotAnimationAsDynamicMontage(Sequence, TEXT("DefaultSlot"), .07f, .18f, Rate,
	                                                       bLooping ? 100000 : 1);
	if (!Next)
		return;
	// 保留收招用于衔接下一动作，到期后再退回移动动画
	Next->bEnableAutoBlendOut = false;
	VisualAction = Next;
	UE_LOG(LogTemp, Display, TEXT("DefenseAnimation: PLAY clip=%s montage=%s rate=%.2f"),
	       *Sequence->GetName(), *Next->GetName(), Rate);
	ReleaseAtWorldSeconds = bLooping ? -1.0 : GetWorld()->GetTimeSeconds() + ActualDuration + ChainGrace;
}

// 每帧动画状态

// 更新本帧移动参数和战斗姿态混合状态
void UPlayerCombatAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// 战斗动作期间关闭附加姿态，避免干扰闪避、格挡和受击
	float TargetPoseAlpha = 0.0f;
	float TargetPlayRate = 1.0f;
	float Pulse = 0.0f;
	bool bMoving = false;
	if (const APawn* Pawn = TryGetPawnOwner())
	{
		bMoving = Pawn->GetVelocity().SizeSquared2D() > 225.0f;
		const UPlayerRhythmCombatComponent* Combat =
		    Pawn->FindComponentByClass<UPlayerRhythmCombatComponent>();
		const auto* Attacks = Pawn->FindComponentByClass<UPlayerBeatAttackComponent>();
		if (Attacks && Attacks->IsAttacking())
		{
			SmoothedBeatPoseAlpha = 0.f;
			BeatPoseAlpha = 0.f;
			BeatPelvisTranslation = FVector::ZeroVector;
			BeatSpineRotation = FRotator::ZeroRotator;
		}
		const bool bActionBusy = (Combat && Combat->IsPerformingAction()) ||
		                         (Attacks && Attacks->IsAttacking()) ||
		                         (VisualAction && Montage_IsPlaying(VisualAction));
		if (!bActionBusy && !bDirectionalHitActive)
		{
			if (const UBeatClockSubsystem* Clock =
			        GetWorld() ? GetWorld()->GetSubsystem<UBeatClockSubsystem>() : nullptr;
			    Clock && Clock->IsRunning())
			{
				const double Beat = Clock->GetCurrentBeat();
				const float Phase = static_cast<float>(Beat - FMath::FloorToDouble(Beat));
				// 按一拍内的位置计算起伏，当前平移节点已关闭，这些值不改变身体位置
				auto SmoothSegment =
				    [](float T, float FromPhase, float ToPhase, float FromValue, float ToValue)
				{
					const float Local = FMath::Clamp(
					    (T - FromPhase) / FMath::Max(ToPhase - FromPhase, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
					// 让相邻片段的变化平滑衔接
					const float Smooth = Local * Local * Local * (Local * (Local * 6.0f - 15.0f) + 10.0f);
					return FMath::Lerp(FromValue, ToValue, Smooth);
				};
				if (Phase < 0.16f)
					Pulse = SmoothSegment(Phase, 0.0f, 0.16f, -0.52f, 1.0f);
				else if (Phase < 0.44f)
					Pulse = SmoothSegment(Phase, 0.16f, 0.44f, 1.0f, 0.0f);
				else if (Phase < 0.72f)
					Pulse = SmoothSegment(Phase, 0.44f, 0.72f, 0.0f, 0.10f);
				else
					Pulse = SmoothSegment(Phase, 0.72f, 1.0f, 0.10f, -0.52f);
				TargetPoseAlpha = 1.0f;
				// 移动动画速度不随每拍起伏，避免脚步忽快忽慢
				TargetPlayRate = 1.0f;
			}
		}
	}
	SmoothedBeatPoseAlpha = FMath::FInterpTo(SmoothedBeatPoseAlpha, TargetPoseAlpha, DeltaSeconds,
	                                         TargetPoseAlpha > SmoothedBeatPoseAlpha ? 16.0f : 24.0f);
	SmoothedLocomotionPlayRate =
	    FMath::FInterpTo(SmoothedLocomotionPlayRate, TargetPlayRate, DeltaSeconds, 12.0f);
	BeatPoseAlpha = SmoothedBeatPoseAlpha;
	LocomotionPlayRate = SmoothedLocomotionPlayRate * SpeedMatchPlayRate;
	const float BodyAmount = bMoving ? 0.65f : 1.0f;
	BeatPelvisTranslation = FVector(0.0f, 0.0f, Pulse * 2.60f * BodyAmount);
	// 不附加脊柱旋转，保留原动画朝向
	BeatSpineRotation = FRotator::ZeroRotator;

	if (bDirectionalHitActive && GetWorld() && GetWorld()->GetTimeSeconds() >= HitReactionEndWorldSeconds)
		EndDirectionalHit();
	HitBlendAlpha = FMath::FInterpTo(HitBlendAlpha, bDirectionalHitActive ? 1.0f : 0.0f, DeltaSeconds,
	                                 bDirectionalHitActive ? 55.0f : 8.0f);
	if (ReleaseAtWorldSeconds >= 0.0 && GetWorld() && GetWorld()->GetTimeSeconds() >= ReleaseAtWorldSeconds)
	{
		if (VisualAction)
			Montage_Stop(.18f, VisualAction);
		ReleaseAtWorldSeconds = -1.0;
		VisualAction = nullptr;
	}
}
