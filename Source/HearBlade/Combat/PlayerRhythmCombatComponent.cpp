#include "Combat/PlayerRhythmCombatComponent.h"
#include "Combat/PlayerBeatAttackComponent.h"
#include "Components/CapsuleComponent.h"
#include "Combat/TransientCombatFX.h"
#include "Combat/PlayerCombatAnimInstance.h"
#include "HearBladeCharacter.h"
#include "Combat/BeatSyncedComboComponent.h"
#include "Combat/BossCombatCameraComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Particles/ParticleSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

UPlayerRhythmCombatComponent::UPlayerRhythmCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	DodgeForwardAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/SwordAnim/Animation/UE5/roll_front.roll_front"));
	DodgeBackwardAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/SwordAnim/Animation/UE5/roll_back.roll_back"));
	DodgeLeftAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/SwordAnim/Animation/UE5/roll_left.roll_left"));
	DodgeRightAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/SwordAnim/Animation/UE5/roll_right.roll_right"));
	BlockStartAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Defense01_Start.Defense01_Start"));
	BlockLoopAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Defense01_Loop.Defense01_Loop"));
	BlockEndAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Defense01_End.Defense01_End"));
	CounterAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Defense01_CounterAttack.Defense01_CounterAttack"));
	BlockHitReactAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Anim_DKM_Hit_Alert_Fwd.Anim_DKM_Hit_Alert_Fwd"));
	BlockImpactEffect = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/SwordAnim/demo/FX/P_Spark.P_Spark"));
	CounterImpactEffect = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/SwordAnim/demo/FX/P_Hit.P_Hit"));
	PerfectHoldShockEffect = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Combat/Feedback/NS_PerfectDodge_Ring.NS_PerfectDodge_Ring"));
	BloodHitEffect = LoadObject<UNiagaraSystem>(nullptr,
		TEXT("/Game/Blood_VFX_Pack/Particles/Systems/P_Splash_Hit_01.P_Splash_Hit_01"));
	BloodPoiseBreakEffect = LoadObject<UNiagaraSystem>(nullptr,
		TEXT("/Game/Blood_VFX_Pack/Particles/Systems/P_SplashWithBurst_Hit_01.P_SplashWithBurst_Hit_01"));
}

void UPlayerRhythmCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if(bPhaseCinematic) return;
	if (GetWorld())
	{
		const double WorldNow = GetWorld()->GetTimeSeconds();
		if(bRecoveryStrikePending && (!bCountering || bHitReacting || bDodging || Health<=0.f)) bRecoveryStrikePending=false;
		if(bRecoveryStrikePending && WorldNow-CounterStartWorldSeconds>=.20)
		{
			bRecoveryStrikePending=false;
			for(TActorIterator<AActor> It(GetWorld());It;++It)
				if(auto* Boss=It->FindComponentByClass<UBeatSyncedComboComponent>();Boss && Boss->TryRecoveryStrike(GetOwner()))
				{
					Resonance=FMath::Min(MaxResonance,Resonance+8.f);LastResonanceGainWorldSeconds=WorldNow;
					if(CounterStrikeSound) UGameplayStatics::PlaySound2D(this,CounterStrikeSound,1.15f); // the counter's blade lands
					PlayResponseFeedback(ERhythmResponseType::Counter,ERhythmDodgeJudgment::Good,It->GetActorLocation(),GetOwner()->GetActorForwardVector(),false);break;
				}
		}
		if (bHitReacting)
		{
			if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
			{
				const float TurnSpeed = HitTurnSeconds > KINDA_SMALL_NUMBER ? 1.0f / HitTurnSeconds * 5.0f : 1000.0f;
				Character->SetActorRotation(FMath::RInterpTo(Character->GetActorRotation(), HitFacingRotation, DeltaTime, TurnSpeed));
				if (bHitTurnPending && WorldNow >= HitTurnEndWorldSeconds)
				{
					Character->SetActorRotation(HitFacingRotation);
					bHitTurnPending = false;
					if (UPlayerCombatAnimInstance* Animation = Cast<UPlayerCombatAnimInstance>(Character->GetMesh()->GetAnimInstance()))
						// The asset's Fwd sample recoils backward from a frontal strike.
						// Its Bwd sample is a strike from behind and lunges forward.
						Animation->StartDirectionalHit(FVector2D(1.0f, 0.0f),
							FMath::Max(0.10f, static_cast<float>(HitReactionEndWorldSeconds - WorldNow)));
				}
			}
			if (WorldNow >= HitReactionEndWorldSeconds) FinishHitReaction();
		}
		if (bBlockReleasePending && WorldNow - DefenseAnimationStartWorldSeconds >= .22)
		{
			bBlockReleasePending = false;
			DefenseAnimationStartWorldSeconds = WorldNow;
			PlayDefenseAnimation(BlockEndAnimation, false);
		}
		if (bBlocking && BlockStartAnimation && WorldNow - DefenseAnimationStartWorldSeconds >= BlockStartAnimation->GetPlayLength())
		{
			PlayDefenseAnimation(BlockLoopAnimation, true);
			DefenseAnimationStartWorldSeconds = BIG_NUMBER;
		}
		if (bBlockRecovering && !bBlockReleasePending && WorldNow - DefenseAnimationStartWorldSeconds >= DefenseAnimationDurationSeconds)
			bBlockRecovering = false;
		if (bCountering && WorldNow - CounterStartWorldSeconds >= CounterDurationSeconds)
			bCountering = false;
		if (bPoiseBroken && WorldNow - PoiseBreakStartWorldSeconds >= PoiseBreakDurationSeconds)
			EndPoiseBreak();
		else if (!bPoiseBroken && Poise < MaxPoise &&
			WorldNow - LastPoiseDamageWorldSeconds >= PoiseRecoveryDelaySeconds)
			Poise = FMath::Min(MaxPoise, Poise + PoiseRecoveryPerSecond * DeltaTime);
		const auto* Attacks = GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>();
		if (Resonance > 0.0f && Resonance < MaxResonance && (!Attacks || Attacks->ResonanceState == EPlayerResonanceState::Filling) && WorldNow - LastResonanceGainWorldSeconds >= ResonanceDecayDelaySeconds)
			Resonance = FMath::Max(0.0f, Resonance - ResonanceDecayPerSecond * DeltaTime);
	}
	ProcessBufferedInput();
	if (bHUDPreviewMode && GetWorld() && HUDPreviewStep < 6 && GetWorld()->GetTimeSeconds() >= NextHUDPreviewWorldSeconds)
	{
		static const ERhythmDodgeJudgment PreviewGrades[] = {
			ERhythmDodgeJudgment::Perfect, ERhythmDodgeJudgment::Good, ERhythmDodgeJudgment::Bad,
			ERhythmDodgeJudgment::Perfect, ERhythmDodgeJudgment::Good, ERhythmDodgeJudgment::Miss };
		bSubmittingHUDPreviewJudgment = true;
		SubmitDodgeJudgment(PreviewGrades[HUDPreviewStep], -18.0f + HUDPreviewStep * 7.0f, HUDPreviewStep % 4 == 0);
		bSubmittingHUDPreviewJudgment = false;
		++HUDPreviewStep;
		NextHUDPreviewWorldSeconds = GetWorld()->GetTimeSeconds() + 0.24;
	}
	if (bPerfectFeedbackPreviewMode && PerfectFeedbackPreviewStep < 2 && GetWorld() && GetOwner() && GetWorld()->GetTimeSeconds() >= NextPerfectFeedbackPreviewWorldSeconds)
	{
		const bool bStrong = PerfectFeedbackPreviewStep % 2 == 1;
		TryDodge(GetOwner()->GetActorRightVector() * (bStrong ? -1.f : 1.f));
		SubmitDodgeJudgment(ERhythmDodgeJudgment::Perfect, bStrong ? -9.f : 14.f, bStrong);
		BroadcastPerfectFeedback(bStrong ? -9.f : 14.f, bStrong,
			GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 110.f + FVector(0,0,90),
			GetOwner()->GetActorRightVector());
		++PerfectFeedbackPreviewStep;
		NextPerfectFeedbackPreviewWorldSeconds = GetWorld()->GetTimeSeconds() + 1.4;
	}
	if (bAutomationTestMode && GetWorld() && GetOwner())
	{
		if (const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>())
		{
			if (bBlocking && AutomationReleaseAudioSeconds > 0.0 && Clock->GetAudioNowSeconds() >= AutomationReleaseAudioSeconds)
			{
				EndBlock();
				AutomationReleaseAudioSeconds = -1000.0;
			}
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			{
				UBeatSyncedComboComponent* Combo = It->FindComponentByClass<UBeatSyncedComboComponent>();
				TArray<FUpcomingImpactCue> Cues;
				if (!Combo) continue;
#if !UE_BUILD_SHIPPING
				if(FParse::Param(FCommandLine::Get(),TEXT("GroundBurstPursuitTest")) && Combo->IsGroundBurstRecovery())
				{
					if(auto* Character=Cast<ACharacter>(GetOwner());Character && !IsPerformingAction())
					{
						const FVector Toward=(It->GetActorLocation()-GetOwner()->GetActorLocation()).GetSafeNormal2D();
						Character->SetActorRotation(Toward.Rotation());
						if(FVector::Dist2D(It->GetActorLocation(),GetOwner()->GetActorLocation())>250.f) Character->AddMovementInput(Toward,1.f,true);
						else TryCounter();
					}
					continue;
				}
#endif
				Combo->GetUpcomingImpactCues(Cues, 1);
				if (Cues.IsEmpty()) continue;
				const FUpcomingImpactCue& Cue = Cues[0];
				// Ground bursts require spatial escape, not an automated defense key.
				if (Cue.DamageShape == EBossAttackDamageShape::GroundBurst)
				{
#if !UE_BUILD_SHIPPING
					const bool bEarlyEscape=FParse::Param(FCommandLine::Get(),TEXT("GroundBurstEarlyTest"));
					const bool bReenter=FParse::Param(FCommandLine::Get(),TEXT("GroundBurstReenterTest"));
					if(FParse::Param(FCommandLine::Get(),TEXT("GroundBurstEscapeTest"))||bEarlyEscape||bReenter)
					{
						FVector Center;float Radius,Progress;
						if(auto* Character=Cast<ACharacter>(GetOwner());Character && Combo->GetGroundBurstPreview(Center,Radius,Progress))
						{
							auto* Movement=Character->GetCharacterMovement();
							const float Boundary=Radius+Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
							if(bReenter&&FVector::Dist2D(Center,Character->GetActorLocation())>Boundary+2.f) bGroundBurstTestExited=true;
							const float Distance=FMath::Max(0.f,Boundary-FVector::Dist2D(Center,Character->GetActorLocation()));
							const float Speed=FMath::Max(1.f,Movement->MaxWalkSpeed);
							const double Until=Cue.ImpactAudioSeconds-Clock->GetAudioNowSeconds();
							if(FParse::Param(FCommandLine::Get(),TEXT("GroundBurstWalkDodgeTest")) && Until<=DodgeDurationSeconds+.1 && Until>.1 && Distance>20.f && LastDodgeInputAudioSeconds<Cue.ImpactAudioSeconds-1.)
								TryDodge((Character->GetActorLocation()-Center).GetSafeNormal2D());
							const double Travel=Distance/Speed+.5*Speed/FMath::Max(1.f,Movement->MaxAcceleration)+.08;
							if(Until<=Travel||bEarlyEscape||bReenter)
							{
								const float Direction=bReenter&&bGroundBurstTestExited?-1.f:1.f;
								if(Distance>0.f||Direction<0.f||!bEarlyEscape)
									Character->AddMovementInput((Character->GetActorLocation()-Center).GetSafeNormal2D(),Direction,true);
							}
						}
					}
#endif
					continue;
				}
				const double SecondsUntilImpact = Cue.ImpactAudioSeconds - Clock->GetAudioNowSeconds();
				const bool bNewImpact = Cue.ImpactAudioSeconds > LastAutomatedImpactAudioSeconds + 0.001;
				if (Cue.RequiredResponse == ERhythmResponseType::HoldBlock && Cue.HoldMarker == ERhythmHoldMarkerType::End &&
					bBlocking && bNewImpact && SecondsUntilImpact <= .040 && SecondsUntilImpact >= -.010)
				{
					EndBlock();
					LastAutomatedImpactAudioSeconds = Cue.ImpactAudioSeconds;
					UE_LOG(LogTemp, Display, TEXT("RhythmCombatAutomation: hold release leadMs=%.1f"), SecondsUntilImpact * 1000.0);
					break;
				}
				const double InputLead = Cue.IdealDodgeLeadSeconds;
				if (!bNewImpact || SecondsUntilImpact > InputLead + .055 || SecondsUntilImpact < InputLead - .055) continue;
				bool bIssued = false;
				const ERhythmResponseType TestResponse=Cue.RequiredResponse==ERhythmResponseType::DodgeOrBlock
					? (Cue.ImpactIndex%2==0?ERhythmResponseType::Block:ERhythmResponseType::Dodge):Cue.RequiredResponse;
				switch (TestResponse)
				{
				case ERhythmResponseType::Dodge:
				{
					if (bAutomationFailureTestMode && !bAutomationWrongActionIssued)
					{
						bIssued = TryBlockStart();
						if (bIssued)
						{
							bAutomationWrongActionIssued = true;
							AutomationReleaseAudioSeconds = Cue.ImpactAudioSeconds + .035;
							UE_LOG(LogTemp, Display, TEXT("RhythmCombatFailureAutomation: deliberately issued Block for Dodge"));
						}
						break;
					}
					const FVector ToBoss = (It->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal2D();
					bIssued = TryDodge(FVector::CrossProduct(FVector::UpVector, ToBoss));
					break;
				}
				case ERhythmResponseType::Block:
					bIssued = TryBlockStart();
					if (bIssued) AutomationReleaseAudioSeconds = Cue.ImpactAudioSeconds + .035;
					break;
				case ERhythmResponseType::Counter: bIssued = TryCounter(); break;
				case ERhythmResponseType::HoldBlock:
					if (Cue.HoldMarker == ERhythmHoldMarkerType::Start)
					{
						bIssued = TryBlockStart();
						if (bIssued && bAutomationFailureTestMode)
						{
							AutomationReleaseAudioSeconds = Cue.ImpactAudioSeconds + .12;
							UE_LOG(LogTemp, Display, TEXT("RhythmCombatFailureAutomation: scheduled early hold release"));
						}
					}
					break;
				}
				if (bIssued)
				{
					LastAutomatedImpactAudioSeconds = Cue.ImpactAudioSeconds;
					UE_LOG(LogTemp, Display, TEXT("RhythmCombatAutomation: response=%s leadMs=%.1f impactAudio=%.3f"),
						*UEnum::GetValueAsString(Cue.RequiredResponse), SecondsUntilImpact * 1000.0, Cue.ImpactAudioSeconds);
					break;
				}
			}
		}
	}
	if (!bDodging || !GetWorld() || !GetOwner()) return;
	const float Elapsed = static_cast<float>(GetWorld()->GetTimeSeconds() - DodgeStartWorldSeconds);
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		const float Alpha = FMath::Clamp(Elapsed / DodgeDurationSeconds, 0.0f, 1.0f);
		const float EasedAlpha = 1.0f - FMath::Pow(1.0f - Alpha, 2.4f);
		const float TargetDistance = DodgeDistanceCm * EasedAlpha;
		const float Step = FMath::Max(0.0f, TargetDistance - DodgeDistanceApplied);
		if (Step > KINDA_SMALL_NUMBER)
		{
			FHitResult Hit;
			Character->AddActorWorldOffset(DodgeDirection * Step, true, &Hit);
			DodgeDistanceApplied += Step * (Hit.IsValidBlockingHit() ? Hit.Time : 1.0f);
		}
	}
	// Holding a direction lets the last 30% of the roll flow straight into a run
	// with momentum, instead of stopping dead and waiting for the clip to end
	// (the "hitch" after every dodge).
	const AHearBladeCharacter* Player = Cast<AHearBladeCharacter>(GetOwner());
	const FVector Intent = Player ? Player->GetWorldMoveIntent() : FVector::ZeroVector;
	const bool bRunOut = Intent.SizeSquared() > .09f && Elapsed >= DodgeDurationSeconds * .7f;
	if (Elapsed >= DodgeDurationSeconds || bRunOut)
	{
		bDodging = false;
		if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
		{
			// Keep translation suppressed until locomotion replaces the outgoing roll.
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			{
				Movement->SetMovementMode(MOVE_Walking);
				if (Intent.SizeSquared() > .09f)
				{
					const FVector Dir = Intent.GetSafeNormal2D();
					Movement->Velocity = Dir * Movement->MaxWalkSpeed * (FVector::DotProduct(Dir, DodgeDirection) > .3f ? .85f : .55f);
					if (auto* Instance = Cast<UPlayerCombatAnimInstance>(Character->GetMesh()->GetAnimInstance()))
						Instance->ReleaseCombatAction(.16f);
				}
			}
			UE_LOG(LogTemp, Display, TEXT("RhythmDodgeMotion: completed distance=%.1fcm meshRelative=(%.1f,%.1f,%.1f)"),
				FVector::Dist2D(DodgeStartLocation, Character->GetActorLocation()),
				Character->GetMesh()->GetRelativeLocation().X, Character->GetMesh()->GetRelativeLocation().Y,
				Character->GetMesh()->GetRelativeLocation().Z);
		}
		return;
	}
}

void UPlayerRhythmCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	auto LoadHit=[](const TCHAR* Name){ return LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/Audio/SFX/HitRush/%s.%s"),Name,Name)); };
	DodgeSwishSound=LoadHit(TEXT("SW_HitRush_WhooshLight"));
	CounterSwishSound=LoadHit(TEXT("SW_HitRush_WhooshHeavy"));
	GuardUpSound=LoadHit(TEXT("SW_HitRush_HoldHit"));
	CounterStrikeSound=LoadHit(TEXT("SW_HitRush_Counter"));
	// Migrate existing Blueprint templates that still serialize the old 100 HP.
	MaxHealth = FMath::Max(MaxHealth,500.f);
	Health = MaxHealth;
	// Existing Blueprint component templates can retain older native defaults.
	// Apply the responsive floor/ceiling here so old level instances receive the
	// same input contract without requiring every placed actor to be recreated.
	DodgeChainStartSeconds = FMath::Min(DodgeChainStartSeconds, .12f);
	CounterCancelStartSeconds = FMath::Min(CounterCancelStartSeconds, .12f);
	InputBufferSeconds = FMath::Max(InputBufferSeconds, .26f);
	PerfectWindowMs = FMath::Max(PerfectWindowMs, 80.f);
	GoodWindowMs = FMath::Max(GoodWindowMs, 160.f);
	Health = FMath::Clamp(Health, 0.0f, MaxHealth);
	Poise = FMath::Clamp(Poise, 0.0f, MaxPoise);
	LastResonanceGainWorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	bAutomationFailureTestMode = FParse::Param(FCommandLine::Get(), TEXT("RhythmCombatFailureTest"));
	bAutomationTestMode = FParse::Param(FCommandLine::Get(), TEXT("RhythmCombatAutoTest")) || bAutomationFailureTestMode;
	if (bAutomationTestMode)
		UE_LOG(LogTemp, Display, TEXT("RhythmCombatAutomation: enabled for %s"), *GetNameSafe(GetOwner()));
	if (FParse::Param(FCommandLine::Get(), TEXT("RhythmHUDPreview")))
	{
		Health = MaxHealth * 0.72f;
		bHUDPreviewMode = true;
		NextHUDPreviewWorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() + 0.5 : 0.5;
		OnHealthChanged.Broadcast(Health, MaxHealth);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("PerfectDodgePreview")))
	{
		bPerfectFeedbackPreviewMode = true;
		NextPerfectFeedbackPreviewWorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() + 1.0 : 1.0;
	}
}

void UPlayerRhythmCombatComponent::ApplyCombatDamage(float Damage)
{
	if (Damage <= 0.0f || Health <= 0.0f) return;
	Health = FMath::Clamp(Health - Damage, 0.0f, MaxHealth);
	UE_LOG(LogTemp,Display,TEXT("PlayerHealth: %.1f/%.1f%s"),Health,MaxHealth,Health<=0.f?TEXT(" DEFEATED - Enter to retry"):TEXT(""));
	ComboCount = 0;
	OnHealthChanged.Broadcast(Health, MaxHealth);
}

void UPlayerRhythmCombatComponent::RestoreHealth(float Amount)
{
	if (Amount <= 0.0f) return;
	Health = FMath::Clamp(Health + Amount, 0.0f, MaxHealth);
	OnHealthChanged.Broadcast(Health, MaxHealth);
}

void UPlayerRhythmCombatComponent::ApplyGroundBurstDamage(float Damage, const FVector& Center)
{
	if (Health <= 0.f || Damage <= 0.f) return;
	ApplyCombatDamage(Damage);
	const FVector Direction = (GetOwner()->GetActorLocation() - Center).GetSafeNormal2D();
	StartHitReaction(GetOwner()->GetActorLocation(), Direction, false, Center);
	UE_LOG(LogTemp, Display, TEXT("GroundBurst: player damaged %.1f (area requires spatial exit)"), Damage);
}

void UPlayerRhythmCombatComponent::AwardRhythmicEscape(float Amount, float ErrorSeconds)
{
	if (Health <= 0.f || !GetWorld() || !GetOwner()) return;
	const float PreviousResonance=Resonance;
	Resonance = FMath::Clamp(Resonance + Amount, 0.f, MaxResonance);
	LastResonanceGainWorldSeconds = GetWorld()->GetTimeSeconds();
	LastRhythmicEscapeWorldSeconds=LastResonanceGainWorldSeconds;
	LastRhythmicEscapeGain=Resonance-PreviousResonance;
	FRhythmSuccessFeedbackData Feedback;
	Feedback.Response=ERhythmResponseType::Dodge;Feedback.Grade=ERhythmDodgeJudgment::Perfect;
	Feedback.PreviousCombo=Feedback.ComboCount=ComboCount;
	Feedback.FeedbackTier=GetFeedbackTierForCombo(ComboCount);
	// The area dodge is a headline moment: always the strong-beat version, with
	// the monochrome impact frame, ghosts, rift and camera release.
	Feedback.bSuccess=Feedback.bPerfect=true;Feedback.bStrongBeat=true;Feedback.bKeyPerfect=true;
	Feedback.WorldLocation=GetOwner()->GetActorLocation();Feedback.AttackDirection=GetOwner()->GetActorForwardVector();
	Feedback.EventId=++FeedbackEventSerial;
	OnRhythmSuccessFeedback.Broadcast(Feedback);
	PlayResponseFeedback(ERhythmResponseType::Dodge,ERhythmDodgeJudgment::Perfect,Feedback.WorldLocation,Feedback.AttackDirection,true);
	// Spatial escape is its own success, not another defense judgment or Combo increment.
	UE_LOG(LogTemp, Display, TEXT("GroundBurst: rhythmic escape reward=%.1f errorMs=%.1f"), Amount, ErrorSeconds*1000.f);
}

void UPlayerRhythmCombatComponent::SubmitDodgeJudgment(ERhythmDodgeJudgment Grade, float SignedErrorMs, bool bStrongBeat)
{
	if (bHUDPreviewMode && HUDPreviewStep >= 6 && !bSubmittingHUDPreviewJudgment) return;
	LastJudgment = Grade;
	LastJudgmentErrorMs = SignedErrorMs;
	LastJudgmentWorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Grade == ERhythmDodgeJudgment::Perfect || Grade == ERhythmDodgeJudgment::Good ||
		Grade == ERhythmDodgeJudgment::GoodHold)
	{
		++ComboCount;
		const float BaseGain = Grade == ERhythmDodgeJudgment::Perfect ? (LastResponse == ERhythmResponseType::Counter ? 8.f : 4.f) : 2.f;
		const float ComboMultiplier = ComboCount >= 8 ? 1.5f : (ComboCount >= 4 ? 1.25f : 1.0f);
		Resonance = FMath::Clamp(Resonance + BaseGain * ComboMultiplier * (bStrongBeat ? 1.25f : 1.0f), 0.0f, MaxResonance);
		LastResonanceGainWorldSeconds = LastJudgmentWorldSeconds;
	}
	else if (Grade == ERhythmDodgeJudgment::Bad || Grade == ERhythmDodgeJudgment::Miss)
	{
		ComboCount = 0;
	}
	else if (Grade == ERhythmDodgeJudgment::PerfectHold)
	{
		++ComboCount;
		Resonance = FMath::Clamp(Resonance + 4.0f, 0.0f, MaxResonance);
		LastResonanceGainWorldSeconds = LastJudgmentWorldSeconds;
	}
	else if (Grade == ERhythmDodgeJudgment::HoldBreak)
	{
		ComboCount = 0;
	}
	OnDodgeJudged.Broadcast(Grade, SignedErrorMs, ComboCount);
}

int32 UPlayerRhythmCombatComponent::GetFeedbackTierForCombo(int32 Combo)
{
	return Combo >= 16 ? 3 : Combo >= 8 ? 2 : Combo >= 4 ? 1 : 0;
}

