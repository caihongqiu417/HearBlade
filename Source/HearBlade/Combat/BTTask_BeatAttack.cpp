#include "Combat/BTTask_BeatAttack.h"

#include "AIController.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Combat/BeatSyncedComboComponent.h"
#include "Combat/AttackData.h"
#include "Combat/BossLocomotionAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "NiagaraSystem.h"

namespace
{
UBeatSyncedComboComponent* GetComboComponent(UBehaviorTreeComponent& OwnerComp, FName BossActorKey)
{
	if (const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent())
	{
		if (AActor* Boss = Cast<AActor>(Blackboard->GetValueAsObject(BossActorKey)))
		{
			return Boss->FindComponentByClass<UBeatSyncedComboComponent>();
		}
	}
	const AAIController* Controller = OwnerComp.GetAIOwner();
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	return Pawn ? Pawn->FindComponentByClass<UBeatSyncedComboComponent>() : nullptr;
}

void SetBossLocomotion(AActor* Boss, float Speed, const FVector& LocalDirection, float ReferenceSpeed,
	float DeltaSeconds, const UBeatClockSubsystem* Clock = nullptr, float ClipSeconds = 0.f,
	float CycleBeats = 2.f, float AccentStrength = 0.f)
{
	if (!Boss) return;
	const bool bBeatWalk = Clock && Clock->IsRunning() && ClipSeconds > KINDA_SMALL_NUMBER &&
		Clock->GetSecondsPerBeat() > KINDA_SMALL_NUMBER;
	const float BeatPhase = bBeatWalk ? static_cast<float>(FMath::Frac(Clock->GetCurrentBeat())) : 0.f;
	const float Pulse = bBeatWalk ? 1.f + FMath::Clamp(AccentStrength, 0.f, .3f) *
		FMath::Cos(2.f * UE_PI * BeatPhase) : 1.f;
	const float TempoRate = bBeatWalk ? FMath::Clamp(static_cast<float>(ClipSeconds /
		(FMath::Max(CycleBeats, 1.f) * Clock->GetSecondsPerBeat())), .7f, 1.5f) : 1.f;
	TArray<USkeletalMeshComponent*> Meshes;
	Boss->GetComponents<USkeletalMeshComponent>(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (UBossLocomotionAnimInstance* Anim = Cast<UBossLocomotionAnimInstance>(Mesh->GetAnimInstance()))
		{
			Anim->SetLocomotion(Speed * Pulse, LocalDirection, DeltaSeconds);
			const float TargetRate = Speed > 5.0f ? (bBeatWalk
				? FMath::Clamp(TempoRate * Pulse, .65f, 1.6f)
				: FMath::Clamp(Speed / FMath::Max(ReferenceSpeed, 50.0f), .65f, 1.35f)) : 1.0f;
			Mesh->GlobalAnimRateScale = Speed > 5.0f
				? FMath::FInterpTo(Mesh->GlobalAnimRateScale, TargetRate, DeltaSeconds, 10.0f)
				: 1.0f;
		}
	}
}
}

