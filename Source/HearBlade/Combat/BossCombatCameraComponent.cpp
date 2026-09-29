#include "Combat/BossCombatCameraComponent.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Combat/BeatSyncedComboComponent.h"
#include "Combat/BossBeatDirector.h"
#include "Combat/BossCameraConfig.h"
#include "Combat/BossCameraModifier.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HearBladeCharacter.h"
#include "Components/CapsuleComponent.h"

UBossCombatCameraComponent::UBossCombatCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

const UBossCameraConfig* UBossCombatCameraComponent::Settings() const
{
	return Config ? Config.Get() : GetDefault<UBossCameraConfig>();
}

void UBossCombatCameraComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!Config)
		Config = LoadObject<UBossCameraConfig>(nullptr,
			TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_BossCamera.DA_BossCamera"));
	if (const AHearBladeCharacter* Player = Cast<AHearBladeCharacter>(GetOwner()))
		if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
			if (PC->PlayerCameraManager)
				Modifier = Cast<UBossCameraModifier>(PC->PlayerCameraManager->AddNewCameraModifier(UBossCameraModifier::StaticClass()));
}

void UBossCombatCameraComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearCombatTarget();
	if (const AHearBladeCharacter* Player = Cast<AHearBladeCharacter>(GetOwner()))
		if (const APlayerController* PC = Cast<APlayerController>(Player->GetController()))
			if (PC->PlayerCameraManager && Modifier.IsValid()) PC->PlayerCameraManager->RemoveCameraModifier(Modifier.Get());
	Super::EndPlay(Reason);
}

void UBossCombatCameraComponent::BindCombo(UBeatSyncedComboComponent* Combo)
{
	if (UBeatSyncedComboComponent* Old = BoundCombo.Get())
	{
		Old->OnBlinkWarning.RemoveDynamic(this, &ThisClass::HandleWarning);
		Old->OnBlinkSucceeded.RemoveDynamic(this, &ThisClass::HandleBlink);
		Old->OnBlinkCancelled.RemoveDynamic(this, &ThisClass::HandleCancelled);
		Old->OnConfirmedHit.RemoveDynamic(this, &ThisClass::HandleConfirmedHit);
		Old->OnCinematicAccentStarted.RemoveDynamic(this, &ThisClass::HandleCinematicStarted);
		Old->OnCinematicAccentImpact.RemoveDynamic(this, &ThisClass::HandleCinematicImpact);
		Old->OnCinematicAccentEnded.RemoveDynamic(this, &ThisClass::HandleCinematicEnded);
	}
	BoundCombo = Combo;
	if (Combo)
	{
		Combo->OnBlinkWarning.AddUniqueDynamic(this, &ThisClass::HandleWarning);
		Combo->OnBlinkSucceeded.AddUniqueDynamic(this, &ThisClass::HandleBlink);
		Combo->OnBlinkCancelled.AddUniqueDynamic(this, &ThisClass::HandleCancelled);
		Combo->OnConfirmedHit.AddUniqueDynamic(this, &ThisClass::HandleConfirmedHit);
		Combo->OnCinematicAccentStarted.AddUniqueDynamic(this, &ThisClass::HandleCinematicStarted);
		Combo->OnCinematicAccentImpact.AddUniqueDynamic(this, &ThisClass::HandleCinematicImpact);
		Combo->OnCinematicAccentEnded.AddUniqueDynamic(this, &ThisClass::HandleCinematicEnded);
	}
}

void UBossCombatCameraComponent::SetCombatTarget(AActor* NewTarget)
{
	if (!IsValid(NewTarget) || NewTarget == GetOwner()) return;
	if (Target.Get() == NewTarget) return;
	ClearCombatTarget();
	Target = NewTarget;
	bManualSuppression = false;
	OutOfRangeTime = OccludedTime = 0.f;
	BindCombo(NewTarget->FindComponentByClass<UBeatSyncedComboComponent>());
	UE_LOG(LogTemp, Display, TEXT("CombatCamera: locked %s"), *GetNameSafe(NewTarget));
}

