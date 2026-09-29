#include "Combat/PlayerBeatAttackComponent.h"
#include "HearBladeCharacter.h"
#include "Combat/TransientCombatFX.h"
#include "Combat/PerfectDodgeFeedbackComponent.h"
#include "Combat/PlayerRhythmCombatComponent.h"
#include "Combat/PlayerCombatAnimInstance.h"
#include "Combat/BeatSyncedComboComponent.h"
#include "Combat/BossCombatCameraComponent.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Particles/ParticleSystemComponent.h"
#include "NiagaraSystem.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundBase.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

UPlayerBeatAttackComponent::UPlayerBeatAttackComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	AttackSet = LoadObject<UPlayerAttackSet>(nullptr, TEXT("/Game/Combat/Player/DA_PlayerAttackSet.DA_PlayerAttackSet"));
	HitSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/SW_RhythmPerfect.SW_RhythmPerfect"));
	LightSwingSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/SW_BossBladeWeak.SW_BossBladeWeak"));
	HeavySwingSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/SW_BossBladeStrong.SW_BossBladeStrong"));
	ContactSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/SOUND/SW_DragonKnight_SwordSlice.SW_DragonKnight_SwordSlice"));
	StartSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/SW_ResonanceCrowd.SW_ResonanceCrowd"));
	HitEffect = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/InsaneAirCombo/Particles/Sparks_P.Sparks_P"));
	TrailEffect = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/InsaneAirCombo/Particles/Trail_P.Trail_P"));
	BeatKickSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/SW_BossDrumKick.SW_BossDrumKick"));
	GoodHitSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/SW_RhythmGood.SW_RhythmGood"));
	// NOTE: SW_ComboTier1Low/2Snare/3High are LOOPING music layers - never fire them as one-shots
	// (each hit left an endless loop running, which starved the voice pool and cut the music).
	SnareSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/SW_BossDrumTick.SW_BossDrumTick"));
	FinisherSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/SW_ComboTierUp.SW_ComboTierUp"));
	FinisherPerfectSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/SW_RhythmPerfect.SW_RhythmPerfect"));
	PerfectBurstEffect = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Combat/Feedback/NS_PerfectDodge_Ring.NS_PerfectDodge_Ring"));
}

