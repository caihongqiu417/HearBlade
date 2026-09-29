#include "Combat/CombatTutorialComponent.h"
#include "Combat/BeatSyncedComboComponent.h"
#include "Combat/BossBeatDirector.h"
#include "Combat/BossLocomotionAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Combat/PlayerBeatAttackComponent.h"
#include "Combat/PlayerRhythmCombatComponent.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	TWeakObjectPtr<UGameInstance> GTutorialDoneFor;
	struct FTutorialCard
	{
		ECombatTutorialStep Step;
		const TCHAR* Title;
		const TCHAR* Body;
		const TCHAR* Keyboard; // comma-separated T_TUT_K_* suffixes
		const TCHAR* Pad;      // comma-separated T_TUT_P_* suffixes
		int32 Goal;            // actions required (0 = timed)
		float Seconds;         // minimum time on screen (timed steps advance after it)
		ECombatTutorialStep Next;
	};
	const FTutorialCard Cards[] = {
		{ECombatTutorialStep::Dodge, TEXT("DODGE"),
			TEXT("Boss attacks slide along the rail at the bottom. Press DODGE the moment the icon reaches the diamond."),
			TEXT("Shift"), TEXT("B"), 2, 2.f, ECombatTutorialStep::Block},
		{ECombatTutorialStep::Block, TEXT("BLOCK"),
			TEXT("Shield icon: press BLOCK as it reaches the diamond. (Right mouse blocks only while an attack is coming.)"),
			TEXT("RMB"), TEXT("LB"), 2, 2.f, ECombatTutorialStep::Counter},
		{ECombatTutorialStep::Counter, TEXT("COUNTER"),
			TEXT("Crossed swords: strike back ON the beat. (Left mouse counters while an attack is coming.)"),
			TEXT("LMB,Q"), TEXT("RB"), 2, 2.f, ECombatTutorialStep::LightCombo},
		{ECombatTutorialStep::LightCombo, TEXT("LIGHT COMBO"),
			TEXT("Walk up to the boss and press LIGHT 4 times on the beat: a 4-hit combo."),
			TEXT("LMB,LMB,LMB,LMB"), TEXT("X,X,X,X"), 4, 2.f, ECombatTutorialStep::HeavyCombo},
		{ECombatTutorialStep::HeavyCombo, TEXT("HEAVY COMBO"),
			TEXT("Now press HEAVY 4 times on the beat. Slower, but every hit lands harder."),
			TEXT("RMB,RMB,RMB,RMB"), TEXT("Y,Y,Y,Y"), 4, 2.f, ECombatTutorialStep::Openings},
		{ECombatTutorialStep::Openings, TEXT("STRIKE IN THE GAPS"),
			TEXT("After the boss swings it is open for a moment. Dodge, then hit it right away: hits in the gap deal BONUS damage."),
			TEXT("Shift,LMB"), TEXT("B,X"), 2, 2.f, ECombatTutorialStep::Resonance},
		{ECombatTutorialStep::Resonance, TEXT("RESONANCE"),
			TEXT("On-beat play fills RESONANCE. Yours is FULL now: unleash it, then attack - every hit is stronger for 16 beats."),
			TEXT("R"), TEXT("LT"), 1, 2.f, ECombatTutorialStep::Done},
		{ECombatTutorialStep::Done, TEXT("TUTORIAL COMPLETE"),
			TEXT("Keep the rhythm. The real fight starts now!"),
			TEXT(""), TEXT(""), 0, 2.5f, ECombatTutorialStep::None},
	};

	const FTutorialCard* CardFor(ECombatTutorialStep Step)
	{
		for (const FTutorialCard& C : Cards) if (C.Step == Step) return &C;
		return nullptr;
	}
	void SplitGlyphs(const TCHAR* List, const TCHAR* Prefix, TArray<FName>& Out)
	{
		TArray<FString> Parts; FString(List).ParseIntoArray(Parts, TEXT(","), true);
		for (const FString& P : Parts) Out.Add(FName(*(FString(Prefix) + P)));
	}
}

UCombatTutorialComponent::UCombatTutorialComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

bool UCombatTutorialComponent::IsDone() const
{
	// The GameInstance lives across OpenLevel (fight restart, retry after death) but is new
	// for every launch / Play-In-Editor session, so the tutorial runs once per game.
	const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	return GI && GTutorialDoneFor.Get() == GI;
}