void UBossCombatCameraComponent::CancelFeedback()
{
	bWarning = false;
	if (UBossCameraModifier* M = Modifier.Get()) M->Clear();
}

void UBossCombatCameraComponent::ClearCombatTarget()
{
	BindCombo(nullptr);
	Target.Reset();
	OutOfRangeTime = OccludedTime = 0.f;
	ManualYaw = ManualPitch = 0.f;
	CancelFeedback();
}

void UBossCombatCameraComponent::ToggleLockOn()
{
	if(bPhasePresentation) return;
	if (Target.IsValid())
	{
		ClearCombatTarget();
		bManualSuppression = true;
		return;
	}
	bManualSuppression = false;
	if (ABossBeatDirector* D = Director.Get())
		if (IsValid(D->BossActor)) SetCombatTarget(D->BossActor);
}

void UBossCombatCameraComponent::AddLookInput(float Yaw, float Pitch)
{
	ManualYaw = FMath::Clamp(ManualYaw + Yaw, -22.f, 22.f);
	ManualPitch = FMath::Clamp(ManualPitch + Pitch, -12.f, 12.f);
	LastLookInputTime = GetWorld()->GetTimeSeconds();
}

void UBossCombatCameraComponent::TriggerPerfectDodgeFeedback(float FOVExpansionDegrees, float ShakeDegrees, float Strength,
	FVector BladeWorldLocation, FVector AttackDirection, bool bStrongBeat,
	float LateralShiftCm, float DollyShiftCm, float YawKickDegrees, float PitchKickDegrees,
	float ReboundStrength)
{
	AHearBladeCharacter* Player = Cast<AHearBladeCharacter>(GetOwner());
	if (!Player || !Modifier.IsValid()) return;
	float ObstructionScale = 1.0f;
	if (USpringArmComponent* Boom = Player->GetCameraBoom())
	{
		const float Compression = Boom->TargetArmLength > 1.f ? FVector::Dist(Boom->GetComponentLocation(), Player->GetFollowCamera()->GetComponentLocation()) / Boom->TargetArmLength : 1.f;
		if (Compression < .72f) ObstructionScale = .25f;
	}
	if (Target.IsValid() && FVector::Dist2D(Target->GetActorLocation(), Player->GetActorLocation()) < 170.f)
		ObstructionScale = FMath::Min(ObstructionScale, .25f);
	float SideSign = 1.f;
	if (const UCameraComponent* Camera = Player->GetFollowCamera())
	{
		FVector ToBlade = BladeWorldLocation - Player->GetActorLocation();
		if (ToBlade.IsNearlyZero()) ToBlade = AttackDirection;
		SideSign = FVector::DotProduct(ToBlade.GetSafeNormal2D(), Camera->GetRightVector().GetSafeNormal2D()) >= 0.f ? 1.f : -1.f;
	}
	const float BeatAccent = bStrongBeat ? 1.12f : 1.f;
	const float FinalStrength = Strength * ObstructionScale * BeatAccent;
	// Positive kick briefly narrows the lens, then the modifier's negative rebound
	// opens past the baseline before settling. This makes success read as impact,
	// release and recovery instead of a single generic zoom.
	Modifier->TriggerPerfectDodge(FOVExpansionDegrees * FinalStrength,
		ShakeDegrees * FinalStrength, LateralShiftCm * FinalStrength, DollyShiftCm * FinalStrength,
		YawKickDegrees * FinalStrength, PitchKickDegrees * FinalStrength, SideSign, ReboundStrength,
		.032f, .040f, bStrongBeat ? .19f : .16f);
	UE_LOG(LogTemp, Display, TEXT("PerfectDodgeCamera: side=%.0f strong=%d scale=%.2f lateral=%.1f yaw=%.2f"),
		SideSign, bStrongBeat ? 1 : 0, FinalStrength, LateralShiftCm * FinalStrength, YawKickDegrees * FinalStrength);
}