void UPlayerRhythmCombatComponent::ResetCombatState()
{
	if (auto* Attacks = GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>()) Attacks->Reset();
	Health = MaxHealth;
	Resonance = 0.0f;
	ComboCount = 0;
	LastJudgment = ERhythmDodgeJudgment::None;
	LastJudgmentErrorMs = 0.0f;
	LastJudgmentWorldSeconds = -1000.0;
	LastResonanceGainWorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	ActiveHoldGroup = NAME_None;
	bActiveHoldBroken = false;
	ReportedHoldBreakGroups.Reset();
	BufferedAction = EBufferedRhythmAction::None;
	bPoiseBroken = false;
	FinishHitReaction();
	Poise = MaxPoise;
	OnHealthChanged.Broadcast(Health, MaxHealth);
	BlockInputHistory.Reset();
	CounterInputHistory.Reset();
}

bool UPlayerRhythmCombatComponent::TryDodge(FVector WorldDirection)
{
	if (Health<=0.f) return false;
	if (auto* Attacks = GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>(); Attacks && !Attacks->CancelForDefense(true)) return false;
	if(bPhaseCinematic) return false;
	if (!GetWorld() || !GetOwner()) return false;
	const double InputAudioSeconds = GetCurrentInputAudioSeconds();
	if (BufferWhilePoiseLocked(EBufferedRhythmAction::Dodge, InputAudioSeconds, WorldDirection)) return true;
	if (bHitReacting) FinishHitReaction();
	if (bPoiseBroken) EndPoiseBreak();
	if (bBlocking) EndBlock();
	if (bCountering)
	{
		const double Elapsed = GetWorld()->GetTimeSeconds() - CounterStartWorldSeconds;
		if (Elapsed < CounterCancelStartSeconds)
		{
			BufferInput(EBufferedRhythmAction::Dodge, InputAudioSeconds, WorldDirection);
			return true;
		}
		bCountering = false;
	}
	if (bDodging)
	{
		const double Elapsed = GetWorld()->GetTimeSeconds() - DodgeStartWorldSeconds;
		if (Elapsed < DodgeChainStartSeconds)
		{
			BufferInput(EBufferedRhythmAction::Dodge, InputAudioSeconds, WorldDirection);
			return true;
		}
		UE_LOG(LogTemp, Verbose, TEXT("RhythmDodgeMotion: chained after %.3fs"), Elapsed);
	}
	return StartDodge(WorldDirection, InputAudioSeconds);
}

bool UPlayerRhythmCombatComponent::StartDodge(FVector WorldDirection, double InputAudioSeconds)
{
	if (!GetWorld() || !GetOwner()) return false;
	if(auto* Attack=GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>(); Attack && !Attack->CancelForDefense(true)) return false;
	if (bPoiseBroken) EndPoiseBreak();
	// (No separate start swish: it doubled up with the dodge's own "fwoosh".)
	BufferedAction = EBufferedRhythmAction::None;
	bBlockRecovering = false;
	bBlockReleasePending = false;
	WorldDirection.Z = 0.0f;
	if (!WorldDirection.Normalize()) WorldDirection = GetOwner()->GetActorForwardVector().GetSafeNormal2D();
	DodgeDirection = WorldDirection;
	DodgeStartLocation = GetOwner()->GetActorLocation();
	bDodging = true;
	DodgeDistanceApplied = 0.0f;
	++DodgeSequenceId;
	DodgeStartWorldSeconds = GetWorld()->GetTimeSeconds();
	LastDodgeInputAudioSeconds = InputAudioSeconds;

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
		}
		// Capsule sweep below owns translation. Suppress authored root translation
		// so changing back to locomotion cannot visually snap the mesh to the capsule.
		Character->SetAnimRootMotionTranslationScale(0.0f);
		const FVector Local = Character->GetActorTransform().InverseTransformVectorNoScale(DodgeDirection);
		UAnimSequence* Chosen = DodgeForwardAnimation;
		if (FMath::Abs(Local.Y) > FMath::Abs(Local.X))
			Chosen = Local.Y >= 0.0f ? DodgeRightAnimation : DodgeLeftAnimation;
		else if (Local.X < 0.0f)
			Chosen = DodgeBackwardAnimation;
		if (Chosen && Character->GetMesh())
		{
			if (auto* Instance = Cast<UPlayerCombatAnimInstance>(Character->GetMesh()->GetAnimInstance()))
				Instance->PlayCombatAction(Chosen, false, DodgeDurationSeconds, 0.f);
		}
	}
	return true;
}

void UPlayerRhythmCombatComponent::PlayDefenseAnimation(UAnimSequence* Animation, bool bLooping, float DesiredDuration)
{
	// This is called only after the defense state has accepted the input. Stop
	// the attack clock writer before starting the independently timed defense.
	if(auto* Attack=GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>()) Attack->NotifyInterrupted();
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Animation || !Character || !Character->GetMesh()) return;
	DefenseAnimationDurationSeconds = DesiredDuration > 0.0f ? DesiredDuration : Animation->GetPlayLength();
	Character->SetAnimRootMotionTranslationScale(0.0f);
	Character->ConsumeMovementInputVector();
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		Movement->StopMovementImmediately();
	if (auto* Instance = Cast<UPlayerCombatAnimInstance>(Character->GetMesh()->GetAnimInstance()))
		Instance->PlayCombatAction(Animation, bLooping, DefenseAnimationDurationSeconds);
}

bool UPlayerRhythmCombatComponent::TryBlockStart()
{
	if(Health<=0.f || bPhaseCinematic) return false;
	if (auto* Attacks = GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>(); Attacks && !Attacks->CancelForDefense(false)) return false;
	if(bPhaseCinematic) return false;
	if (!GetWorld() || bBlocking) return false;
	const double InputAudioSeconds = GetCurrentInputAudioSeconds();
	BlockInputHistory.Add(InputAudioSeconds);
	if (BlockInputHistory.Num() > 16) BlockInputHistory.RemoveAt(0);
	if (BufferWhilePoiseLocked(EBufferedRhythmAction::Block, InputAudioSeconds)) return true;
	if (bHitReacting) FinishHitReaction();
	if (bPoiseBroken) EndPoiseBreak();
	if (bDodging)
	{
		const double Elapsed = GetWorld()->GetTimeSeconds() - DodgeStartWorldSeconds;
		if (Elapsed < DodgeChainStartSeconds)
		{
			BufferInput(EBufferedRhythmAction::Block, InputAudioSeconds);
			return true;
		}
		CancelDodgeForTransition();
	}
	if (bCountering)
	{
		const double Elapsed = GetWorld()->GetTimeSeconds() - CounterStartWorldSeconds;
		if (Elapsed < CounterCancelStartSeconds)
		{
			BufferInput(EBufferedRhythmAction::Block, InputAudioSeconds);
			return true;
		}
		bCountering = false;
	}
	return StartBlock(InputAudioSeconds);
}

bool UPlayerRhythmCombatComponent::StartBlock(double InputAudioSeconds)
{
	if(auto* Attack=GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>(); Attack && !Attack->CancelForDefense(false)) return false;
	BufferedAction = EBufferedRhythmAction::None;
	bBlocking = true;
	bBlockRecovering = false;
	bBlockReleasePending = false;
	bActiveHoldBroken = false;
	DefenseAnimationStartWorldSeconds = GetWorld()->GetTimeSeconds();
	LastBlockStartAudioSeconds = InputAudioSeconds;
	BlockStartWorldSeconds = GetWorld()->GetTimeSeconds();
	if (GuardUpSound) UGameplayStatics::PlaySound2D(this, GuardUpSound, .3f, 1.35f); // soft leather guard-up
	PlayDefenseAnimation(BlockStartAnimation, false);
	return true;
}

void UPlayerRhythmCombatComponent::EndBlock()
{
	if(bPhaseCinematic) return;
	if (!GetWorld()) return;
	// Preserve a buffered tap, including its release; it must not become a stuck hold.
	if (!bBlocking && BufferedAction == EBufferedRhythmAction::Block)
	{
		bBufferedBlockReleased = true;
		BufferedBlockReleaseAudioSeconds = GetCurrentInputAudioSeconds();
		LastBlockReleaseAudioSeconds = BufferedBlockReleaseAudioSeconds;
		return;
	}
	if (!bBlocking) return;
	bBlocking = false;
	if (UBossCombatCameraComponent* Camera = GetOwner()->FindComponentByClass<UBossCombatCameraComponent>())
		Camera->StopHoldFeedback(.06f);
	bBlockRecovering = true;
	const double ReleaseWorldSeconds = GetWorld()->GetTimeSeconds();
	const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	LastBlockReleaseAudioSeconds = Clock ? Clock->GetAudioNowSeconds() : ReleaseWorldSeconds;
	// Loop playback uses a sentinel animation time; release timing must use the
	// real press time or a held guard can remain in recovery forever.
	bBlockReleasePending = ReleaseWorldSeconds - BlockStartWorldSeconds < .22;
	if (!bBlockReleasePending)
	{
		DefenseAnimationStartWorldSeconds = ReleaseWorldSeconds;
		PlayDefenseAnimation(BlockEndAnimation, false);
	}
}

