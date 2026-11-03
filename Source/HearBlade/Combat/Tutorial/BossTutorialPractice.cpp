// 按教程阶段安排敌人陪练攻击
#include "Combat/Boss/BeatSyncedComboComponent.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"

// Sparring note for the tutorial: the same trimmed single-hit attack the player
// resonance uses, but with the requested response, gentle damage, a 4-beat
// prompt on the rail, always judged (even at range), landing exactly on TargetBeat.
bool UBeatSyncedComboComponent::QueuePracticeAttack(ERhythmResponseType Response, double TargetBeat,
                                                    AActor* Target)
{
	if (!GetWorld() || !IsValid(Target) || bQueued || bPlaying || BossHealth <= 0.f || bPhaseTransitionActive)
		return false;
	const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	if (!Clock || !Clock->IsRunning())
		return false;
	const UAttackDataAsset* Template = LoadObject<UAttackDataAsset>(
	    nullptr, TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_DragonKnight_Combo01_Beat"));
	if (!Template || Template->ImpactMarkers.IsEmpty())
		return false;
	PracticeAttack = DuplicateObject<UAttackDataAsset>(Template, this);
	PracticeAttack->ImpactMarkers.SetNum(1);
	FAttackImpactMarker& Hit = PracticeAttack->ImpactMarkers[0];
	Hit.RequiredResponse = Response;
	Hit.HoldMarker = ERhythmHoldMarkerType::None;
	Hit.HoldGroup = NAME_None;
	Hit.DamageShape = EBossAttackDamageShape::Blade;
	Hit.Cinematic.bEnabled = false;
	Hit.Damage = 6.f;
	Hit.PromptLeadBeats = 4.f;
	PracticeAttack->ResponsePatterns.Reset();
	PracticeAttack->SegmentPlayRates.Reset();
	PracticeAttack->PlayRate = .8f;
	PracticeAttack->ClipEndSeconds =
	    FMath::Min(Hit.AnimationTimeSeconds + .35f, Template->GetClipEndSeconds());
	PracticeAttack->bWarpToTarget = Template->bWarpToTarget; // lunge to the player like a real attack
	EndBossHitReaction();
	// The behaviour tree normally assigns the target; it is paused while sparring.
	SetAttackTarget(Target);
	if (!QueueAttack(PracticeAttack))
		return false;
	bPracticeNote = true;
	const double Windup = PracticeAttack->GetElapsedAtAnimationTime(GetImpactTime(0)) -
	                      PracticeAttack->GetElapsedAtAnimationTime(PracticeAttack->ClipStartSeconds);
	FirstImpactBeat = FMath::RoundToInt(TargetBeat);
	ScheduledFirstImpactAudioSeconds = Clock->GetBeatTimeSeconds(TargetBeat);
	ScheduledStartAudioSeconds = ScheduledFirstImpactAudioSeconds - Windup;
	UE_LOG(LogTemp, Display, TEXT("Tutorial: practice note response=%d targetBeat=%.1f"), int32(Response),
	       TargetBeat);
	return true;
}
