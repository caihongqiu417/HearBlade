#include "Combat/PerfectDodgeFeedbackComponent.h"
#include "Combat/TransientCombatFX.h"
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
#include "Sound/SoundBase.h"
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
	EnsureGhostPool(); // pay the mesh setup cost at load, not mid-dodge
	auto LoadHit=[](const TCHAR* Name){ return LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/Audio/SFX/HitRush/%s.%s"),Name,Name)); };
	DodgeHitSound=LoadHit(TEXT("SW_HitRush_Light1")); // for now: the attack punch (SW_HitRush_Dodge kept for later)
	BlockHitSound=LoadHit(TEXT("SW_HitRush_Block"));
	CounterHitSound=LoadHit(TEXT("SW_HitRush_Counter"));
	HoldHitSound=LoadHit(TEXT("SW_HitRush_HoldHit"));
	TierUpHitSound=LoadHit(TEXT("SW_HitRush_Finisher"));
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

void UPerfectDodgeFeedbackComponent::EnsureGhostPool()
{
	if (GhostPool.Num() > 0 || !Config || !Config->GhostMaterial || !GetOwner() || !GetWorld()) return;
	USkeletalMeshComponent* Source = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	if (!Source || !Source->GetSkeletalMeshAsset()) return;
	for (int32 Index = 0; Index < FMath::Clamp(Config->GhostCount, 1, 5); ++Index)
	{
		UPoseableMeshComponent* Ghost = NewObject<UPoseableMeshComponent>(GetOwner());
		Ghost->SetSkinnedAssetAndUpdate(Source->GetSkeletalMeshAsset(), true);
		Ghost->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ghost->SetCastShadow(false);
		Ghost->RegisterComponentWithWorld(GetWorld());
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Config->GhostMaterial, Ghost);
		for (int32 MatIndex = 0; MatIndex < Ghost->GetNumMaterials(); ++MatIndex) Ghost->SetMaterial(MatIndex, MID);
		if (MID) MID->SetVectorParameterValue(TEXT("GhostColor"), FLinearColor(.0f, .32f, 1.0f, 1.0f));
		Ghost->SetVisibility(false);
		GhostPool.Add({Ghost, MID, Index * .018f});
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
	for (FGhostVisual& Visual : GhostPool) if (Visual.Mesh.IsValid()) Visual.Mesh->DestroyComponent();
	GhostPool.Reset();
	DestroyHoldArmorVisual();
	Super::EndPlay(EndPlayReason);
}

void UPerfectDodgeFeedbackComponent::HandleRhythmFeedback(const FRhythmSuccessFeedbackData& Feedback)
{
	if (!Config || !GetOwner()) return;
	if (Feedback.bPlayerAttack) return; // Attacks own their lighter, high-frequency feedback.
	if (Feedback.bSuccess)
	{
		const bool bTierUp = Feedback.FeedbackTier > UPlayerRhythmCombatComponent::GetFeedbackTierForCombo(Feedback.PreviousCombo);
		// Same punch family as the attacks: a slipped-punch snap for dodges, a gloved
		// thud for blocks/parries, the heaviest punch for counters. Timing sets the
		// level; a combo tier-up layers the finisher hit underneath.
		USoundBase* Punch = nullptr;
		switch (Feedback.Response)
		{
		case ERhythmResponseType::Dodge: Punch = DodgeHitSound; break;
		case ERhythmResponseType::Counter: Punch = CounterHitSound; break;
		case ERhythmResponseType::HoldBlock:
			Punch = Feedback.HoldMarker == ERhythmHoldMarkerType::Start ? BlockHitSound.Get() : HoldHitSound.Get(); break;
		default: Punch = BlockHitSound; break; // Block, DodgeOrBlock
		}
		const float Pitch = 1.f + .03f * FMath::Clamp(Feedback.FeedbackTier, 0, 3);
		if (Punch) UGameplayStatics::PlaySound2D(this, Punch, Feedback.bPerfect ? 1.2f : .9f, Pitch);
		if (bTierUp && TierUpHitSound) UGameplayStatics::PlaySound2D(this, TierUpHitSound, .7f);
		if (!Punch) // assets not imported yet: keep the old cues
		{
			USoundBase* Sound = bTierUp ? Config->TierUpSound.Get() :
				Feedback.bPerfect ? Config->PerfectSound.Get() : Config->GoodSound.Get();
			if (Sound) UGameplayStatics::PlaySound2D(this, Sound, bTierUp ? .82f : Feedback.bPerfect ? .68f : .48f);
		}
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
			if (Feedback.bSuccess && Feedback.HoldMarker != ERhythmHoldMarkerType::End && GetWorld())
				HoldArmorImpactWorldSeconds = GetWorld()->GetTimeSeconds();
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

void UPerfectDodgeFeedbackComponent::PlayResonanceAttack(bool bBurst)
{
	if(!Config || !GetOwner()) return;
	if(!bBurst) { SpawnGhosts(.65f); return; }
	FPerfectDodgeFeedbackData Data;
	Data.BladeWorldLocation=GetOwner()->GetActorLocation()+GetOwner()->GetActorForwardVector()*90.f;
	Data.AttackDirection=GetOwner()->GetActorForwardVector();Data.bStrongBeat=true;Data.ResonanceFraction=1.f;
	PlayPerfectFeedback(Data,1.2f,false);
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
		SpawnStandalonePerfectSparks(GetWorld(), Config->RingParticles, Feedback.BladeWorldLocation,
			Feedback.AttackDirection.Rotation(), ActiveStrength);
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
	EnsureGhostPool();
	const int32 Count = GhostPool.Num();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FGhostVisual& Pooled = GhostPool[Index];
		UPoseableMeshComponent* Ghost = Pooled.Mesh.Get();
		if (!Ghost) continue;
		Ghost->CopyPoseFromSkeletalComponent(Source);
		const float Fraction = static_cast<float>(Index + 1) / static_cast<float>(Count + 1);
		const FVector Location = FMath::Lerp(GetOwner()->GetActorLocation(), Combat->GetDodgeStartLocation(), Fraction);
		Ghost->SetWorldTransform(Source->GetComponentTransform());
		Ghost->AddWorldOffset(Location - GetOwner()->GetActorLocation());
		Ghost->SetVisibility(true);
		if (UMaterialInstanceDynamic* MID = Pooled.Material.Get())
		{
			MID->SetScalarParameterValue(TEXT("Opacity"), .28f * Strength);
			MID->SetScalarParameterValue(TEXT("GhostIndex"), Index);
		}
		Ghosts.Add(Pooled);
	}
}

void UPerfectDodgeFeedbackComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetWorld()) return;
	UpdateHoldArmorVisual(DeltaTime);
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

void UPerfectDodgeFeedbackComponent::CreateHoldArmorVisual()
{
	if (HoldArmorVisuals.Num() > 0 || !Config || !GetOwner() || !GetWorld()) return;
	UMaterialInterface* ArmorMaterial = Config->HoldArmorMaterial.Get();
	if (!ArmorMaterial)
		ArmorMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Combat/Feedback/M_PlayerHoldArmor.M_PlayerHoldArmor"));
	if (!ArmorMaterial) return;
	TInlineComponentArray<USkeletalMeshComponent*> Sources(GetOwner());
	for (USkeletalMeshComponent* Source : Sources)
	{
		if (!IsValid(Source) || !Source->GetSkeletalMeshAsset()) continue;
		UMaterialInterface* PreviousOverlay = Source->GetOverlayMaterial();
		USkeletalMeshComponent* Shell = NewObject<USkeletalMeshComponent>(GetOwner(),
			*FString::Printf(TEXT("SustainedHoldArmor_%s"), *Source->GetName()));
		Shell->SetupAttachment(Source);
		Shell->SetSkinnedAssetAndUpdate(Source->GetSkeletalMeshAsset(), true);
		Shell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Shell->SetGenerateOverlapEvents(false);
		Shell->SetCastShadow(false);
		Shell->SetReceivesDecals(false);
		Shell->RegisterComponentWithWorld(GetWorld());
		Shell->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
		Shell->SetRelativeScale3D(FVector(Config->HoldArmorShellScale));
		Shell->SetLeaderPoseComponent(Source);

		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ArmorMaterial, Shell);
		if (!MID)
		{
			Shell->DestroyComponent();
			continue;
		}
		for (int32 Index = 0; Index < Shell->GetNumMaterials(); ++Index) Shell->SetMaterial(Index, MID);
		MID->SetScalarParameterValue(TEXT("Opacity"), 0.0f);
		MID->SetScalarParameterValue(TEXT("GlowIntensity"), 0.0f);
		MID->SetVectorParameterValue(TEXT("ArmorColor"), Config->HoldArmorColor);
		// Direct overlay colors every material section, including the separate cape
		// and dark accessory slots. The expanded shell adds the outer cracked rim.
		Source->SetOverlayMaterial(MID);
		HoldArmorVisuals.Add({Source, Shell, MID, PreviousOverlay});
	}
	UE_LOG(LogTemp, Display, TEXT("HoldArmorVisual: activated shells=%d"), HoldArmorVisuals.Num());
}