UBTTask_BeatAttack::UBTTask_BeatAttack()
{
	NodeName = TEXT("节拍攻击（下一可用拍）");
	bCreateNodeInstance = true;
	bNotifyTick = true;
	BossActorKey.SelectedKeyName = TEXT("BossActor");
	AttackData = LoadObject<UAttackDataAsset>(nullptr,
		TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_DragonKnight_Combo01_Beat.DA_DragonKnight_Combo01_Beat"));
	if (IsValid(AttackData)) AttackPool.Add(AttackData);
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> FireEffect(
		TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/VFX/NS_Fire_local_System.NS_Fire_local_System"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> EmbersEffect(
		TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/VFX/NS_Embers_System.NS_Embers_System"));
	if (FireEffect.Succeeded())
	{
		Blink.WarningEffect = FireEffect.Object;
		Blink.OriginEffect = FireEffect.Object;
		Blink.LandingEffect = FireEffect.Object;
	}
	if (EmbersEffect.Succeeded()) Blink.LandingEffect = EmbersEffect.Object;
}

EBTNodeResult::Type UBTTask_BeatAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBeatSyncedComboComponent* Combo = GetComboComponent(OwnerComp, BossActorKey.SelectedKeyName);
	if (!Combo) return EBTNodeResult::Failed;
	bAttackStarted = false;
	bLoggedWaitingForRange = false;
	bLoggedTurning = false;
	OrbitStartWorldSeconds = Combo->GetWorld()->GetTimeSeconds();
	LastMovementLogWorldSeconds = -1.0e9;
	if (const UBeatClockSubsystem* Clock = Combo->GetWorld()->GetSubsystem<UBeatClockSubsystem>();
		Clock && Clock->IsRunning())
		UE_LOG(LogTemp, Display, TEXT("BeatWalk: Clip=%.2fs Cycle=%.1f beats Rate=%.2f Accent=%.2f"),
			Combo->GetWalkClipLengthSeconds(), WalkCycleBeats,
			Combo->GetWalkClipLengthSeconds() / FMath::Max(WalkCycleBeats * Clock->GetSecondsPerBeat(), .01),
			WalkBeatAccentStrength);
	return EBTNodeResult::InProgress;
}

void UBTTask_BeatAttack::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	UBeatSyncedComboComponent* Combo = GetComboComponent(OwnerComp, BossActorKey.SelectedKeyName);
	if (!Combo)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}
	UWorld* World = Combo->GetWorld();
	const UBeatClockSubsystem* Clock = World->GetSubsystem<UBeatClockSubsystem>();
	if (!bAttackStarted)
	{
		const APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
		AActor* Boss = Combo->GetOwner();
		if (!Player || !Boss)
		{
			SetBossLocomotion(Boss, 0.0f, FVector::ZeroVector, AnimationReferenceSpeed, DeltaSeconds);
			return;
		}
		const FVector ToPlayer = (Player->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D();
		const float Distance = FVector::Dist2D(Player->GetActorLocation(), Boss->GetActorLocation());
		const float DesiredRadius = FMath::Clamp(ApproachStopDistance, MinAttackDistance + 75.0f,
			FMath::Max(MinAttackDistance + 75.0f, ActivationRange - 100.0f));
		const FVector Inward = ToPlayer.IsNearlyZero() ? Boss->GetActorForwardVector().GetSafeNormal2D() : ToPlayer;
		const FVector Tangent = FVector(-Inward.Y, Inward.X, 0.0f) * (bOrbitClockwise ? 1.0f : -1.0f);
		const float RadialError = Distance - DesiredRadius;
		const float RadialVelocity = FMath::Clamp(RadialError * 1.5f, -ApproachSpeed, ApproachSpeed);
		// Prioritize getting clear of the player if a combo ended almost on top of them.
		const float TangentialVelocity = Distance < MinAttackDistance ? OrbitSpeed * 0.35f : OrbitSpeed;
		FVector WantedVelocity = Inward * RadialVelocity + Tangent * TangentialVelocity;
		if (WantedVelocity.Size2D() > FMath::Max(ApproachSpeed, OrbitSpeed))
			WantedVelocity = WantedVelocity.GetSafeNormal2D() * FMath::Max(ApproachSpeed, OrbitSpeed);
		const FRotator FacePlayer(0.0f, Inward.Rotation().Yaw, 0.0f);
		Boss->SetActorRotation(FMath::RInterpConstantTo(Boss->GetActorRotation(), FacePlayer,
			DeltaSeconds, TurnSpeedDegreesPerSecond));
		const FVector LocalStep = Boss->GetActorTransform().InverseTransformVectorNoScale(WantedVelocity);
		SetBossLocomotion(Boss, WantedVelocity.Size2D(), LocalStep, AnimationReferenceSpeed, DeltaSeconds,
			Clock, Combo->GetWalkClipLengthSeconds(), WalkCycleBeats, WalkBeatAccentStrength);
		const float ActualSpeed = Combo->GetLastLocomotionSpeed();
		if (Combo->WasLastLocomotionBlocked() && World->GetTimeSeconds() - LastBlockedWorldSeconds > 1.0f)
		{
			bOrbitClockwise = !bOrbitClockwise;
			LastBlockedWorldSeconds = World->GetTimeSeconds();
			UE_LOG(LogTemp, Warning, TEXT("BTTask_BeatAttack: Orbit blocked Distance=%.0f, changing direction"), Distance);
		}
		if (World->GetTimeSeconds() - LastMovementLogWorldSeconds > 2.0f)
		{
			UE_LOG(LogTemp, Display, TEXT("BTTask_BeatAttack: Orbit Distance=%.0f Target=%.0f ActualSpeed=%.0f Boss=%s"),
				Distance, DesiredRadius, ActualSpeed, *Boss->GetActorLocation().ToString());
			LastMovementLogWorldSeconds = World->GetTimeSeconds();
		}
		if (!Clock || !Clock->IsRunning() ||
			World->GetTimeSeconds() - LastAttackCompletedWorldSeconds < PostAttackCooldownSeconds ||
			World->GetTimeSeconds() - OrbitStartWorldSeconds < MinimumOrbitSeconds) return;
		const float CurrentDistance = FVector::Dist2D(Player->GetActorLocation(), Boss->GetActorLocation());
		// Arena geometry can prevent reaching the preferred orbit radius. Keep the fight moving
		// once we've visibly tried to circle, while preserving the hard minimum attack distance.
		const float RequiredAttackDistance = Combo->WasLastLocomotionBlocked()
			? MinAttackDistance : FMath::Max(MinAttackDistance, DesiredRadius - 175.0f);
		if (CurrentDistance > ActivationRange || CurrentDistance < RequiredAttackDistance)
		{
			if (!bLoggedWaitingForRange)
			{
				UE_LOG(LogTemp, Display, TEXT("BTTask_BeatAttack: Waiting for %.0f..%.0f cm (current %.0f cm)"),
					RequiredAttackDistance, ActivationRange, CurrentDistance);
				bLoggedWaitingForRange = true;
			}
			return;
		}
		const FVector FlatDirection = FVector(Player->GetActorLocation().X - Combo->GetOwner()->GetActorLocation().X,
			Player->GetActorLocation().Y - Combo->GetOwner()->GetActorLocation().Y, 0.0f);
		if (!FlatDirection.IsNearlyZero())
		{
			const FRotator Desired(0.0f, FlatDirection.Rotation().Yaw, 0.0f);
			const FRotator Current = Combo->GetOwner()->GetActorRotation();
			const float YawError = FMath::Abs(FMath::FindDeltaAngleDegrees(Current.Yaw, Desired.Yaw));
			if (YawError > FacingToleranceDegrees)
			{
				if (!bLoggedTurning)
				{
					UE_LOG(LogTemp, Display, TEXT("BTTask_BeatAttack: Turning toward player YawError=%.1f"), YawError);
					bLoggedTurning = true;
				}
				const FRotator Next = FMath::RInterpConstantTo(Current, Desired, DeltaSeconds, TurnSpeedDegreesPerSecond);
				Combo->GetOwner()->SetActorRotation(Next);
				return;
			}
			Combo->GetOwner()->SetActorRotation(Desired);
		}
		TArray<UAttackDataAsset*> Candidates;
		for (UAttackDataAsset* Candidate : AttackPool)
		{
			if (IsValid(Candidate)) Candidates.Add(Candidate);
		}
		if (Candidates.IsEmpty() && IsValid(AttackData)) Candidates.Add(AttackData);
		if (Candidates.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("BTTask_BeatAttack: AttackPool is empty"));
			FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
			return;
		}
		UAttackDataAsset* Selected = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
		Combo->SetAttackTarget(const_cast<APawn*>(Player));
		Combo->ConfigureNextBlink(Blink);
		if (!Combo->QueueAttack(Selected))
		{
			Combo->CancelCombo();
			UE_LOG(LogTemp, Warning, TEXT("BTTask_BeatAttack: QueueAttack failed for %s"), *GetNameSafe(Selected));
			FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
			return;
		}
		bAttackStarted = true;
		SetBossLocomotion(Boss, 0.0f, FVector::ZeroVector, AnimationReferenceSpeed, DeltaSeconds);
		UE_LOG(LogTemp, Display, TEXT("BTTask_BeatAttack: PlayerDistance=%.0f Attack=%s Queued=true BlinkImpactAudio=%.3f"),
			CurrentDistance, *GetNameSafe(Selected), Combo->GetScheduledFirstImpactAudioSeconds());
		return;
	}
	if (!Clock || !Clock->IsRunning())
	{
		Combo->CancelCombo();
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}
	if (!Combo->IsComboQueuedOrPlaying())
	{
		const bool bCompleted = Combo->WasLastAttackCompleted();
		UE_LOG(LogTemp, Display, TEXT("BTTask_BeatAttack: Finished=%s"), bCompleted ? TEXT("true") : TEXT("false"));
		if (bCompleted) LastAttackCompletedWorldSeconds = World->GetTimeSeconds();
		if (bCompleted) bOrbitClockwise = !bOrbitClockwise;
		bAttackStarted = false;
		FinishLatentTask(OwnerComp, bCompleted ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
	}
}

EBTNodeResult::Type UBTTask_BeatAttack::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (bAttackStarted)
	{
		if (UBeatSyncedComboComponent* Combo = GetComboComponent(OwnerComp, BossActorKey.SelectedKeyName)) Combo->CancelCombo();
	}
	bAttackStarted = false;
	if (UBeatSyncedComboComponent* Combo = GetComboComponent(OwnerComp, BossActorKey.SelectedKeyName))
	{
		SetBossLocomotion(Combo->GetOwner(), 0.0f, FVector::ZeroVector, AnimationReferenceSpeed, 0.0f);
	}
	return EBTNodeResult::Aborted;
}
