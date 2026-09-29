#include "Combat/BeatSyncedComboComponent.h"
#include "Combat/BossAscensionComponent.h"
#include "Combat/PlayerRhythmCombatComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"

void UBeatSyncedComboComponent::RecordPhaseTwoAttackOutcome(bool bCompleted)
{
	if(bPlayerResonance) return;
	if(BossPhase!=2 || bPhaseTwoAttackCounted || !AttackData) return;
	const bool bArea=AttackData->ImpactMarkers.ContainsByPredicate([](const FAttackImpactMarker& M){return M.DamageShape==EBossAttackDamageShape::GroundBurst;});
	if(bArea)
	{
		if(!bGroundBurstResolved) return;
		CompletedPhaseTwoMelee=0;NextAreaAfterMelee=FMath::RandRange(2,3);
	}
	else
	{
		if(!bCompleted && (!bPlaying || ActiveDamageImpactIndex!=GetImpactCount()-1)) return;
		++CompletedPhaseTwoMelee;
	}
	bPhaseTwoAttackCounted=true;
	UE_LOG(LogTemp,Display,TEXT("BossPhase: rotation melee=%d required=%d area=%d"),CompletedPhaseTwoMelee,NextAreaAfterMelee,bArea);
}

float UBeatSyncedComboComponent::GetPhaseTransitionProgress() const
{
	const auto* Ascension=GetOwner()?GetOwner()->FindComponentByClass<UBossAscensionComponent>():nullptr;
	return bPhaseTransitionActive && Ascension?Ascension->GetProgress():-1.f;
}

void UBeatSyncedComboComponent::UpdateBossPhase(const UBeatClockSubsystem* Clock)
{
	if(bPlayerResonance) return;
	if(BossHealth<=0.f) {bPhaseChangePending=false;return;}
	if(!bPhaseChangePending || bPhaseTransitionActive || !Clock || !Clock->IsRunning()) return;
	// Give the current and following attack a chance to connect with a real parry.
	// A player who only evades cannot deadlock progression: after 10 seconds, finish
	// the committed combo and perform the full flourish from its recovery.
	if(!bPlaying && !bQueued && GetWorld()->GetTimeSeconds()-PhaseRequestWorldSeconds>10.)
		if(auto* Player=UGameplayStatics::GetPlayerCharacter(this,0))
			TryBeginPhaseAscension(Player,(GetOwner()->GetActorLocation()+Player->GetActorLocation())*.5f+FVector(0,0,55),false);
}

bool UBeatSyncedComboComponent::TryBeginPhaseAscension(AActor* Player,FVector Contact,bool bFromClash)
{
	if(bPlayerResonance) return false;
#if !UE_BUILD_SHIPPING
	if(bFromClash && FParse::Param(FCommandLine::Get(),TEXT("PhaseTransitionNoClashTest"))) return false;
#endif
	if(!bPhaseChangePending || bPhaseTransitionActive || BossPhase!=1 || BossHealth<=0.f) return false;
	auto* Character=Cast<ACharacter>(Player);
	const auto* State=Player?Player->FindComponentByClass<UPlayerRhythmCombatComponent>():nullptr;
	if(!Character || !State || State->Health<=0.f) return false;
	auto* Ascension=GetOwner()->FindComponentByClass<UBossAscensionComponent>();
	if(!Ascension) {Ascension=NewObject<UBossAscensionComponent>(GetOwner());Ascension->RegisterComponent();}
	EndCinematicAccent(true);
	if(!Ascension->BeginAscension(Character,Contact,bFromClash)) return false;
	bPhaseTransitionActive=true;
	CancelCombo(); EndBossHitReaction();
	OnPhaseChanged.Broadcast(0);
	return true;
}

void UBeatSyncedComboComponent::FinishPhaseAscension(bool bCompleted)
{
	bPhaseTransitionActive=false;
	if(!bCompleted)
	{
		bPhaseChangePending=BossHealth>0.f;
		PhaseRequestWorldSeconds=GetWorld()->GetTimeSeconds();
		return;
	}
	BossPhase=2; bPhaseChangePending=false;
	CompletedPhaseTwoMelee=0;NextAreaAfterMelee=2;
	LocomotionResumeWorldSeconds=GetWorld()->GetTimeSeconds()+.45;
	OnPhaseChanged.Broadcast(2);
	UE_LOG(LogTemp,Display,TEXT("BossPhase: ascension completed; phase 2 gameplay resumes"));
}