void UCombatTutorialComponent::MarkDone() const
{
	GTutorialDoneFor = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
}

void UCombatTutorialComponent::SetCursorMode(bool bOn)
{
	bSkipHover = false;
	if (bOn == bCursorMode) return;
	bCursorMode = bOn;
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC) return;
	PC->SetShowMouseCursor(bOn);
	if (bOn)
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
		PC->SetInputMode(Mode);
	}
	else PC->SetInputMode(FInputModeGameOnly());
}

void UCombatTutorialComponent::Bind()
{
	if (!Combat.IsValid()) Combat = GetOwner()->FindComponentByClass<UPlayerRhythmCombatComponent>();
	if (!Attacks.IsValid()) Attacks = GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>();
	if (!Boss.IsValid())
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			if (auto* Combo = It->FindComponentByClass<UBeatSyncedComboComponent>()) { Boss = Combo; break; }
	if (!Director.IsValid())
		for (TActorIterator<ABossBeatDirector> It(GetWorld()); It; ++It) { Director = *It; break; }
	if (!Combat.IsValid() || !Attacks.IsValid() || !Boss.IsValid()) return;
	Combat->OnRhythmSuccessFeedback.AddUniqueDynamic(this, &UCombatTutorialComponent::HandleRhythmFeedback);
	Attacks->OnAttackHit.AddUniqueDynamic(this, &UCombatTutorialComponent::HandleAttackHit);
	Boss->OnPhaseChanged.AddUniqueDynamic(this, &UCombatTutorialComponent::HandleBossPhase);
	bBound = true;
}

void UCombatTutorialComponent::SetBossHold(bool bHold)
{
	if (bHold == bBossHeld) return;
	UBehaviorTreeComponent* BT = Director.IsValid() ? Director->BehaviorTreeComponent.Get() : nullptr;
	if (bHold)
	{
		if (BT) BT->PauseLogic(TEXT("Tutorial"));
		if (Boss.IsValid()) Boss->CancelCombo();
	}
	else
	{
		TickBossApproach(0.f, false); // hand locomotion back to the behaviour tree standing still
		if (BT) BT->ResumeLogic(TEXT("Tutorial"));
	}
	bBossHeld = bHold;
	if (Boss.IsValid()) Boss->bTutorialNoDamage = bHold;
	UE_LOG(LogTemp, Display, TEXT("Tutorial: boss %s"), bHold ? TEXT("sparring") : TEXT("released"));
}

void UCombatTutorialComponent::Begin(ECombatTutorialStep NewStep)
{
	const FTutorialCard* Card = CardFor(NewStep);
	if (!Card) { Finish(true); return; }
	Step = NewStep; Progress = 0; Goal = Card->Goal; AdvanceAtWorld = -1.;
	Title = Card->Title; Body = Card->Body;
	StepStartWorld = GetWorld()->GetTimeSeconds();
	NextPracticeWorld = StepStartWorld + 2.0; bPracticePending = false;
	MoveStart = GetOwner()->GetActorLocation();
	OpeningMarksAtStart = 0;
	SetBossHold(true);
	RefreshGlyphs();
	UE_LOG(LogTemp, Display, TEXT("Tutorial: step %s"), *Title);
}

void UCombatTutorialComponent::Finish(bool bRestartFight)
{
	MarkDone();
	SetCursorMode(false);
	Step = ECombatTutorialStep::None; Glyphs.Reset(); Goal = Progress = 0; SkipHold = 0.f;
	SetBossHold(false);
	if (bRestartFight && GetWorld())
	{
		// Fresh boss, fresh health, music from the top. The ini flag keeps the tutorial from coming back.
		UE_LOG(LogTemp, Display, TEXT("Tutorial: done, restarting the boss fight"));
		UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
	}
}

void UCombatTutorialComponent::CountCombo(TCHAR Letter)
{
	// InputSequence is the running chain, e.g. "L L L ". Progress = length of an all-Letter chain,
	// so dropping the chain (or mixing inputs) starts the count again.
	if (!Attacks.IsValid() || Goal <= 0 || AdvanceAtWorld > 0.) return;
	int32 Length = 0;
	for (TCHAR C : Attacks->InputSequence)
	{
		if (C == TEXT(' ')) continue;
		if (C != Letter) { Length = 0; break; }
		++Length;
	}
	const int32 NewProgress = FMath::Min(Goal, Length);
	if (NewProgress > Progress) SuccessFlashWorld = GetWorld()->GetTimeSeconds();
	Progress = NewProgress;
	if (Progress >= Goal) AdvanceAtWorld = GetWorld()->GetTimeSeconds() + 1.2;
}

