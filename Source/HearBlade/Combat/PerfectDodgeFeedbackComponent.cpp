#include "Combat/PerfectDodgeFeedbackComponent.h"
#include "Combat/PerfectDodgeFeedbackConfig.h"
#include "Combat/BossCombatCameraComponent.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "HearBladeCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "UObject/ConstructorHelpers.h"

UPerfectDodgeFeedbackComponent::UPerfectDodgeFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UPerfectDodgeFeedbackConfig> ConfigAsset(TEXT("/Game/Combat/Feedback/DA_PerfectDodgeFeedback.DA_PerfectDodgeFeedback"));
	if (ConfigAsset.Succeeded()) Config = ConfigAsset.Object;
}

void UPerfectDodgeFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UPlayerRhythmCombatComponent* Combat = GetOwner()->FindComponentByClass<UPlayerRhythmCombatComponent>())
		Combat->OnRhythmSuccessFeedback.AddDynamic(this, &UPerfectDodgeFeedbackComponent::HandleRhythmFeedback);
	AHearBladeCharacter* Player = Cast<AHearBladeCharacter>(GetOwner());
	if (Player && Config && Config->PostProcessMaterial && Config->Accessibility != EPerfectDodgeAccessibility::NoFullscreen)
	{
		PostProcessMID = UMaterialInstanceDynamic::Create(Config->PostProcessMaterial, this);
		Player->GetFollowCamera()->PostProcessSettings.AddBlendable(PostProcessMID, 1.0f);
		PostProcessMID->SetScalarParameterValue(TEXT("Intensity"), 0.0f);
	}
}

void UPerfectDodgeFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UPlayerRhythmCombatComponent* Combat = GetOwner() ? GetOwner()->FindComponentByClass<UPlayerRhythmCombatComponent>() : nullptr)
		Combat->OnRhythmSuccessFeedback.RemoveDynamic(this, &UPerfectDodgeFeedbackComponent::HandleRhythmFeedback);
	RestoreImpactFrame();
	if (AHearBladeCharacter* Player = Cast<AHearBladeCharacter>(GetOwner()); Player && PostProcessMID)
		Player->GetFollowCamera()->PostProcessSettings.AddBlendable(PostProcessMID, 0.0f);
	ClearTransientVisuals();
	Super::EndPlay(EndPlayReason);
}

void UPerfectDodgeFeedbackComponent::HandleRhythmFeedback(const FRhythmSuccessFeedbackData& Feedback)
{
	if (!Config || !GetOwner()) return;
	if (Feedback.bSuccess)
	{
		const bool bTierUp = Feedback.FeedbackTier > UPlayerRhythmCombatComponent::GetFeedbackTierForCombo(Feedback.PreviousCombo);
		USoundBase* Sound = bTierUp ? Config->TierUpSound.Get() :
			Feedback.bPerfect ? Config->PerfectSound.Get() : Config->GoodSound.Get();
		if (Sound) UGameplayStatics::PlaySound2D(this, Sound, bTierUp ? .82f : Feedback.bPerfect ? .68f : .48f);
		if (FParse::Param(FCommandLine::Get(), TEXT("RhythmCombatCaptureTest")) &&
			(Feedback.ComboCount == 1 || Feedback.ComboCount == 5 || Feedback.ComboCount == 6 ||
			 Feedback.ComboCount == 8 || Feedback.ComboCount == 16))
		{
			const FString File = FPaths::ProjectSavedDir() / TEXT("Screenshots") /
				FString::Printf(TEXT("RhythmFeedback_Combo%02d_Event%lld.png"), Feedback.ComboCount, Feedback.EventId);
			FScreenshotRequest::RequestScreenshot(File, false, false);
		}
	}
	if (UBossCombatCameraComponent* Camera = GetOwner()->FindComponentByClass<UBossCombatCameraComponent>())
	{
		if (Feedback.Response == ERhythmResponseType::HoldBlock)
		{
			if (!Feedback.bSuccess || Feedback.HoldMarker == ERhythmHoldMarkerType::End)
			{
				Camera->StopHoldFeedback(Config->HoldStopSeconds);
			}
			else if (Config->Accessibility != EPerfectDodgeAccessibility::NoCameraShake)
			{
				if (Feedback.HoldMarker == ERhythmHoldMarkerType::Start)
					Camera->StartHoldFeedback(Config->HoldShakeDegrees);
				else if (Feedback.HoldMarker == ERhythmHoldMarkerType::Continue)
					Camera->TriggerHoldImpact(Config->HoldImpactShakeDegrees, .06f);
			}
		}
	}
	if (!Feedback.bPerfect) return;

	FPerfectDodgeFeedbackData Perfect;
	Perfect.BladeWorldLocation = Feedback.WorldLocation;
	Perfect.AttackDirection = Feedback.AttackDirection;
	Perfect.ComboCount = Feedback.ComboCount;
	Perfect.bStrongBeat = Feedback.bStrongBeat;
	if (const UPlayerRhythmCombatComponent* Combat = GetOwner()->FindComponentByClass<UPlayerRhythmCombatComponent>())
		Perfect.ResonanceFraction = Combat->GetResonanceFraction();
	const int32 Tier = FMath::Clamp(Feedback.FeedbackTier, 0, 3);
	const float TierScale = Tier == 0 ? Config->TierStrengths.X : Tier == 1 ? Config->TierStrengths.Y :
		Tier == 2 ? Config->TierStrengths.Z : Config->TierStrengths.W;
	PlayPerfectFeedback(Perfect, TierScale, Feedback.bKeyPerfect);
}

