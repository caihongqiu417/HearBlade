// 让行为树选择并等待一次节拍攻击，同时维持接近、转向和绕行
#include "Combat/AI/BTTask_BeatAttack.h"

#include "AIController.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Combat/Boss/BeatSyncedComboComponent.h"
#include "Combat/Data/AttackData.h"
#include "Combat/Boss/BossLocomotionAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "NiagaraSystem.h"
#include "Misc/CommandLine.h"

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
                       float DeltaSeconds, const UBeatClockSubsystem* Clock = nullptr,
                       float ClipSeconds = 0.f, float CycleBeats = 2.f, float AccentStrength = 0.f)
{
	if (!Boss)
		return;
	const bool bBeatWalk = Clock && Clock->IsRunning() && ClipSeconds > KINDA_SMALL_NUMBER &&
	                       Clock->GetSecondsPerBeat() > KINDA_SMALL_NUMBER;
	const float BeatPhase = bBeatWalk ? static_cast<float>(FMath::Frac(Clock->GetCurrentBeat())) : 0.f;
	const float Pulse =
	    bBeatWalk ? 1.f + FMath::Clamp(AccentStrength, 0.f, .3f) * FMath::Cos(2.f * UE_PI * BeatPhase) : 1.f;
	const float TempoRate = bBeatWalk
	                            ? FMath::Clamp(static_cast<float>(ClipSeconds / (FMath::Max(CycleBeats, 1.f) *
	                                                                             Clock->GetSecondsPerBeat())),
	                                           .7f, 1.5f)
	                            : 1.f;
	TArray<USkeletalMeshComponent*> Meshes;
	Boss->GetComponents<USkeletalMeshComponent>(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (UBossLocomotionAnimInstance* Anim = Cast<UBossLocomotionAnimInstance>(Mesh->GetAnimInstance()))
		{
			Anim->SetLocomotion(Speed * Pulse, LocalDirection, DeltaSeconds);
			const float TargetRate =
			    Speed > 5.0f
			        ? (bBeatWalk ? FMath::Clamp(TempoRate * Pulse, .65f, 1.6f)
			                     : FMath::Clamp(Speed / FMath::Max(ReferenceSpeed, 50.0f), .65f, 1.35f))
			        : 1.0f;
			Mesh->GlobalAnimRateScale =
			    Speed > 5.0f ? FMath::FInterpTo(Mesh->GlobalAnimRateScale, TargetRate, DeltaSeconds, 10.0f)
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
	AttackData = LoadObject<UAttackDataAsset>(
	    nullptr,
	    TEXT(
	        "/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_DragonKnight_Combo01_Beat.DA_DragonKnight_Combo01_Beat"));
	if (IsValid(AttackData))
		AttackPool.Add(AttackData);
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
	if (EmbersEffect.Succeeded())
		Blink.LandingEffect = EmbersEffect.Object;
}

// 根据与玩家的距离调整靠近和绕行，并更新行走动画
float UBTTask_BeatAttack::UpdateOrbitMovement(UBeatSyncedComboComponent* Combo, AActor* Boss,
                                              const APawn* Player, const UBeatClockSubsystem* Clock,
                                              float DeltaSeconds)
{
	if (!Combo || !Boss || !Player)
		return 0.0f;
	const FVector ToPlayer = (Player->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D();
	const float Distance = FVector::Dist2D(Player->GetActorLocation(), Boss->GetActorLocation());
	const float DesiredRadius = FMath::Clamp(ApproachStopDistance, MinAttackDistance + 75.0f,
	                                         FMath::Max(MinAttackDistance + 75.0f, ActivationRange - 100.0f));
	const FVector Inward =
	    ToPlayer.IsNearlyZero() ? Boss->GetActorForwardVector().GetSafeNormal2D() : ToPlayer;
	const FVector Tangent = FVector(-Inward.Y, Inward.X, 0.0f) * (bOrbitClockwise ? 1.0f : -1.0f);
	const float RadialError = Distance - DesiredRadius;
	// 近距离保持绕行，不强制连续后退
	const float RadialVelocity = RadialError < 0.0f ? FMath::Max(RadialError * 0.8f, -ApproachSpeed * 0.45f)
	                                                : FMath::Min(RadialError * 1.5f, ApproachSpeed);
	// 离玩家较远时先靠近，接近绕行距离后再逐渐加入横向移动


	const float OrbitScale = .5f * FMath::Clamp(1.f - (RadialError - 40.f) / 140.f, 0.f, 1.f);
	FVector WantedVelocity = Inward * RadialVelocity + Tangent * OrbitSpeed * OrbitScale;
	if (WantedVelocity.Size2D() > FMath::Max(ApproachSpeed, OrbitSpeed))
		WantedVelocity = WantedVelocity.GetSafeNormal2D() * FMath::Max(ApproachSpeed, OrbitSpeed);
	const FRotator FacePlayer(0.0f, Inward.Rotation().Yaw, 0.0f);
	Boss->SetActorRotation(FMath::RInterpConstantTo(Boss->GetActorRotation(), FacePlayer, DeltaSeconds,
	                                                TurnSpeedDegreesPerSecond));
	const FVector LocalStep = Boss->GetActorTransform().InverseTransformVectorNoScale(WantedVelocity);
	SetBossLocomotion(Boss, WantedVelocity.Size2D(), LocalStep, AnimationReferenceSpeed, DeltaSeconds, Clock,
	                  Combo->GetWalkClipLengthSeconds(), WalkCycleBeats, WalkBeatAccentStrength);
	if (Combo->WasLastLocomotionBlocked() &&
	    Combo->GetWorld()->GetTimeSeconds() - LastBlockedWorldSeconds > 1.0f)
	{
		bOrbitClockwise = !bOrbitClockwise;
		LastBlockedWorldSeconds = Combo->GetWorld()->GetTimeSeconds();
		UE_LOG(LogTemp, Warning, TEXT("BTTask_BeatAttack: Orbit blocked Distance=%.0f, changing direction"),
		       Distance);
	}
	if (Combo->GetWorld()->GetTimeSeconds() - LastMovementLogWorldSeconds > 2.0f)
	{
		UE_LOG(LogTemp, Display,
		       TEXT("BTTask_BeatAttack: Orbit Distance=%.0f Target=%.0f ActualSpeed=%.0f Queued=%d Boss=%s"),
		       Distance, DesiredRadius, Combo->GetLastLocomotionSpeed(), Combo->IsComboQueued() ? 1 : 0,
		       *Boss->GetActorLocation().ToString());
		LastMovementLogWorldSeconds = Combo->GetWorld()->GetTimeSeconds();
	}
	return Distance;
}

// 行为树入口：找到敌人连击组件并初始化本轮任务；等待期间保持任务进行中，避免每帧重复启动
EBTNodeResult::Type UBTTask_BeatAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBeatSyncedComboComponent* Combo = GetComboComponent(OwnerComp, BossActorKey.SelectedKeyName);
	if (!Combo)
		return EBTNodeResult::Failed;
	bAttackStarted = false;
	bLoggedWaitingForRange = false;
	bLoggedTurning = false;
	OrbitStartWorldSeconds = Combo->GetWorld()->GetTimeSeconds();
	LastMovementLogWorldSeconds = -1.0e9;
	if (const UBeatClockSubsystem* Clock = Combo->GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	    Clock && Clock->IsRunning())
		UE_LOG(LogTemp, Display, TEXT("BeatWalk: Clip=%.2fs Cycle=%.1f beats Rate=%.2f Accent=%.2f"),
		       Combo->GetWalkClipLengthSeconds(), WalkCycleBeats,
		       Combo->GetWalkClipLengthSeconds() /
		           FMath::Max(WalkCycleBeats * Clock->GetSecondsPerBeat(), .01),
		       WalkBeatAccentStrength);
	return EBTNodeResult::InProgress;
}

// 等待已安排的攻击执行，期间更新移动并检查任务是否结束
void UBTTask_BeatAttack::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("ComboPoseAudit")))
		return;