void UCombatTutorialComponent::RefreshGlyphs()
{
	Glyphs.Reset();
	if (const FTutorialCard* Card = CardFor(Step))
		SplitGlyphs(bGamepad ? Card->Pad : Card->Keyboard, bGamepad ? TEXT("T_TUT_P_") : TEXT("T_TUT_K_"), Glyphs);
}

void UCombatTutorialComponent::Count(int32 Amount)
{
	if (Goal <= 0 || AdvanceAtWorld > 0.) return;
	Progress = FMath::Min(Goal, Progress + Amount);
	SuccessFlashWorld = GetWorld()->GetTimeSeconds();
	if (Progress >= Goal) AdvanceAtWorld = SuccessFlashWorld + .9; // let the "done" beat land
}

ERhythmResponseType UCombatTutorialComponent::PracticeResponse() const
{
	switch (Step)
	{
	case ECombatTutorialStep::Block: return ERhythmResponseType::Block;
	case ECombatTutorialStep::Counter: return ERhythmResponseType::Counter;
	default: return ERhythmResponseType::Dodge;
	}
}

void UCombatTutorialComponent::HandleRhythmFeedback(const FRhythmSuccessFeedbackData& Feedback)
{
	if (Feedback.bPlayerAttack || Step == ECombatTutorialStep::None) return;
	if (!Feedback.bSuccess) { MissWorld = GetWorld()->GetTimeSeconds(); return; }
	const bool bMatch =
		(Step == ECombatTutorialStep::Dodge && Feedback.Response == ERhythmResponseType::Dodge) ||
		(Step == ECombatTutorialStep::Block && Feedback.Response == ERhythmResponseType::Block) ||
		(Step == ECombatTutorialStep::Counter && Feedback.Response == ERhythmResponseType::Counter);
	if (bMatch) Count();
}

void UCombatTutorialComponent::HandleAttackHit(AActor*, float, EPlayerAttackGrade Grade)
{
	if (Step == ECombatTutorialStep::LightCombo) CountCombo(TEXT('L'));
	else if (Step == ECombatTutorialStep::HeavyCombo) CountCombo(TEXT('H'));
}

void UCombatTutorialComponent::HandleBossPhase(int32 Phase)
{
	// Phase two has no blocking lesson any more; the one-time tips cover its new attacks.
}

void UCombatTutorialComponent::TickDevice()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC) return;
	static const FKey PadKeys[] = { EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_FaceButton_Left,
		EKeys::Gamepad_FaceButton_Top, EKeys::Gamepad_LeftShoulder, EKeys::Gamepad_RightShoulder, EKeys::Gamepad_LeftTrigger,
		EKeys::Gamepad_RightTrigger, EKeys::Gamepad_Special_Left, EKeys::Gamepad_Special_Right };
	static const FKey DeskKeys[] = { EKeys::W, EKeys::A, EKeys::S, EKeys::D, EKeys::LeftShift, EKeys::LeftMouseButton,
		EKeys::RightMouseButton, EKeys::Q, EKeys::F, EKeys::R, EKeys::SpaceBar, EKeys::Tab };
	bool bPad = bGamepad;
	for (const FKey& K : PadKeys) if (PC->WasInputKeyJustPressed(K)) bPad = true;
	if (FMath::Abs(PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX)) > .4f || FMath::Abs(PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftY)) > .4f) bPad = true;
	for (const FKey& K : DeskKeys) if (PC->WasInputKeyJustPressed(K)) bPad = false;
	float MX = 0.f, MY = 0.f; PC->GetInputMouseDelta(MX, MY);
	if (FMath::Abs(MX) + FMath::Abs(MY) > 3.f) bPad = false;
	if (bPad != bGamepad) { bGamepad = bPad; RefreshGlyphs(); }
}

