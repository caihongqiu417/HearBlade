// 检查攻击接触与玩家防御状态的衔接
#include "Combat/Player/PlayerBeatAttackComponent.h"
#include "Combat/Player/PlayerRhythmCombatComponent.h"
#include "Combat/Boss/BeatSyncedComboComponent.h"
#include "Combat/Boss/BossBeatDirector.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Kismet/KismetSystemLibrary.h"

void UPlayerBeatAttackComponent::TickDefenseContactProbe()
{
#if !UE_BUILD_SHIPPING
	if (!Clock() || !Clock()->IsRunning() || !Combat.IsValid() || GetWorld()->GetTimeSeconds() < 5.)
		return;
	auto* PC = Cast<APlayerController>(Cast<APawn>(GetOwner())->GetController());
	if (!PC)
		return;
	auto Key = [&](FKey K, EInputEvent E)
	{
		PC->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0), K, E,
		                                E == IE_Pressed ? 1.f : 0.f, false, FPlatformTime::Cycles64()));
	};
	if (!bProbeStarted)
	{
		bProbeStarted = true;
		ProbeStartWorld = GetWorld()->GetTimeSeconds();
		Combat->ResetCombatState();
		Combat->Health = 1000.f;
		for (TActorIterator<ABossBeatDirector> It(GetWorld()); It; ++It)
			if (It->BehaviorTreeComponent)
				It->BehaviorTreeComponent->StopTree(EBTStopMode::Forced);
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			if (auto* Boss = It->FindComponentByClass<UBeatSyncedComboComponent>())
			{
				ResonanceBoss = Boss;
				Boss->CancelCombo();
				Boss->SetAttackTarget(GetOwner());
				GetOwner()->SetActorLocation(It->GetActorLocation() + It->GetActorForwardVector() * 180.f);
				DefenseProbeAttack = DuplicateObject<UAttackDataAsset>(
				    LoadObject<UAttackDataAsset>(
				        nullptr,
				        TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_DragonKnight_Combo01_Beat")),
				    this);
				DefenseProbeAttack->ResponsePatterns.Reset();
				for (int32 I = 0; I < DefenseProbeAttack->ImpactMarkers.Num(); ++I)
				{
					auto& Marker = DefenseProbeAttack->ImpactMarkers[I];
					Marker.RequiredResponse =
					    I % 2 ? ERhythmResponseType::Counter : ERhythmResponseType::Block;
					Marker.HoldMarker = ERhythmHoldMarkerType::None;
					Marker.Cinematic.bEnabled = false;
					Marker.Damage = 1.f;
				}
				Boss->QueueAttack(DefenseProbeAttack);
				break;
			}
	}
	const double Now = Clock()->GetAudioNowSeconds();
	if (DefenseProbeRelease > 0. && Now >= DefenseProbeRelease)
	{
		Key(EKeys::LeftMouseButton, IE_Released);
		Key(EKeys::RightMouseButton, IE_Released);
		DefenseProbeRelease = -1.;
	}
	if (ResonanceBoss.IsValid())
	{
		TArray<FUpcomingImpactCue> Cues;
		ResonanceBoss->GetUpcomingImpactCues(Cues, 4, true);
		if (Cues.IsEmpty() && DefenseProbeMask != 7 && ResonanceBoss->QueueAttack(DefenseProbeAttack))
		{
			ResonanceBoss->SetAttackTarget(GetOwner());
			// A successful counter knocks the Boss away; restore contact distance
			// for the next independent physical-contact test.
			const auto* BossOwner = ResonanceBoss->GetOwner();
			GetOwner()->SetActorLocation(BossOwner->GetActorLocation() +
			                             BossOwner->GetActorForwardVector() * 180.f);
		}
		for (const auto& Cue : Cues)
		{
			const double Ideal = Cue.ImpactAudioSeconds - Cue.IdealDodgeLeadSeconds;
			if (Now >= Ideal - .015 && Now < Ideal + .06 && ProbeNextInputBeat < Ideal)
			{
				ProbeNextInputBeat = Ideal + .001;
				// Intentionally fail the first strike, then verify both defenses recover.
				const bool bWrongFirst = ProbeStage == 0;
				++ProbeStage;
				const bool bBlock = (Cue.RequiredResponse == ERhythmResponseType::Block) != bWrongFirst;
				Key(bBlock ? EKeys::RightMouseButton : EKeys::LeftMouseButton, IE_Pressed);
				DefenseProbeRelease = Now + .08;
			}
		}
	}
	if (Combat->GetLastJudgmentWorldSeconds() > DefenseProbeLastJudgment)
	{
		DefenseProbeLastJudgment = Combat->GetLastJudgmentWorldSeconds();
		if (Combat->LastJudgment == ERhythmDodgeJudgment::Miss ||
		    Combat->LastJudgment == ERhythmDodgeJudgment::Bad)
			DefenseProbeMask |= 4;
		if (Combat->LastJudgment == ERhythmDodgeJudgment::Perfect ||
		    Combat->LastJudgment == ERhythmDodgeJudgment::Good)
		{
			if (Combat->LastResponse == ERhythmResponseType::Block)
				DefenseProbeMask |= 1;
			if (Combat->LastResponse == ERhythmResponseType::Counter)
				DefenseProbeMask |= 2;
		}
	}
	if (DefenseProbeMask == 7 || GetWorld()->GetTimeSeconds() - ProbeStartWorld > 16.)
	{
		if (DefenseProbeMask == 7 && AttackSerial == 0)
		{
			UE_LOG(
			    LogTemp, Display,
			    TEXT(
			        "DefenseContactProbe: PASS wrong first then RMB Block and LMB Counter defended ordinary=0"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("DefenseContactProbe: FAIL mask=%d ordinary=%llu"), DefenseProbeMask,
			       AttackSerial);
		}
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
	}
#endif
}
