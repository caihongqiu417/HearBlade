#include "Combat/BeatSyncedComboComponent.h"

#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "Combat/AttackData.h"
#include "Combat/BossMotionWarpingComponent.h"
#include "Combat/BossLocomotionAnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "RootMotionModifier_SkewWarp.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "Engine/World.h"
#include "Components/PoseableMeshComponent.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

namespace
{
UNiagaraComponent* SpawnBriefFire(UWorld* World, UNiagaraSystem* Effect, const FVector& Location,
	float Lifetime, float Scale = 1.f)
{
	if (!World || !Effect) return nullptr;
	if (UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Effect, Location))
	{
		Component->SetWorldScale3D(FVector(Scale));
		FTimerHandle Timer;
		World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(Component,
			[Component]() { Component->Deactivate(); }), FMath::Max(0.1f, Lifetime), false);
		return Component;
	}
	return nullptr;
}

void AlignBossToGround(AActor* Boss, AActor* Target)
{
	const UCapsuleComponent* Capsule = Boss ? Cast<UCapsuleComponent>(Boss->GetRootComponent()) : nullptr;
	if (!Capsule) return;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BossGroundAlign), false, Boss);
	if (Target) Query.AddIgnoredActor(Target);
	const FVector Location = Boss->GetActorLocation();
	FHitResult Ground;
	if (!Boss->GetWorld()->LineTraceSingleByChannel(Ground, Location + FVector(0, 0, 60),
		Location - FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight() + 180), ECC_Visibility, Query) ||
		Ground.ImpactNormal.Z < 0.6f) return;
	const float GroundedZ = Ground.ImpactPoint.Z + Capsule->GetScaledCapsuleHalfHeight() + 2.f;
	if (FMath::Abs(GroundedZ - Location.Z) > 90.f) return;
	if (FMath::Abs(GroundedZ - Location.Z) > 0.5f)
		Boss->SetActorLocation(FVector(Location.X, Location.Y, GroundedZ), true);
}
}

UBeatSyncedComboComponent::UBeatSyncedComboComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	LocomotionAnimClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(
		TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/ABP_DragonKnight_Movement.ABP_DragonKnight_Movement_C")));
}