void UBossCombatCameraComponent::TriggerKickAccent(float FOVKickDegrees, float RollDegrees, float PitchDipDegrees,
	float ShakeDegrees, FVector ContactWorld, float HoldSeconds)
{
	AHearBladeCharacter* Player = Cast<AHearBladeCharacter>(GetOwner());
	UBossCameraModifier* CameraModifier = Modifier.Get();
	if (!Player || !CameraModifier) return;
	float SideSign = 1.f;
	if (const UCameraComponent* Camera = Player->GetFollowCamera())
	{
		const FVector ToContact = ContactWorld - Player->GetActorLocation();
		SideSign = FVector::DotProduct(ToContact.GetSafeNormal2D(), Camera->GetRightVector().GetSafeNormal2D()) >= 0.f ? 1.f : -1.f;
	}
	CameraModifier->TriggerPerfectDodge(FMath::Clamp(FOVKickDegrees, 0.f, 6.f), FMath::Clamp(ShakeDegrees, 0.f, .25f),
		0.f, 6.f, 0.f, FMath::Clamp(PitchDipDegrees, 0.f, .6f), SideSign, .25f,
		.03f, FMath::Clamp(HoldSeconds, .04f, .22f), .32f);
	CameraModifier->RollKick = FMath::Clamp(RollDegrees, 0.f, 4.f);
}

void UBossCombatCameraComponent::TriggerUppercutAccent(float Strength)
{
	UBossCameraModifier* CameraModifier=Modifier.Get();
	if(!CameraModifier || !GetOwner()) return;
	const float Power=FMath::Clamp(Strength,0.f,1.5f);
	// The Boss is usually within arm's reach here. Keep this accent independent
	// of the distance suppression used for incoming Boss attacks; the bounded
	// lens/pitch values still keep the target readable.
	CameraModifier->TriggerPerfectDodge(2.1f+1.1f*Power,.035f+.035f*Power,
		0.f,4.f,0.f,.32f+.28f*Power,1.f,.22f,.028f,.055f,.25f);
	UE_LOG(LogTemp,Display,TEXT("UppercutCamera: fov=%.2f pitch=%.2f"),2.1f+1.1f*Power,.32f+.28f*Power);
}

void UBossCombatCameraComponent::TriggerFailureFeedback(bool bMiss)
{
	AHearBladeCharacter* Player = Cast<AHearBladeCharacter>(GetOwner());
	UBossCameraModifier* CameraModifier = Modifier.Get();
	if (!Player || !CameraModifier) return;

	float ObstructionScale = 1.0f;
	if (const USpringArmComponent* Boom = Player->GetCameraBoom())
	{
		const float ActualArm = FVector::Dist(Boom->GetComponentLocation(), Player->GetFollowCamera()->GetComponentLocation());
		if (Boom->TargetArmLength > 1.0f && ActualArm / Boom->TargetArmLength < .72f)
			ObstructionScale = .25f;
	}
	if (Target.IsValid() && FVector::Dist2D(Target->GetActorLocation(), Player->GetActorLocation()) < 170.f)
		ObstructionScale = FMath::Min(ObstructionScale, .35f);

	// Failure is a brief loss of stability: a tiny pull-back and downward hitch. It is deliberately
	// weaker than PERFECT feedback, and never rolls or permanently changes player camera input.
	const float Shake = (bMiss ? .18f : .10f) * ObstructionScale;
	const float Dolly = (bMiss ? -3.2f : -1.4f) * ObstructionScale;
	const float Pitch = (bMiss ? -.12f : -.055f) * ObstructionScale;
	CameraModifier->TriggerPerfectDodge(0.0f, Shake, 0.0f, Dolly, 0.0f, Pitch,
		1.0f, 0.0f, .018f, .012f, bMiss ? .18f : .14f);
	UE_LOG(LogTemp, Verbose, TEXT("RhythmFailureCamera: miss=%d shake=%.2f obstruction=%.2f"),
		bMiss ? 1 : 0, Shake, ObstructionScale);
}