bool UPlayerRhythmCombatComponent::PrepareForOrdinaryAttack()
{
	if (!GetWorld() || Health<=0.f || bPhaseCinematic || bDodging || bCountering || bPoiseBroken || bHitReacting) return false;
	// An intentional attack leaves guard without scheduling a later guard-end
	// montage which could replace the new attack in DefaultSlot.
	bBlocking=false; bBlockRecovering=false; bBlockReleasePending=false;
	BufferedAction=EBufferedRhythmAction::None;
	bBufferedBlockReleased=false;
	if(auto* Camera=GetOwner()->FindComponentByClass<UBossCombatCameraComponent>()) Camera->StopHoldFeedback(.06f);
	return true;
}

bool UPlayerRhythmCombatComponent::IsMouseDefenseWindow() const
{
	return !GetMouseDefensePrompt().IsEmpty();
}

FString UPlayerRhythmCombatComponent::GetMouseDefensePrompt() const
{
	if(!GetWorld() || Health<=0.f || bPhaseCinematic) return FString();
	const auto* Clock=GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	if(!Clock || !Clock->IsRunning()) return FString();
	for(TActorIterator<AActor> It(GetWorld());It;++It)
	{
		const auto* Boss=It->FindComponentByClass<UBeatSyncedComboComponent>();
		if(!Boss || Boss->BossHealth<=0.f || Boss->IsPhaseTransitionActive() || FVector::DistSquared2D(It->GetActorLocation(),GetOwner()->GetActorLocation())>FMath::Square(600.f)) continue;
		TArray<FUpcomingImpactCue> Cues; Boss->GetUpcomingImpactCues(Cues,3,true);
		for(const auto& Cue:Cues)
		{
			if(Cue.DamageShape==EBossAttackDamageShape::GroundBurst) continue;
			const double Remaining=Cue.ImpactAudioSeconds-Clock->GetAudioNowSeconds();
			if(!PlayerAttackTiming::IsDefenseInputWindow(Remaining,Cue.IdealDodgeLeadSeconds)) continue;
			switch(Cue.RequiredResponse)
			{
			case ERhythmResponseType::Block: return TEXT("RMB  BLOCK");
			case ERhythmResponseType::Counter: return TEXT("LMB  COUNTER");
			case ERhythmResponseType::HoldBlock: return Cue.HoldMarker==ERhythmHoldMarkerType::End?TEXT("RELEASE RMB"):TEXT("HOLD RMB");
			case ERhythmResponseType::DodgeOrBlock: return TEXT("RMB BLOCK / SHIFT DODGE");
			default: return TEXT("SHIFT  DODGE");
			}
		}
	}
	return FString();
}

bool UPlayerRhythmCombatComponent::TryCounter()
{
	if(Health<=0.f || bPhaseCinematic) return false;
	if (auto* Attacks = GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>(); Attacks && !Attacks->CancelForDefense(false)) return false;
	if(bPhaseCinematic) return false;
	if (!GetWorld()) return false;
	const double InputAudioSeconds = GetCurrentInputAudioSeconds();
	CounterInputHistory.Add(InputAudioSeconds);
	if (CounterInputHistory.Num() > 16) CounterInputHistory.RemoveAt(0);
	if (BufferWhilePoiseLocked(EBufferedRhythmAction::Counter, InputAudioSeconds)) return true;
	if (bHitReacting) FinishHitReaction();
	if (bPoiseBroken) EndPoiseBreak();
	// Counter is an intentional transition out of guard. Requiring the block key
	// to finish its release event first made counter input unreliable in fast command
	// strings and replaced the player's real timing with a later animation time.
	if (bBlocking)
	{
		EndBlock();
		bBlockRecovering = false;
	}
	if (bDodging)
	{
		const double Elapsed = GetWorld()->GetTimeSeconds() - DodgeStartWorldSeconds;
		if (Elapsed < DodgeChainStartSeconds)
		{
			BufferInput(EBufferedRhythmAction::Counter, InputAudioSeconds);
			return true;
		}
		CancelDodgeForTransition();
	}
	if (bCountering)
	{
		const double Elapsed = GetWorld()->GetTimeSeconds() - CounterStartWorldSeconds;
		if (Elapsed < CounterCancelStartSeconds)
		{
			BufferInput(EBufferedRhythmAction::Counter, InputAudioSeconds);
			return true;
		}
		bCountering = false;
	}
	return StartCounter(InputAudioSeconds);
}

bool UPlayerRhythmCombatComponent::StartCounter(double InputAudioSeconds)
{
	if(auto* Attack=GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>(); Attack && !Attack->CancelForDefense(false)) return false;
	BufferedAction = EBufferedRhythmAction::None;
	bCountering = true;
	bRecoveryStrikePending = false; // General weapon sweeps now own pursuit damage.
	bBlockRecovering = false;
	bBlockReleasePending = false;
	CounterStartWorldSeconds = GetWorld()->GetTimeSeconds();
	LastCounterInputAudioSeconds = InputAudioSeconds;
	if (CounterSwishSound) UGameplayStatics::PlaySound2D(this, CounterSwishSound, .6f, .92f);
	PlayDefenseAnimation(CounterAnimation, false, CounterDurationSeconds);
	return true;
}

double UPlayerRhythmCombatComponent::GetCurrentInputAudioSeconds() const
{
	if (!GetWorld()) return -1000.0;
	if (const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>())
		return Clock->GetAudioNowSeconds();
	return GetWorld()->GetTimeSeconds();
}

void UPlayerRhythmCombatComponent::BufferInput(EBufferedRhythmAction Action, double InputAudioSeconds, FVector Direction)
{
	BufferedAction = Action;
	bBufferedBlockReleased = false;
	BufferedInputAudioSeconds = InputAudioSeconds;
	BufferedInputWorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : -1000.0;
	if (Action == EBufferedRhythmAction::Dodge) BufferedDodgeDirection = Direction;
	UE_LOG(LogTemp, Verbose, TEXT("RhythmInputBuffer: queued action=%d audio=%.3f"), static_cast<int32>(Action), InputAudioSeconds);
}

void UPlayerRhythmCombatComponent::CancelDodgeForTransition()
{
	if (!bDodging) return;
	bDodging = false;
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			Movement->SetMovementMode(MOVE_Walking);
	}
}

bool UPlayerRhythmCombatComponent::BufferWhilePoiseLocked(EBufferedRhythmAction Action,
	double InputAudioSeconds, FVector Direction)
{
	if (!GetWorld()) return false;
	const double Now = GetWorld()->GetTimeSeconds();
	const bool bPoiseLocked = bPoiseBroken && Now - PoiseBreakStartWorldSeconds < PoiseBreakInputLockSeconds;
	const bool bHitLocked = bHitReacting && Now < HitInputUnlockWorldSeconds;
	if (!bPoiseLocked && !bHitLocked) return false;
	BufferInput(Action, InputAudioSeconds, Direction);
	return true;
}

void UPlayerRhythmCombatComponent::ApplyPoiseDamage(float Amount, FVector ImpactLocation, FVector AttackDirection,
	FVector AttackerWorldLocation)
{
	if (Amount <= 0.0f || bPoiseBroken || !GetWorld()) return;
	Poise = FMath::Max(0.0f, Poise - Amount);
	LastPoiseDamageWorldSeconds = GetWorld()->GetTimeSeconds();
	UE_LOG(LogTemp, Display, TEXT("PlayerPoise: damage=%.1f remaining=%.1f/%.1f"), Amount, Poise, MaxPoise);
	if (Poise <= KINDA_SMALL_NUMBER) StartPoiseBreak(ImpactLocation, AttackDirection, AttackerWorldLocation);
}