void UPerfectDodgeFeedbackComponent::PlayPerfectFeedback(const FPerfectDodgeFeedbackData& Feedback,
	float TierScale, bool bKeyPerfect)
{
	if (!Config || !GetWorld()) return;
	ClearTransientVisuals();
	ActiveStrength = FMath::Min(1.65f, TierScale * (Feedback.bStrongBeat ? Config->StrongBeatMultiplier : 1.0f));
	ActiveDuration = Config->DurationSeconds;
	if (const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>(); Clock && Clock->IsRunning())
		ActiveDuration = static_cast<float>(Clock->GetSecondsPerBeat() * .25);
	ActiveDuration = FMath::Clamp(ActiveDuration, 0.11f, 0.18f);
	EffectStartWorldSeconds = GetWorld()->GetTimeSeconds();
	if (PostProcessMID && Config->Accessibility != EPerfectDodgeAccessibility::NoFullscreen)
	{
		PostProcessMID->SetVectorParameterValue(TEXT("RiftOrigin"), FLinearColor(.5f, .5f, 0, 0));
		PostProcessMID->SetScalarParameterValue(TEXT("GoldMix"), FMath::Clamp(0.50f + Feedback.ResonanceFraction * .50f, 0.f, 1.f));
		PostProcessMID->SetScalarParameterValue(TEXT("DesaturationAmount"), Feedback.bStrongBeat ? .42f : .30f);
	}
	SpawnRing(Feedback, 0.0f);
	if (Feedback.bStrongBeat) SpawnRing(Feedback, ActiveDuration * .5f);
	SpawnGhosts(ActiveStrength);
	if (bKeyPerfect) StartImpactFrame();
	if (Config->RingParticles)
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Config->RingParticles, Feedback.BladeWorldLocation,
			Feedback.AttackDirection.Rotation(), FVector(ActiveStrength), true, true);
	if (Config->Accessibility != EPerfectDodgeAccessibility::NoCameraShake)
		if (UBossCombatCameraComponent* Camera = GetOwner()->FindComponentByClass<UBossCombatCameraComponent>())
		Camera->TriggerPerfectDodgeFeedback(Config->FOVExpansionDegrees, Config->CameraShakeDegrees,
			FMath::Min(1.45f, TierScale),
			Feedback.BladeWorldLocation, Feedback.AttackDirection, Feedback.bStrongBeat,
			Config->CameraLateralShiftCm, Config->CameraDollyShiftCm, Config->CameraYawKickDegrees,
			Config->CameraPitchKickDegrees, Config->CameraReboundStrength);
	UE_LOG(LogTemp, Display, TEXT("BeatRift: spawned strong=%d key=%d rings=%d ghosts=%d strength=%.2f"),
		Feedback.bStrongBeat ? 1 : 0, bKeyPerfect ? 1 : 0, Feedback.bStrongBeat ? 2 : 1, Ghosts.Num(), ActiveStrength);
}