void UPerfectDodgeFeedbackComponent::DestroyHoldArmorVisual()
{
	for (FHoldArmorVisual& Visual : HoldArmorVisuals)
	{
		if (USkeletalMeshComponent* Source = Visual.Source.Get())
			Source->SetOverlayMaterial(Visual.PreviousOverlay.Get());
		if (Visual.Mesh.IsValid()) Visual.Mesh->DestroyComponent();
	}
	HoldArmorVisuals.Reset();
	if (bHoldArmorWasActive)
		UE_LOG(LogTemp, Display, TEXT("HoldArmorVisual: cleared"));
	bHoldArmorWasActive = false;
}

void UPerfectDodgeFeedbackComponent::UpdateHoldArmorVisual(float DeltaTime)
{
	const UPlayerRhythmCombatComponent* Combat = GetOwner()
		? GetOwner()->FindComponentByClass<UPlayerRhythmCombatComponent>() : nullptr;
	const bool bShouldBeActive = Combat && Combat->IsSustainedHoldActive();
	if (!bShouldBeActive)
	{
		DestroyHoldArmorVisual();
		return;
	}
	CreateHoldArmorVisual();
	if (HoldArmorVisuals.IsEmpty() || !Config || !GetWorld()) return;
	bHoldArmorWasActive = true;

	float BeatPulse = 0.0f;
	bool bStrongBeat = false;
	if (const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>(); Clock && Clock->IsRunning())
	{
		const double Beat = Clock->GetCurrentBeat();
		const double Fraction = Beat - FMath::FloorToDouble(Beat);
		const float DistanceToBeat = static_cast<float>(FMath::Min(Fraction, 1.0 - Fraction));
		BeatPulse = FMath::Pow(FMath::Clamp(1.0f - DistanceToBeat * 2.0f, 0.0f, 1.0f), 5.0f);
		const int32 BeatIndex = FMath::RoundToInt(Beat);
		bStrongBeat = Clock->GetBeatsPerBar() > 0 && BeatIndex % Clock->GetBeatsPerBar() == 0;
	}
	const float ImpactAge = static_cast<float>(GetWorld()->GetTimeSeconds() - HoldArmorImpactWorldSeconds);
	const float ImpactPulse = Config->HoldArmorImpactSeconds > KINDA_SMALL_NUMBER
		? FMath::Square(FMath::Clamp(1.0f - ImpactAge / Config->HoldArmorImpactSeconds, 0.0f, 1.0f)) : 0.0f;
	const float Opacity = FMath::Clamp(Config->HoldArmorBaseOpacity +
		BeatPulse * Config->HoldArmorBeatOpacity + ImpactPulse * Config->HoldArmorImpactOpacity, 0.0f, 0.85f);
	const FLinearColor BeatColor = bStrongBeat ? Config->HoldArmorStrongBeatColor : Config->HoldArmorColor;
	const FLinearColor Color = FMath::Lerp(Config->HoldArmorColor, BeatColor, BeatPulse);
	for (FHoldArmorVisual& Visual : HoldArmorVisuals)
	{
		if (Visual.Material.IsValid())
		{
			Visual.Material->SetScalarParameterValue(TEXT("Opacity"), Opacity);
			Visual.Material->SetScalarParameterValue(TEXT("GlowIntensity"), Config->HoldArmorGlowIntensity +
				BeatPulse * Config->HoldArmorBeatGlowBoost + ImpactPulse * Config->HoldArmorImpactGlowBoost);
			Visual.Material->SetVectorParameterValue(TEXT("ArmorColor"), Color);
		}
		if (Visual.Mesh.IsValid())
			Visual.Mesh->SetRelativeScale3D(FVector(Config->HoldArmorShellScale + BeatPulse * .006f + ImpactPulse * .004f));
	}
}

void UPerfectDodgeFeedbackComponent::ClearTransientVisuals()
{
	for (FGhostVisual& Visual : Ghosts) if (Visual.Mesh.IsValid()) Visual.Mesh->SetVisibility(false); // pooled
	for (FRingVisual& Visual : Rings) if (Visual.Mesh.IsValid()) Visual.Mesh->DestroyComponent();
	Ghosts.Reset(); Rings.Reset();
}