void UPlayerRhythmCombatComponent::SpawnBloodFeedback(FVector ImpactLocation, FVector AttackDirection,
	bool bPoiseBreak) const
{
	if (!GetWorld() || !GetOwner()) return;
	UNiagaraSystem* Effect = bPoiseBreak ? BloodPoiseBreakEffect.Get() : BloodHitEffect.Get();
	if (!Effect) return;
	const FVector Torso = GetOwner()->GetActorLocation() + FVector(0, 0, 85.0f);
	if (ImpactLocation.IsNearlyZero() || FVector::DistSquared(ImpactLocation, Torso) > FMath::Square(190.0f))
		ImpactLocation = Torso;
	if (!AttackDirection.Normalize()) AttackDirection = -GetOwner()->GetActorForwardVector();
	const float Scale = bPoiseBreak ? BloodPoiseBreakScale : BloodHitScale;
	RetireCombatFX(UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Effect, ImpactLocation,
		AttackDirection.Rotation(), FVector(Scale*.65f)), .12f, .65f);
	UE_LOG(LogTemp, Display, TEXT("PlayerBloodVFX: heavy=%d effect=%s location=%s"),
		bPoiseBreak ? 1 : 0, *GetNameSafe(Effect), *ImpactLocation.ToString());
}

void UPlayerRhythmCombatComponent::StartPoiseBreak(FVector ImpactLocation, FVector AttackDirection,
	FVector AttackerWorldLocation)
{
	if (!GetWorld() || !GetOwner()) return;
	bPoiseBroken = true;
	Poise = 0.0f;
	PoiseBreakStartWorldSeconds = GetWorld()->GetTimeSeconds();
	bActiveHoldBroken = true;
	StartHitReaction(ImpactLocation, AttackDirection, true, AttackerWorldLocation);
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !Character->GetMesh()) return;
	FVector ReactionDirection = -HitFacingRotation.Vector();
	// A small capsule-swept displacement makes the break readable while keeping
	// the player close enough to continue the rhythm exchange.
	FHitResult PushHit;
	Character->AddActorWorldOffset(ReactionDirection * 34.0f, true, &PushHit);
	UE_LOG(LogTemp, Display, TEXT("PlayerPoise: BROKEN facingYaw=%.1f lock=%.2fs duration=%.2fs"),
		HitFacingRotation.Yaw, PoiseBreakInputLockSeconds, PoiseBreakDurationSeconds);
}

void UPlayerRhythmCombatComponent::EndPoiseBreak()
{
	if (!bPoiseBroken) return;
	bPoiseBroken = false;
	Poise = MaxPoise;
	FinishHitReaction();
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			Movement->SetMovementMode(MOVE_Walking);
	}
	UE_LOG(LogTemp, Display, TEXT("PlayerPoise: recovered %.1f/%.1f"), Poise, MaxPoise);
}

void UPlayerRhythmCombatComponent::StartHitReaction(FVector ImpactLocation, FVector AttackDirection, bool bHeavy,
	FVector AttackerWorldLocation)
{
	if(auto* Attacks=GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>()) Attacks->NotifyInterrupted();
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!GetWorld() || !Character || !Character->GetMesh()) return;
	FVector TowardAttacker = AttackerWorldLocation.IsNearlyZero()
		? FVector::ZeroVector
		: (AttackerWorldLocation - Character->GetActorLocation()).GetSafeNormal2D();
	if (TowardAttacker.IsNearlyZero())
		TowardAttacker = (ImpactLocation - Character->GetActorLocation()).GetSafeNormal2D();
	if (TowardAttacker.IsNearlyZero())
		TowardAttacker = -AttackDirection.GetSafeNormal2D();
	if (TowardAttacker.IsNearlyZero())
		TowardAttacker = Character->GetActorForwardVector().GetSafeNormal2D();
	HitFacingRotation = FRotator(0.0f, TowardAttacker.Rotation().Yaw, 0.0f);
	const double Now = GetWorld()->GetTimeSeconds();
	const float Duration = bHeavy ? PoiseBreakDurationSeconds : LightHitDurationSeconds;
	bHitReacting = true;
	bHitTurnPending = true;
	HitTurnEndWorldSeconds = Now + HitTurnSeconds;
	HitReactionEndWorldSeconds = Now + HitTurnSeconds + Duration;
	// Input lock is independent of the longer visual recoil and turn duration.
	HitInputUnlockWorldSeconds = Now + (bHeavy ? PoiseBreakInputLockSeconds : LightHitInputLockSeconds);
	BufferedAction = EBufferedRhythmAction::None;
	bBufferedBlockReleased = false;
	bDodging = false;
	bBlocking = false;
	bBlockRecovering = false;
	bBlockReleasePending = false;
	bCountering = false;
	Character->ConsumeMovementInputVector();
	Character->SetAnimRootMotionTranslationScale(0.0f);
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
		Movement->StopMovementImmediately();
	}
	if (UAnimInstance* Animation = Character->GetMesh()->GetAnimInstance())
		Animation->Montage_Stop(.04f);
	if (UBossCombatCameraComponent* Camera = Character->FindComponentByClass<UBossCombatCameraComponent>())
		Camera->StopHoldFeedback(.04f);
	UE_LOG(LogTemp, Display, TEXT("PlayerHitReaction: heavy=%d turnYaw=%.1f turn=%.2fs duration=%.2fs"),
		bHeavy ? 1 : 0, HitFacingRotation.Yaw, HitTurnSeconds, Duration);
}

void UPlayerRhythmCombatComponent::FinishHitReaction()
{
	if (!bHitReacting) return;
	bHitReacting = false;
	bHitTurnPending = false;
	HitReactionEndWorldSeconds = -1000.0;
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
		if (UPlayerCombatAnimInstance* Animation = Cast<UPlayerCombatAnimInstance>(Character->GetMesh()->GetAnimInstance()))
			Animation->EndDirectionalHit();
}

void UPlayerRhythmCombatComponent::ProcessBufferedInput()
{
	if (Health<=0.f) { BufferedAction=EBufferedRhythmAction::None; return; }
	if (BufferedAction == EBufferedRhythmAction::None || !GetWorld()) return;
	const double Now = GetWorld()->GetTimeSeconds();
	double BufferDeadline = BufferedInputWorldSeconds + InputBufferSeconds;
	if (bHitReacting) BufferDeadline = FMath::Max(BufferDeadline, HitInputUnlockWorldSeconds + .05);
	if (bPoiseBroken) BufferDeadline = FMath::Max(BufferDeadline, PoiseBreakStartWorldSeconds + PoiseBreakInputLockSeconds + .05);
	if (Now > BufferDeadline)
	{
		UE_LOG(LogTemp, Verbose, TEXT("RhythmInputBuffer: expired action=%d"), static_cast<int32>(BufferedAction));
		BufferedAction = EBufferedRhythmAction::None;
		return;
	}
	if (bDodging && Now - DodgeStartWorldSeconds < DodgeChainStartSeconds) return;
	if (bCountering && Now - CounterStartWorldSeconds < CounterCancelStartSeconds) return;
	if (bPoiseBroken && Now - PoiseBreakStartWorldSeconds < PoiseBreakInputLockSeconds) return;
	if (bHitReacting && Now < HitInputUnlockWorldSeconds) return;
	if (bPoiseBroken) EndPoiseBreak();
	if (bHitReacting) FinishHitReaction();

	const EBufferedRhythmAction Action = BufferedAction;
	const double OriginalInputAudioSeconds = BufferedInputAudioSeconds;
	const FVector OriginalDirection = BufferedDodgeDirection;
	const bool bReleasedBlock = bBufferedBlockReleased;
	const double OriginalReleaseAudioSeconds = BufferedBlockReleaseAudioSeconds;
	BufferedAction = EBufferedRhythmAction::None;
	if (bDodging) CancelDodgeForTransition();
	if (bCountering) bCountering = false;
	bool bStarted = false;
	switch (Action)
	{
	case EBufferedRhythmAction::Dodge: bStarted = StartDodge(OriginalDirection, OriginalInputAudioSeconds); break;
	case EBufferedRhythmAction::Block:
		bStarted = !bBlocking && StartBlock(OriginalInputAudioSeconds);
		if (bStarted && bReleasedBlock)
		{
			EndBlock();
			LastBlockReleaseAudioSeconds = OriginalReleaseAudioSeconds;
		}
		break;
	case EBufferedRhythmAction::Counter:
		if (bBlocking)
		{
			EndBlock();
			bBlockRecovering = false;
		}
		bStarted = StartCounter(OriginalInputAudioSeconds);
		break;
	default: break;
	}
	UE_LOG(LogTemp, Display, TEXT("RhythmInputBuffer: dispatched action=%d accepted=%d delayMs=%.1f originalAudio=%.3f"),
		static_cast<int32>(Action), bStarted ? 1 : 0,
		static_cast<float>((Now - BufferedInputWorldSeconds) * 1000.0), OriginalInputAudioSeconds);
}

