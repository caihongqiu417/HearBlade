// 更新敌人移动动画和方向性受击表现
#include "Combat/Boss/BossLocomotionAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/BlendSpace.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "Engine/World.h"
#include "Animation/AnimNodeSpaceConversions.h"
#include "BoneControllers/AnimNode_ModifyBone.h"

struct FBossCombatAnimProxy : FAnimInstanceProxy
{
	FAnimNode_BlendSpacePlayer_Standalone LocomotionNode;
	FAnimNode_Slot AttackSlot;
	FAnimNode_BlendSpacePlayer_Standalone HitNode;
	FAnimNode_TwoWayBlend RootBlend;
	FAnimNode_ConvertLocalToComponentSpace ToComponent;
	FAnimNode_ModifyBone Flinch;
	FAnimNode_ConvertComponentToLocalSpace ToLocal;
	int32 LastHitReactionSerial = INDEX_NONE;

	explicit FBossCombatAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance)
	{
		LocomotionNode.SetLoop(true);
		AttackSlot.SlotName = TEXT("DefaultSlot");
		AttackSlot.bAlwaysUpdateSourcePose = true;
		AttackSlot.Source.SetLinkNode(&LocomotionNode);
		HitNode.SetLoop(false);
		// 先在移动和攻击上叠加方向受击，再单独调整脊柱的轻微受击反应
		RootBlend.A.SetLinkNode(&AttackSlot);
		RootBlend.B.SetLinkNode(&HitNode);
		RootBlend.AlphaInputType = EAnimAlphaInputType::Float;
		ToComponent.LocalPose.SetLinkNode(&RootBlend);
		Flinch.ComponentPose.SetLinkNode(&ToComponent);
		Flinch.BoneToModify.BoneName = TEXT("spine_03");
		Flinch.RotationMode = BMM_Additive;
		Flinch.RotationSpace = BCS_ComponentSpace;
		Flinch.Alpha = 1.f;
		ToLocal.ComponentPose.SetLinkNode(&Flinch);
	}

	virtual FAnimNode_Base* GetCustomRootNode() override
	{
		return &ToLocal;
	}

	virtual void Initialize(UAnimInstance* Instance) override
	{
		const UBossLocomotionAnimInstance* Combat = CastChecked<UBossLocomotionAnimInstance>(Instance);
		LocomotionNode.SetBlendSpace(Combat->Locomotion);
		HitNode.SetBlendSpace(Combat->DirectionalHit);
		FAnimInstanceProxy::Initialize(Instance);
	}

	virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
	{
		FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
		const UBossLocomotionAnimInstance* Combat = CastChecked<UBossLocomotionAnimInstance>(Instance);
		LocomotionNode.SetBlendSpace(Combat->Locomotion);
		LocomotionNode.SetPosition(FVector(Combat->MoveForward, Combat->MoveRight, 0.0f));
		HitNode.SetBlendSpace(Combat->DirectionalHit);
		HitNode.SetPosition(Combat->HitBlendPosition);
		// 新受击事件才重置播放时间，持续更新同一次受击时不反复从头播放
		if (LastHitReactionSerial != Combat->HitReactionSerial)
		{
			HitNode.SetAccumulatedTime(0.0f);
			LastHitReactionSerial = Combat->HitReactionSerial;
		}
		RootBlend.Alpha = Combat->HitBlendAlpha;
		Flinch.Rotation = Combat->ArmorFlinchRotation;
	}
};

UBossLocomotionAnimInstance::UBossLocomotionAnimInstance()
{
	Locomotion = LoadObject<UBlendSpace>(
	    nullptr,
	    TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/BS_DragonKnight_Walk8.BS_DragonKnight_Walk8"));
	DirectionalHit = LoadObject<UBlendSpace>(
	    nullptr,
	    TEXT(
	        "/Game/Enemies/DF_DRAGON_KNIGHT/Anim/BS_DragonKnight_DirectionalHit.BS_DragonKnight_DirectionalHit"));
	SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
}

// 创建原生动画图，连接行走、攻击与受击姿态
FAnimInstanceProxy* UBossLocomotionAnimInstance::CreateAnimInstanceProxy()
{
	return new FBossCombatAnimProxy(this);
}

// 释放原生动画图代理
void UBossLocomotionAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy)
{
	delete Proxy;
}

// 保存受击方向与时长，开始混合受击姿态
void UBossLocomotionAnimInstance::StartDirectionalHit(const FVector2D& InDirection, float Duration)
{
	if (!GetWorld() || !DirectionalHit)
		return;
	FVector2D Direction = InDirection;
	if (!Direction.Normalize())
		Direction = FVector2D(-1.0f, 0.0f);
	HitBlendPosition = FVector(Direction.X, Direction.Y, 0.0f);
	bDirectionalHitActive = true;
	HitReactionEndWorldSeconds = GetWorld()->GetTimeSeconds() + FMath::Max(0.12f, Duration);
	++HitReactionSerial;
	Montage_Stop(0.045f);
}

// 结束方向受击，让姿态回到基础动作
void UBossLocomotionAnimInstance::EndDirectionalHit()
{
	bDirectionalHitActive = false;
	HitReactionEndWorldSeconds = -1.0;
}

// 更新行走与受击混合使用的参数
void UBossLocomotionAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const float Age = GetWorld() ? float(GetWorld()->GetTimeSeconds() - ArmorFlinchStart) : 1.f;
	// 短时间快速抖动后衰减到零，轻微受击不替换整套动作
	const float Envelope =
	    Age >= 0.f && Age < .20f
	        ? FMath::Sin(FMath::Clamp(Age / .035f, 0.f, 1.f) * HALF_PI) * FMath::Square(1.f - Age / .20f)
	        : 0.f;
	ArmorFlinchRotation = FRotator(-ArmorFlinchDirection.X * 12.f, 0.f, ArmorFlinchDirection.Y * 9.f) *
	                      ArmorFlinchStrength * Envelope;
	if (bDirectionalHitActive && GetWorld() && GetWorld()->GetTimeSeconds() >= HitReactionEndWorldSeconds)
		EndDirectionalHit();
	HitBlendAlpha = FMath::FInterpTo(HitBlendAlpha, bDirectionalHitActive ? 1.0f : 0.0f, DeltaSeconds,
	                                 bDirectionalHitActive ? 38.0f : 10.0f);
}

// 触发不打断整套攻击的轻微护甲受击姿态
void UBossLocomotionAnimInstance::PlayArmorFlinch(FVector2D Direction, float Strength)
{
	ArmorFlinchStart = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.;
	ArmorFlinchDirection = Direction.GetSafeNormal();
	ArmorFlinchStrength = Strength;
}