void UBeatSyncedComboComponent::BeginPlay()
{
	Super::BeginPlay();
	// The Boss is an Actor rather than a Character. Its root capsule is the only
	// primitive swept by locomotion and montage root motion. Old placed instances
	// can retain a tiny overlap-only capsule even when the Blueprint default changed.
	if (UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(GetOwner()->GetRootComponent()))
	{
		Capsule->SetCapsuleSize(FMath::Max(Capsule->GetUnscaledCapsuleRadius(), 60.f),
			FMath::Max(Capsule->GetUnscaledCapsuleHalfHeight(), 110.f), true);
		Capsule->SetCollisionProfileName(TEXT("Pawn"));
		// Keep Pawn/world blocking, but do not let a circling or blinking Boss
		// repeatedly shorten the player's spring arm on the Camera channel.
		Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		// Existing placed Boss actors can also retain the old -90 cm mesh offset.
		// The greaves reference bounds reach approximately z=0 in mesh space, so
		// their root must sit at the bottom of the 110 cm capsule.
		TArray<USkeletalMeshComponent*> BossMeshes;
		GetOwner()->GetComponents<USkeletalMeshComponent>(BossMeshes);
		for (USkeletalMeshComponent* Mesh : BossMeshes)
		{
			if (!Mesh) continue;
			Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
			if (Mesh->GetAttachParent() != Capsule) continue;
			FVector Relative = Mesh->GetRelativeLocation();
			if (!FMath::IsNearlyEqual(Relative.Z, -90.f, 2.f) &&
				!FMath::IsNearlyEqual(Relative.Z, -110.f, 2.f)) continue;
			Relative.Z = -Capsule->GetUnscaledCapsuleHalfHeight();
			Mesh->SetRelativeLocation(Relative);
		}
		UE_LOG(LogTemp, Display, TEXT("BossCollision: Root capsule radius=%.0f halfHeight=%.0f profile=%s pawnResponse=%d"),
			Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight(),
			*Capsule->GetCollisionProfileName().ToString(), static_cast<int32>(Capsule->GetCollisionResponseToChannel(ECC_Pawn)));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("BossCollision: %s needs a root capsule; swept movement cannot block the player"),
			*GetNameSafe(GetOwner()));
	}
	// Every Boss primitive must ignore the spring arm probe. A weapon or an
	// accessory with its own collision can otherwise still retract the camera.
	TArray<UPrimitiveComponent*> BossPrimitives;
	GetOwner()->GetComponents<UPrimitiveComponent>(BossPrimitives);
	for (UPrimitiveComponent* Primitive : BossPrimitives)
		if (IsValid(Primitive)) Primitive->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	UClass* AnimClass = LocomotionAnimClass.LoadSynchronous();
	if (!AnimClass) return;
	const USkeleton* BossSkeleton = LoadObject<USkeleton>(nullptr,
		TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/MESHES/UE5/SK_Mannequin.SK_Mannequin"));
	if (!BossSkeleton) return;
	TArray<USkeletalMeshComponent*> Meshes;
	GetOwner()->GetComponents<USkeletalMeshComponent>(Meshes);
	int32 Assigned = 0;
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (!IsValid(Mesh) || !IsValid(Mesh->GetSkeletalMeshAsset()) ||
			Mesh->GetSkeletalMeshAsset()->GetSkeleton() != BossSkeleton ||
			(!MeshComponentTag.IsNone() && !Mesh->ComponentHasTag(MeshComponentTag))) continue;
		Mesh->SetAnimInstanceClass(AnimClass);
		if (UAnimInstance* Instance = Mesh->GetAnimInstance())
			Instance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
		if (!LocomotionMesh.IsValid() || Mesh->GetFName() == TEXT("UE5_body"))
			LocomotionMesh = Mesh;
		++Assigned;
	}
	if (LocomotionMesh.IsValid()) AddTickPrerequisiteComponent(LocomotionMesh.Get());
	static const TCHAR* WalkNames[] = {
		TEXT("move_walk_front"), TEXT("move_walk_frontR45"), TEXT("move_walk_right"), TEXT("move_walk_backR45"),
		TEXT("move_walk_back"), TEXT("move_walk_backL45"), TEXT("move_walk_left"), TEXT("move_walk_frontL45")
	};
	DirectionalWalkAnimations.Reset();
	for (const TCHAR* Name : WalkNames)
	{
		const FString Path = FString::Printf(TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/DEMO/UE5_ANIMATIONS/%s.%s"), Name, Name);
		UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, *Path);
		DirectionalWalkAnimations.Add(Sequence);
		if (Sequence)
		{
			const float Length = Sequence->GetPlayLength();
			const float RootDistance = Sequence->ExtractRootMotionFromRange(0.0f, Length,
				FAnimExtractContext(Length, true)).GetTranslation().Size2D();
			UE_LOG(LogTemp, Display, TEXT("BossLocomotion: Clip=%s RootDistance=%.1f Length=%.2f"), Name, RootDistance, Length);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("BeatSyncedCombo: Assigned locomotion AnimBP to %d meshes on %s"),
		Assigned, *GetNameSafe(GetOwner()));
	TArray<UNiagaraComponent*> NiagaraComponents;
	GetOwner()->GetComponents<UNiagaraComponent>(NiagaraComponents);
	for (UNiagaraComponent* Component : NiagaraComponents)
	{
		if (Component && Component->GetName().Contains(TEXT("Fire"), ESearchCase::IgnoreCase))
			FirePulseTargets.Add({Component, Component->GetRelativeScale3D()});
	}
	UE_LOG(LogTemp, Display, TEXT("BeatFire: %d persistent fire components follow the music beat"),
		FirePulseTargets.Num());
	TArray<UMeshComponent*> VisualMeshes;
	GetOwner()->GetComponents<UMeshComponent>(VisualMeshes);
	USceneComponent* WeaponAnchor = nullptr;
	for (UMeshComponent* VisualMesh : VisualMeshes)
	{
		if (!IsValid(VisualMesh)) continue;
		bool bArmorMesh = false;
		for (int32 Index = 0; Index < VisualMesh->GetNumMaterials(); ++Index)
		{
			UMaterialInterface* Material = VisualMesh->GetMaterial(Index);
			if (!IsValid(Material)) continue;
			const bool bSword = Material->GetName().Contains(TEXT("Sword_Fire"), ESearchCase::IgnoreCase);
			const bool bBodyFire = !bSword && Material->GetName().Contains(TEXT("_Fire"), ESearchCase::IgnoreCase);
			if (!bSword && !bBodyFire) continue;
			bArmorMesh |= bBodyFire;
			float BaseIntensity = 20.f;
			if (!bSword || !Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Emissive Intensity")), BaseIntensity)) continue;
			if (UMaterialInstanceDynamic* DynamicMaterial = VisualMesh->CreateDynamicMaterialInstance(Index))
			{
				SwordGlowTargets.Add({DynamicMaterial, BaseIntensity});
				if (!WeaponAnchor) WeaponAnchor = VisualMesh;
			}
		}
		if (bArmorMesh) BeatOverlayMeshes.Add(VisualMesh);
	}
	if (!BeatOverlayMaterial)
		BeatOverlayMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/MATERIALS/M_BossBeatOverlay.M_BossBeatOverlay"));
	if (BeatOverlayMaterial)
	{
		BeatOverlayInstance = UMaterialInstanceDynamic::Create(BeatOverlayMaterial, GetOwner());
		BeatOverlayInstance->SetScalarParameterValue(TEXT("StrongPulse"), 0.f);
		BeatOverlayInstance->SetScalarParameterValue(TEXT("WeakPulse"), 0.f);
		for (const TWeakObjectPtr<UMeshComponent>& Mesh : BeatOverlayMeshes)
			if (UMeshComponent* Component = Mesh.Get()) Component->SetOverlayMaterial(BeatOverlayInstance);
	}
	FName WeaponSocket = NAME_None;
	if (!WeaponAnchor)
	{
		WeaponAnchor = LocomotionMesh.Get();
		if (USkeletalMeshComponent* BodyMesh = LocomotionMesh.Get();
			BodyMesh && BodyMesh->DoesSocketExist(TEXT("hand_r")))
			WeaponSocket = TEXT("hand_r");
	}
	if (!WeaponAnchor) WeaponAnchor = GetOwner()->GetRootComponent();
	if (WeaponAnchor)
	{
		UPointLightComponent* Light = NewObject<UPointLightComponent>(GetOwner(), TEXT("BossStrongBeatSwordLight"));
		GetOwner()->AddInstanceComponent(Light);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetupAttachment(WeaponAnchor, WeaponSocket);
		Light->SetRelativeLocation(WeaponAnchor == GetOwner()->GetRootComponent()
			? FVector(0.f, 0.f, 150.f) : FVector(25.f, 0.f, 0.f));
		Light->SetLightColor(FLinearColor(1.f, .18f, .025f));
		Light->SetAttenuationRadius(270.f);
		Light->SetCastShadows(false);
		Light->SetIntensity(0.f);
		Light->RegisterComponent();
		SwordChargeLight = Light;
	}
	USoundBase* UserSwordSlice = LoadObject<USoundBase>(nullptr,
		TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/SOUND/SW_DragonKnight_SwordSlice.SW_DragonKnight_SwordSlice"));
	const bool bLegacyStrong = StrongBeatSound && StrongBeatSound->GetName().StartsWith(TEXT("SW_BossBlade"));
	const bool bLegacyWeak = WeakBladeSound && WeakBladeSound->GetName().StartsWith(TEXT("SW_BossBlade"));
	if (UserSwordSlice && (!StrongBeatSound || bLegacyStrong)) StrongBeatSound = UserSwordSlice;
	if (UserSwordSlice && (!WeakBladeSound || bLegacyWeak)) WeakBladeSound = UserSwordSlice;
	UE_LOG(LogTemp, Display, TEXT("BossStrongBeat: swordMaterials=%d overlayMeshes=%d overlay=%s anchor=%s socket=%s strongSound=%s weakSound=%s"),
		SwordGlowTargets.Num(), BeatOverlayMeshes.Num(), *GetNameSafe(BeatOverlayMaterial), *GetNameSafe(WeaponAnchor), *WeaponSocket.ToString(),
		*GetNameSafe(StrongBeatSound), *GetNameSafe(WeakBladeSound));
}

float UBeatSyncedComboComponent::GetWalkClipLengthSeconds() const
{
	return DirectionalWalkAnimations.IsValidIndex(0) && DirectionalWalkAnimations[0]
		? DirectionalWalkAnimations[0]->GetPlayLength() : 0.f;
}

void UBeatSyncedComboComponent::UpdateBeatFirePulse(const UBeatClockSubsystem* Clock)
{
	float Scale = 1.f;
	if (Clock && Clock->IsRunning() && FireBeatPulseStrength > 0.f)
	{
		const double Beat = Clock->GetCurrentBeat();
		const float Phase = static_cast<float>(FMath::Frac(Beat));
		const float Accent = FMath::Pow(FMath::Max(0.f, FMath::Cos(2.f * UE_PI * Phase)), 4.f);
		const int32 NearestBeat = FMath::RoundToInt(Beat);
		const bool bBarStart = Clock->GetBeatsPerBar() > 0 && NearestBeat % Clock->GetBeatsPerBar() == 0;
		Scale += FireBeatPulseStrength * Accent * (bBarStart ? 1.35f : 1.f);
	}
	if (Clock && Clock->IsRunning() && bStrongBeatThisAttack && (bQueued || bPlaying))
	{
		const double UntilImpact = ScheduledFirstImpactAudioSeconds - Clock->GetAudioNowSeconds();
		const double ChargeSeconds = FMath::Max(.01, StrongBeatChargeBeats * Clock->GetSecondsPerBeat());
		if (UntilImpact > 0.0 && UntilImpact <= ChargeSeconds)
		{
			const float Alpha = FMath::Clamp(static_cast<float>(1.0 - UntilImpact / ChargeSeconds), 0.f, 1.f);
			const float Ease = Alpha * Alpha * (3.f - 2.f * Alpha);
			Scale *= FMath::Lerp(1.f, .32f, Ease);
		}
		else if (bBlinkExecuted && UntilImpact <= 0.0 && UntilImpact > -.3)
		{
			Scale *= 1.f + .95f * FMath::Exp(static_cast<float>(UntilImpact / .11));
		}
	}
	for (const FFirePulseTarget& Target : FirePulseTargets)
		if (UNiagaraComponent* Component = Target.Component.Get())
			Component->SetRelativeScale3D(Target.BaseScale * Scale);
}

void UBeatSyncedComboComponent::UpdateStrongBeatPresentation(const UBeatClockSubsystem* Clock)
{
	float Glow = 1.f;
	float LightIntensity = 0.f;
	if (Clock && Clock->IsRunning() && bStrongBeatThisAttack && (bQueued || bPlaying))
	{
		const double UntilImpact = ScheduledFirstImpactAudioSeconds - Clock->GetAudioNowSeconds();
		const double ChargeSeconds = FMath::Max(.01, StrongBeatChargeBeats * Clock->GetSecondsPerBeat());
		if (UntilImpact > 0.0 && UntilImpact <= ChargeSeconds)
		{
			const float Alpha = FMath::Clamp(static_cast<float>(1.0 - UntilImpact / ChargeSeconds), 0.f, 1.f);
			const float Ease = Alpha * Alpha * (3.f - 2.f * Alpha);
			Glow = FMath::Lerp(1.f, StrongBeatSwordGlowMultiplier, Ease);
			LightIntensity = 3800.f * Ease;
			if (!bStrongBeatChargeStarted)
			{
				bStrongBeatChargeStarted = true;
				if (BlinkSettings.WarningEffect)
				{
					const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(GetOwner()->GetRootComponent());
					const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 110.f;
					OriginWarningFX = SpawnBriefFire(GetWorld(), BlinkSettings.WarningEffect,
						GetOwner()->GetActorLocation() - FVector(0, 0, HalfHeight - 2.f),
						static_cast<float>(ChargeSeconds + .1), 1.4f);
				}
				UE_LOG(LogTemp, Display, TEXT("BossStrongBeat: charge beat=%d audio=%.3f target=%.3f"),
					FirstImpactBeat, Clock->GetAudioNowSeconds(), ScheduledFirstImpactAudioSeconds);
			}
			if (UNiagaraComponent* Effect = OriginWarningFX.Get())
				Effect->SetWorldScale3D(FVector(FMath::Lerp(1.4f, .35f, Ease)));
		}
		else if (bBlinkExecuted && UntilImpact <= 0.0 && UntilImpact > -.3)
		{
			const float Flash = FMath::Exp(static_cast<float>(UntilImpact / .11));
			Glow = 1.f + 4.f * Flash;
			LightIntensity = 7600.f * Flash;
		}
		if (bBlinkWarningShown && !bBlinkExecuted && BlinkSettings.WarningBeats > 0.f)
		{
			const float WarningProgress = FMath::Clamp(static_cast<float>(1.0 -
				UntilImpact / (BlinkSettings.WarningBeats * Clock->GetSecondsPerBeat())), 0.f, 1.f);
			const float WarningScale = FMath::Lerp(1.3f, .42f, WarningProgress);
			if (UNiagaraComponent* Effect = LandingWarningFX.Get()) Effect->SetWorldScale3D(FVector(WarningScale));
		}
	}
	for (const FSwordGlowTarget& Target : SwordGlowTargets)
		if (UMaterialInstanceDynamic* Material = Target.Material.Get())
			Material->SetScalarParameterValue(TEXT("Emissive Intensity"), Target.BaseIntensity * Glow);
	if (UPointLightComponent* Light = SwordChargeLight.Get()) Light->SetIntensity(LightIntensity);
}

void UBeatSyncedComboComponent::TriggerBeatOverlay(bool bStrong, double AudioNow)
{
	if (!BeatOverlayInstance) return;
	if (bStrong)
	{
		LastStrongOverlayAudioSeconds = AudioNow;
		BeatOverlayInstance->SetScalarParameterValue(TEXT("StrongPulse"), 1.f);
	}
	else
	{
		LastWeakOverlayAudioSeconds = AudioNow;
		BeatOverlayInstance->SetScalarParameterValue(TEXT("WeakPulse"), 1.f);
	}
}

void UBeatSyncedComboComponent::UpdateBeatOverlay(const UBeatClockSubsystem* Clock)
{
	if (!BeatOverlayInstance || !Clock || !Clock->IsRunning()) return;
	const double Now = Clock->GetAudioNowSeconds();
	auto Envelope = [Now](double Last, float Duration) -> float
	{
		const double Age = Now - Last;
		if (Age < 0.0 || Age >= Duration) return 0.f;
		const float Alpha = static_cast<float>(Age / Duration);
		return FMath::Square(1.f - Alpha);
	};
	BeatOverlayInstance->SetScalarParameterValue(TEXT("StrongPulse"),
		Envelope(LastStrongOverlayAudioSeconds, FMath::Max(.1f, StrongOverlaySeconds)));
	BeatOverlayInstance->SetScalarParameterValue(TEXT("WeakPulse"),
		Envelope(LastWeakOverlayAudioSeconds, FMath::Max(.05f, WeakOverlaySeconds)));
}

void UBeatSyncedComboComponent::ResetStrongBeatPresentation()
{
	const bool bWasCharging = bStrongBeatChargeStarted;
	for (const FSwordGlowTarget& Target : SwordGlowTargets)
		if (UMaterialInstanceDynamic* Material = Target.Material.Get())
			Material->SetScalarParameterValue(TEXT("Emissive Intensity"), Target.BaseIntensity);
	if (UPointLightComponent* Light = SwordChargeLight.Get()) Light->SetIntensity(0.f);
	if (UNiagaraComponent* Effect = OriginWarningFX.Get()) Effect->Deactivate();
	if (UNiagaraComponent* Effect = LandingWarningFX.Get()) Effect->Deactivate();
	OriginWarningFX.Reset();
	LandingWarningFX.Reset();
	bStrongBeatChargeStarted = false;
	bStrongBeatThisAttack = false;
	if (bWasCharging) UE_LOG(LogTemp, Display, TEXT("BossStrongBeat: restored sword glow, light and warning fire"));
}

bool UBeatSyncedComboComponent::QueueAttack(UAttackDataAsset* InAttackData)
{
	if (bQueued || bPlaying || !IsValid(InAttackData)) return false;
	AttackData = InAttackData;
	return QueueCombo();
}

void UBeatSyncedComboComponent::ConfigureNextBlink(const FBeatBlinkSettings& Settings)
{
	BlinkSettings = Settings;
	bBlinkRequested = true;
	bBlinkWarningShown = false;
	bBlinkExecuted = false;
}

void UBeatSyncedComboComponent::ClearBlink()
{
	if (bBlinkWarningShown && !bBlinkExecuted) OnBlinkCancelled.Broadcast();
	ResetStrongBeatPresentation();
	bBlinkRequested = false;
	bBlinkWarningShown = false;
	bBlinkExecuted = false;
}

bool UBeatSyncedComboComponent::LockBlinkDestination()
{
	AActor* Boss = GetOwner();
	AActor* Target = AttackTarget.Get();
	if (!IsValid(Boss) || !IsValid(Target)) return false;
	const FVector TargetForward = Target->GetActorForwardVector().GetSafeNormal2D();
	const float Side = bNextBlinkOnRight ? 1.0f : -1.0f;
	const FVector Direction = TargetForward.RotateAngleAxis(Side * BlinkSettings.SideAngleDegrees, FVector::UpVector);
	const FVector Desired = Target->GetActorLocation() + Direction * BlinkSettings.LandingDistance;
	FHitResult Ground;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BeatBlinkGround), false, Boss);
	Query.AddIgnoredActor(Target);
	if (!GetWorld()->LineTraceSingleByChannel(Ground, Desired + FVector(0, 0, 250),
		Desired - FVector(0, 0, 600), ECC_Visibility, Query)) return false;
	const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Boss->GetRootComponent());
	if (!Capsule) return false;
	FVector Candidate = Ground.ImpactPoint + FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight() + 3.0f);
	const FRotator Facing(0, (Target->GetActorLocation() - Candidate).Rotation().Yaw, 0);
	if (!GetWorld()->FindTeleportSpot(Boss, Candidate, Facing) ||
		FVector::Dist2D(Candidate, Desired) > 75.0f ||
		FVector::Dist2D(Candidate, Target->GetActorLocation()) < 150.0f) return false;
	LockedBlinkLocation = Candidate;
	LockedBlinkGroundZ = Ground.ImpactPoint.Z;
	LockedBlinkRotation = Facing;
	bNextBlinkOnRight = !bNextBlinkOnRight;
	return true;
}