double UPlayerRhythmCombatComponent::ConsumeClosestInput(TArray<double>& History, double IdealAudioSeconds)
{
	History.RemoveAll([&](double Time) { return Time < IdealAudioSeconds - .5; });
	int32 Best = INDEX_NONE;
	double Error = .5;
	for (int32 Index = 0; Index < History.Num(); ++Index)
	{
		const double Candidate = FMath::Abs(History[Index] - IdealAudioSeconds);
		if (Candidate < Error) { Best = Index; Error = Candidate; }
	}
	if (Best == INDEX_NONE) return -1000.0;
	const double Result = History[Best];
	History.RemoveAt(Best);
	return Result;
}

ERhythmDodgeJudgment UPlayerRhythmCombatComponent::JudgeTimedInput(double InputAudioSeconds, double IdealAudioSeconds) const
{
	const float ErrorMs = static_cast<float>(FMath::Abs(InputAudioSeconds - IdealAudioSeconds) * 1000.0);
	if (ErrorMs <= PerfectWindowMs) return ERhythmDodgeJudgment::Perfect;
	if (ErrorMs <= GoodWindowMs) return ERhythmDodgeJudgment::Good;
	return ERhythmDodgeJudgment::Bad;
}

ERhythmDodgeJudgment UPlayerRhythmCombatComponent::JudgeCounterInput(double InputAudioSeconds, double IdealAudioSeconds) const
{
	// Counter uses the same windows drawn by the command rail. Previously this
	// silently used 45/80 ms while the HUD showed 60/120 ms, so a visibly valid
	// click could be graded BAD.
	return JudgeTimedInput(InputAudioSeconds, IdealAudioSeconds);
}

bool UPlayerRhythmCombatComponent::IsInvulnerable() const
{
	if (!bDodging || !GetWorld()) return false;
	// Buffered input may switch the animation a few frames after the button was
	// pressed. Gameplay timing follows the original audio-clock timestamp so the
	// buffer cannot move an otherwise correct dodge outside its i-frames.
	double Elapsed = GetWorld()->GetTimeSeconds() - DodgeStartWorldSeconds;
	if (const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
		Clock && LastDodgeInputAudioSeconds > -900.0)
	{
		Elapsed = Clock->GetAudioNowSeconds() - LastDodgeInputAudioSeconds;
	}
	return Elapsed >= InvulnerabilityStartSeconds && Elapsed <= InvulnerabilityEndSeconds;
}

bool UPlayerRhythmCombatComponent::ResolveBossContact(double ContactAudioSeconds, double TargetBeat, float Damage,
	float IdealDodgeLeadSeconds, bool bStrongBeat, FVector BladeWorldLocation, FVector AttackDirection,
	ERhythmResponseType RequiredResponse, ERhythmHoldMarkerType HoldMarker, FName HoldGroup,
	FVector AttackerWorldLocation)
{
	return ResolveResponse(ContactAudioSeconds, TargetBeat, Damage, IdealDodgeLeadSeconds, bStrongBeat,
		BladeWorldLocation, AttackDirection, RequiredResponse, HoldMarker, HoldGroup, true,
		AttackerWorldLocation);
}

bool UPlayerRhythmCombatComponent::ResolveBossAvoidance(double ImpactAudioSeconds, double TargetBeat,
	float IdealDodgeLeadSeconds, bool bStrongBeat, FVector BladeWorldLocation, FVector AttackDirection,
	ERhythmResponseType RequiredResponse, ERhythmHoldMarkerType HoldMarker, FName HoldGroup)
{
	return ResolveResponse(ImpactAudioSeconds, TargetBeat, 0.0f, IdealDodgeLeadSeconds, bStrongBeat,
		BladeWorldLocation, AttackDirection, RequiredResponse, HoldMarker, HoldGroup, false);
}