void UBossCombatCameraComponent::HandleWarning(FVector Landing, double)
{
	bWarning = true;
	WarningLanding = Landing;
	UE_LOG(LogTemp, Display, TEXT("CombatCamera: warning Landing=%s"), *Landing.ToString());
}

void UBossCombatCameraComponent::StartHoldFeedback(float ShakeDegrees)
{
	if (UBossCameraModifier* M = Modifier.Get()) M->StartHoldShake(ShakeDegrees);
}

void UBossCombatCameraComponent::TriggerHoldImpact(float ShakeDegrees, float DurationSeconds)
{
	if (UBossCameraModifier* M = Modifier.Get()) M->TriggerHoldImpact(ShakeDegrees, DurationSeconds);
}

void UBossCombatCameraComponent::StopHoldFeedback(float FadeSeconds)
{
	if (UBossCameraModifier* M = Modifier.Get()) M->StopHoldShake(FadeSeconds);
}

void UBossCombatCameraComponent::HandleBlink(FVector, double)
{
	bWarning = false;
	const AHearBladeCharacter* Player = Cast<AHearBladeCharacter>(GetOwner());
	const USpringArmComponent* Boom = Player ? Player->GetCameraBoom() : nullptr;
	float Strength = 1.f;
	if (Boom && FVector::Dist(Boom->GetSocketTransform(USpringArmComponent::SocketName).GetLocation(), Boom->GetComponentLocation()) < Boom->TargetArmLength * .55f)
		Strength = .25f;
	if (Player && Target.IsValid() && FVector::Dist(Player->GetActorLocation(), Target->GetActorLocation()) < 300.f)
		Strength = FMath::Min(Strength, .45f);
	if (const APlayerController* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr)
	{
		int32 Width = 0, Height = 0;
		FVector2D Screen;
		PC->GetViewportSize(Width, Height);
		if (!PC->ProjectWorldLocationToScreen(Target->GetActorLocation() + FVector(0,0,130), Screen) ||
			Screen.X < Width * .12f || Screen.X > Width * .88f || Screen.Y < Height * .1f || Screen.Y > Height * .9f)
			Strength = FMath::Min(Strength, .25f);
	}
	if (UBossCameraModifier* M = Modifier.Get())
	{
		const UBossCameraConfig* C = Settings();
		M->Trigger(C->BlinkFOVKick * Strength, C->BlinkShakeDegrees * Strength,
			C->BlinkPushSeconds, C->BlinkHoldSeconds, C->BlinkRecoverSeconds);
		UE_LOG(LogTemp, Display, TEXT("CombatCamera: blink camera kick=%.1f shake=%.2f"),
			C->BlinkFOVKick * Strength, C->BlinkShakeDegrees * Strength);
	}
}

void UBossCombatCameraComponent::HandleCancelled() { CancelFeedback(); }

void UBossCombatCameraComponent::HandleConfirmedHit()
{
	if (UBossCameraModifier* M = Modifier.Get())
	{
		const UBossCameraConfig* C = Settings();
		M->Trigger(C->BlinkFOVKick, C->HitShakeDegrees,
			C->BlinkPushSeconds, C->BlinkHoldSeconds, C->BlinkRecoverSeconds);
	}
}

void UBossCombatCameraComponent::HandleCinematicStarted(float Strength, bool bStrongBeat)
{
	if (UBossCameraModifier* M = Modifier.Get())
	{
		const float Accent = FMath::Clamp(Strength, .25f, 2.f) * (bStrongBeat ? 1.12f : 1.f);
		// A measured lens push announces the slow section without hiding the attack read.
		M->Trigger(3.4f * Accent, .035f * Accent, .10f, .08f, .42f);
		UE_LOG(LogTemp, Display, TEXT("BeatCinematicCamera: PREP strength=%.2f strong=%d"), Accent, bStrongBeat ? 1 : 0);
	}
}

