#include "Combat/PlayerCombatAnimInstance.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "Combat/PlayerRhythmCombatComponent.h"
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

		// A tiny additive body pulse makes idle and locomotion feel connected to
		// the music without shifting the collision capsule or planting the feet
		// through CharacterMovement.
		LocalToComponentNode.LocalPose.SetLinkNode(&RootBlend);
		PelvisBeatNode.ComponentPose.SetLinkNode(&LocalToComponentNode);
		// Drive the torso from the first spine bone instead of translating the
		// pelvis. Moving the pelvis lifts both legs and makes the feet visibly
		// leave the floor; spine_01 keeps the authored foot planting intact.
		PelvisBeatNode.BoneToModify.BoneName = TEXT("spine_01");
		PelvisBeatNode.TranslationMode = BMM_Additive;
		// Component-space Z is the character's real vertical axis.
		PelvisBeatNode.TranslationSpace = BCS_ComponentSpace;
		SpineBeatNode.ComponentPose.SetLinkNode(&PelvisBeatNode);
		SpineBeatNode.BoneToModify.BoneName = TEXT("spine_03");
		SpineBeatNode.RotationMode = BMM_Additive;
		SpineBeatNode.RotationSpace = BCS_BoneSpace;
		ComponentToLocalNode.ComponentPose.SetLinkNode(&SpineBeatNode);
	}
	virtual FAnimNode_Base* GetCustomRootNode() override { return &ComponentToLocalNode; }
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
	Locomotion = LoadObject<UBlendSpace>(nullptr, TEXT("/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/BS_DKM_CombatLocomotion.BS_DKM_CombatLocomotion"));
	DirectionalHit = LoadObject<UBlendSpace>(nullptr, TEXT("/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/BS_DKM_DirectionalHit.BS_DKM_DirectionalHit"));
	SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
}

void UPlayerCombatAnimInstance::StartDirectionalHit(FVector2D ReactionDirection, float Duration)
{
	if (!GetWorld()) return;
	if (!ReactionDirection.Normalize()) ReactionDirection = FVector2D(1.0f, 0.0f);
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

	// Locomotion rhythm follows the same musical clock used by attacks. Combat
	// actions own the full body, so the additive pose fades out before it can
	// bend a dodge, block, counter or hit reaction.
	float TargetPoseAlpha = 0.0f;
	float TargetPlayRate = 1.0f;
	float Pulse = 0.0f;
	bool bMoving = false;
	if (const APawn* Pawn = TryGetPawnOwner())
	{
		bMoving = Pawn->GetVelocity().SizeSquared2D() > 225.0f;
		const UPlayerRhythmCombatComponent* Combat = Pawn->FindComponentByClass<UPlayerRhythmCombatComponent>();
		const bool bActionBusy = (Combat && Combat->IsPerformingAction()) ||
			(VisualAction && Montage_IsPlaying(VisualAction));
		if (!bActionBusy && !bDirectionalHitActive)
		{
			if (const UBeatClockSubsystem* Clock = GetWorld() ? GetWorld()->GetSubsystem<UBeatClockSubsystem>() : nullptr;
				Clock && Clock->IsRunning())
			{
				const double Beat = Clock->GetCurrentBeat();
				const float Phase = static_cast<float>(Beat - FMath::FloorToDouble(Beat));
				// Stylised musical bounce: compress exactly on the beat, rebound soon
				// afterwards, then settle before the next anticipation. Each segment
				// uses SmoothStep, including the wrap back to phase zero, so this reads
				// as authored body motion rather than a frame hitch.
				auto SmoothSegment = [](float T, float FromPhase, float ToPhase, float FromValue, float ToValue)
				{
					const float Local = FMath::Clamp((T - FromPhase) / FMath::Max(ToPhase - FromPhase, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
					// Quintic smoother-step keeps acceleration continuous at each key,
					// giving the larger bounce a soft, animation-authored transition.
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
				// Never modulate locomotion playback rate every frame. Even a small
				// oscillation reads as foot animation stutter while the capsule moves
				// at a constant speed.
				TargetPlayRate = 1.0f;
			}
		}
	}
	SmoothedBeatPoseAlpha = FMath::FInterpTo(SmoothedBeatPoseAlpha, TargetPoseAlpha,
		DeltaSeconds, TargetPoseAlpha > SmoothedBeatPoseAlpha ? 16.0f : 24.0f);
	SmoothedLocomotionPlayRate = FMath::FInterpTo(SmoothedLocomotionPlayRate, TargetPlayRate, DeltaSeconds, 12.0f);
	BeatPoseAlpha = SmoothedBeatPoseAlpha;
	LocomotionPlayRate = SmoothedLocomotionPlayRate;
	const float BodyAmount = bMoving ? 0.65f : 1.0f;
	BeatPelvisTranslation = FVector(0.0f, 0.0f, Pulse * 2.60f * BodyAmount);
	// Keep this layer purely vertical. The authored locomotion continues to own
	// the hips and torso orientation, so the beat never reads as hip twisting.
	BeatSpineRotation = FRotator::ZeroRotator;

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