void UBeatSyncedComboComponent::SetAttackTarget(AActor* InTarget)
{
	AttackTarget = InTarget;
}

void UBeatSyncedComboComponent::ClearWarping()
{
	if (IsValid(WarpComponent))
	{
		WarpComponent->RemoveAllWarpTargets();
		WarpComponent->DestroyComponent();
		WarpComponent = nullptr;
	}
	ActiveWarpWindowIndex = INDEX_NONE;
}

void UBeatSyncedComboComponent::UpdateWarpTarget(float AnimationPosition)
{
	if (!IsValid(WarpComponent) || !AttackTarget.IsValid() || !IsValid(AttackData)) return;
	for (int32 Index = 0; Index < AttackData->ImpactMarkers.Num(); ++Index)
	{
		const float Impact = AttackData->ImpactMarkers[Index].AnimationTimeSeconds;
		const float PreviousImpact = Index > 0 ? AttackData->ImpactMarkers[Index - 1].AnimationTimeSeconds : AttackData->ClipStartSeconds;
		const float WindowStart = FMath::Max(PreviousImpact + (Index > 0 ? 0.05f : 0.0f),
			Impact - AttackData->WarpLeadAnimationSeconds);
		if (AnimationPosition < WindowStart || AnimationPosition > Impact) continue;
		if (ActiveWarpWindowIndex != Index)
		{
			ActiveWarpWindowIndex = Index;
			WarpAnchorLocation = GetOwner()->GetActorLocation();
			UE_LOG(LogTemp, Display, TEXT("BeatSyncedCombo: MotionWarping window=%d start=%.3f end=%.3f playerDistance=%.0f"),
				Index + 1, WindowStart, Impact, FVector::Dist2D(WarpAnchorLocation, AttackTarget->GetActorLocation()));
		}
		FVector TargetLocation = AttackTarget->GetActorLocation();
		FVector ToPlayer = TargetLocation - WarpAnchorLocation;
		ToPlayer.Z = 0.0f;
		if (ToPlayer.IsNearlyZero()) return;
		const FVector Direction = ToPlayer.GetSafeNormal();
		TargetLocation -= Direction * AttackData->WarpStopDistance;
		FVector Travel = TargetLocation - WarpAnchorLocation;
		Travel.Z = 0.0f;
		Travel = Travel.GetClampedToMaxSize(AttackData->MaxWarpTravelPerStrike);
		TargetLocation = WarpAnchorLocation + Travel;
		const FRotator Facing(0.0f, (AttackTarget->GetActorLocation() - TargetLocation).Rotation().Yaw, 0.0f);
		WarpComponent->AddOrUpdateWarpTargetFromTransform(TEXT("Player"), FTransform(Facing, TargetLocation));
		return;
	}
}