void UBossCombatCameraComponent::HandleCinematicImpact(float Strength, bool bStrongBeat, FVector ClashLocation)
{
	CinematicFocusLocation = ClashLocation;
	CinematicFocusEndWorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() + .22 : -1000.0;
	if (UBossCameraModifier* M = Modifier.Get())
	{
		const float Accent = FMath::Clamp(Strength, .25f, 2.f) * (bStrongBeat ? 1.18f : 1.f);
		M->TriggerPerfectDodge(4.2f * Accent, .16f * Accent,
			5.f * Accent, 7.f * Accent, .14f * Accent, -.08f * Accent,
			1.f, .18f, .025f, .04f, .26f);
		UE_LOG(LogTemp, Display, TEXT("BeatCinematicCamera: IMPACT strength=%.2f strong=%d"), Accent, bStrongBeat ? 1 : 0);
	}
}

void UBossCombatCameraComponent::HandleCinematicEnded()
{
	UE_LOG(LogTemp, Verbose, TEXT("BeatCinematicCamera: recovery"));
}

void UBossCombatCameraComponent::TriggerLaunchShot(AActor* Subject, float Seconds)
{
	AHearBladeCharacter* Player = Cast<AHearBladeCharacter>(GetOwner());
	if (!Subject || !Player || !GetWorld() || bPhasePresentation) return;
	// Frame from the side the camera is already on, so the move is a swing, not a flip.
	FVector Axis = Subject->GetActorLocation() - Player->GetActorLocation(); Axis.Z = 0.f;
	if (!Axis.Normalize()) Axis = Player->GetActorForwardVector();
	const FVector Side(-Axis.Y, Axis.X, 0.f);
	const FVector Mid = (Player->GetActorLocation() + Subject->GetActorLocation()) * .5f;
	const FVector CameraLoc = Player->GetFollowCamera() ? Player->GetFollowCamera()->GetComponentLocation() : Mid - Axis * 300.f;
	ShotSide = FVector::DotProduct(CameraLoc - Mid, Side) >= 0.f ? 1.f : -1.f;
	// If that side is walled off, take the other one.
	FCollisionQueryParams Q(SCENE_QUERY_STAT(LaunchShot), false, Player); Q.AddIgnoredActor(Subject);
	if (GetWorld()->LineTraceTestByChannel(Mid + FVector(0, 0, 60), Mid + Side * ShotSide * 380.f + FVector(0, 0, 60), ECC_Camera, Q))
		ShotSide = -ShotSide;
	ShotSubject = Subject; ShotStartWorld = GetWorld()->GetTimeSeconds(); ShotSeconds = FMath::Max(.6f, Seconds); bShotInit = false;
	UE_LOG(LogTemp, Display, TEXT("LaunchShot: side=%.0f seconds=%.2f"), ShotSide, ShotSeconds);
}

