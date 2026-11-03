// 管理敌人阶段切换条件、过场启动和第二阶段攻击轮换
#include "Combat/Boss/BeatSyncedComboComponent.h"
#include "Combat/Boss/BossAscensionComponent.h"
#include "Combat/Player/PlayerRhythmCombatComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"

// 记录第二阶段已完成的近战次数，用于安排后续范围攻击
void UBeatSyncedComboComponent::RecordPhaseTwoAttackOutcome(bool bCompleted)
{
	if (bPlayerResonance)
		return;
	// 同一轮攻击只记一次阶段进度，取消与正常结束不会重复累计
	if (BossPhase != 2 || bPhaseTwoAttackCounted || !AttackData)
		return;
	const bool bArea = AttackData->ImpactMarkers.ContainsByPredicate(
	    [](const FAttackImpactMarker& M)
	    {
		    return M.DamageShape == EBossAttackDamageShape::GroundBurst;
	    });
	if (bArea)
	{
		if (!bGroundBurstResolved)
			return;
		CompletedPhaseTwoMelee = 0;
		NextAreaAfterMelee = FMath::RandRange(2, 3);
	}
	else
	{
		if (!bCompleted && (!bPlaying || ActiveDamageImpactIndex != GetImpactCount() - 1))
			return;
		++CompletedPhaseTwoMelee;
	}
	bPhaseTwoAttackCounted = true;
	UE_LOG(LogTemp, Display, TEXT("BossPhase: rotation melee=%d required=%d area=%d"), CompletedPhaseTwoMelee,
	       NextAreaAfterMelee, bArea);
}

float UBeatSyncedComboComponent::GetPhaseTransitionProgress() const
{
	const auto* Ascension =
	    GetOwner() ? GetOwner()->FindComponentByClass<UBossAscensionComponent>() : nullptr;
	return bPhaseTransitionActive && Ascension ? Ascension->GetProgress() : -1.f;
}

// 等待阶段切换条件，超时后在连击结束时启动过场
void UBeatSyncedComboComponent::UpdateBossPhase(const UBeatClockSubsystem* Clock)
{
	if (bPlayerResonance)
		return;
	if (BossHealth <= 0.f)
	{
		bPhaseChangePending = false;
		return;
	}
	if (!bPhaseChangePending || bPhaseTransitionActive || !Clock || !Clock->IsRunning())
		return;
	// 优先等待防御接触触发演出，超过十秒后在连招结束时启动，避免阶段一直无法切换
	if (!bPlaying && !bQueued && GetWorld()->GetTimeSeconds() - PhaseRequestWorldSeconds > 10.)
		if (auto* Player = UGameplayStatics::GetPlayerCharacter(this, 0))
			TryBeginPhaseAscension(Player,
			                       (GetOwner()->GetActorLocation() + Player->GetActorLocation()) * .5f +
			                           FVector(0, 0, 55),
			                       false);
}

bool UBeatSyncedComboComponent::TryBeginPhaseAscension(AActor* Player, FVector Contact, bool bFromClash)
{
	if (bPlayerResonance)
		return false;
#if !UE_BUILD_SHIPPING
	if (bFromClash && FParse::Param(FCommandLine::Get(), TEXT("PhaseTransitionNoClashTest")))
		return false;
#endif
	if (!bPhaseChangePending || bPhaseTransitionActive || BossPhase != 1 || BossHealth <= 0.f)
		return false;
	auto* Character = Cast<ACharacter>(Player);
	const auto* State = Player ? Player->FindComponentByClass<UPlayerRhythmCombatComponent>() : nullptr;
	if (!Character || !State || State->Health <= 0.f)
		return false;
	auto* Ascension = GetOwner()->FindComponentByClass<UBossAscensionComponent>();
	if (!Ascension)
	{
		Ascension = NewObject<UBossAscensionComponent>(GetOwner());
		Ascension->RegisterComponent();
	}
	EndCinematicAccent(true);
	if (!Ascension->BeginAscension(Character, Contact, bFromClash))
		return false;
	// 演出成功启动后才取消连招，启动失败时保留原战斗状态
	bPhaseTransitionActive = true;
	CancelCombo();
	EndBossHitReaction();
	OnPhaseChanged.Broadcast(0);
	return true;
}

// 根据过场是否完成，进入第二阶段或等待重新播放
void UBeatSyncedComboComponent::FinishPhaseAscension(bool bCompleted)
{
	bPhaseTransitionActive = false;
	// 过场中断时重新等待触发，不提前把 Boss 切换到第二阶段
	if (!bCompleted)
	{
		bPhaseChangePending = BossHealth > 0.f;
		PhaseRequestWorldSeconds = GetWorld()->GetTimeSeconds();
		return;
	}
	BossPhase = 2;
	bPhaseChangePending = false;
	CompletedPhaseTwoMelee = 0;
	NextAreaAfterMelee = 2;
	LocomotionResumeWorldSeconds = GetWorld()->GetTimeSeconds() + .45;
	OnPhaseChanged.Broadcast(2);
	UE_LOG(LogTemp, Display, TEXT("BossPhase: ascension completed; phase 2 gameplay resumes"));
}
