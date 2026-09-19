#include "Combat/PlayerCombatAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "Engine/World.h"

namespace
{
struct FPlayerCombatAnimProxy : FAnimInstanceProxy
{
	FAnimNode_BlendSpacePlayer_Standalone LocomotionNode;
	FAnimNode_Slot ActionSlot;
	explicit FPlayerCombatAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance)
	{
		ActionSlot.SlotName = TEXT("DefaultSlot");
		ActionSlot.bAlwaysUpdateSourcePose = true;
		ActionSlot.Source.SetLinkNode(&LocomotionNode);
		LocomotionNode.SetLoop(true);
	}
	virtual FAnimNode_Base* GetCustomRootNode() override { return &ActionSlot; }
	virtual void Initialize(UAnimInstance* Instance) override
	{
		LocomotionNode.SetBlendSpace(CastChecked<UPlayerCombatAnimInstance>(Instance)->Locomotion);
		FAnimInstanceProxy::Initialize(Instance);
	}
	virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
	{
		FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
		const auto* Combat = CastChecked<UPlayerCombatAnimInstance>(Instance);
		LocomotionNode.SetBlendSpace(Combat->Locomotion);
		LocomotionNode.SetPosition(Combat->LocomotionPosition);
	}
};
}

UPlayerCombatAnimInstance::UPlayerCombatAnimInstance()
{
	Locomotion = LoadObject<UBlendSpace>(nullptr, TEXT("/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/BS_DKM_CombatLocomotion.BS_DKM_CombatLocomotion"));
	SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
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
	if (ReleaseAtWorldSeconds >= 0.0 && GetWorld() && GetWorld()->GetTimeSeconds() >= ReleaseAtWorldSeconds)
	{
		if (VisualAction) Montage_Stop(.18f, VisualAction);
		ReleaseAtWorldSeconds = -1.0;
		VisualAction = nullptr;
	}
}