void UBossCombatCameraComponent::TickLaunchShot(float DeltaTime)
{
	UBossCameraModifier* M = Modifier.Get();
	if (!M) return;
	AHearBladeCharacter* Player = Cast<AHearBladeCharacter>(GetOwner());
	AActor* Subject = ShotSubject.Get();
	const float Age = GetWorld() ? float(GetWorld()->GetTimeSeconds() - ShotStartWorld) : 1.e9f;
	if (!Subject || !Player || bPhasePresentation || Age > ShotSeconds)
	{
		M->ShotWeight = 0.f; ShotSubject.Reset(); bShotInit = false; return;
	}
	constexpr float In = .2f, Out = .45f;
	float W = Age < In ? FMath::SmoothStep(0.f, 1.f, Age / In)
		: Age > ShotSeconds - Out ? 1.f - FMath::SmoothStep(0.f, 1.f, (Age - (ShotSeconds - Out)) / Out) : 1.f;
	W *= .94f;
	// Frame from the GROUND up: actor locations are capsule centres (the boss's is
	// far higher than the player's), so framing on them aimed the shot at the sky.
	auto Ground = [](const AActor* A)
	{
		const UCapsuleComponent* Cap = A->FindComponentByClass<UCapsuleComponent>();
		const float Half = Cap ? Cap->GetScaledCapsuleHalfHeight() : 90.f;
		return TPair<FVector, float>(A->GetActorLocation() - FVector(0, 0, Half), Half);
	};
	const TPair<FVector, float> PG = Ground(Player), BG = Ground(Subject);
	FVector Axis = BG.Key - PG.Key; Axis.Z = 0.f;
	const float Sep = Axis.Size();
	Axis = Sep > 1.f ? Axis / Sep : Player->GetActorForwardVector();
	const FVector Side(-Axis.Y * ShotSide, Axis.X * ShotSide, 0.f);
	const FVector MidGround = (PG.Key + BG.Key) * .5f;
	const float T = FMath::Clamp(Age / ShotSeconds, 0.f, 1.f);
	// Both bodies' chests, averaged; the launched boss only nudges it up a little.
	const float ChestZ = (PG.Key.Z + PG.Value * 1.3f + BG.Key.Z + BG.Value * 1.2f) * .5f;
	const float Lift = 25.f * FMath::Sin(FMath::Clamp(Age / .9f, 0.f, 1.f) * PI);
	const float Dist = FMath::Clamp(Sep * 1.2f + 480.f, 560.f, 1000.f) * FMath::Lerp(1.f, .93f, T); // wide enough for both
	const FVector Look(MidGround.X, MidGround.Y, ChestZ + Lift);
	// Camera at chest height, a touch above: a level side view, slightly looking down.
	FVector Desired = FVector(MidGround.X, MidGround.Y, ChestZ + 45.f) + Side * Dist - Axis * 30.f;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(LaunchShotTick), false, Player); Q.AddIgnoredActor(Subject);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Look, Desired, ECC_Camera, Q)) Desired = Hit.Location + Hit.ImpactNormal * 25.f;
	FRotator Rot = (Look - Desired).Rotation(); Rot.Roll = -3.f * ShotSide; // slight Dutch angle
	if (!bShotInit) { ShotSmoothedLoc = Desired; ShotSmoothedRot = Rot; bShotInit = true; }
	ShotSmoothedLoc = FMath::VInterpTo(ShotSmoothedLoc, Desired, DeltaTime, 12.f);
	ShotSmoothedRot = FMath::RInterpTo(ShotSmoothedRot, Rot, DeltaTime, 12.f);
	M->ShotLocation = ShotSmoothedLoc; M->ShotRotation = ShotSmoothedRot; M->ShotFOV = 60.f; M->ShotWeight = W;
}

