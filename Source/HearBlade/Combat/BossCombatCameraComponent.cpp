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
	}
	BoundCombo = Combo;
	if (Combo)
	{
		Combo->OnBlinkWarning.AddUniqueDynamic(this, &ThisClass::HandleWarning);
		Combo->OnBlinkSucceeded.AddUniqueDynamic(this, &ThisClass::HandleBlink);
		Combo->OnBlinkCancelled.AddUniqueDynamic(this, &ThisClass::HandleCancelled);
		Combo->OnConfirmedHit.AddUniqueDynamic(this, &ThisClass::HandleConfirmedHit);
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

void UBossCombatCameraComponent::TickComponent(float DeltaTime, ELevelTick Type, FActorComponentTickFunction* Tick)
{
	Super::TickComponent(DeltaTime, Type, Tick);
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
		const float Near = 1.f - FMath::Clamp((Distance - 250.f) / 650.f, 0.f, 1.f);
		Boom->TargetOffset = FMath::VInterpTo(Boom->TargetOffset,
			FVector(0.f, 0.f, C->CombatPivotHeight), DeltaTime, C->DistanceSpeed);
		Boom->SocketOffset = FMath::VInterpTo(Boom->SocketOffset,
			FVector(0.f, C->CombatShoulderOffset, 0.f), DeltaTime, C->DistanceSpeed);
		const FVector Eye = Player->GetActorLocation() + FVector(0,0,105);
		const FVector Chest = Locked->GetActorLocation() + FVector(0,0,125);
		FVector Focus = FMath::Lerp(Eye, Chest, FMath::Lerp(C->BossFocusWeight * .75f, C->BossFocusWeight, 1.f - Near));
		if (bWarning) Focus = FMath::Lerp(Focus, WarningLanding + FVector(0,0,70), C->WarningLandingWeight);
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
		Aim.Yaw += ManualYaw;
		Aim.Pitch += ManualPitch;
		PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(), Aim, DeltaTime, C->RotationSpeed));
		const float DesiredArm = FMath::Clamp(FMath::Max(C->CombatArmLengthAtCloseRange,
			400.f + Distance * .12f), C->FreeArmLength, C->MaxCombatArmLength);
		Boom->TargetArmLength = FMath::FInterpTo(Boom->TargetArmLength, DesiredArm, DeltaTime, C->DistanceSpeed);
		const float DesiredFOV = FMath::Max(FMath::Lerp(C->FreeFOV, C->CloseRangeFOV, Near),
			FMath::GetMappedRangeValueClamped(FVector2D(500.f, 1800.f),
			FVector2D(C->FreeFOV, C->MaxCombatFOV), Distance));
		Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, DesiredFOV, DeltaTime, 5.f));
	}
	else
	{
		Boom->TargetOffset = FMath::VInterpTo(Boom->TargetOffset, FVector::ZeroVector, DeltaTime, 4.f);
		Boom->SocketOffset = FMath::VInterpTo(Boom->SocketOffset, FVector::ZeroVector, DeltaTime, 4.f);
		Boom->TargetArmLength = FMath::FInterpTo(Boom->TargetArmLength, C->FreeArmLength, DeltaTime, 4.f);
		Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, C->FreeFOV, DeltaTime, 5.f));
	}
}