bool UPlayerRhythmCombatComponent::ResolveResponse(double ImpactAudioSeconds, double TargetBeat, float Damage,
	float IdealLeadSeconds, bool bStrongBeat, FVector BladeWorldLocation, FVector AttackDirection,
	ERhythmResponseType RequiredResponse, ERhythmHoldMarkerType HoldMarker, FName HoldGroup, bool bPhysicalContact,
	FVector AttackerWorldLocation)
{
	const int64 ImpactKey = FMath::RoundToInt64(ImpactAudioSeconds * 10000.0);
	if (Health<=0.f) return false;
	if (ResolvedImpactKeys.Contains(ImpactKey)) return true;
	ResolvedImpactKeys.Add(ImpactKey);
	if (ResolvedImpactKeys.Num() > 64) ResolvedImpactKeys.Reset();

	const double IdealInput = ImpactAudioSeconds - IdealLeadSeconds;
	if(RequiredResponse==ERhythmResponseType::DodgeOrBlock)
	{
		// Select one actual defense and consume only its input. A stale block must
		// never turn a valid dodge into BAD, or vice versa.
		double BlockError=1000.;
		for(double Time:BlockInputHistory) BlockError=FMath::Min(BlockError,FMath::Abs(Time-IdealInput));
		const bool CanDodge=IsInvulnerable() || (!bPhysicalContact && DodgeSequenceId>0 &&
			ImpactAudioSeconds-LastDodgeInputAudioSeconds>=-.10 && ImpactAudioSeconds-LastDodgeInputAudioSeconds<=.55);
		const double DodgeError=FMath::Abs(LastDodgeInputAudioSeconds-IdealInput);
		const bool GoodBlock=BlockError*1000.<=GoodWindowMs;
		RequiredResponse=CanDodge && (!GoodBlock || DodgeError<BlockError)
			? ERhythmResponseType::Dodge:ERhythmResponseType::Block;
	}
	LastResponse = RequiredResponse;
	double JudgmentTargetAudioSeconds = IdealInput;
	double InputTime = -1000.0;
	bool bDefended = false;
	float DamageMultiplier = 1.0f;
	ERhythmDodgeJudgment Grade = ERhythmDodgeJudgment::Miss;

	switch (RequiredResponse)
	{
	case ERhythmResponseType::Dodge:
		InputTime = LastDodgeInputAudioSeconds;
		bDefended = IsInvulnerable() || (!bPhysicalContact && DodgeSequenceId > 0 &&
			ImpactAudioSeconds - LastDodgeInputAudioSeconds >= -0.10 && ImpactAudioSeconds - LastDodgeInputAudioSeconds <= 0.55);
		Grade = bDefended ? JudgeTimedInput(InputTime, IdealInput) : ERhythmDodgeJudgment::Miss;
		if (bDodging) LastResolvedDodgeSequenceId = DodgeSequenceId;
		break;
	case ERhythmResponseType::Block:
		InputTime = ConsumeClosestInput(BlockInputHistory, IdealInput);
		Grade = InputTime > -900.0 ? JudgeTimedInput(InputTime, IdealInput) : ERhythmDodgeJudgment::Miss;
		bDefended = Grade == ERhythmDodgeJudgment::Perfect || Grade == ERhythmDodgeJudgment::Good;
		if (Grade == ERhythmDodgeJudgment::Bad) DamageMultiplier = .35f;
		break;
	case ERhythmResponseType::Counter:
		InputTime = ConsumeClosestInput(CounterInputHistory, IdealInput);
		Grade = JudgeCounterInput(InputTime, IdealInput);
		bDefended = Grade == ERhythmDodgeJudgment::Perfect || Grade == ERhythmDodgeJudgment::Good;
		if (Grade == ERhythmDodgeJudgment::Bad) DamageMultiplier = .60f;
		if (!bDefended && InputTime < 0.0) Grade = ERhythmDodgeJudgment::Miss;
		break;
	case ERhythmResponseType::HoldBlock:
		if (HoldMarker == ERhythmHoldMarkerType::Start)
		{
			ActiveHoldGroup = HoldGroup;
			ReportedHoldBreakGroups.Remove(HoldGroup);
			bActiveHoldBroken = !bBlocking;
			InputTime = LastBlockStartAudioSeconds;
			bDefended = bBlocking;
			Grade = bDefended ? JudgeTimedInput(InputTime, IdealInput) : ERhythmDodgeJudgment::HoldBreak;
		}
		else if (HoldMarker == ERhythmHoldMarkerType::End)
		{
			JudgmentTargetAudioSeconds = ImpactAudioSeconds;
			const bool bSameGroup = ActiveHoldGroup == HoldGroup && !bActiveHoldBroken;
			if (bBlocking && bSameGroup)
			{
				bDefended = true;
				Grade = ERhythmDodgeJudgment::Good; // held through the tail, safe but no release bonus
			}
			else if (bSameGroup && LastBlockReleaseAudioSeconds > -900.0)
			{
				InputTime = LastBlockReleaseAudioSeconds;
				const float ReleaseErrorMs = FMath::Abs(static_cast<float>((InputTime - ImpactAudioSeconds) * 1000.0));
				bDefended = ReleaseErrorMs <= GoodWindowMs;
				Grade = ReleaseErrorMs <= PerfectWindowMs ? ERhythmDodgeJudgment::PerfectHold :
					ReleaseErrorMs <= GoodWindowMs ? ERhythmDodgeJudgment::GoodHold : ERhythmDodgeJudgment::HoldBreak;
			}
			else Grade = ERhythmDodgeJudgment::HoldBreak;
			ActiveHoldGroup = NAME_None;
		}
		else
		{
			bDefended = bBlocking && !bActiveHoldBroken && ActiveHoldGroup == HoldGroup;
			Grade = bDefended ? ERhythmDodgeJudgment::Good : ERhythmDodgeJudgment::HoldBreak;
			if (!bDefended) bActiveHoldBroken = true;
		}
		break;
	}

	const float ErrorMs = InputTime > -900.0 ? static_cast<float>((InputTime - JudgmentTargetAudioSeconds) * 1000.0) : 0.0f;
	// A broken hold is one player mistake, even if several blade contacts remain inside
	// the same authored hold range. Keep resolving damage, but emit only one BREAK card.
	const bool bDuplicateHoldBreak = Grade == ERhythmDodgeJudgment::HoldBreak &&
		ReportedHoldBreakGroups.Contains(HoldGroup);
	const int32 PreviousCombo = ComboCount;
	if (!bDuplicateHoldBreak)
	{
		SubmitDodgeJudgment(Grade, ErrorMs, bStrongBeat);
		if (Grade == ERhythmDodgeJudgment::HoldBreak && !HoldGroup.IsNone())
			ReportedHoldBreakGroups.Add(HoldGroup);
	}
	if (!bDuplicateHoldBreak)
	{
		FRhythmSuccessFeedbackData Feedback;
		Feedback.Response = RequiredResponse;
		Feedback.HoldMarker = HoldMarker;
		Feedback.Grade = Grade;
		Feedback.PreviousCombo = PreviousCombo;
		Feedback.ComboCount = ComboCount;
		Feedback.FeedbackTier = GetFeedbackTierForCombo(ComboCount);
		Feedback.bSuccess = Grade == ERhythmDodgeJudgment::Perfect || Grade == ERhythmDodgeJudgment::Good ||
			Grade == ERhythmDodgeJudgment::GoodHold || Grade == ERhythmDodgeJudgment::PerfectHold;
		Feedback.bPerfect = Grade == ERhythmDodgeJudgment::Perfect || Grade == ERhythmDodgeJudgment::PerfectHold;
		const bool bTierCrossed = GetFeedbackTierForCombo(ComboCount) > GetFeedbackTierForCombo(PreviousCombo);
		Feedback.bKeyPerfect = Feedback.bPerfect && (RequiredResponse == ERhythmResponseType::Counter ||
			Grade == ERhythmDodgeJudgment::PerfectHold || bTierCrossed);
		Feedback.bStrongBeat = bStrongBeat;
		Feedback.WorldLocation = BladeWorldLocation;
		Feedback.AttackDirection = AttackDirection;
		Feedback.EventId = ++FeedbackEventSerial;
		OnRhythmSuccessFeedback.Broadcast(Feedback);
	}
	PlayResponseFeedback(RequiredResponse, Grade, BladeWorldLocation, AttackDirection, bStrongBeat);
	if (RequiredResponse == ERhythmResponseType::Dodge && Grade == ERhythmDodgeJudgment::Perfect)
		BroadcastPerfectFeedback(ErrorMs, bStrongBeat, BladeWorldLocation, AttackDirection);
	if (!bDefended && Damage > 0.0f) ApplyCombatDamage(Damage * DamageMultiplier);
	if (!bDuplicateHoldBreak)
	{
		if (Grade == ERhythmDodgeJudgment::Bad)
		{
			if (bPhysicalContact && !bDefended && Damage > 0.0f)
				SpawnBloodFeedback(BladeWorldLocation, AttackDirection, Poise <= BadPoiseDamage);
			ApplyPoiseDamage(BadPoiseDamage, BladeWorldLocation, AttackDirection, AttackerWorldLocation);
		}
		else if (bPhysicalContact && !bDefended && Damage > 0.0f)
		{
			SpawnBloodFeedback(BladeWorldLocation, AttackDirection, Poise <= DirectHitPoiseDamage);
			ApplyPoiseDamage(DirectHitPoiseDamage, BladeWorldLocation, AttackDirection, AttackerWorldLocation);
		}
	}
	// Every blade contact that really damages the player now produces a visible
	// reaction. A poise break already starts the stronger version above.
	if (bPhysicalContact && !bDefended && Damage > 0.0f && !bPoiseBroken)
		StartHitReaction(BladeWorldLocation, AttackDirection, false, AttackerWorldLocation);
	UE_LOG(LogTemp, Display, TEXT("RhythmResponse: type=%s hold=%s grade=%s errorMs=%.1f defended=%d damage=%.1f beat=%.2f"),
		*UEnum::GetValueAsString(RequiredResponse), *UEnum::GetValueAsString(HoldMarker),
		*UEnum::GetValueAsString(Grade), ErrorMs, bDefended ? 1 : 0, bDefended ? 0.0f : Damage, TargetBeat);
	return bDefended;
}

void UPlayerRhythmCombatComponent::PlayResponseFeedback(ERhythmResponseType Response,
	ERhythmDodgeJudgment Grade, FVector WorldLocation, FVector AttackDirection, bool bStrongBeat)
{
	if (!GetWorld() || Grade == ERhythmDodgeJudgment::Miss ||
		Grade == ERhythmDodgeJudgment::HoldBreak) return;
	if (WorldLocation.IsNearlyZero() && GetOwner()) WorldLocation = GetOwner()->GetActorLocation() + FVector(0,0,90);
	const FRotator Rotation = AttackDirection.IsNearlyZero() ? FRotator::ZeroRotator : AttackDirection.Rotation();
	UParticleSystem* Effect = Response == ERhythmResponseType::Counter ? CounterImpactEffect.Get() : BlockImpactEffect.Get();
	if (Effect)
	{
		const float GradeScale = Grade == ERhythmDodgeJudgment::Perfect || Grade == ERhythmDodgeJudgment::PerfectHold ? 1.65f :
			(Grade == ERhythmDodgeJudgment::Good || Grade == ERhythmDodgeJudgment::GoodHold) ? 1.15f : .75f;
		RetireCombatFX(UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Effect, WorldLocation, Rotation,
			FVector(GradeScale * (bStrongBeat ? 1.25f : 1.0f)), true), .15f, .45f);
	}
	// Timing feedback must not replace the player's action animation. A BAD can
	// resolve after a newer command has begun, including an attack with no contact.
	if (Grade == ERhythmDodgeJudgment::PerfectHold && PerfectHoldShockEffect && GetOwner())
	{
		SpawnStandalonePerfectSparks(GetWorld(), PerfectHoldShockEffect,
			GetOwner()->GetActorLocation() + FVector(0, 0, 70), FRotator::ZeroRotator, 1.85f);
	}
}

void UPlayerRhythmCombatComponent::BroadcastPerfectFeedback(float ErrorMs, bool bStrongBeat,
	FVector BladeWorldLocation, FVector AttackDirection)
{
	if (!GetOwner()) return;
	if (BladeWorldLocation.IsNearlyZero())
		BladeWorldLocation = GetOwner()->GetActorLocation() + FVector(0, 0, 80.0f);
	if (!AttackDirection.Normalize())
		AttackDirection = -GetOwner()->GetActorForwardVector();
	FPerfectDodgeFeedbackData Feedback;
	Feedback.BladeWorldLocation = BladeWorldLocation;
	Feedback.AttackDirection = AttackDirection;
	Feedback.SignedErrorMs = ErrorMs;
	Feedback.ComboCount = ComboCount;
	Feedback.ResonanceFraction = GetResonanceFraction();
	Feedback.bStrongBeat = bStrongBeat;
	OnPerfectDodge.Broadcast(Feedback);
	UE_LOG(LogTemp, Display, TEXT("PerfectDodgeFeedback: strong=%d errorMs=%.1f combo=%d resonance=%.2f location=%s"),
		bStrongBeat ? 1 : 0, ErrorMs, ComboCount, Feedback.ResonanceFraction, *BladeWorldLocation.ToCompactString());
}