void UBossCombatCameraComponent::TickComponent(float DeltaTime, ELevelTick Type, FActorComponentTickFunction* Tick)
{
	Super::TickComponent(DeltaTime, Type, Tick);
	TickLaunchShot(DeltaTime);
	if(bPhasePresentation) return;
	AHearBladeCharacter* Player = Cast<AHearBladeCharacter>(GetOwner());
	APlayerController* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
	if (!PC || !PC->IsLocalController()) return;
	const UBossCameraConfig* C = Settings();
	if (!Modifier.IsValid() && PC->PlayerCameraManager)
		Modifier = Cast<UBossCameraModifier>(PC->PlayerCameraManager->AddNewCameraModifier(UBossCameraModifier::StaticClass()));
	if (!Director.IsValid())
		for (TActorIterator<ABossBeatDirector> It(GetWorld()); It; ++It) { Director = *It; break; }
	ABossBeatDirector* D = Director.Get();
	const bool bEncounter = IsValid(D) && IsValid(D->BossActor) &&
		IsValid(D->BehaviorTreeComponent) && D->BehaviorTreeComponent->IsRunning();
	AActor* Boss = bEncounter ? D->BossActor.Get() : nullptr;
	const float Distance = Boss ? FVector::Dist(Player->GetActorLocation(), Boss->GetActorLocation()) : BIG_NUMBER;
	if (bManualSuppression && Distance > C->ReleaseDistance) bManualSuppression = false;
	if (!Target.IsValid() && !bManualSuppression && Boss && Distance < C->AcquireDistance)
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BossCameraAcquire), false, Player);
		Q.AddIgnoredActor(Boss);
		if (!GetWorld()->LineTraceTestByChannel(Player->GetFollowCamera()->GetComponentLocation(),
			Boss->GetActorLocation() + FVector(0,0,120), ECC_Visibility, Q)) SetCombatTarget(Boss);
	}
	if (Target.IsValid())
	{
		if (!bEncounter || Target.Get() != Boss) { ClearCombatTarget(); return; }
		OutOfRangeTime = Distance > C->ReleaseDistance ? OutOfRangeTime + DeltaTime : 0.f;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BossCameraOcclusion), false, Player);
		Q.AddIgnoredActor(Boss);
		OccludedTime = GetWorld()->LineTraceTestByChannel(Player->GetFollowCamera()->GetComponentLocation(),
			Boss->GetActorLocation() + FVector(0,0,120), ECC_Visibility, Q) ? OccludedTime + DeltaTime : 0.f;
		if (OutOfRangeTime > C->ReleaseDelay || OccludedTime > C->OcclusionGrace)
		{
			ClearCombatTarget();
			bManualSuppression = true;
		}
	}
	USpringArmComponent* Boom = Player->GetCameraBoom();
	UCameraComponent* Camera = Player->GetFollowCamera();
	if (!Boom || !Camera) return;
	if (AActor* Locked = Target.Get())
	{
		FVector AreaCenter; float AreaRadius=0.f, AreaProgress=0.f;
		const auto* AreaCombo=Locked->FindComponentByClass<UBeatSyncedComboComponent>();
		const bool bAreaWarning=AreaCombo && AreaCombo->GetGroundBurstPreview(AreaCenter,AreaRadius,AreaProgress);
		const float Near = 1.f - FMath::Clamp((Distance - 250.f) / 650.f, 0.f, 1.f);
		Boom->TargetOffset = FMath::VInterpTo(Boom->TargetOffset,
			FVector(0.f, 0.f, C->CombatPivotHeight), DeltaTime, C->DistanceSpeed);
		Boom->SocketOffset = FMath::VInterpTo(Boom->SocketOffset,
			FVector(0.f, C->CombatShoulderOffset, 0.f), DeltaTime, C->DistanceSpeed);
		const FVector Eye = Player->GetActorLocation() + FVector(0,0,105);
		const FVector Chest = Locked->GetActorLocation() + FVector(0,0,125);
		FVector Focus = FMath::Lerp(Eye, Chest, FMath::Lerp(C->BossFocusWeight * .75f, C->BossFocusWeight, 1.f - Near));
		if (bWarning) Focus = FMath::Lerp(Focus, WarningLanding + FVector(0,0,70), C->WarningLandingWeight);
		if (bAreaWarning) Focus = FMath::Lerp(Focus,AreaCenter+FVector(0,0,60),.45f);
		const bool bClashFocus = GetWorld()->GetTimeSeconds() < CinematicFocusEndWorldSeconds;
		if (bClashFocus) Focus = FMath::Lerp(Focus, CinematicFocusLocation, .72f);
		// Use the unobstructed spring arm endpoint for lock-on aiming. The real
		// camera may be retracted by a wall; feeding that position back into the
		// desired rotation creates a visible back-and-forth near obstructions.
		const FRotator CurrentRotation = PC->GetControlRotation();
		const FVector VirtualCamera = Boom->GetComponentLocation() + Boom->TargetOffset
			- CurrentRotation.Vector() * Boom->TargetArmLength
			+ CurrentRotation.RotateVector(Boom->SocketOffset);
		FRotator Aim = (Focus - VirtualCamera).Rotation();
		Aim.Pitch = FMath::ClampAngle(Aim.Pitch, -40.f, FMath::Lerp(20.f, C->MaxCloseRangeUpwardPitch, Near));
		if (GetWorld()->GetTimeSeconds() - LastLookInputTime > .6f)
		{
			ManualYaw = FMath::FInterpTo(ManualYaw, 0.f, DeltaTime, 3.f);
			ManualPitch = FMath::FInterpTo(ManualPitch, 0.f, DeltaTime, 3.f);
		}
		// Calmer lock-on: close to the boss every dodge / sidestep swung the aim hard.
		// A yaw dead zone ignores small drifts (the boss stays framed anyway), the
		// follow is softer, and the turn rate is capped so it never whips around.
		const bool bLooking = GetWorld()->GetTimeSeconds() - LastLookInputTime <= .6f;
		if (!bLooking)
		{
			const float DeadZone = FMath::Lerp(6.f, 14.f, Near); // wider when close
			const float Err = FRotator::NormalizeAxis(Aim.Yaw - CurrentRotation.Yaw);
			Aim.Yaw = CurrentRotation.Yaw + FMath::Sign(Err) * FMath::Max(0.f, FMath::Abs(Err) - DeadZone);
		}
		Aim.Yaw += ManualYaw;
		Aim.Pitch += ManualPitch;
		FRotator Next = FMath::RInterpTo(CurrentRotation, Aim, DeltaTime, FMath::Min(C->RotationSpeed, 3.5f));
		const float MaxTurn = 150.f * DeltaTime;
		Next.Yaw = CurrentRotation.Yaw + FMath::Clamp(FRotator::NormalizeAxis(Next.Yaw - CurrentRotation.Yaw), -MaxTurn, MaxTurn);
		PC->SetControlRotation(Next);
		// Bring both actors slightly closer without changing the established
		// framing, lock-on aim, or FOV behavior. Keep the configured distance
		// limits authoritative even when the data asset overrides C++ defaults.
		float DesiredArm = FMath::Clamp(FMath::Max(C->CombatArmLengthAtCloseRange,
			400.f + Distance * .12f), C->FreeArmLength, C->MaxCombatArmLength) * .92f;
		if (bClashFocus) DesiredArm *= .72f;
		if (bAreaWarning) DesiredArm=FMath::Max(DesiredArm,FMath::Min(850.f,AreaRadius*1.65f));
		Boom->TargetArmLength = FMath::FInterpTo(Boom->TargetArmLength, DesiredArm, DeltaTime, C->DistanceSpeed);
		float DesiredFOV = FMath::Max(FMath::Lerp(C->FreeFOV, C->CloseRangeFOV, Near),
			FMath::GetMappedRangeValueClamped(FVector2D(500.f, 1800.f),
			FVector2D(C->FreeFOV, C->MaxCombatFOV), Distance));
		if (bAreaWarning) DesiredFOV=FMath::Max(DesiredFOV,95.f);
		Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, DesiredFOV, DeltaTime, 5.f));
	}
	else
	{
		Boom->TargetOffset = FMath::VInterpTo(Boom->TargetOffset, FVector::ZeroVector, DeltaTime, 4.f);
		Boom->SocketOffset = FMath::VInterpTo(Boom->SocketOffset, FVector::ZeroVector, DeltaTime, 4.f);
		Boom->TargetArmLength = FMath::FInterpTo(Boom->TargetArmLength, C->FreeArmLength * .92f, DeltaTime, 4.f);
		Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, C->FreeFOV, DeltaTime, 5.f));
	}
}
