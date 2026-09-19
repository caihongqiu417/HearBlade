#include "Combat/PlayerRhythmCombatComponent.h"
#include "Combat/BeatSyncedComboComponent.h"
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

UPlayerRhythmCombatComponent::UPlayerRhythmCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	DodgeForwardAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/SwordAnim/Animation/UE5/roll_front.roll_front"));
	DodgeBackwardAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/SwordAnim/Animation/UE5/roll_back.roll_back"));
	DodgeLeftAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/SwordAnim/Animation/UE5/roll_left.roll_left"));
	DodgeRightAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/SwordAnim/Animation/UE5/roll_right.roll_right"));
}

void UPlayerRhythmCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
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
	const bool bCanChainAutomatedDodge = !bDodging || (GetWorld() &&
		GetWorld()->GetTimeSeconds() - DodgeStartWorldSeconds >= DodgeChainStartSeconds);
	if (bAutomationTestMode && GetWorld() && GetOwner() && bCanChainAutomatedDodge)
	{
		if (const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>())
		{
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			{
				UBeatSyncedComboComponent* Combo = It->FindComponentByClass<UBeatSyncedComboComponent>();
				double ImpactAudioSeconds = 0.0;
				float IdealLeadSeconds = 0.14f;
				if (!Combo || !Combo->GetNextImpactDodgeTiming(ImpactAudioSeconds, IdealLeadSeconds)) continue;
				const double SecondsUntilImpact = ImpactAudioSeconds - Clock->GetAudioNowSeconds();
				// Sample the full PERFECT interval so a throttled background editor cannot skip it between frames.
				const double EarliestPerfectLead = IdealLeadSeconds + PerfectWindowMs / 1000.0;
				const double LatestPerfectLead = FMath::Max<double>(InvulnerabilityStartSeconds, IdealLeadSeconds - PerfectWindowMs / 1000.0);
				if (ImpactAudioSeconds > LastAutomatedImpactAudioSeconds + 0.001 &&
					SecondsUntilImpact <= EarliestPerfectLead && SecondsUntilImpact >= LatestPerfectLead)
				{
					const FVector ToBoss = (It->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal2D();
					if (TryDodge(FVector::CrossProduct(FVector::UpVector, ToBoss)))
					{
						LastAutomatedImpactAudioSeconds = ImpactAudioSeconds;
						UE_LOG(LogTemp, Display, TEXT("RhythmCombatAutomation: dodge requested leadMs=%.1f impactAudio=%.3f"),
							SecondsUntilImpact * 1000.0, ImpactAudioSeconds);
					}
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
	if (Elapsed >= DodgeDurationSeconds)
	{
		bDodging = false;
		if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
		{
			Character->SetAnimRootMotionTranslationScale(1.0f);
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
				Movement->SetMovementMode(MOVE_Walking);
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
	Health = FMath::Clamp(Health, 0.0f, MaxHealth);
	bAutomationTestMode = FParse::Param(FCommandLine::Get(), TEXT("RhythmCombatAutoTest"));
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
	ComboCount = 0;
	OnHealthChanged.Broadcast(Health, MaxHealth);
}

void UPlayerRhythmCombatComponent::RestoreHealth(float Amount)
{
	if (Amount <= 0.0f) return;
	Health = FMath::Clamp(Health + Amount, 0.0f, MaxHealth);
	OnHealthChanged.Broadcast(Health, MaxHealth);
}

void UPlayerRhythmCombatComponent::SubmitDodgeJudgment(ERhythmDodgeJudgment Grade, float SignedErrorMs, bool bStrongBeat)
{
	if (bHUDPreviewMode && HUDPreviewStep >= 6 && !bSubmittingHUDPreviewJudgment) return;
	LastJudgment = Grade;
	LastJudgmentErrorMs = SignedErrorMs;
	LastJudgmentWorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Grade == ERhythmDodgeJudgment::Perfect || Grade == ERhythmDodgeJudgment::Good)
	{
		++ComboCount;
		const float BaseGain = Grade == ERhythmDodgeJudgment::Perfect ? 12.0f : 6.0f;
		const float ComboMultiplier = ComboCount >= 8 ? 1.5f : (ComboCount >= 4 ? 1.25f : 1.0f);
		Resonance = FMath::Clamp(Resonance + BaseGain * ComboMultiplier * (bStrongBeat ? 1.25f : 1.0f), 0.0f, MaxResonance);
	}
	else if (Grade == ERhythmDodgeJudgment::Bad || Grade == ERhythmDodgeJudgment::Miss)
	{
		ComboCount = 0;
	}
	OnDodgeJudged.Broadcast(Grade, SignedErrorMs, ComboCount);
}

void UPlayerRhythmCombatComponent::ResetCombatState()
{
	Health = MaxHealth;
	Resonance = 0.0f;
	ComboCount = 0;
	LastJudgment = ERhythmDodgeJudgment::None;
	LastJudgmentErrorMs = 0.0f;
	LastJudgmentWorldSeconds = -1000.0;
	OnHealthChanged.Broadcast(Health, MaxHealth);
}

bool UPlayerRhythmCombatComponent::TryDodge(FVector WorldDirection)
{
	if (!GetWorld() || !GetOwner()) return false;
	if (bDodging)
	{
		const double Elapsed = GetWorld()->GetTimeSeconds() - DodgeStartWorldSeconds;
		if (Elapsed < DodgeChainStartSeconds) return false;
		UE_LOG(LogTemp, Verbose, TEXT("RhythmDodgeMotion: chained after %.3fs"), Elapsed);
	}
	WorldDirection.Z = 0.0f;
	if (!WorldDirection.Normalize()) WorldDirection = GetOwner()->GetActorForwardVector().GetSafeNormal2D();
	DodgeDirection = WorldDirection;
	DodgeStartLocation = GetOwner()->GetActorLocation();
	bDodging = true;
	DodgeDistanceApplied = 0.0f;
	++DodgeSequenceId;
	DodgeStartWorldSeconds = GetWorld()->GetTimeSeconds();
	if (const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>())
		LastDodgeInputAudioSeconds = Clock->GetAudioNowSeconds();
	else LastDodgeInputAudioSeconds = DodgeStartWorldSeconds;

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
			Character->GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			Character->GetMesh()->PlayAnimation(Chosen, false);
			if (UAnimSingleNodeInstance* Instance = Character->GetMesh()->GetSingleNodeInstance())
				Instance->SetPlayRate(Chosen->GetPlayLength() / FMath::Max(DodgeDurationSeconds, 0.1f));
		}
	}
	return true;
}

bool UPlayerRhythmCombatComponent::IsInvulnerable() const
{
	if (!bDodging || !GetWorld()) return false;
	const double Elapsed = GetWorld()->GetTimeSeconds() - DodgeStartWorldSeconds;
	return Elapsed >= InvulnerabilityStartSeconds && Elapsed <= InvulnerabilityEndSeconds;
}

bool UPlayerRhythmCombatComponent::ResolveBossContact(double ContactAudioSeconds, double TargetBeat, float Damage,
	float IdealDodgeLeadSeconds, bool bStrongBeat, FVector BladeWorldLocation, FVector AttackDirection)
{
	const bool bAvoided = IsInvulnerable();
	const double IdealInput = ContactAudioSeconds - IdealDodgeLeadSeconds;
	float ErrorMs = 0.0f;
	ERhythmDodgeJudgment Grade = ERhythmDodgeJudgment::Miss;
	if (bDodging)
	{
		ErrorMs = static_cast<float>((LastDodgeInputAudioSeconds - IdealInput) * 1000.0);
		const float AbsoluteError = FMath::Abs(ErrorMs);
		if (bAvoided && AbsoluteError <= PerfectWindowMs) Grade = ERhythmDodgeJudgment::Perfect;
		else if (bAvoided && AbsoluteError <= GoodWindowMs) Grade = ERhythmDodgeJudgment::Good;
		else Grade = ERhythmDodgeJudgment::Bad;
	}
	const bool bAlreadyResolvedThisDodge = bDodging && LastResolvedDodgeSequenceId == DodgeSequenceId;
	if (!bAlreadyResolvedThisDodge)
	{
		SubmitDodgeJudgment(Grade, ErrorMs, bStrongBeat);
		if (Grade == ERhythmDodgeJudgment::Perfect)
			BroadcastPerfectFeedback(ErrorMs, bStrongBeat, BladeWorldLocation, AttackDirection);
		if (bDodging) LastResolvedDodgeSequenceId = DodgeSequenceId;
	}
	if (!bAvoided) ApplyCombatDamage(Damage);
	UE_LOG(LogTemp, Display, TEXT("RhythmDodge: grade=%s errorMs=%.1f avoided=%d duplicate=%d damage=%.1f beat=%.2f combo=%d health=%.1f"),
		*UEnum::GetValueAsString(Grade), ErrorMs, bAvoided ? 1 : 0,
		bAlreadyResolvedThisDodge ? 1 : 0, bAvoided ? 0.0f : Damage, TargetBeat, ComboCount, Health);
	return bAvoided;
}

bool UPlayerRhythmCombatComponent::ResolveBossAvoidance(double ImpactAudioSeconds, double TargetBeat,
	float IdealDodgeLeadSeconds, bool bStrongBeat, FVector BladeWorldLocation, FVector AttackDirection)
{
	// The damage window can be crossed during a low frame-rate hitch after the visual roll has ended.
	// Judge from the recorded input timestamp so a real spatial evade is not lost to tick order.
	const double InputLeadSeconds = ImpactAudioSeconds - LastDodgeInputAudioSeconds;
	if (DodgeSequenceId == 0 || LastResolvedDodgeSequenceId == DodgeSequenceId ||
		InputLeadSeconds < -0.10 || InputLeadSeconds > 0.55) return false;
	const double IdealInput = ImpactAudioSeconds - IdealDodgeLeadSeconds;
	const float ErrorMs = static_cast<float>((LastDodgeInputAudioSeconds - IdealInput) * 1000.0);
	const float AbsoluteError = FMath::Abs(ErrorMs);
	ERhythmDodgeJudgment Grade = ERhythmDodgeJudgment::Bad;
	if (AbsoluteError <= PerfectWindowMs) Grade = ERhythmDodgeJudgment::Perfect;
	else if (AbsoluteError <= GoodWindowMs) Grade = ERhythmDodgeJudgment::Good;
	SubmitDodgeJudgment(Grade, ErrorMs, bStrongBeat);
	if (Grade == ERhythmDodgeJudgment::Perfect)
		BroadcastPerfectFeedback(ErrorMs, bStrongBeat, BladeWorldLocation, AttackDirection);
	LastResolvedDodgeSequenceId = DodgeSequenceId;
	UE_LOG(LogTemp, Display, TEXT("RhythmDodgeAvoidance: grade=%s errorMs=%.1f beat=%.2f combo=%d resonance=%.1f"),
		*UEnum::GetValueAsString(Grade), ErrorMs, TargetBeat, ComboCount, Resonance);
	return true;
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