void UPerfectDodgeFeedbackComponent::StartImpactFrame()
{
	if (!Config || !GetWorld() || Config->Accessibility != EPerfectDodgeAccessibility::Full || bImpactFrameActive) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastImpactFrameWorldSeconds < Config->ImpactFrameCooldown) return;
	AHearBladeCharacter* Player = Cast<AHearBladeCharacter>(GetOwner());
	UCameraComponent* Camera = Player ? Player->GetFollowCamera() : nullptr;
	if (!Camera) return;
	FPostProcessSettings& PP = Camera->PostProcessSettings;
	bSavedSaturationOverride = PP.bOverride_ColorSaturation;
	bSavedContrastOverride = PP.bOverride_ColorContrast;
	SavedSaturation = PP.ColorSaturation;
	SavedContrast = PP.ColorContrast;
	PP.bOverride_ColorSaturation = true;
	PP.bOverride_ColorContrast = true;
	PP.ColorSaturation = FVector4(.02f, .02f, .02f, 1.f);
	PP.ColorContrast = FVector4(1.55f, 1.55f, 1.55f, 1.f);
	bImpactFrameActive = true;
	LastImpactFrameWorldSeconds = Now;
	ImpactFrameEndWorldSeconds = Now + Config->ImpactFrameSeconds;
}

void UPerfectDodgeFeedbackComponent::RestoreImpactFrame()
{
	if (!bImpactFrameActive) return;
	if (AHearBladeCharacter* Player = Cast<AHearBladeCharacter>(GetOwner()))
	{
		if (UCameraComponent* Camera = Player->GetFollowCamera())
		{
			FPostProcessSettings& PP = Camera->PostProcessSettings;
			PP.ColorSaturation = SavedSaturation;
			PP.ColorContrast = SavedContrast;
			PP.bOverride_ColorSaturation = bSavedSaturationOverride;
			PP.bOverride_ColorContrast = bSavedContrastOverride;
		}
	}
	bImpactFrameActive = false;
}

void UPerfectDodgeFeedbackComponent::SpawnRing(const FPerfectDodgeFeedbackData& Feedback, float DelaySeconds)
{
	if (!Config->RingMaterial || !GetWorld()) return;
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (!Plane) return;
	UStaticMeshComponent* Ring = NewObject<UStaticMeshComponent>(GetOwner());
	Ring->SetStaticMesh(Plane);
	Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ring->SetCastShadow(false);
	Ring->RegisterComponentWithWorld(GetWorld());
	Ring->SetWorldLocation(Feedback.BladeWorldLocation);
	FVector ToCamera = FVector::UpVector;
	if (const AHearBladeCharacter* Player = Cast<AHearBladeCharacter>(GetOwner()); Player && Player->GetFollowCamera())
		ToCamera = (Player->GetFollowCamera()->GetComponentLocation() - Feedback.BladeWorldLocation).GetSafeNormal();
	Ring->SetWorldRotation(FRotationMatrix::MakeFromZ(ToCamera).Rotator());
	Ring->SetWorldScale3D(FVector(1.7f * ActiveStrength));
	UMaterialInstanceDynamic* MID = Ring->CreateDynamicMaterialInstance(0, Config->RingMaterial);
	if (MID)
	{
		MID->SetScalarParameterValue(TEXT("Opacity"), 0.0f);
		MID->SetScalarParameterValue(TEXT("GoldMix"), FMath::Clamp(.35f + Feedback.ResonanceFraction * .65f, 0.f, 1.f));
		MID->SetVectorParameterValue(TEXT("RingColor"), FLinearColor(1.0f, .26f, .008f, 1.0f));
	}
	Rings.Add({Ring, MID, EffectStartWorldSeconds + DelaySeconds});
}