void UPlayerBeatAttackComponent::LoadHitRushSounds()
{
	if(LightHitSounds.Num()) return;
	for(int32 I=1;I<=4;++I)
	{
		LightHitSounds.Add(LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/Audio/SFX/HitRush/SW_HitRush_Light%d.SW_HitRush_Light%d"),I,I)));
		HeavyHitSounds.Add(LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/Audio/SFX/HitRush/SW_HitRush_Heavy%d.SW_HitRush_Heavy%d"),I,I)));
	}
	LightHitSounds.RemoveAll([](const TObjectPtr<USoundBase>& S){return !S;});
	HeavyHitSounds.RemoveAll([](const TObjectPtr<USoundBase>& S){return !S;});
	FinisherHitSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/HitRush/SW_HitRush_Finisher.SW_HitRush_Finisher"));
	KickHitSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/HitRush/SW_HitRush_Kick.SW_HitRush_Kick"));
	OffBeatHitSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/HitRush/SW_HitRush_OffBeat.SW_HitRush_OffBeat"));
	ChargeSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/HitRush/SW_HitRush_Charge.SW_HitRush_Charge"));
	// Air whooshes replace the boss blade swings (those rang metallic).
	LightSwingSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/HitRush/SW_HitRush_WhooshLight.SW_HitRush_WhooshLight"));
	HeavySwingSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/HitRush/SW_HitRush_WhooshHeavy.SW_HitRush_WhooshHeavy"));
}

UBeatClockSubsystem* UPlayerBeatAttackComponent::Clock() const { return GetWorld() ? GetWorld()->GetSubsystem<UBeatClockSubsystem>() : nullptr; }

void UPlayerBeatAttackComponent::BeginPlay()
{
	Super::BeginPlay();
	Combat = GetOwner()->FindComponentByClass<UPlayerRhythmCombatComponent>();
	LoadHitRushSounds(); // loaded here (not the constructor) so a fresh import works without restart
	FString Error;
	if (!AttackSet || !AttackSet->Validate(Error)) UE_LOG(LogTemp, Error, TEXT("PlayerAttack: invalid data %s"), *Error);
}

void UPlayerBeatAttackComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	Reset();
	Super::EndPlay(Reason);
}

AActor* UPlayerBeatAttackComponent::FindTarget() const
{
	if (const auto* Camera = GetOwner()->FindComponentByClass<UBossCombatCameraComponent>())
		if (auto* Target = Camera->GetCombatTarget()) return Target;
	AActor* Best = nullptr; float BestDistance = 600.f;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		if (const auto* Boss = It->FindComponentByClass<UBeatSyncedComboComponent>(); Boss && Boss->BossHealth > 0.f)
		{
			const FVector Offset = It->GetActorLocation() - GetOwner()->GetActorLocation();
			if (Offset.Size2D() < BestDistance && FVector::DotProduct(Offset.GetSafeNormal2D(), GetOwner()->GetActorForwardVector()) > .25f)
			{ Best = *It; BestDistance = Offset.Size2D(); }
		}
	return Best;
}

bool UPlayerBeatAttackComponent::TryAttack(EPlayerAttackInput Input)
{
	if (!Combat.IsValid() || Combat->Health <= 0.f || Combat->IsPhaseCinematic() || !Clock() || !Clock()->IsRunning()) return false;
	if (Input==EPlayerAttackInput::Heavy && !bHeavyAttacksEnabled) return false; // Temporarily light-only.
	const double Now = Clock()->GetAudioNowSeconds();
	if (IsAttacking())
	{
		// A press during a swing is kept and snapped onto the next beat cue. The
		// FIRST press is the one judged: mashing can no longer "find" the beat by
		// letting the latest press land near it. Extra presses before the next
		// strike mark the step as mashed (Bad). The combo always continues.
		// A press with no valid branch restarts the route on that same cue.
		if (Now>LatestCueAudio+ChainLateSeconds()) return false;
		++PressCountSinceCommit;
		if (!bHasBufferedInput) { BufferedInput = Input; BufferedInputTime = Now; bHasBufferedInput = true; }
		return true;
	}
	auto QueuePending=[&]()
	{
		++PressCountSinceCommit;
		if(bHasPendingAttack) return true; // keep (and judge) the first press
		PendingAttack=Input; PendingAttackAudio=Now;
		PendingAttackDeadline=GetWorld()->GetTimeSeconds()+1.0; bHasPendingAttack=true; // a root press may wait up to a beat for its beat
		UE_LOG(LogTemp,Display,TEXT("PlayerAttack: queued ordinary input during recovery"));
		return true;
	};
	if(bHasPendingAttack || Now<RecoveryUntilAudio || Now<NextStartAllowedAudio) return QueuePending();
	PressCountSinceCommit=0; // a fresh root press; stale presses belong to nothing
	if (Combat->PrepareForOrdinaryAttack() && StartAttack(Input, Now)) { bHasPendingAttack=false; return true; }
	return QueuePending();
}

double UPlayerBeatAttackComponent::ChainLateSeconds() const
{
	// Two-beat heavy steps get a wider late window: the gap is long, players come in late.
	const auto* Node=AttackSet?AttackSet->Find(CurrentAttack):nullptr;
	return Node && Node->DurationBeats>1.2f ? .26 : PlayerAttackTiming::ComboQueueLateSeconds;
}

double UPlayerBeatAttackComponent::StrikeLead(const FPlayerAttackNode& Node)
{
	return FMath::Clamp(double(Node.ImpactTime-Node.StrikeStart)/1.2,.05,.12);
}

double UPlayerBeatAttackComponent::WindupBeats(const FPlayerAttackNode& Node, bool bResonance)
{
	// Heavy nodes: DurationBeats = beats from the press to the hit, on the half-beat
	// grid. 1.5 = press on a beat, hit on the "and" after the next one (syncopated).
	// Disabled: without an authored charge clip the hold read as a stall / slow motion.
	// Heavies now use the normal contact-on-beat path with a 1-beat cadence.
	static const bool bHeavyCharge=false;
	if(!bHeavyCharge || Node.Input!=EPlayerAttackInput::Heavy) return 0.;
	return FMath::Max(.5,FMath::RoundToDouble(double(Node.DurationBeats)*(bResonance?.5:1.)*2.)*.5);
}

float UPlayerBeatAttackComponent::WindupPosition(double Now, double SampleNow) const
{
	// Not slow motion: the chamber snaps into the raised pose at ~authored speed
	// and decelerates into it, the pose then HOLDS (a few % of creep for tension),
	// and the strike itself is 2x authored speed, accelerating into the contact.
	const auto* Node=AttackSet?AttackSet->Find(CurrentAttack):nullptr;
	if(!Node) return ActiveClipStart;
	const double Start=ActiveClipStart, Hold=HoldPosition, Impact=Node->ImpactTime;
	const double Creep=Hold+(Impact-Hold)*.06;
	auto U=[](double T,double A,double B){ return FMath::Clamp((T-A)/FMath::Max(.001,B-A),0.,1.); };
	if(Now<ChamberStartAudio) return float(Start);
	if(Now<ChamberEndAudio){ const double X=U(Now,ChamberStartAudio,ChamberEndAudio); return float(FMath::Lerp(Start,Hold,1.-FMath::Pow(1.-X,3.))); }
	if(Now<SnapStartAudio){ const double X=U(Now,ChamberEndAudio,SnapStartAudio); return float(FMath::Lerp(Hold,Creep,X*X)); }
	if(Now<ImpactAudio){ const double X=U(Now,SnapStartAudio,ImpactAudio); return float(FMath::Lerp(Creep,Impact,X*X*(.5+.5*X))); }
	const double X=U(SampleNow,ImpactAudio,EndAudio);
	return float(FMath::Lerp(Impact,double(ActiveClipEnd),1.-(1.-X)*(1.-X)));
}

bool UPlayerBeatAttackComponent::StartAttack(EPlayerAttackInput Input, double InputAudio, bool bChained, double TargetImpactAudio)
{
	if (!AttackSet || !Clock()) return false;
	if (!Combat.IsValid() || !Combat->PrepareForOrdinaryAttack()) return false;
	const double Now = Clock()->GetAudioNowSeconds(), SPB = Clock()->GetSecondsPerBeat();
	// Scheduled (cue-targeted) starts may begin one strike lead before the cue.
	if(TargetImpactAudio<0. && Now<NextStartAllowedAudio) return false;
	// A buffered press may be arbitrarily early; it still belongs to the route it
	// was pressed in. Loose presses only chain inside the ordinary cue window.
	const FPlayerAttackNode* Previous = (bChained || PlayerAttackTiming::CanQueue(InputAudio,LastChainAudio,false)) ? AttackSet->Find(ChainNode) : nullptr;
	FName Next = Input == EPlayerAttackInput::Light ? TEXT("L") : TEXT("H");
	if (Previous)
	{
		const FName Branch = Input == EPlayerAttackInput::Light ? Previous->NextLight : Previous->NextHeavy;
		const auto* Candidate = AttackSet->Find(Branch);
		if (Candidate && (!Candidate->bResonanceOnly || IsResonanceActive())) Next = Branch;
		else Previous = nullptr;
	}
	const auto* Node = AttackSet->Find(Next);
	if (!Node || !Node->Animation) return false;
	// What is on screen right now (a heavy chained on the hit starts from this pose).
	const auto* Playing=AttackSet->Find(CurrentAttack);
	const float PlayingPos=LastRootMotionPosition;
	const double PreviousImpact=ImpactAudio;
	const bool bPlayingVisible=Playing && !bAttackVisualEnded;
	const double Windup=WindupBeats(*Node,IsResonanceActive());
	const bool bWindup=Windup>0.;
	auto* Character = Cast<ACharacter>(GetOwner());
	auto* Anim = Character ? Cast<UPlayerCombatAnimInstance>(Character->GetMesh()->GetAnimInstance()) : nullptr;
	if (!Anim || Character->GetCharacterMovement()->IsFalling()) return false;
	// ---- Contact-on-beat timing -------------------------------------------
	// Every attack starts at its strike pose and connects after a short lead, so
	// the blade lands on the beat. The chamber before the strike is played as the
	// tail of the previous slice (see ActiveClipEnd), never after the press.
	const double Beat0=Clock()->GetBeatTimeSeconds(0.);
	double Target=TargetImpactAudio;
	if(Target<0.)
	{
		// Root press snaps to the beat (Hi-Fi Rush): pressing faster never attacks faster.
		//  early (<=150 ms before a beat) -> connect on that beat
		//  late  (<=120 ms after a beat)  -> connect now (that beat, judged late)
		//  otherwise                      -> wait: the swing starts 150 ms before the next beat
		// Timing accuracy only changes the grade / presentation, never the tempo.
		const double RootGrid=(IsResonanceActive()?.5:1.)*SPB;
		const double Nearest=Beat0+FMath::RoundToDouble((Now-Beat0)/RootGrid)*RootGrid;
		if(Nearest-Now>=.033 && Nearest-Now<=.15) Target=Nearest;
		else if(Now-Nearest>=-.033 && Now-Nearest<=.12) Target=Now+.033;
		else
		{
			Target=Beat0+FMath::CeilToDouble((Now-Beat0)/RootGrid)*RootGrid;
			if(!bWindup && Target-Now>.15) return false; // not yet: the caller keeps the press pending
		}
		// Heavy: the press beat starts the charge; the hit lands Windup beats later.
		if(bWindup) Target=Beat0+FMath::RoundToDouble((Now-Beat0)/SPB)*SPB+Windup*SPB;
	}
	ImpactAudio=FMath::Max(Target,Now+.033);
	const double StartupSeconds=ImpactAudio-Now;
	const float StrikeFrom=FMath::Clamp(Node->StrikeStart,Node->ClipStart,Node->ImpactTime-.01f);
	// A late press shortens the lead; skip into the strike rather than playing it >1.5x.
	const float PreviousClipEnd=ActiveClipEnd;
	ActiveClipStart=FMath::Min(FMath::Max(StrikeFrom,float(Node->ImpactTime-StartupSeconds*1.5)),Node->ImpactTime-.005f);
	bool bContinuesPose=false;
	if(bWindup)
	{
		// Chained on the previous hit in the same clip: charge from the pose we are in.
		bContinuesPose=bPlayingVisible && Playing->Animation==Node->Animation &&
			PlayingPos>=Node->ClipStart-.4f && PlayingPos<Node->StrikeStart;
		ActiveClipStart=bContinuesPose?PlayingPos:Node->ClipStart;
	}
	// Which step most likely follows: the branch that continues the same clip
	// (H->HH, L->LL...), else the same button's branch. Using only NextLight made
	// every heavy stop dead after its hit, so HHHH read as separate attacks.
	auto Usable=[this](FName Id)->const FPlayerAttackNode*{ const auto* N=AttackSet->Find(Id); return N && (!N->bResonanceOnly || IsResonanceActive())?N:nullptr; };
	// A charged heavy plays its own chamber, so nothing flows into it (or out of one).
	auto Flows=[this,Node,bWindup](const FPlayerAttackNode* N){ return !bWindup && N && WindupBeats(*N,IsResonanceActive())<=0. && N->Animation==Node->Animation && FMath::IsNearlyEqual(N->ClipStart,Node->ClipEnd,.02f); };
	const FPlayerAttackNode* SameInput=Usable(Node->Input==EPlayerAttackInput::Heavy?Node->NextHeavy:Node->NextLight);
	const FPlayerAttackNode* OtherInput=Usable(Node->Input==EPlayerAttackInput::Heavy?Node->NextLight:Node->NextHeavy);
	const FPlayerAttackNode* NextGuess=Flows(SameInput)?SameInput:Flows(OtherInput)?OtherInput:SameInput?SameInput:OtherInput;
	const bool bFlowsOn=Flows(NextGuess);
	ActiveClipEnd=bFlowsOn ? FMath::Clamp(NextGuess->StrikeStart,Node->ClipEnd,NextGuess->ImpactTime-.01f) : Node->ClipEnd;
	const bool bSeamless=bContinuesPose || (Previous && Previous->Animation==Node->Animation && FMath::IsNearlyEqual(PreviousClipEnd,ActiveClipStart,.02f));
	AttackMontage = Anim->BeginBeatAttack(Node->Animation, ActiveClipStart, bSeamless ? .03f : .07f);
	if (!AttackMontage) return false;
	CurrentAttack = Next; ++AttackSerial; HitActors.Reset(); bImpactPresented = bRewarded = false;
	HitStopSeconds = 0.f; HitStopStartAudio = -1000.;
	LastRootMotionPosition=ActiveClipStart;
	Character->GetCharacterMovement()->StopMovementImmediately();
	bSwingPresented = false; bAttackVisualEnded = false;
	StartAudio = Now; LastAudio = Now; bHasBufferedInput = false;
	// Light follow-up after a charged heavy: the beat after its hit.
	// Lights: DurationBeats = beats from this hit to the next (half-beat grid, >= .5).
	// Fixed tempo: every step lands one beat after the previous hit (half a beat in
	// resonance), however fast the button is pressed. (DurationBeats' half-beat
	// variations made mashing faster than playing on the beat.)
	const double Cadence=bWindup?1.:(IsResonanceActive()?.5:1.);
	// Fractional cadences (1.5-beat heavies) land on the half-beat grid (the "and").
	const double CueGrid=FMath::IsNearlyEqual(FMath::Frac(Cadence),0.,.01)?1.:.5;
	const double Grid=bWindup?1.:Cadence;
	AttackPlayRate=1.f;
	SwingStartAudio=Now+StartupSeconds*.55;
	bWindupAttack=bWindup; NextChargeCueAudio=-1.;
	if(bWindup)
	{
		HoldPosition=FMath::Clamp(Node->StrikeStart,ActiveClipStart,Node->ImpactTime-.01f);
		// Chained on a hit: keep that contact frozen for a moment (the hit-stop).
		double Pre=bPlayingVisible && Now-PreviousImpact<.05 ? .06 : 0.;
		double Chamber=HoldPosition>ActiveClipStart ? FMath::Max(.06,double(HoldPosition-ActiveClipStart)/1.15) : 0.;
		double Snap=FMath::Clamp(double(Node->ImpactTime-HoldPosition)/2.2,.035,.08);
		const double Budget=ImpactAudio-Now;
		if(Pre+Chamber+Snap>Budget){ const double K=Budget/(Pre+Chamber+Snap); Pre*=K; Chamber*=K; Snap*=K; } // late press
		ChamberStartAudio=Now+Pre; ChamberEndAudio=ChamberStartAudio+Chamber; SnapStartAudio=ImpactAudio-Snap;
		SwingStartAudio=SnapStartAudio; // keep turning toward the target through the charge
		// Charge cue on every beat between the press and the hit (Hi-Fi Rush's "clap").
		NextChargeCueAudio=Beat0+FMath::CeilToDouble((Now+.1-Beat0)/SPB)*SPB;
		if(NextChargeCueAudio>=ImpactAudio-.1) NextChargeCueAudio=-1.;
	}
	const double StrikeDuration=ImpactAudio-SwingStartAudio;
	// Next contact beat: one cadence after this contact. This slice's recovery
	// (plus the next chamber) runs until the next strike must begin.
	const double NextLead=NextGuess?StrikeLead(*NextGuess):0.;
	double CueBeat=FMath::RoundToDouble(((ImpactAudio-Beat0)/SPB+Cadence)/CueGrid)*CueGrid;
	// After a charged heavy the light follow-up is half a beat after its hit.
	if(bWindup) CueBeat=FMath::CeilToDouble(((ImpactAudio-Beat0)/SPB+.5-.02)*2.)*.5;
	while(Beat0+CueBeat*SPB-NextLead<ImpactAudio+.12) CueBeat+=CueGrid;
	ComboInputAudio=Beat0+CueBeat*SPB;
	// One cue only: the next hit always lands on it (an early press waits for it,
	// a slightly late one fires at once). No later slots, so the tempo is fixed.
	LatestCueAudio=ComboInputAudio;
	LinkAudio=ComboInputAudio;
	// The follow-through is paced for an on-the-beat next press; a faster
	// (half-beat) press simply cuts in earlier.
	EndAudio=LatestCueAudio-NextLead;
	// A slice that does not flow into another strike (finisher / end of route)
	// plays its own short recovery at ~authored speed instead of being stretched
	// across the whole beat, then eases back to locomotion. The chain window
	// still stays open until the cue.
	if(!bFlowsOn)
		EndAudio=FMath::Min(EndAudio,ImpactAudio+FMath::Max(.2,double(ActiveClipEnd-Node->ImpactTime)+.1));
	NextStartAllowedAudio=EndAudio-.025; // One-frame scheduling tolerance, not a speed-up.
	RecoveryUntilAudio=EndAudio+(Node->bFinisher?.06:0.);
	// A route's last hit (finisher) keeps playing its follow-through, but a new
	// combo may cut into it shortly after contact instead of waiting it out.
	if(!bFlowsOn)
	{
		NextStartAllowedAudio=FMath::Min(NextStartAllowedAudio,ImpactAudio+.22);
		RecoveryUntilAudio=FMath::Min(RecoveryUntilAudio,ImpactAudio+.22);
	}
	// Heavy presses come at least a beat apart: the next heavy is pressed one beat
	// (or Windup beats, if longer) after this one's press; else on the light cue.
	HeavyPressAudio=bWindup?ImpactAudio+(FMath::Max(1.,Windup)-Windup)*SPB:ComboInputAudio;
	// Keep the follow-through moving until that press instead of dropping to
	// locomotion for a few frames in between (that gap read as a stall).
	if(bWindup && (Usable(Node->NextHeavy) || Usable(Node->NextLight)))
		EndAudio=FMath::Max(EndAudio,FMath::Min(ComboInputAudio,HeavyPressAudio)+.04);
	const double InputBeat = (InputAudio - Clock()->GetBeatTimeSeconds(0.))/SPB;
	const double IdealInput = Previous ? LastChainAudio : Clock()->GetBeatTimeSeconds(FMath::RoundToDouble(InputBeat/Grid)*Grid);
	LastErrorMs = (InputAudio - IdealInput)*1000.; LastGrade = PlayerAttackTiming::Judge(LastErrorMs/1000.);
	if (PressCountSinceCommit>1) LastGrade = EPlayerAttackGrade::Bad; // mashed
	UE_LOG(LogTemp,Display,TEXT("PlayerAttack: judged presses=%d errorMs=%.1f grade=%d"),PressCountSinceCommit,LastErrorMs,int(LastGrade));
	PressCountSinceCommit=0;
	LastJudgedWorldSeconds = GetWorld()->GetTimeSeconds();
	if (!Previous) InputSequence.Empty();
	InputSequence += Input == EPlayerAttackInput::Light ? TEXT("L ") : TEXT("H ");
	if (LastGrade == EPlayerAttackGrade::Bad) Combat->ComboCount = 0;
	OnAttackJudged.Broadcast(LastGrade, LastErrorMs, Next);
	AttackTarget = FindTarget();
	if (AttackTarget.IsValid())
	{
		const FRotator Face=(AttackTarget->GetActorLocation()-Character->GetActorLocation()).GetSafeNormal2D().Rotation();
		if (FMath::Abs(FRotator::NormalizeAxis(Face.Yaw-Character->GetActorRotation().Yaw))<75.f) Character->SetActorRotation(FRotator(0.f,Face.Yaw,0.f));
	}
	if (Trail) { Trail->DeactivateSystem(); Trail = nullptr; }
	if (TrailEffect) Trail = UGameplayStatics::SpawnEmitterAttached(TrailEffect, Character->GetMesh(), TEXT("hand_r"), FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
	if (auto* Sword = GetOwner()->FindComponentByClass<UStaticMeshComponent>())
	{ LastBladeBase = Sword->GetComponentLocation(); LastBladeTip = LastBladeBase + Sword->GetComponentTransform().TransformVectorNoScale(FVector::YAxisVector)*150.f; }
	SampleLegs(LastLegA,LastLegB);
	UE_LOG(LogTemp, Display, TEXT("PlayerAttack: start id=%llu node=%s grade=%d errorMs=%.2f impact=%.6f end=%.6f leadMs=%.1f strikeMs=%.1f link=%.6f"), AttackSerial, *Next.ToString(), int(LastGrade), LastErrorMs, ImpactAudio, EndAudio,(SwingStartAudio-Now)*1000.,StrikeDuration*1000.,LinkAudio);
	return true;
}

float UPlayerBeatAttackComponent::GetMovementScale() const
{
	const auto* Node = AttackSet ? AttackSet->Find(CurrentAttack) : nullptr;
	if(bAttackVisualEnded) return 1.f;
	return Node ? 0.f : 1.f; // Authored root motion owns attack travel, not movement input.
}

FString UPlayerBeatAttackComponent::GetComboPrompt() const
{
	if(!Clock() || !AttackSet) return FString();
	const double Now=Clock()->GetAudioNowSeconds();
	const auto* Node=AttackSet->Find(CurrentAttack);
	if(!Node) return Now<RecoveryUntilAudio?TEXT("RECOVERY"):(bHeavyAttacksEnabled?TEXT("L L L L   |   H H H H   |   L H   |   H L L H"):TEXT("L L L L"));
	FString Choices;
	for(bool Heavy:{false,true})
	{
		if(Heavy && !bHeavyAttacksEnabled) continue;
		const auto* Branch=AttackSet->Find(Heavy?Node->NextHeavy:Node->NextLight);
		if(Branch && (!Branch->bResonanceOnly || IsResonanceActive())) Choices+=Heavy?TEXT("RMB "):TEXT("LMB ");
	}
	if(bHasBufferedInput) return InputSequence+TEXT(" > ")+(BufferedInput==EPlayerAttackInput::Light?TEXT("L  QUEUED"):TEXT("H  QUEUED"));
	if(Choices.IsEmpty()) return InputSequence+TEXT("  FINISHER");
	if(Now>LatestCueAudio+ChainLateSeconds()) return InputSequence+TEXT("  CHAIN CLOSED");
	return InputSequence+(Now>=FMath::Max(ImpactAudio,ComboInputAudio-PlayerAttackTiming::ComboQueueEarlySeconds)?TEXT("  LINK NOW: "):TEXT("  NEXT BEAT: "))+Choices;
}

bool UPlayerBeatAttackComponent::CancelForDefense(bool bDodge)
{
	bHasPendingAttack=false;
	if (!IsAttacking()) return true;
	if (Combat.IsValid() && Combat->IsMouseDefenseWindow()) { CancelAttack(); return true; }
	const auto* Node = AttackSet->Find(CurrentAttack);
	// A charging heavy can always be dodged out of (Hi-Fi Rush lets you bail).
	if (Clock()->GetAudioNowSeconds() < ImpactAudio && (!bDodge || (Node->Input == EPlayerAttackInput::Heavy && !bWindupAttack))) return false;
	CancelAttack(); return true;
}

void UPlayerBeatAttackComponent::CancelAttack()
{
	if(IsAttacking()) UE_LOG(LogTemp,Display,TEXT("PlayerAttack: end id=%llu node=%s audio=%.3f expectedEnd=%.3f"),AttackSerial,*CurrentAttack.ToString(),Clock()?Clock()->GetAudioNowSeconds():0.,EndAudio);
	// Once the visual has started its ease-out, do not restart the blend with a
	// shorter time; that re-introduced the snap to locomotion.
	if (auto* C = Cast<ACharacter>(GetOwner()); C && !bAttackVisualEnded)
		if (auto* Anim = Cast<UPlayerCombatAnimInstance>(C->GetMesh()->GetAnimInstance())) Anim->EndBeatAttack(AttackMontage);
	AttackMontage=nullptr; CurrentAttack=NAME_None; ChainNode=NAME_None; bHasBufferedInput=false; PressCountSinceCommit=0; bWindupAttack=false; NextChargeCueAudio=-1.;
	if (Trail) { Trail->DeactivateSystem(); Trail=nullptr; }
}

void UPlayerBeatAttackComponent::NotifyInterrupted() { bHasPendingAttack=false; CancelAttack(); InputSequence.Empty(); }
void UPlayerBeatAttackComponent::Reset() { bHasPendingAttack=false; CancelAttack(); LastCommittedImpactBeat=-1000.; NextStartAllowedAudio=-1000.; RecoveryUntilAudio=-1000.; InputSequence.Empty(); EndResonance(); SetResonanceState(EPlayerResonanceState::Filling); }

void UPlayerBeatAttackComponent::TickAttack(double Now, float Delta)
{
	const auto* Node = AttackSet->Find(CurrentAttack);
	if (!Node) { CancelAttack(); return; }
	if (!bSwingPresented && Now >= ImpactAudio - .07)
	{
		bSwingPresented = true;
		USoundBase* Swing = Node->Input == EPlayerAttackInput::Heavy ? HeavySwingSound : LightSwingSound;
		if (Swing) UGameplayStatics::PlaySoundAtLocation(this, Swing, GetOwner()->GetActorLocation(), .55f, Node->bFinisher ? .95f : 1.05f);
	}
	if (bWindupAttack && NextChargeCueAudio>0. && Now>=NextChargeCueAudio)
	{
		// Charge beat: a clap + a spark at the sword hand, rising toward the hit.
		const double SPB=Clock()->GetSecondsPerBeat();
		const bool bLast=NextChargeCueAudio+SPB>=ImpactAudio-.1;
		if(ChargeSound) UGameplayStatics::PlaySound2D(this,ChargeSound,bLast?.9f:.7f,bLast?1.12f:1.f);
		if(auto* C=Cast<ACharacter>(GetOwner()))
			if(UParticleSystem* Spark=LoadObject<UParticleSystem>(nullptr,TEXT("/Game/SwordAnim/demo/FX/P_Spark.P_Spark")))
				RetireCombatFX(UGameplayStatics::SpawnEmitterAtLocation(GetWorld(),Spark,C->GetMesh()->GetSocketLocation(TEXT("hand_r")),C->GetActorRotation(),FVector(bLast?.45f:.3f),true),.03f,.12f);
		NextChargeCueAudio=bLast?-1.:NextChargeCueAudio+SPB;
	}
	if (auto* C = Cast<ACharacter>(GetOwner()))
	{
		if (auto* Anim = C->GetMesh()->GetAnimInstance(); Anim && !bAttackVisualEnded)
		{
			// Absolute musical time prevents frame accumulation and stale timer damage.
			// Soft hit-stop: at contact the pose stops dead, then the recovery
			// eases back in along u^k (zero speed at contact, no second jolt when
			// the freeze ends) and still reaches the next cue on time. A hard
			// freeze + sudden fast-forward is what read as stutter.
			// Never slow motion: the follow-through plays at >= authored speed (plus the
			// hit-stop). If the beat leaves spare time, the pose waits in the next
			// strike's chamber (a short anticipation) instead of being stretched.
			// With spare time (heavies on a full beat): freeze for the hit-stop, then
			// the follow-through runs at ~authored speed and DECELERATES into the next
			// chamber (zero speed on arrival), so the short wait there reads as the
			// wind-up of the next strike, not a stop. No spare time: original curve.
			const double Natural=FMath::Max(0.,double(ActiveClipEnd-Node->ImpactTime));
			const double FreezeEnd=HitStopSeconds>0.f?FMath::Max(ImpactAudio,HitStopStartAudio+double(HitStopSeconds)):ImpactAudio;
			// (The "decelerate into the chamber and wait there" variant is off: the
			// wait read as the recovery freezing. Recovery always spans to EndAudio.)
			const bool bSpare=false;
			const double VisualEnd=bSpare?FreezeEnd+Natural*1.3:EndAudio;
			double SampleNow=Now;
			if(HitStopSeconds>0.f && Now>=HitStopStartAudio && VisualEnd>HitStopStartAudio+.02)
			{
				// Short hard freeze, then the pose snaps back into motion (ease-OUT) and
				// glides into the next chamber. The old u^2 ease-in kept the pose crawling
				// near the contact for ~40% of the beat, which read as a stall between hits.
				const double Span=VisualEnd-HitStopStartAudio;
				const double Freeze=FMath::Min(double(HitStopSeconds)*.6,Span*.25);
				if(Now<HitStopStartAudio+Freeze) SampleNow=HitStopStartAudio;
				else
				{
					const double V=FMath::Clamp((Now-HitStopStartAudio-Freeze)/FMath::Max(.001,Span-Freeze),0.,1.);
					SampleNow=HitStopStartAudio+Span*(1.-FMath::Pow(1.-V,1.4));
				}
			}
			FPlayerAttackNode Active=*Node; Active.ClipStart=ActiveClipStart; Active.ClipEnd=ActiveClipEnd;
			float Position;
			if(bWindupAttack) Position=WindupPosition(Now,SampleNow);
			else if(bSpare && Now>=ImpactAudio)
			{
				const double U=FMath::Clamp((Now-FreezeEnd)/FMath::Max(.001,VisualEnd-FreezeEnd),0.,1.);
				Position=float(FMath::Lerp(double(Node->ImpactTime),double(ActiveClipEnd),1.-(1.-U)*(1.-U)));
			}
			else Position=PlayerAttackTiming::ContinuousComboPosition(bSpare?Now:SampleNow,StartAudio,ImpactAudio,VisualEnd,Active);
			if(Position>LastRootMotionPosition && Node->Animation)
			{
				// The montage is positioned explicitly, so engine montage advancement
				// cannot extract this delta. Consume it once on the collision capsule.
				const FTransform LocalRoot=Node->Animation->ExtractRootMotionFromRange(LastRootMotionPosition,Position,FAnimExtractContext(Position,true));
				const FTransform WorldRoot=C->GetMesh()->ConvertLocalRootMotionToWorld(LocalRoot);
				FVector Travel=WorldRoot.GetTranslation(); Travel.Z=0.; // Grounded combo: gravity/floor stay owned by CharacterMovement.
				const FQuat Facing=FRotator(0.f,WorldRoot.GetRotation().Rotator().Yaw,0.f).Quaternion()*C->GetActorQuat();
				FHitResult Hit;
				C->GetCharacterMovement()->SafeMoveUpdatedComponent(Travel,Facing,true,Hit);
				LastRootMotionPosition=Position; // Collision never banks displacement for a later teleport.
			}
			Anim->Montage_SetPosition(AttackMontage,Position);
			C->GetMesh()->TickAnimation(0.f, false); C->GetMesh()->RefreshBoneTransforms();
			if(auto* Player=Cast<AHearBladeCharacter>(C)) Player->SyncAttackWeapon();
		}
		if(Now<SwingStartAudio && AttackTarget.IsValid())
			C->SetActorRotation(FMath::RInterpTo(C->GetActorRotation(),(AttackTarget->GetActorLocation()-C->GetActorLocation()).GetSafeNormal2D().Rotation(),Delta,12.f));
	}
	const uint64 SerialBeforeSweep=AttackSerial;
	SweepBlade(Now);
	if(!IsAttacking() || AttackSerial!=SerialBeforeSweep) return;
	if (Trail && Now>=ImpactAudio+.08) { Trail->DeactivateSystem(); Trail=nullptr; }
	if (!bImpactPresented && Now>=ImpactAudio)
	{
		bImpactPresented=true; PresentImpact(false, GetOwner()->GetActorLocation());
		UE_LOG(LogTemp, Display, TEXT("PlayerAttack: impact id=%llu errorMs=%.2f"), AttackSerial, (Now-ImpactAudio)*1000.);
#if !UE_BUILD_SHIPPING
		if(AttackSerial==8 && FParse::Param(FCommandLine::Get(),TEXT("PlayerAttackProbe")))
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/PlayerAttack_Corrected.png"),false,false);
#endif
	}
	LastAudio=Now;
	// Finish the authored follow-through before consuming the next buffered
	// input. Cutting at impact discarded the swing and snapped between poses.
	// A buffered press fires its strike one lead before the cue, so the blade
	// connects exactly on the beat. Presses after that fire at once (~1 frame).
	const auto* Branch=bHasBufferedInput?AttackSet->Find(BufferedInput==EPlayerAttackInput::Light?Node->NextLight:Node->NextHeavy):nullptr;
	const bool bCanChain=Branch && (!Branch->bResonanceOnly || IsResonanceActive());
	const auto* Upcoming=bCanChain?Branch:(bHasBufferedInput?AttackSet->Find(BufferedInput==EPlayerAttackInput::Light?TEXT("L"):TEXT("H")):nullptr);
	// A heavy follow-up starts charging on its press cue (the heavy hit itself for
	// heavy->heavy); its strike connects Windup beats later.
	const double UpWindup=Upcoming?WindupBeats(*Upcoming,IsResonanceActive()):0.;
	// Early presses wait for the cue; the strike starts one lead before it.
	const double Slot=ComboInputAudio; // fixed tempo: the next hit is always on the next cue
	const double FireAt=UpWindup>0.?HeavyPressAudio:(Upcoming?Slot-StrikeLead(*Upcoming):0.);
	if (bHasBufferedInput && Upcoming && Now>=FireAt)
	{
		const auto Input=BufferedInput; const double Timestamp=BufferedInputTime;
		const FName Finished=CurrentAttack;
		const double FinishedAt=UpWindup>0.?HeavyPressAudio:Slot;
		const double TargetAudio=UpWindup>0.?HeavyPressAudio+UpWindup*Clock()->GetSecondsPerBeat():Slot;
		ChainNode=Finished; LastChainAudio=FinishedAt;
		if (bCanChain && StartAttack(Input,Timestamp,true,TargetAudio)) return;
		// No route continues with this button (finisher, resonance-only branch):
		// start a fresh root on the same cue instead of swallowing the press.
		UAnimMontage* OldMontage=AttackMontage.Get();
		CurrentAttack=NAME_None; ChainNode=NAME_None; bHasBufferedInput=false; InputSequence.Empty();
		if (StartAttack(Input,Timestamp,false,TargetAudio)) return;
		AttackMontage=OldMontage; CurrentAttack=Finished;
		CancelAttack();
		return;
	}
	if(Now>=EndAudio)
	{
		const auto* Light=AttackSet->Find(Node->NextLight); const auto* Heavy=AttackSet->Find(Node->NextHeavy);
		const bool CanLink=(Light && (!Light->bResonanceOnly || IsResonanceActive())) || (Heavy && (!Heavy->bResonanceOnly || IsResonanceActive()));
		// The combo stopped here. Ease out of the last sampled pose instead of
		// snapping back to locomotion in a couple of frames. While the chain window
		// is still open, keep holding the pose: a slightly late press used to land
		// after the blend-out began, so the next swing popped back in from idle.
		// No pose hold while waiting for a late press: holding the chamber pose read
		// as the recovery freezing. Blend out now; a late press still chains.
		const bool bHoldForLink=false;
		if(!bAttackVisualEnded && !bHoldForLink)
		{
			if(auto* C=Cast<ACharacter>(GetOwner()))
				if(auto* Anim=Cast<UPlayerCombatAnimInstance>(C->GetMesh()->GetAnimInstance())) Anim->EndBeatAttack(AttackMontage,.28f);
			bAttackVisualEnded=true;
		}
		// Keep a press buffered during a finisher alive: it restarts the route on the cue.
		if((!CanLink && !bHasBufferedInput) || Now>LatestCueAudio+ChainLateSeconds()) CancelAttack();
	}
}

void UPlayerBeatAttackComponent::RegisterOpeningHit(const FBossOpening& Opening, bool bGap)
{
	OpeningHitMarks.Add({ImpactAudio,bGap});
	if(OpeningHitMarks.Num()>24) OpeningHitMarks.RemoveAt(0);
	LastOpeningHitWorldTime=GetWorld()?GetWorld()->GetTimeSeconds():-1000.; bLastOpeningHitGap=bGap;
	if(!FMath::IsNearlyEqual(OpeningKey,Opening.StartAudio,.01)) { OpeningKey=Opening.StartAudio; OpeningHits=0; bOpeningFilled=false; }
	++OpeningHits;
	// FILL: about one hit per beat of the opening (at least two).
	const double SPB=Clock()?Clock()->GetSecondsPerBeat():.5;
	const int32 Needed=FMath::Max(2,FMath::FloorToInt((Opening.EndAudio-Opening.StartAudio)/SPB)+1);
	if(!bOpeningFilled && OpeningHits>=Needed)
	{
		bOpeningFilled=true; FillWorldTime=LastOpeningHitWorldTime; FillAudio=ImpactAudio; bFillGap=bGap;
		ReceiveReward(EPlayerAttackGrade::Perfect,bGap?8.f:5.f,true);
		if(FinisherSound) UGameplayStatics::PlaySound2D(this,FinisherSound,.75f,bGap?1.1f:1.f);
		UE_LOG(LogTemp,Display,TEXT("PlayerAttack: FILL gap=%d hits=%d"),bGap?1:0,OpeningHits);
	}
}

void UPlayerBeatAttackComponent::SampleLegs(FVector OutA[2], FVector OutB[2]) const
{
	const auto* C=Cast<ACharacter>(GetOwner());
	const USkeletalMeshComponent* Mesh=C?C->GetMesh():nullptr;
	for(int32 I=0;I<2;++I)
	{
		OutA[I]=Mesh?Mesh->GetSocketLocation(I?TEXT("calf_r"):TEXT("calf_l")):FVector::ZeroVector;
		OutB[I]=Mesh?Mesh->GetSocketLocation(I?TEXT("ball_r"):TEXT("ball_l")):FVector::ZeroVector;
	}
}

void UPlayerBeatAttackComponent::SweepBlade(double Now)
{
	const auto* Node = AttackSet->Find(CurrentAttack);
	auto* Sword = GetOwner()->FindComponentByClass<UStaticMeshComponent>();
	if (!Sword || !Node) return;
	// Match the animated hand, including the grip correction owned by Character.
	const FVector Base=Sword->GetComponentLocation();
	const FVector Tip=Base+Sword->GetComponentTransform().TransformVectorNoScale(FVector::YAxisVector)*150.f;
	if (Now>=ImpactAudio-.04 && LastAudio<=ImpactAudio+.04)
	{
		FCollisionQueryParams Query(SCENE_QUERY_STAT(PlayerBlade), false, GetOwner());
		// The Boss capsule is not required to block Visibility. Intersect the swept
		// weapon with its actual hurt capsule, then separately test world occlusion.
		TArray<FHitResult> Contacts;
		if(AActor* Target=AttackTarget.Get(); Target && !HitActors.Contains(Target))
		{
			if(auto* Capsule=Target->FindComponentByClass<UCapsuleComponent>())
			{
				const FVector Center=Capsule->GetComponentLocation();
				const float Radius=Capsule->GetScaledCapsuleRadius();
				const FVector HalfAxis=FVector::UpVector*FMath::Max(0.f,Capsule->GetScaledCapsuleHalfHeight()-Radius);
				float Closest=MAX_flt; FVector Contact=Center;
				// Swept segments: the blade always; both shins for a kick.
				FVector LegA[2],LegB[2]; SampleLegs(LegA,LegB);
				struct FSweep{FVector A0,B0,A1,B1;};
				TArray<FSweep,TInlineAllocator<3>> Sweeps; Sweeps.Add({LastBladeBase,LastBladeTip,Base,Tip});
				if(Node->bKick) for(int32 L=0;L<2;++L) Sweeps.Add({LastLegA[L],LastLegB[L],LegA[L],LegB[L]});
				for(const FSweep& S:Sweeps)
					for(int32 I=0;I<=10;++I)
					{
						FVector OnBlade,OnBody;
						FMath::SegmentDistToSegmentSafe(FMath::Lerp(S.A0,S.B0,I/10.f),FMath::Lerp(S.A1,S.B1,I/10.f),Center-HalfAxis,Center+HalfAxis,OnBlade,OnBody);
						const float Distance=FVector::Distance(OnBlade,OnBody);
						if(Distance<Closest){Closest=Distance;Contact=OnBlade;}
					}
				Query.AddIgnoredActor(Target); FHitResult Wall;
				if(FParse::Param(FCommandLine::Get(),TEXT("PlayerAttackProbe")) && AttackSerial<=3)
				{
					UE_LOG(LogTemp,Display,TEXT("PlayerAttackProbe: blade id=%llu node=%s closest=%.1f radius=%.1f base=%s tip=%s target=%s"),AttackSerial,*CurrentAttack.ToString(),Closest,Radius,*Base.ToCompactString(),*Tip.ToCompactString(),*Center.ToCompactString());
				}
				const bool Blocked=GetWorld()->LineTraceSingleByChannel(Wall,GetOwner()->GetActorLocation(),Center,ECC_Visibility,Query);
				if(Closest<=Radius+16.f && !Blocked)
				{ FHitResult Hit(Target,Capsule,Contact,FVector::UpVector);Hit.ImpactPoint=Contact;Contacts.Add(Hit); }
			}
		}
		for(const auto& Hit:Contacts)
			{
				AActor* Target=Hit.GetActor(); if(!Target || HitActors.Contains(Target)) continue;
				auto* Boss=Target->FindComponentByClass<UBeatSyncedComboComponent>();
				if(!Boss || Boss->BossHealth<=0 || Boss->IsPhaseTransitionActive()) continue;
				HitActors.Add(Target);
				// Openings: judged at the contact beat, before this hit can stagger the boss.
				FBossOpening Opening;
				const bool bOpen=Boss->GetOpeningAt(ImpactAudio,Opening);
				const bool bGap=bOpen && Opening.Kind==EBossOpeningKind::Gap;
				const float OpenMul=bGap?2.f:bOpen?1.2f:1.f;
				const float Damage=Node->Damage*OpenMul;
				// HHH is the rising slash: it launches the boss (visual lift + stagger).
				const bool bLauncher=Node->Input==EPlayerAttackInput::Heavy && Node->Id==FName(TEXT("HHH"));
				Boss->ReceivePlayerStrike(Damage,Node->PoiseDamage*(bGap?1.5f:1.f),GetOwner(),Node->Input==EPlayerAttackInput::Heavy || Node->bFinisher,Hit.ImpactPoint,bLauncher);
				if (!bRewarded)
				{
					bRewarded=true;
					float Gain=Node->Input==EPlayerAttackInput::Heavy?6.f:4.f;
					if(LastGrade==EPlayerAttackGrade::Good) Gain*=.5f;
					if(Node->bFinisher && LastGrade==EPlayerAttackGrade::Perfect) Gain+=4.f;
					ReceiveReward(LastGrade,Gain*OpenMul,true);
				}
				if(bOpen) RegisterOpeningHit(Opening,bGap);
				PresentImpact(true,Hit.ImpactPoint,Target); OnAttackHit.Broadcast(Target,Damage,LastGrade);
				UE_LOG(LogTemp,Display,TEXT("PlayerAttack: HIT id=%llu target=%s damage=%.1f resonance=%.1f"),AttackSerial,*Target->GetName(),Damage,Combat->Resonance);
			}
	}
	LastBladeBase=Base; LastBladeTip=Tip;
	SampleLegs(LastLegA,LastLegB);
}

void UPlayerBeatAttackComponent::ReceiveReward(EPlayerAttackGrade Grade,float Amount,bool bHit)
{
	if (!Combat.IsValid() || !bHit || Grade==EPlayerAttackGrade::Bad) return;
	const float Previous=Combat->Resonance;
	Combat->AwardPlayerAttack(Grade==EPlayerAttackGrade::Perfect, IsResonanceActive()?0.f:Amount);
	LastResonanceGain=Combat->Resonance-Previous;LastHitWorldSeconds=GetWorld()->GetTimeSeconds();
	Combat->LastJudgmentErrorMs=LastErrorMs;
}

void UPlayerBeatAttackComponent::PresentImpact(bool bHit,FVector Location,AActor* Target)
{
	if(!bHit) return;
	const auto* Node=AttackSet->Find(CurrentAttack);
	const bool bHeavy=Node && Node->Input==EPlayerAttackInput::Heavy;
	const bool bFinisher=Node && Node->bFinisher;
	const bool bKick=Node && Node->bKick;
	const bool bLauncher=bHeavy && Node->Id==FName(TEXT("HHH"));
	const bool bPerfect=LastGrade==EPlayerAttackGrade::Perfect, bGood=LastGrade==EPlayerAttackGrade::Good;
	// Dead-as-Disco style: timing scales everything. Off-beat hits still land and
	// still read, on-beat hits hit much harder, finishers hardest.
	const float Grade=bPerfect?1.f:bGood?.7f:.4f;
	const float Power=Grade*(bFinisher?1.6f:1.f)*(IsResonanceActive()?1.25f:1.f);
	const int32 Step=FMath::Clamp(InputSequence.Len()/2-1,0,4); // L=0, LL=1, LLL=2
	const float Pitch=1.f+.06f*Step;
	const FVector Forward=GetOwner()->GetActorForwardVector();
	UWorld* World=GetWorld();

	// 1. Hit-stop on the player pose (music clock) and on the enemy meshes.
	// Short, DaD-like stops; the player's ease-out curve carries the weight.
	HitStopSeconds=(bPerfect?.07f:bGood?.05f:.03f)+(bFinisher?.03f:0.f)+(bKick?.02f:0.f)+(bHeavy?.02f:0.f)+(bLauncher?.03f:0.f);
	HitStopEase=(bPerfect?2.1f:bGood?1.8f:1.5f)+(bKick?.3f:0.f);
	// Freeze on the authored contact pose even if the sweep registered a frame early.
	HitStopStartAudio=Clock()?FMath::Max(Clock()->GetAudioNowSeconds(),ImpactAudio):-1000.;

	// 2. Sound (Hi-Fi-Rush style): every connecting hit is a fist-impact on the beat (Dead-as-Disco punch).
	//    Each combo step has its own sample (light rises through LLLL, heavy
	//    through HHHH), finishers/kicks have their own. Timing sets the level;
	//    an off-beat hit gets a thin, quiet version instead of the full drum.
	{
		const TArray<TObjectPtr<USoundBase>>& Set=bHeavy?HeavyHitSounds:LightHitSounds;
		USoundBase* Main=bFinisher&&FinisherHitSound?FinisherHitSound.Get():bKick&&KickHitSound?KickHitSound.Get():
			Set.Num()?Set[FMath::Min(Step,Set.Num()-1)].Get():nullptr;
		if(!bPerfect && !bGood && OffBeatHitSound) Main=OffBeatHitSound;
		if(Main) UGameplayStatics::PlaySound2D(this,Main,bPerfect?1.3f:bGood?1.05f:.8f,1.f);
		// No blade slice / chime layers: the fist impact alone carries the hit.
	}

	// 2b. Comic layer for the HUD: onomatopoeia on big hits (every heavy, kick and
	//     finisher; lights only when Perfect), burst lines, and an impact frame
	//     (one white + one dark frame) on on-beat heavies/finishers.
	{
		static const TCHAR* HeavyWords[]={TEXT("BAM!"),TEXT("WHAM!"),TEXT("SMASH!"),TEXT("KRAKOOM!")};
		static const TCHAR* LightWords[]={TEXT("POW!"),TEXT("BAP!"),TEXT("PAK!")};
		const bool bBig=bHeavy||bFinisher||bKick;
		ImpactFxWorld=Location; ImpactFxWorldTime=World?World->GetTimeSeconds():-1000.;
		ImpactFxPower=FMath::Clamp(Power,.4f,1.8f);
		bImpactFxHeavy=bBig; bImpactFxPerfect=bPerfect;
		bImpactFxFrame=bBig && (bPerfect||bGood);
		bImpactFxLaunch=bLauncher;
		ImpactFxWord=(!bPerfect && !bGood)?FString():bFinisher?FString(bHeavy?TEXT("KRAKOOM!"):TEXT("BOOM!")):
			bKick?FString(TEXT("THWACK!")):bLauncher?FString(TEXT("LAUNCH!")):bHeavy?FString(HeavyWords[Step%4]):bPerfect?FString(LightWords[Step%3]):FString();
		if((bHeavy||bFinisher) && !bKick)
			if(auto* Sword=GetOwner()->FindComponentByClass<UStaticMeshComponent>())
			{
				TipBurstWorld=Sword->GetComponentLocation()+Sword->GetComponentTransform().TransformVectorNoScale(FVector::YAxisVector)*120.f;
				TipBurstWorldTime=ImpactFxWorldTime;
			}
	}

	// 3. Sparks at the contact point, sized by timing; on-beat adds a burst.
	if(HitEffect)
		RetireCombatFX(UGameplayStatics::SpawnEmitterAtLocation(World,HitEffect,Location,Forward.Rotation(),FVector(.8f+1.1f*Power),true),.03f,.12f);
	// Heavies/finishers get the comic tip explosion (HUD) instead of the spark burst.
	if((bPerfect || bFinisher) && !bHeavy && !bFinisher && PerfectBurstEffect)
	{
		// Short burst only: emit for one frame, keep a brief tail.
		UParticleSystem* Burst=LoadObject<UParticleSystem>(nullptr,TEXT("/Game/SwordAnim/demo/FX/P_Spark.P_Spark"));
		if(Burst) RetireCombatFX(UGameplayStatics::SpawnEmitterAtLocation(World,Burst,Location,Forward.Rotation(),FVector(.65f+.6f*Power),true),.03f,.15f);
	}

	// 4. Enemy: flash, shove, freeze. Poise/hit-reaction stays in ReceivePlayerStrike.
	if(Target)
		if(auto* Boss=Target->FindComponentByClass<UBeatSyncedComboComponent>())
			Boss->PlayPlayerHitFeedback(GetOwner(),Power*(bKick?1.5f:1.f),HitStopSeconds+.02f);

	// 5. Camera punch: FOV kick + shake, stronger on the beat.
	if(IsResonanceActive()) if(auto* FX=GetOwner()->FindComponentByClass<UPerfectDodgeFeedbackComponent>()) FX->PlayResonanceAttack(bFinisher);
	if(auto* Camera=GetOwner()->FindComponentByClass<UBossCombatCameraComponent>(); Camera && bKick)
		// Kick finisher: punch-in + Dutch roll toward the kick, held through the freeze.
		Camera->TriggerKickAccent(2.5f+1.5f*Grade,1.f+1.f*Grade,.15f+.1f*Grade,0.f,Location,HitStopSeconds);
	else if(Camera && bLauncher)
	{
		// Launcher: side close-up that follows the boss up, plus an upward accent.
		if(Target) Camera->TriggerLaunchShot(Target,1.3f);
		Camera->TriggerKickAccent(1.5f+1.f*Grade,.4f+.4f*Grade,0.f,0.f,Location,HitStopSeconds); // no upward pitch
	}
	else if(Camera && bHeavy)
		// Heavy: the same punch-in + small roll into the blow, a notch below the kick.
		Camera->TriggerKickAccent(2.f+1.3f*Grade,.5f+.7f*Grade,.1f+.08f*Grade,0.f,Location,HitStopSeconds);
	else if(Camera)
		// Camera stays small and directional (GDC 'Juicing Your Cameras': in 3D use
		// small rotational motion, not big translation). The weight comes from
		// hit-stop, the enemy's shudder, flash and sound instead. Strength is 1 so
		// values are final: <=~3.5 deg FOV, <=~.2 deg shake, <=~.3 deg pitch dip.
		// No oscillating shake on ordinary hits (a 12 Hz wobble read as judder):
		// only a small smooth FOV punch and pitch dip.
		Camera->TriggerPerfectDodgeFeedback(.6f+.9f*Power,0.f,1.f,Location,Forward,bPerfect,
			0.f,0.f,0.f,.05f+.07f*Power,.08f);

	// 6. Screen punch + controller rumble.
	PunchStartWorld=World?World->GetTimeSeconds():-1000.; PunchStrength=Power*(bKick?1.35f:bHeavy?1.3f:1.f);
	if(auto* PC=Cast<APlayerController>(Cast<APawn>(GetOwner())?Cast<APawn>(GetOwner())->GetController():nullptr))
		PC->PlayDynamicForceFeedback(FMath::Clamp(.35f+.4f*Power,0.f,1.f),.08f+.06f*Power,true,true,true,true);
}

void UPlayerBeatAttackComponent::TickScreenPunch()
{
	auto* Player=Cast<AHearBladeCharacter>(GetOwner());
	UCameraComponent* Camera=Player?Player->GetFollowCamera():nullptr;
	if(!Camera || !GetWorld()) return;
	auto& PP=Camera->PostProcessSettings;
	if(BaseFringe<0.f){BaseFringe=PP.bOverride_SceneFringeIntensity?PP.SceneFringeIntensity:0.f;BaseVignette=PP.bOverride_VignetteIntensity?PP.VignetteIntensity:.4f;}
	const float Age=float(GetWorld()->GetTimeSeconds()-PunchStartWorld), Duration=.08f+.04f*PunchStrength;
	const float Env=Age>=0.f && Age<Duration?FMath::Square(1.f-Age/Duration):0.f;
	PP.bOverride_SceneFringeIntensity=true; PP.SceneFringeIntensity=BaseFringe+Env*2.f*PunchStrength;
	PP.bOverride_VignetteIntensity=true; PP.VignetteIntensity=BaseVignette+Env*.3f*PunchStrength;
}

void UPlayerBeatAttackComponent::SetResonanceState(EPlayerResonanceState State)
{
	if(State==ResonanceState) return;
	ResonanceState=State; OnResonanceChanged.Broadcast(State);
	UE_LOG(LogTemp,Display,TEXT("PlayerResonance: state=%d beat=%.4f"),int(State),Clock()?Clock()->GetCurrentBeat():0.);
}

bool UPlayerBeatAttackComponent::TryActivateResonance()
{
	if(!Combat.IsValid() || Combat->Health<=0 || Combat->IsPhaseCinematic() || !Clock() || !Clock()->IsRunning()) return false;
	if(Combat->Resonance>=Combat->MaxResonance && ResonanceState==EPlayerResonanceState::Filling) SetResonanceState(EPlayerResonanceState::Ready);
	if(ResonanceState!=EPlayerResonanceState::Ready) return false;
	AActor* Target=FindTarget(); auto* Boss=Target?Target->FindComponentByClass<UBeatSyncedComboComponent>():nullptr;
	if(!Boss || Boss->BossHealth<=0 || Boss->IsPhaseTransitionActive()) return false;
	ResonanceBoss=Boss;
	const int32 Bar=Clock()->GetBeatsPerBar();
	ArmedBeat=(FMath::FloorToDouble(Clock()->GetCurrentBeat()/Bar)+1.)*Bar;
	SetResonanceState(EPlayerResonanceState::Armed); return true;
}

float UPlayerBeatAttackComponent::GetRemainingBeats() const { return IsResonanceActive() && Clock()?float(FMath::Clamp(ActiveStartBeat+16.-Clock()->GetCurrentBeat(),0.,16.)):0.f; }

void UPlayerBeatAttackComponent::EndResonance()
{
	if(ResonanceState!=EPlayerResonanceState::Active && ResonanceState!=EPlayerResonanceState::Armed) return;
	const bool WasActive=IsResonanceActive();
	if(ResonanceBoss.IsValid()) ResonanceBoss->EndPlayerResonance();
	ResonanceBoss.Reset();
	if(WasActive && Combat.IsValid()) Combat->Resonance=0.f;
	SetResonanceState(WasActive?EPlayerResonanceState::Filling:EPlayerResonanceState::Ready);
}

void UPlayerBeatAttackComponent::TickComponent(float Delta,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(Delta,TickType,TickFunction);
	TickRuntimeProbe();
	TickScreenPunch();
	if(!Combat.IsValid() || !Clock()) return;
	if(Combat->Health<=0 || Combat->IsPhaseCinematic() || !Clock()->IsRunning()) { Reset(); return; }
	if(bHasPendingAttack && !IsAttacking())
	{
		const double AudioNow=Clock()->GetAudioNowSeconds();
		if(GetWorld()->GetTimeSeconds()>PendingAttackDeadline) bHasPendingAttack=false;
		else if(AudioNow>=RecoveryUntilAudio && AudioNow>=NextStartAllowedAudio &&
			Combat->PrepareForOrdinaryAttack() && StartAttack(PendingAttack,PendingAttackAudio)) bHasPendingAttack=false;
	}
	if(Combat->IsPerformingAction() && IsAttacking()) CancelAttack();
	if(IsAttacking()) TickAttack(Clock()->GetAudioNowSeconds(),Delta);
	if(ResonanceState==EPlayerResonanceState::Filling && Combat->Resonance>=Combat->MaxResonance) SetResonanceState(EPlayerResonanceState::Ready);
	if(ResonanceState==EPlayerResonanceState::Armed || IsResonanceActive())
	{
		if(!ResonanceBoss.IsValid() || ResonanceBoss->BossHealth<=0 || ResonanceBoss->IsPhaseTransitionActive()) { EndResonance(); return; }
		if(ResonanceState==EPlayerResonanceState::Armed && Clock()->GetCurrentBeat()>=ArmedBeat)
		{
			ActiveStartBeat=ArmedBeat; SetResonanceState(EPlayerResonanceState::Active);
			ResonanceBoss->BeginPlayerResonance(ActiveStartBeat,GetOwner());
			if(auto* FX=GetOwner()->FindComponentByClass<UPerfectDodgeFeedbackComponent>()) FX->PlayResonanceAttack(true);
			if(StartSound) UGameplayStatics::PlaySound2D(this,StartSound,.65f);
			if(auto* Camera=GetOwner()->FindComponentByClass<UBossCombatCameraComponent>()) Camera->TriggerPerfectDodgeFeedback(5.f,.3f,1.3f,GetOwner()->GetActorLocation(),GetOwner()->GetActorForwardVector(),true,0.f,8.f,0.f,.1f,.4f);
		}
		if(IsResonanceActive())
		{
			Combat->Resonance=Combat->MaxResonance*GetRemainingBeats()/16.f;
			if(GetRemainingBeats()<=0.f) EndResonance();
		}
	}
}