int32 UBeatSyncedComboComponent::GetImpactCount() const
{
	return AttackData && !AttackData->ImpactMarkers.IsEmpty() ? AttackData->ImpactMarkers.Num() : 1;
}

float UBeatSyncedComboComponent::GetImpactTime(int32 Index) const
{
	return AttackData->ImpactMarkers.IsEmpty()
		? AttackData->AuthoredImpactTime
		: AttackData->ImpactMarkers[Index].AnimationTimeSeconds;
}

bool UBeatSyncedComboComponent::QueueCombo()
{
	if (bQueued || bPlaying || !IsValid(AttackData) || !IsValid(AttackData->AttackMontage))
	{
		return false;
	}
	if (AttackData->ImpactMarkers.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("BeatSyncedCombo: 动作尚未标定命中时间，请先在攻击数据中添加 ImpactMarkers"));
		return false;
	}

	UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	if (!Clock || !Clock->IsRunning() || AttackData->PlayRate <= 0.0f)
	{
		return false;
	}

	const float MontageLength = AttackData->AttackMontage->GetPlayLength();
	const float ClipStart = AttackData->ClipStartSeconds;
	const float ClipEnd = AttackData->GetClipEndSeconds();
	if (ClipStart < 0.0f || ClipStart >= ClipEnd || ClipEnd > MontageLength)
	{
		UE_LOG(LogTemp, Warning, TEXT("BeatSyncedCombo: 攻击片段起止时间无效"));
		return false;
	}
	float PreviousTime = ClipStart - KINDA_SMALL_NUMBER;
	double PreviousBeatOffset = -1.0;
	for (int32 Index = 0; Index < GetImpactCount(); ++Index)
	{
		const float Time = GetImpactTime(Index);
		const double Offset = AttackData->ImpactMarkers.IsEmpty() ? 0.0 : AttackData->ImpactMarkers[Index].BeatOffset;
		if (Time < ClipStart || Time > ClipEnd || Time <= PreviousTime || Offset < PreviousBeatOffset || (Index == 0 && Offset != 0))
		{
			UE_LOG(LogTemp, Warning, TEXT("BeatSyncedCombo: AttackData 中的命中时间或拍数无效"));
			return false;
		}
		PreviousTime = Time;
		PreviousBeatOffset = Offset;
	}
	for (const float Rate : AttackData->SegmentPlayRates)
	{
		if (Rate <= 0.0f)
		{
			UE_LOG(LogTemp, Warning, TEXT("BeatSyncedCombo: 逐段播放速度必须大于 0"));
			return false;
		}
	}

	const double ClipStartElapsed = AttackData->GetElapsedAtAnimationTime(ClipStart);
	const double FirstImpactDelay = AttackData->GetElapsedAtAnimationTime(GetImpactTime(0)) - ClipStartElapsed;
	const double LeadSeconds = FMath::Max<double>(SchedulingLeadSeconds,
		bBlinkRequested ? FMath::Max(BlinkSettings.WarningBeats, StrongBeatChargeBeats) * Clock->GetSecondsPerBeat() + 0.05 : 0.0);
	const double EarliestImpactTime = Clock->GetAudioNowSeconds() + FirstImpactDelay + LeadSeconds;
	const double BeatDuration = Clock->GetSecondsPerBeat();
	for (int32 Index = 1; Index < GetImpactCount(); ++Index)
	{
		const double AttackGap = AttackData->GetElapsedAtAnimationTime(GetImpactTime(Index)) - FirstImpactDelay;
		const double BeatGap = AttackData->ImpactMarkers[Index].BeatOffset * BeatDuration;
		if (FMath::Abs(AttackGap - BeatGap) > 0.05)
		{
			UE_LOG(LogTemp, Warning, TEXT("BeatSyncedCombo: 第 %d 击与目标拍点相差 %.3f 秒；请调整动画间隔或 PlayRate"),
				Index + 1, AttackGap - BeatGap);
		}
	}
	int32 CandidateBeat = FMath::Max(0, FMath::CeilToInt(Clock->GetCurrentBeat() +
		(FirstImpactDelay + LeadSeconds) / BeatDuration));
	while (Clock->GetBeatTimeSeconds(CandidateBeat) < EarliestImpactTime)
	{
		++CandidateBeat;
	}
	if (AttackData->FirstImpactBeatInBar > 0)
	{
		const int32 BeatsPerBar = FMath::Max(1, Clock->GetBeatsPerBar());
		const int32 WantedBeatInBar = FMath::Clamp(AttackData->FirstImpactBeatInBar, 1, BeatsPerBar) - 1;
		CandidateBeat += (WantedBeatInBar - CandidateBeat % BeatsPerBar + BeatsPerBar) % BeatsPerBar;
	}
	FirstImpactBeat = CandidateBeat;
	bStrongBeatThisAttack = bBlinkRequested && CandidateBeat % FMath::Max(1, Clock->GetBeatsPerBar()) == 0;
	bStrongBeatChargeStarted = false;
	ScheduledFirstImpactAudioSeconds = Clock->GetBeatTimeSeconds(FirstImpactBeat);
	ScheduledStartAudioSeconds = ScheduledFirstImpactAudioSeconds - FirstImpactDelay;
	UE_LOG(LogTemp, Display, TEXT("BeatSyncedCombo: Boss=%s FirstImpactBeat=%d BeatInBar=%d StartAudio=%.3f FirstImpactDelay=%.3f"),
		*GetNameSafe(GetOwner()), FirstImpactBeat, FirstImpactBeat % FMath::Max(1, Clock->GetBeatsPerBar()) + 1,
		ScheduledStartAudioSeconds, FirstImpactDelay);
	NextImpactIndex = 0;
	NextBladeSoundIndex = 0;
	bLastAttackCompleted = false;
	bQueued = true;
	return true;
}

