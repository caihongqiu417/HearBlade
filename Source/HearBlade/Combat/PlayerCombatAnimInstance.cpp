#include "Combat/PlayerCombatAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "Engine/World.h"

struct FPlayerCombatAnimProxy : FAnimInstanceProxy
{
	FAnimNode_BlendSpacePlayer_Standalone LocomotionNode;
	FAnimNode_Slot ActionSlot;
	FAnimNode_BlendSpacePlayer_Standalone HitNode;
	FAnimNode_TwoWayBlend RootBlend;
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
	}
	virtual FAnimNode_Base* GetCustomRootNode() override { return &RootBlend; }
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
		HitNode.SetBlendSpace(Combat->DirectionalHit);
		HitNode.SetPosition(Combat->HitBlendPosition);
		if (LastHitReactionSerial != Combat->HitReactionSerial)
		{
			HitNode.SetAccumulatedTime(0.0f);
			LastHitReactionSerial = Combat->HitReactionSerial;
		}
		RootBlend.Alpha = Combat->HitBlendAlpha;
	}
};

UPlayerCombatAnimInstance::UPlayerCombatAnimInstance()
{
	Locomotion = LoadObject<UBlendSpace>(nullptr, TEXT("/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/BS_DKM_CombatLocomotion.BS_DKM_CombatLocomotion"));
	DirectionalHit = LoadObject<UBlendSpace>(nullptr, TEXT("/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/BS_DKM_DirectionalHit.BS_DKM_DirectionalHit"));
	SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
}

void UPlayerCombatAnimInstance::StartDirectionalHit(FVector2D ReactionDirection, float Duration)
{
	if (!GetWorld()) return;
	if (!ReactionDirection.Normalize()) ReactionDirection = FVector2D(-1.0f, 0.0f);
	HitBlendPosition = FVector(ReactionDirection.X, ReactionDirection.Y, 0.0f);
	bDirectionalHitActive = true;
	HitReactionEndWorldSeconds = GetWorld()->GetTimeSeconds() + FMath::Max(.1f, Duration);
	++HitReactionSerial;
	if (VisualAction) Montage_Stop(.06f, VisualAction);
}

void UPlayerCombatAnimInstance::EndDirectionalHit()
{
	bDirectionalHitActive = false;
	HitReactionEndWorldSeconds = -1.0;
}

FAnimInstanceProxy* UPlayerCombatAnimInstance::CreateAnimInstanceProxy() { return new FPlayerCombatAnimProxy(this); }
void UPlayerCombatAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }

void UPlayerCombatAnimInstance::PlayCombatAction(UAnimSequence* Sequence, bool bLooping, float Duration, float ChainGrace)
{
	if (!Sequence || !GetWorld()) return;
	const float ActualDuration = Duration > 0.f ? Duration : Sequence->GetPlayLength();
	const float Rate = Sequence->GetPlayLength() / FMath::Max(.05f, ActualDuration);
	UAnimMontage* Next = PlaySlotAnimationAsDynamicMontage(Sequence, TEXT("DefaultSlot"),
		.10f, .18f, Rate, bLooping ? 100000 : 1);
	if (!Next) return;
	// The outgoing action stays available for crossfade until the next operation
	// or the short recovery deadline. No forced idle between consecutive actions.
	Next->bEnableAutoBlendOut = false;
	VisualAction = Next;
	ReleaseAtWorldSeconds = bLooping ? -1.0 : GetWorld()->GetTimeSeconds() + ActualDuration + ChainGrace;
}

void UPlayerCombatAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	if (bDirectionalHitActive && GetWorld() && GetWorld()->GetTimeSeconds() >= HitReactionEndWorldSeconds)
		EndDirectionalHit();
	HitBlendAlpha = FMath::FInterpTo(HitBlendAlpha, bDirectionalHitActive ? 1.0f : 0.0f,
		DeltaSeconds, bDirectionalHitActive ? 55.0f : 8.0f);
	if (ReleaseAtWorldSeconds >= 0.0 && GetWorld() && GetWorld()->GetTimeSeconds() >= ReleaseAtWorldSeconds)
	{
		if (VisualAction) Montage_Stop(.18f, VisualAction);
		ReleaseAtWorldSeconds = -1.0;
		VisualAction = nullptr;
	}
}