void UCombatTutorialComponent::TickBossApproach(float DeltaTime, bool bActive)
{
	AActor* BossActor = Boss.IsValid() ? Boss->GetOwner() : nullptr;
	if (!BossActor) return;
	UBossLocomotionAnimInstance* Anim = nullptr;
	TArray<USkeletalMeshComponent*> Meshes; BossActor->GetComponents(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
		if ((Anim = Cast<UBossLocomotionAnimInstance>(Mesh->GetAnimInstance())) != nullptr) break;
	if (!Anim) return;
	// While an attack is actually swinging the combo owns the body (warp, root motion).
	const bool bSwinging = Boss->IsComboQueuedOrPlaying() && !Boss->IsComboQueued();
	// Resonance drives the boss itself (its counter-attack routine): stay out of the way.
	if (Attacks.IsValid() && (Attacks->ResonanceState == EPlayerResonanceState::Armed || Attacks->IsResonanceActive())) return;
	if (!bActive || bSwinging || Boss->BossHealth <= 0.f)
	{
		if (!bSwinging) Anim->SetLocomotion(0.f, FVector::ZeroVector, DeltaTime);
		return;
	}
	const FVector ToPlayer = (GetOwner()->GetActorLocation() - BossActor->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	const float Distance = ToPlayer.Size();
	if (Distance < 1.f) return;
	// Face the player (yaw only) and walk in until in sword range.
	FRotator Rot = BossActor->GetActorRotation();
	Rot.Yaw = FMath::RInterpTo(FRotator(0.f, Rot.Yaw, 0.f), ToPlayer.Rotation(), DeltaTime, 6.f).Yaw;
	BossActor->SetActorRotation(Rot);
	const bool bWalk = Distance > 320.f;
	Anim->SetLocomotion(bWalk ? 220.f : 0.f, bWalk ? FVector::ForwardVector : FVector::ZeroVector, DeltaTime);
}

void UCombatTutorialComponent::TickPractice(double WorldNow)
{
	const bool bSparring = Step == ECombatTutorialStep::Dodge || Step == ECombatTutorialStep::Block ||
		Step == ECombatTutorialStep::Counter || Step == ECombatTutorialStep::Openings;
	if (!bSparring || !Boss.IsValid() || AdvanceAtWorld > 0.) return;
	if (Boss->IsComboQueuedOrPlaying()) return;
	const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	if (!Clock || !Clock->IsRunning()) return;
	if (bPracticePending) // the last note just resolved: breathe for a couple of beats
	{
		bPracticePending = false;
		NextPracticeWorld = WorldNow + Clock->GetSecondsPerBeat() * (Step == ECombatTutorialStep::Openings ? 4.0 : 2.0);
	}
	if (WorldNow < NextPracticeWorld) return;
	// Let the boss walk into range first (but never wait forever if the player keeps running).
	if (Boss->GetOwner() && FVector::Dist2D(Boss->GetOwner()->GetActorLocation(), GetOwner()->GetActorLocation()) > 480.f &&
		WorldNow < NextPracticeWorld + 6.0) return;
	const double Target = FMath::CeilToDouble(Clock->GetCurrentBeat()) + 4.0; // 4 beats of warning on the rail
	if (Boss->QueuePracticeAttack(PracticeResponse(), Target, GetOwner())) bPracticePending = true;
	else NextPracticeWorld = WorldNow + .5;
}

void UCombatTutorialComponent::TickCallouts(double WorldNow)
{
	if (Step != ECombatTutorialStep::None || !Boss.IsValid() || !IsDone()) return;
	auto Show = [&](const TCHAR* Text, const TCHAR* Kb, const TCHAR* Pad)
	{
		Callout = Text; CalloutWorld = WorldNow; CalloutGlyphs.Reset();
		SplitGlyphs(bGamepad ? Pad : Kb, bGamepad ? TEXT("T_TUT_P_") : TEXT("T_TUT_K_"), CalloutGlyphs);
	};
	TArray<FUpcomingImpactCue> Cues; Boss->GetUpcomingImpactCues(Cues, 4);
	for (const FUpcomingImpactCue& Cue : Cues)
	{
		if (!bCalloutHold && Cue.RequiredResponse == ERhythmResponseType::HoldBlock)
		{ bCalloutHold = true; Show(TEXT("LONG BAR: hold BLOCK until it ends"), TEXT("RMB"), TEXT("LB")); return; }
		if (!bCalloutArea && Cue.DamageShape == EBossAttackDamageShape::GroundBurst)
		{ bCalloutArea = true; Show(TEXT("BURNING CIRCLE: DODGE on the burst beat"), TEXT("Shift"), TEXT("B")); return; }
	}
}

void UCombatTutorialComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetWorld()) return;
	if (!bBound) { Bind(); if (!bBound) return; }
	TickDevice();
	const double WorldNow = GetWorld()->GetTimeSeconds();
	const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();

	if (Step != ECombatTutorialStep::None)
	{
		// Death, victory or a phase change mid-step end the lesson cleanly.
		if (Boss.IsValid() && (Boss->BossHealth <= 0.f || Boss->BossPhase >= 2)) { Finish(false); return; }
		if (!Combat.IsValid() || Combat->Health <= 0.f || !Boss.IsValid())
		{
			// Died mid-lesson: stop cleanly but keep the tutorial for the next attempt.
			Step = ECombatTutorialStep::None; Glyphs.Reset(); Goal = Progress = 0; SkipHold = 0.f;
			SetCursorMode(false);
			SetBossHold(false);
			return;
		}
		// Hold TAB / View for one second to skip the whole tutorial.
		const APawn* Pawn = Cast<APawn>(GetOwner());
		const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
		// Gamepad: hold View for one second. Mouse: hold ALT to show the cursor, click SKIP.
		const bool bHeld = PC && PC->IsInputKeyDown(EKeys::Gamepad_Special_Left);
		SkipHold = bHeld ? FMath::Min(1.f, SkipHold + DeltaTime) : 0.f;
		if (SkipHold >= 1.f) { Finish(true); return; }
		SetCursorMode(PC && (PC->IsInputKeyDown(EKeys::LeftAlt) || PC->IsInputKeyDown(EKeys::RightAlt)));
		if (bCursorMode && PC)
		{
			float MX = 0.f, MY = 0.f;
			bSkipHover = SkipButton.bIsValid && PC->GetMousePosition(MX, MY) && SkipButton.IsInside(FVector2D(MX, MY));
			if (bSkipHover && PC->WasInputKeyJustPressed(EKeys::LeftMouseButton)) { Finish(true); return; }
		}
		// Resonance lesson: keep the meter full until it is used, then wait for the 16 beats to end.
		if (Step == ECombatTutorialStep::Resonance && Attacks.IsValid() && Combat.IsValid() && AdvanceAtWorld < 0.)
		{
			const EPlayerResonanceState R = Attacks->ResonanceState;
			if (Progress == 0)
			{
				if (R == EPlayerResonanceState::Armed || R == EPlayerResonanceState::Active)
				{ Progress = 1; SuccessFlashWorld = WorldNow; Body = TEXT("RESONANCE! Keep hitting the boss on the beat until it runs out."); }
				else Combat->Resonance = Combat->MaxResonance;
			}
			else if (R == EPlayerResonanceState::Filling || R == EPlayerResonanceState::Ready) AdvanceAtWorld = WorldNow + .6;
		}

		const FTutorialCard* Card = CardFor(Step);
		const float Age = float(WorldNow - StepStartWorld);
		if (Step == ECombatTutorialStep::Openings && Attacks.IsValid() && Attacks->LastOpeningHitWorldTime > LastOpeningCounted)
		{ LastOpeningCounted = Attacks->LastOpeningHitWorldTime; if (LastOpeningCounted > StepStartWorld) Count(); }
		if (Card && Card->Goal == 0 && Age >= Card->Seconds && AdvanceAtWorld < 0.) AdvanceAtWorld = WorldNow;
		if (AdvanceAtWorld > 0. && WorldNow >= AdvanceAtWorld && (!Card || Age >= Card->Seconds))
		{
			const ECombatTutorialStep Next = Card ? Card->Next : ECombatTutorialStep::None;
			if (Next == ECombatTutorialStep::None) Finish(true);
			else Begin(Next);
			return;
		}
		TickBossApproach(DeltaTime, true);
		TickPractice(WorldNow);
		return;
	}

	// Start phase one once the encounter is live (behaviour tree running, music on).
	const bool bEncounter = Director.IsValid() && Director->BehaviorTreeComponent && Director->BehaviorTreeComponent->IsRunning();
	if (!IsDone() && bEncounter && Clock && Clock->IsRunning() && Boss.IsValid() && Boss->BossPhase < 2 &&
		Combat.IsValid() && Combat->Health > 0.f)
	{ Begin(ECombatTutorialStep::Dodge); return; }
	TickCallouts(WorldNow);
}

void UCombatTutorialComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetBossHold(false);
	Super::EndPlay(EndPlayReason);
}