#endif
	UBeatSyncedComboComponent* Combo = GetComboComponent(OwnerComp, BossActorKey.SelectedKeyName);
	if (!Combo)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}
	UWorld* World = Combo->GetWorld();
	const UBeatClockSubsystem* Clock = World->GetSubsystem<UBeatClockSubsystem>();
	if (Combo->IsPlayerResonanceActive())
		return;
	if (Combo->IsGroundBurstRecovery())
	{
		SetBossLocomotion(Combo->GetOwner(), 0.f, FVector::ZeroVector, AnimationReferenceSpeed, DeltaSeconds);
		return;
	}
	if (Combo->IsPhaseTransitionBlocking())
	{
		SetBossLocomotion(Combo->GetOwner(), 0.f, FVector::ZeroVector, AnimationReferenceSpeed, DeltaSeconds);
		return;
	}
	if (!bAttackStarted)
	{
		// 上一任务被中断时，已经出手的攻击可能还在继续
		// 先等它结束，当前任务不能同时绕行或再排一轮攻击
		if (Combo->IsComboQueuedOrPlaying())
		{
			SetBossLocomotion(Combo->GetOwner(), 0.0f, FVector::ZeroVector, AnimationReferenceSpeed,
			                  DeltaSeconds);
			return;
		}
		const APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
		AActor* Boss = Combo->GetOwner();
		if (!Player || !Boss)
		{
			SetBossLocomotion(Boss, 0.0f, FVector::ZeroVector, AnimationReferenceSpeed, DeltaSeconds);
			return;
		}
		UpdateOrbitMovement(Combo, Boss, Player, Clock, DeltaSeconds);
		if (!Clock || !Clock->IsRunning() ||
		    World->GetTimeSeconds() - LastAttackCompletedWorldSeconds < PostAttackCooldownSeconds ||
		    World->GetTimeSeconds() - OrbitStartWorldSeconds < MinimumOrbitSeconds)
			return;
		const float CurrentDistance = FVector::Dist2D(Player->GetActorLocation(), Boss->GetActorLocation());
		// 闪现攻击自行选择落点，这里只检查最远激活距离
		if (CurrentDistance > ActivationRange)
		{
			if (!bLoggedWaitingForRange)
			{
				UE_LOG(LogTemp, Display, TEXT("BTTask_BeatAttack: Waiting within %.0f cm (current %.0f cm)"),
				       ActivationRange, CurrentDistance);
				bLoggedWaitingForRange = true;
			}
			return;
		}
		const FVector FlatDirection =
		    FVector(Player->GetActorLocation().X - Combo->GetOwner()->GetActorLocation().X,
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
					UE_LOG(LogTemp, Display, TEXT("BTTask_BeatAttack: Turning toward player YawError=%.1f"),
					       YawError);
					bLoggedTurning = true;
				}
				const FRotator Next =
				    FMath::RInterpConstantTo(Current, Desired, DeltaSeconds, TurnSpeedDegreesPerSecond);
				Combo->GetOwner()->SetActorRotation(Next);
				return;
			}
			Combo->GetOwner()->SetActorRotation(Desired);
		}
		// 先按阶段筛选普通近战，再排除上一招；范围攻击由后面的独立轮换决定
		TArray<UAttackDataAsset*> Candidates;
		for (UAttackDataAsset* Candidate : AttackPool)
		{
			if (IsValid(Candidate) && Candidate->MinimumBossPhase <= Combo->BossPhase &&
			    !Candidate->ImpactMarkers.ContainsByPredicate(
			        [](const auto& Marker)
			        {
				        return Marker.DamageShape == EBossAttackDamageShape::GroundBurst;
			        }))
				Candidates.AddUnique(Candidate);
		}
		if (Candidates.IsEmpty() && IsValid(AttackData) && AttackData->MinimumBossPhase <= Combo->BossPhase)
			Candidates.Add(AttackData);
		if (Candidates.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("BTTask_BeatAttack: AttackPool is empty"));
			FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
			return;
		}
		UAttackDataAsset* Selected = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
		// 有其他可选攻击时排除上一招，减少连续重复
		if (Candidates.Num() > 1)
			Candidates.RemoveAll(
			    [this](const auto* Item)
			    {
				    return Item == LastSelectedAttack.Get();
			    });
		Selected = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
		Combo->ConfigureNextBlink(Blink);
		// 先按音乐重音选择攻击，随后再处理阶段要求和范围攻击轮换
		float BestMusicPriority = 0.f;
		for (auto* Candidate : Candidates)
		{
			float Priority = 0.f;
			Combo->SelectMusicEmphasisBeat(Candidate, Priority);
			if (Priority > BestMusicPriority)
			{
				BestMusicPriority = Priority;
				Selected = Candidate;
			}
		}
		// 范围攻击还要检查预警时间内是否有安全出口，不能只按轮换次数强行释放
		if (Combo->IsAreaAttackDue())
		{
			if (UAttackDataAsset* Area = PhaseTwoAreaAttack.LoadSynchronous())
			{
				const FAttackImpactMarker* Burst = Area->ImpactMarkers.FindByPredicate(
				    [](const FAttackImpactMarker& M)
				    {
					    return M.DamageShape == EBossAttackDamageShape::GroundBurst;
				    });
				if (Burst)
				{
					const int32 Index = static_cast<int32>(Burst - Area->ImpactMarkers.GetData());
					const float LockTime = Index > 0 ? Area->ImpactMarkers[Index - 1].AnimationTimeSeconds
					                                 : Area->ClipStartSeconds;
					const double WarningDuration =
					    Area->GetElapsedAtAnimationTime(Burst->AnimationTimeSeconds) -
					    Area->GetElapsedAtAnimationTime(LockTime);
					if (Combo->HasSafeAreaExit(Boss->GetActorLocation(), Burst->GroundBurst.RadiusCm, Player,
					                           WarningDuration))
						Selected = Area;
					else if (Combo->HasSafeAreaExit(Boss->GetActorLocation(),
					                                FMath::Min(360.f, Burst->GroundBurst.RadiusCm), Player,
					                                WarningDuration))
						Selected = Area;
				}
			}
		}
		Combo->SetAttackTarget(const_cast<APawn*>(Player));
		Combo->ConfigureNextBlink(Blink);
		if (!Combo->QueueAttack(Selected))
		{
			Combo->CancelCombo();
			UE_LOG(LogTemp, Warning, TEXT("BTTask_BeatAttack: QueueAttack failed for %s"),
			       *GetNameSafe(Selected));
			FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
			return;
		}
		bAttackStarted = true;
		LastSelectedAttack = Selected;
		UE_LOG(LogTemp, Display,
		       TEXT("BTTask_BeatAttack: PlayerDistance=%.0f Attack=%s Queued=true BlinkImpactAudio=%.3f"),
		       CurrentDistance, *GetNameSafe(Selected), Combo->GetScheduledFirstImpactAudioSeconds());
		return;
	}
	if (!Clock || !Clock->IsRunning())
	{
		Combo->CancelCombo();
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}
	// 已排队时继续等待音乐拍点，真正出手后停止绕行
	if (Combo->IsComboQueued())
	{
		const APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
		AActor* Boss = Combo->GetOwner();
		if (Player && Boss)
		{
			UpdateOrbitMovement(Combo, Boss, Player, Clock, DeltaSeconds);
			return;
		}
	}
	if (Combo->IsComboPlaying())
	{
		SetBossLocomotion(Combo->GetOwner(), 0.0f, FVector::ZeroVector, AnimationReferenceSpeed,
		                  DeltaSeconds);
	}
	if (!Combo->IsComboQueuedOrPlaying())
	{
		const bool bCompleted = Combo->WasLastAttackCompleted();
		UE_LOG(LogTemp, Display, TEXT("BTTask_BeatAttack: Finished=%s"),
		       bCompleted ? TEXT("true") : TEXT("false"));
		if (bCompleted)
			LastAttackCompletedWorldSeconds = World->GetTimeSeconds();
		if (bCompleted)
			bOrbitClockwise = !bOrbitClockwise;
		bAttackStarted = false;
		FinishLatentTask(OwnerComp, bCompleted ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
	}
}

// 行为树中止任务时取消连招，避免攻击脱离任务继续执行
EBTNodeResult::Type UBTTask_BeatAttack::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (bAttackStarted)
	{
		if (UBeatSyncedComboComponent* Combo = GetComboComponent(OwnerComp, BossActorKey.SelectedKeyName))
		{
			// 行为树中断只撤销尚未播放的攻击，已经出手的连招继续完成
			if (!Combo->IsComboPlaying())
				Combo->CancelCombo();
			else
				UE_LOG(LogTemp, Display,
				       TEXT("BTTask_BeatAttack: BT aborted after playback began; committed combo continues"));
		}
	}
	bAttackStarted = false;
	if (UBeatSyncedComboComponent* Combo = GetComboComponent(OwnerComp, BossActorKey.SelectedKeyName))
	{
		SetBossLocomotion(Combo->GetOwner(), 0.0f, FVector::ZeroVector, AnimationReferenceSpeed, 0.0f);
	}
	return EBTNodeResult::Aborted;
}