void UPerfectDodgeFeedbackComponent::SpawnGhosts(float Strength)
{
	if (!Config->GhostMaterial || !GetOwner() || !GetWorld()) return;
	USkeletalMeshComponent* Source = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	UPlayerRhythmCombatComponent* Combat = GetOwner()->FindComponentByClass<UPlayerRhythmCombatComponent>();
	if (!Source || !Source->GetSkeletalMeshAsset() || !Combat) return;
	const int32 Count = FMath::Clamp(Config->GhostCount, 1, 5);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		UPoseableMeshComponent* Ghost = NewObject<UPoseableMeshComponent>(GetOwner());
		Ghost->SetSkinnedAssetAndUpdate(Source->GetSkeletalMeshAsset(), true);
		Ghost->RegisterComponentWithWorld(GetWorld());
		Ghost->CopyPoseFromSkeletalComponent(Source);
		const float Fraction = static_cast<float>(Index + 1) / static_cast<float>(Count + 1);
		const FVector Location = FMath::Lerp(GetOwner()->GetActorLocation(), Combat->GetDodgeStartLocation(), Fraction);
		Ghost->SetWorldTransform(Source->GetComponentTransform());
		Ghost->AddWorldOffset(Location - GetOwner()->GetActorLocation());
		Ghost->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ghost->SetCastShadow(false);
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Config->GhostMaterial, Ghost);
		for (int32 MatIndex = 0; MatIndex < Ghost->GetNumMaterials(); ++MatIndex) Ghost->SetMaterial(MatIndex, MID);
		if (MID)
		{
			MID->SetScalarParameterValue(TEXT("Opacity"), .28f * Strength);
			MID->SetScalarParameterValue(TEXT("GhostIndex"), Index);
			MID->SetVectorParameterValue(TEXT("GhostColor"), FLinearColor(.0f, .32f, 1.0f, 1.0f));
		}
		Ghosts.Add({Ghost, MID, Index * .018f});
	}
}

void UPerfectDodgeFeedbackComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetWorld()) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (bImpactFrameActive && Now >= ImpactFrameEndWorldSeconds) RestoreImpactFrame();
	if (EffectStartWorldSeconds < 0.0) return;
	const float Phase = FMath::Clamp(static_cast<float>((Now - EffectStartWorldSeconds) / ActiveDuration), 0.f, 1.f);
	const float AttackFraction = .12f;
	const float EnvelopeShape = Phase < AttackFraction
		? Phase / AttackFraction
		: FMath::Pow(FMath::Clamp(1.f - (Phase - AttackFraction) / (1.f - AttackFraction), 0.f, 1.f), .42f);
	const float Envelope = EnvelopeShape * ActiveStrength;
	if (PostProcessMID)
	{
		const float AccessibilityScale = Config && Config->Accessibility == EPerfectDodgeAccessibility::ReducedFlash ? .35f : 1.f;
		PostProcessMID->SetScalarParameterValue(TEXT("Intensity"), Envelope * AccessibilityScale * 2.4f);
	}
	for (FRingVisual& Visual : Rings)
	{
		if (!Visual.Mesh.IsValid()) continue;
		const float RingPhase = FMath::Clamp(static_cast<float>((Now - Visual.Start) / ActiveDuration), 0.f, 1.f);
		Visual.Mesh->SetVisibility(Now >= Visual.Start);
		Visual.Mesh->SetWorldScale3D(FVector(FMath::Lerp(1.1f, 2.4f, RingPhase) * ActiveStrength));
		if (Visual.Material.IsValid()) Visual.Material->SetScalarParameterValue(TEXT("Opacity"), FMath::Sin(RingPhase * UE_PI));
	}
	const float GhostLife = Config ? Config->GhostLifetimeSeconds : .22f;
	for (FGhostVisual& Visual : Ghosts)
	{
		const float GhostPhase = FMath::Clamp(static_cast<float>((Now - EffectStartWorldSeconds - Visual.Delay) / GhostLife), 0.f, 1.f);
		if (Visual.Material.IsValid()) Visual.Material->SetScalarParameterValue(TEXT("Opacity"), (1.f - GhostPhase) * .34f * ActiveStrength);
	}
	if (Now - EffectStartWorldSeconds >= FMath::Max<double>(GhostLife, ActiveDuration * 1.5f))
	{
		if (PostProcessMID) PostProcessMID->SetScalarParameterValue(TEXT("Intensity"), 0.f);
		ClearTransientVisuals(); EffectStartWorldSeconds = -1000.0;
	}
}

void UPerfectDodgeFeedbackComponent::ClearTransientVisuals()
{
	for (FGhostVisual& Visual : Ghosts) if (Visual.Mesh.IsValid()) Visual.Mesh->DestroyComponent();
	for (FRingVisual& Visual : Rings) if (Visual.Mesh.IsValid()) Visual.Mesh->DestroyComponent();
	Ghosts.Reset(); Rings.Reset();
}