void UBeatSyncedComboComponent::CancelCombo()
{
	ClearBlink();
	ClearWarping();
	if (bPlaying && IsValid(AttackData) && IsValid(AttackData->AttackMontage))
	{
		TArray<USkeletalMeshComponent*> Meshes;
		GetOwner()->GetComponents<USkeletalMeshComponent>(Meshes);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (IsValid(Mesh) && IsValid(Mesh->GetSkeletalMeshAsset()) &&
				Mesh->GetSkeletalMeshAsset()->GetSkeleton() == AttackData->AttackMontage->GetSkeleton() &&
				(MeshComponentTag.IsNone() || Mesh->ComponentHasTag(MeshComponentTag)))
			{
				if (Mesh->GetAnimationMode() == EAnimationMode::AnimationBlueprint && Mesh->GetAnimInstance())
				{
					Mesh->GetAnimInstance()->Montage_Stop(0.15f, AttackData->AttackMontage);
					Mesh->GetAnimInstance()->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
				}
				else
					Mesh->Stop();
			}
		}
	}
	bQueued = false;
	bPlaying = false;
	bLastAttackCompleted = false;
	NextImpactIndex = 0;
	NextBladeSoundIndex = 0;
	RootMotionMesh.Reset();
	AttackTarget.Reset();
	LocomotionResumeWorldSeconds = GetWorld()->GetTimeSeconds() + 0.2;
}

bool UBeatSyncedComboComponent::StartPlayback(double AudioNowSeconds)
{
	ClearWarping();
	const float StartPosition = AttackData->GetAnimationTimeAtElapsed(
		AttackData->GetElapsedAtAnimationTime(AttackData->ClipStartSeconds) +
		static_cast<float>(AudioNowSeconds - ScheduledStartAudioSeconds));
	if (StartPosition >= AttackData->GetClipEndSeconds())
	{
		return false;
	}

	TArray<USkeletalMeshComponent*> Meshes;
	GetOwner()->GetComponents<USkeletalMeshComponent>(Meshes);
	int32 PlayedCount = 0;
	RootMotionMesh.Reset();
	LastRootMotionPosition = StartPosition;
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (!IsValid(Mesh) || !IsValid(Mesh->GetSkeletalMeshAsset()) ||
			Mesh->GetSkeletalMeshAsset()->GetSkeleton() != AttackData->AttackMontage->GetSkeleton() ||
			(!MeshComponentTag.IsNone() && !Mesh->ComponentHasTag(MeshComponentTag)))
		{
			continue;
		}
		if (Mesh->GetAnimationMode() == EAnimationMode::AnimationBlueprint && Mesh->GetAnimInstance())
		{
			UAnimInstance* Instance = Mesh->GetAnimInstance();
			Mesh->ConsumeRootMotion();
			Instance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
			if (Instance->Montage_Play(AttackData->AttackMontage,
				AttackData->GetRateAtAnimationTime(StartPosition)) > 0.0f)
			{
				Instance->Montage_SetPosition(AttackData->AttackMontage, StartPosition);
				if (!RootMotionMesh.IsValid()) RootMotionMesh = Mesh;
				++PlayedCount;
			}
		}
		else
		{
			Mesh->PlayAnimation(AttackData->AttackMontage, false);
			if (UAnimSingleNodeInstance* Instance = Mesh->GetSingleNodeInstance())
			{
				Instance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
				Instance->SetPlayRate(AttackData->GetRateAtAnimationTime(StartPosition));
				Instance->SetPosition(StartPosition, false);
				if (!RootMotionMesh.IsValid()) RootMotionMesh = Mesh;
				++PlayedCount;
			}
		}
	}
	if (PlayedCount == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("BeatSyncedCombo: Boss 上没有匹配 Montage 骨架的 SkeletalMeshComponent"));
		return false;
	}
	if (AttackData->bWarpToTarget && AttackTarget.IsValid() && RootMotionMesh.IsValid())
	{
		WarpComponent = NewObject<UBossMotionWarpingComponent>(GetOwner(), TEXT("BeatAttackMotionWarping"));
		GetOwner()->AddOwnedComponent(WarpComponent);
		WarpComponent->RegisterComponent();
		UBossMotionWarpingAdapter* Adapter = WarpComponent->CreateOwnerAdapter<UBossMotionWarpingAdapter>();
		Adapter->SetBoss(GetOwner(), RootMotionMesh.Get());
		for (int32 Index = 0; Index < AttackData->ImpactMarkers.Num(); ++Index)
		{
			const float Impact = AttackData->ImpactMarkers[Index].AnimationTimeSeconds;
			const float PreviousImpact = Index > 0 ? AttackData->ImpactMarkers[Index - 1].AnimationTimeSeconds : AttackData->ClipStartSeconds;
			const float WindowStart = FMath::Max(PreviousImpact + (Index > 0 ? 0.05f : 0.0f),
				Impact - AttackData->WarpLeadAnimationSeconds);
			if (WindowStart >= Impact) continue;
			URootMotionModifier_SkewWarp::AddRootMotionModifierSkewWarp(WarpComponent, AttackData->AttackMontage,
				WindowStart, Impact, TEXT("Player"), EWarpPointAnimProvider::None,
				FTransform::Identity, NAME_None, true, true, true);
		}
		UE_LOG(LogTemp, Display, TEXT("BeatSyncedCombo: MotionWarping enabled for %d impacts"), AttackData->ImpactMarkers.Num());
	}
	OnComboStarted.Broadcast(FirstImpactBeat);
	UE_LOG(LogTemp, Display, TEXT("BeatSyncedCombo: Playback started Boss=%s Audio=%.3f"),
		*GetNameSafe(GetOwner()), AudioNowSeconds);
	return true;
}

void UBeatSyncedComboComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (USkeletalMeshComponent* Mesh = LocomotionMesh.Get())
	{
		// The animation graph plays the pose; extract the same clip's root track here.
		// Actor-based Bosses do not have CharacterMovement to consume animation root motion.
		Mesh->ConsumeRootMotion();
		const UBossLocomotionAnimInstance* Anim = Cast<UBossLocomotionAnimInstance>(Mesh->GetAnimInstance());
		LastLocomotionSpeed = 0.0f;
		bLastLocomotionBlocked = false;
		if (!bQueued && !bPlaying && GetWorld()->GetTimeSeconds() >= LocomotionResumeWorldSeconds &&
			Anim && Anim->bIsMoving && DirectionalWalkAnimations.IsValidIndex(0))
		{
			// All eight walk samples share the same duration and root travel. The Blend Space
			// blends their poses; one continuous root clock moves the collision capsule.
			if (const UAnimSequence* Clip = DirectionalWalkAnimations[0])
			{
				const float Length = Clip->GetPlayLength();
				float Remaining = FMath::Max(0.0f, DeltaTime * Mesh->GlobalAnimRateScale);
				float RootDistance = 0.0f;
				while (Length > KINDA_SMALL_NUMBER && Remaining > KINDA_SMALL_NUMBER)
				{
					const float End = FMath::Min(Length, LocomotionAnimationTime + Remaining);
					RootDistance += Clip->ExtractRootMotionFromRange(LocomotionAnimationTime, End,
						FAnimExtractContext(End, true)).GetTranslation().Size2D();
					Remaining -= End - LocomotionAnimationTime;
					LocomotionAnimationTime = End >= Length - KINDA_SMALL_NUMBER ? 0.0f : End;
				}
				if (RootDistance > KINDA_SMALL_NUMBER)
				{
					const FVector LocalDirection(Anim->MoveForward, Anim->MoveRight, 0.0f);
					const FVector Delta = GetOwner()->GetActorTransform().TransformVectorNoScale(LocalDirection) * RootDistance;
					const FVector Before = GetOwner()->GetActorLocation();
					const float HalfHeight = Cast<UCapsuleComponent>(GetOwner()->GetRootComponent())
						? CastChecked<UCapsuleComponent>(GetOwner()->GetRootComponent())->GetScaledCapsuleHalfHeight() : 100.0f;
					FCollisionQueryParams Query(SCENE_QUERY_STAT(BossLocomotionFloor), false, GetOwner());
					FHitResult Floor;
					const FVector Probe = Before + Delta;
					const bool bFloor = GetWorld()->LineTraceSingleByChannel(Floor,
						Probe + FVector(0, 0, 50.0f), Probe - FVector(0, 0, HalfHeight + 180.0f),
						ECC_Visibility, Query);
					if (bFloor && Floor.ImpactNormal.Z > 0.6f &&
						FMath::Abs(Floor.ImpactPoint.Z + HalfHeight - Before.Z) < 130.0f)
					{
						FHitResult Hit;
						GetOwner()->AddActorWorldOffset(Delta, true, &Hit);
						if (Hit.IsValidBlockingHit())
						{
							const FVector Slide = FVector::VectorPlaneProject(Delta * (1.0f - Hit.Time), Hit.Normal);
							GetOwner()->AddActorWorldOffset(FVector(Slide.X, Slide.Y, 0.0f), true);
						}
						AlignBossToGround(GetOwner(), AttackTarget.Get());
					}
					LastLocomotionSpeed = FVector::Dist2D(Before, GetOwner()->GetActorLocation()) /
						FMath::Max(DeltaTime, KINDA_SMALL_NUMBER);
					bLastLocomotionBlocked = LastLocomotionSpeed < 5.0f;
				}
			}
		}
		else
		{
			LocomotionAnimationTime = 0.0f;
		}
		if (Anim && Anim->bIsMoving && !bQueued && !bPlaying &&
			GetWorld()->GetTimeSeconds() - LastLocomotionLogWorldSeconds > 2.0)
		{
			UE_LOG(LogTemp, Display, TEXT("BossLocomotion: Direction=%d ActualSpeed=%.0f Blocked=%d Boss=%s"),
				Anim->MoveDirectionIndex, LastLocomotionSpeed,
				bLastLocomotionBlocked ? 1 : 0, *GetOwner()->GetActorLocation().ToString());
			LastLocomotionLogWorldSeconds = GetWorld()->GetTimeSeconds();
		}
	}
	UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	UpdateBeatFirePulse(Clock);
	UpdateStrongBeatPresentation(Clock);
	UpdateBeatOverlay(Clock);
	if (!Clock || !Clock->IsRunning())
	{
		return;
	}
	const double Now = Clock->GetAudioNowSeconds();
	if (bQueued || bPlaying)
	{
		const double WarningTime = ScheduledFirstImpactAudioSeconds - BlinkSettings.WarningBeats * Clock->GetSecondsPerBeat();
		if (bBlinkRequested && !bBlinkWarningShown && Now >= WarningTime)
		{
			if (!LockBlinkDestination())
			{
				UE_LOG(LogTemp, Warning, TEXT("BeatBlink: No safe landing; cancelling attack at beat %d"), FirstImpactBeat);
				CancelCombo();
				return;
			}
			bBlinkWarningShown = true;
			OnBlinkWarning.Broadcast(LockedBlinkLocation, ScheduledFirstImpactAudioSeconds);
			if (BlinkSettings.WarningEffect)
			{
				const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(GetOwner()->GetRootComponent());
				const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 110.f;
				const float WarningDuration = BlinkSettings.WarningBeats * Clock->GetSecondsPerBeat();
				if (!OriginWarningFX.IsValid())
					OriginWarningFX = SpawnBriefFire(GetWorld(), BlinkSettings.WarningEffect,
						GetOwner()->GetActorLocation() - FVector(0, 0, HalfHeight - 2.f),
						WarningDuration, bStrongBeatThisAttack ? 1.3f : 1.f);
				LandingWarningFX = SpawnBriefFire(GetWorld(), BlinkSettings.WarningEffect,
					FVector(LockedBlinkLocation.X, LockedBlinkLocation.Y, LockedBlinkGroundZ + 2.f),
					WarningDuration, bStrongBeatThisAttack ? 1.3f : 1.f);
			}
			UE_LOG(LogTemp, Display, TEXT("BeatBlink: Warning beat=%d Audio=%.3f Landing=%s GroundFXZ=%.1f"),
				FirstImpactBeat, Now, *LockedBlinkLocation.ToString(), LockedBlinkGroundZ + 2.f);
		}
		if (bBlinkRequested && bBlinkWarningShown && !bBlinkExecuted && Now >= ScheduledFirstImpactAudioSeconds)
		{
			FVector CheckedLocation = LockedBlinkLocation;
			if (!GetWorld()->FindTeleportSpot(GetOwner(), CheckedLocation, LockedBlinkRotation) ||
				FVector::Dist2D(CheckedLocation, LockedBlinkLocation) > 75.0f)
			{
				UE_LOG(LogTemp, Warning, TEXT("BeatBlink: Landing became blocked; cancelling attack"));
				CancelCombo();
				return;
			}
			const FVector From = GetOwner()->GetActorLocation();
			if (RootMotionMesh.IsValid())
			{
				AActor* Ghost = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
				if (Ghost)
				{
					UPoseableMeshComponent* GhostMesh = NewObject<UPoseableMeshComponent>(Ghost);
					Ghost->AddInstanceComponent(GhostMesh);
					GhostMesh->SetSkinnedAssetAndUpdate(RootMotionMesh->GetSkeletalMeshAsset());
					GhostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					GhostMesh->RegisterComponent();
					GhostMesh->CopyPoseFromSkeletalComponent(RootMotionMesh.Get());
					for (int32 MaterialIndex = 0; MaterialIndex < RootMotionMesh->GetNumMaterials(); ++MaterialIndex)
						GhostMesh->SetMaterial(MaterialIndex, RootMotionMesh->GetMaterial(MaterialIndex));
					GhostMesh->SetWorldTransform(RootMotionMesh->GetComponentTransform());
					Ghost->SetLifeSpan(0.28f);
				}
			}
			if (!GetOwner()->TeleportTo(CheckedLocation, LockedBlinkRotation, false, false))
			{
				CancelCombo();
				return;
			}
			bBlinkExecuted = true;
			if (bStrongBeatThisAttack) TriggerBeatOverlay(true, Now);
			AlignBossToGround(GetOwner(), AttackTarget.Get());
			OnBlinkSucceeded.Broadcast(CheckedLocation, ScheduledFirstImpactAudioSeconds);
			if (UNiagaraComponent* Effect = OriginWarningFX.Get()) Effect->Deactivate();
			if (UNiagaraComponent* Effect = LandingWarningFX.Get()) Effect->Deactivate();
			const UCapsuleComponent* BurstCapsule = Cast<UCapsuleComponent>(GetOwner()->GetRootComponent());
			const float BurstHalfHeight = BurstCapsule ? BurstCapsule->GetScaledCapsuleHalfHeight() : 110.f;
			if (BlinkSettings.OriginEffect)
				SpawnBriefFire(GetWorld(), BlinkSettings.OriginEffect,
					From - FVector(0, 0, BurstHalfHeight - 2.f), 0.35f);
			if (BlinkSettings.LandingEffect)
				SpawnBriefFire(GetWorld(), BlinkSettings.LandingEffect,
					GetOwner()->GetActorLocation() - FVector(0, 0, BurstHalfHeight - 2.f), 0.35f,
					bStrongBeatThisAttack ? 1.45f : 1.f);
			// Discard pre-impact root translation; later strikes keep their normal Motion Warping.
			LastRootMotionPosition = GetImpactTime(0);
			ActiveWarpWindowIndex = INDEX_NONE;
			const UCapsuleComponent* LandedCapsule = Cast<UCapsuleComponent>(GetOwner()->GetRootComponent());
			const float FootClearance = LandedCapsule
				? GetOwner()->GetActorLocation().Z - LandedCapsule->GetScaledCapsuleHalfHeight() - LockedBlinkGroundZ
				: 0.f;
			UE_LOG(LogTemp, Display, TEXT("BeatBlink: Teleported at first-impact beat=%d ErrorMs=%.1f From=%s To=%s FootClearance=%.1f"),
				FirstImpactBeat, (Now - ScheduledFirstImpactAudioSeconds) * 1000.0,
				*From.ToString(), *GetOwner()->GetActorLocation().ToString(), FootClearance);
		}
	}
	if (bQueued && Now >= ScheduledStartAudioSeconds)
	{
		bQueued = false;
		bPlaying = StartPlayback(Now);
		if (!bPlaying) ClearBlink();
	}
	if (!bPlaying)
	{
		return;
	}

	const double AnimationPosition = AttackData->GetAnimationTimeAtElapsed(
		AttackData->GetElapsedAtAnimationTime(AttackData->ClipStartSeconds) +
		static_cast<float>(Now - ScheduledStartAudioSeconds));
	const double ClipStartElapsed = AttackData->GetElapsedAtAnimationTime(AttackData->ClipStartSeconds);
	while (NextBladeSoundIndex < GetImpactCount())
	{
		const double MarkerElapsed = AttackData->GetElapsedAtAnimationTime(GetImpactTime(NextBladeSoundIndex)) - ClipStartElapsed;
		const double MarkerAudioTime = ScheduledStartAudioSeconds + MarkerElapsed;
		if (Now < MarkerAudioTime - BladeWhooshLeadSeconds) break;
		const double Offset = AttackData->ImpactMarkers.IsEmpty() ? 0.0 : AttackData->ImpactMarkers[NextBladeSoundIndex].BeatOffset;
		const double TargetBeat = FirstImpactBeat + Offset;
		const int32 NearestBeat = FMath::RoundToInt(TargetBeat);
		const bool bWholeBeat = FMath::Abs(TargetBeat - NearestBeat) < .05;
		const int32 BeatInBar = NearestBeat % FMath::Max(1, Clock->GetBeatsPerBar());
		const bool bStrongAccent = bWholeBeat && BeatInBar == 0;
		const float AccentVolume = bStrongAccent ? StrongBladeVolume
			: WeakBladeVolume * (bWholeBeat && BeatInBar == 2 ? 1.f : .82f);
		USoundBase* BladeSound = bStrongAccent ? StrongBeatSound.Get() : WeakBladeSound.Get();
		if (BladeSound && AccentVolume > 0.f)
		{
			Clock->TriggerImpactDuck(bStrongAccent ? StrongBeatMusicGain : WeakBeatMusicGain, .15f, 6.f);
			const float Pitch = bStrongAccent ? .98f : 1.05f;
			UAudioComponent* Voice = UGameplayStatics::SpawnSound2D(this, BladeSound,
				AccentVolume * BladeSfxGain, Pitch, BladeSoundStartTimeSeconds);
			UE_LOG(LogTemp, Display, TEXT("BossBladeVoice: impact=%d component=%s playing=%d duration=%.3f gain=%.2f pitch=%.2f sourceStart=%.3f leadMs=%.1f"),
				NextBladeSoundIndex + 1, *GetNameSafe(Voice), Voice && Voice->IsPlaying() ? 1 : 0,
				BladeSound->GetDuration(), AccentVolume * BladeSfxGain, Pitch, BladeSoundStartTimeSeconds,
				(MarkerAudioTime - Now) * 1000.0);
		}
		++NextBladeSoundIndex;
	}
	TArray<USkeletalMeshComponent*> Meshes;
	GetOwner()->GetComponents<USkeletalMeshComponent>(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (!IsValid(Mesh)) continue;
		if (Mesh->GetAnimationMode() == EAnimationMode::AnimationBlueprint)
		{
			UAnimInstance* Instance = Mesh->GetAnimInstance();
			if (!Instance || !Instance->Montage_IsPlaying(AttackData->AttackMontage)) continue;
			Instance->Montage_SetPlayRate(AttackData->AttackMontage,
				AttackData->GetRateAtAnimationTime(static_cast<float>(AnimationPosition)));
			if (FMath::Abs(Instance->Montage_GetPosition(AttackData->AttackMontage) - AnimationPosition) > 0.02)
				Instance->Montage_SetPosition(AttackData->AttackMontage, static_cast<float>(AnimationPosition));
		}
		else if (UAnimSingleNodeInstance* Instance = Mesh->GetSingleNodeInstance())
		{
			if (Instance->GetCurrentAsset() != AttackData->AttackMontage) continue;
			Instance->SetPlayRate(AttackData->GetRateAtAnimationTime(static_cast<float>(AnimationPosition)));
			if (FMath::Abs(Instance->GetCurrentTime() - AnimationPosition) > 0.02)
				Instance->SetPosition(static_cast<float>(AnimationPosition), false);
		}
	}
	const float CurrentRootPosition = FMath::Clamp(static_cast<float>(AnimationPosition), 0.0f,
		AttackData->GetClipEndSeconds());
	if (RootMotionMesh.IsValid() && (!bBlinkRequested || bBlinkExecuted) && CurrentRootPosition > LastRootMotionPosition)
	{
		UpdateWarpTarget(LastRootMotionPosition);
		const FTransform LocalDelta = AttackData->AttackMontage->ExtractRootMotionFromTrackRange(
			LastRootMotionPosition, CurrentRootPosition, FAnimExtractContext(CurrentRootPosition, true));
		const FTransform FinalLocalDelta = IsValid(WarpComponent)
			? WarpComponent->WarpRootMotion(LocalDelta, DeltaTime, AttackData->AttackMontage,
				LastRootMotionPosition, CurrentRootPosition, AttackData->GetRateAtAnimationTime(LastRootMotionPosition))
			: LocalDelta;
		FTransform WorldDelta = RootMotionMesh->ConvertLocalRootMotionToWorld(FinalLocalDelta);
		// Grounded attacks use horizontal root translation; the Actor has no
		// CharacterMovement gravity to undo vertical motion from the montage.
		FVector Translation = WorldDelta.GetTranslation();
		Translation.Z = 0.f;
		WorldDelta.SetTranslation(Translation);
		GetOwner()->AddActorWorldTransform(WorldDelta, true);
		AlignBossToGround(GetOwner(), AttackTarget.Get());
		LastRootMotionPosition = CurrentRootPosition;
	}
	while (NextImpactIndex < GetImpactCount() && AnimationPosition >= GetImpactTime(NextImpactIndex))
	{
		const double Offset = AttackData->ImpactMarkers.IsEmpty() ? 0.0 : AttackData->ImpactMarkers[NextImpactIndex].BeatOffset;
		const double TargetBeat = FirstImpactBeat + Offset;
		const float BeatError = static_cast<float>(Now - Clock->GetBeatTimeSeconds(TargetBeat));
		const int32 NearestBeat = FMath::RoundToInt(TargetBeat);
		const bool bWholeBeat = FMath::Abs(TargetBeat - NearestBeat) < .05;
		const int32 BeatInBar = NearestBeat % FMath::Max(1, Clock->GetBeatsPerBar());
		const bool bStrongAccent = bWholeBeat && BeatInBar == 0;
		TriggerBeatOverlay(bStrongAccent, Now);
		UE_LOG(LogTemp, Display, TEXT("BossBeatOverlay: Impact=%d BeatInBar=%d Strong=%d ErrorMs=%.1f"),
			NextImpactIndex + 1, BeatInBar + 1, bStrongAccent ? 1 : 0, BeatError * 1000.f);
		UE_LOG(LogTemp, Display, TEXT("BeatSyncedCombo: Impact=%d TargetBeat=%.2f ErrorMs=%.1f"),
			NextImpactIndex + 1, TargetBeat, BeatError * 1000.0f);
		if (AttackTarget.IsValid())
		{
			UE_LOG(LogTemp, Display, TEXT("BeatSyncedCombo: Impact=%d playerDistance=%.0f"),
				NextImpactIndex + 1, FVector::Dist2D(GetOwner()->GetActorLocation(), AttackTarget->GetActorLocation()));
		}
		OnImpact.Broadcast(NextImpactIndex, TargetBeat, BeatError);
		++NextImpactIndex;
	}
	if (AnimationPosition >= AttackData->GetClipEndSeconds())
	{
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (!IsValid(Mesh)) continue;
			if (Mesh->GetAnimationMode() == EAnimationMode::AnimationBlueprint && Mesh->GetAnimInstance())
			{
				Mesh->GetAnimInstance()->Montage_Stop(0.15f, AttackData->AttackMontage);
				Mesh->GetAnimInstance()->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
			}
			else if (IsValid(Mesh->GetSingleNodeInstance()) &&
				Mesh->GetSingleNodeInstance()->GetCurrentAsset() == AttackData->AttackMontage)
			{
				Mesh->Stop();
			}
		}
		bPlaying = false;
		bLastAttackCompleted = true;
		RootMotionMesh.Reset();
		LocomotionResumeWorldSeconds = GetWorld()->GetTimeSeconds() + 0.2;
		ClearWarping();
		AttackTarget.Reset();
		ClearBlink();
		OnComboFinished.Broadcast();
	}
}
