// 显示攻击识别提示、目标路径和应对信息
#include "Combat/Feedback/AttackReadabilityComponent.h"
#include "Combat/Boss/BeatSyncedComboComponent.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Combat/Feedback/TransientCombatFX.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Sound/SoundBase.h"

UAttackReadabilityComponent::UAttackReadabilityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UAttackReadabilityComponent::BeginPlay()
{
	Super::BeginPlay();
	WarningPulseSound =
	    LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/SW_BossDrumKick.SW_BossDrumKick"));
	CounterCueSound = LoadObject<USoundBase>(
	    nullptr, TEXT("/Game/Audio/SFX/HitRush/SW_HitRush_Counter.SW_HitRush_Counter"));
	Symbol = NewObject<UStaticMeshComponent>(GetOwner());
	Symbol->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
	Symbol->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Symbol->SetCastShadow(false);
	Symbol->SetMobility(EComponentMobility::Movable);
	Symbol->RegisterComponent();
	if (auto* Mat = LoadObject<UMaterialInterface>(
	        nullptr, TEXT("/Game/Combat/Readability/M_AttackRecognition.M_AttackRecognition")))
	{
		SymbolMaterial = UMaterialInstanceDynamic::Create(Mat, this);
		Symbol->SetMaterial(0, SymbolMaterial);
	}
	GroundWarning = NewObject<UDecalComponent>(GetOwner());
	GroundWarning->RegisterComponent();
	DodgePathWarning = NewObject<UDecalComponent>(GetOwner());
	DodgePathWarning->RegisterComponent();
	if (auto* Mat = LoadObject<UMaterialInterface>(
	        nullptr, TEXT("/Game/Combat/Readability/M_GroundBurstWarning.M_GroundBurstWarning")))
	{
		GroundMaterial = UMaterialInstanceDynamic::Create(Mat, this);
		GroundWarning->SetDecalMaterial(GroundMaterial);
		DodgePathMaterial = UMaterialInstanceDynamic::Create(Mat, this);
		DodgePathWarning->SetDecalMaterial(DodgePathMaterial);
	}
	Symbol->SetVisibility(false);
	GroundWarning->SetVisibility(false);
	DodgePathWarning->SetVisibility(false);
	TInlineComponentArray<UMeshComponent*> Meshes(GetOwner());
	for (auto* Mesh : Meshes)
		if (Mesh->GetName().Contains(TEXT("Sword"), ESearchCase::IgnoreCase) ||
		    Mesh->GetName().Contains(TEXT("Weapon"), ESearchCase::IgnoreCase) ||
		    Mesh->ComponentHasTag(TEXT("DamageBlade")))
		{
			Blade = Mesh;
			OriginalBladeOverlay = Mesh->GetOverlayMaterial();
			break;
		}
	if (auto* Mat = LoadObject<UMaterialInterface>(
	        nullptr, TEXT("/Game/Combat/Readability/M_BladeRecognition.M_BladeRecognition")))
	{
		BladeMaterial = UMaterialInstanceDynamic::Create(Mat, this);
		DodgeGhostMaterial = UMaterialInstanceDynamic::Create(Mat, this);
	}
	if (Blade.IsValid())
	{
		if (USkeletalMeshComponent* SkeletalBlade = Cast<USkeletalMeshComponent>(Blade.Get()))
			if (SkeletalBlade->GetBoneIndex(TEXT("hand_joint")) != INDEX_NONE &&
			    SkeletalBlade->GetBoneIndex(TEXT("joint10")) != INDEX_NONE)
				if (UParticleSystem* Trail = LoadObject<UParticleSystem>(
				        nullptr,
				        TEXT(
				            "/Game/TrailMaster/Particle/Trail/Trail_021-025/Trail_021/P_Trail_021.P_Trail_021")))
				{
					DodgeBladeTrail =
					    NewObject<UParticleSystemComponent>(GetOwner(), TEXT("DodgeOnlyBladeTrail"));
					DodgeBladeTrail->SetupAttachment(SkeletalBlade);
					DodgeBladeTrail->SetTemplate(Trail);
					DodgeBladeTrail->SetAutoActivate(false);
					DodgeBladeTrail->SetCastShadow(false);
					DodgeBladeTrail->RegisterComponent();
					DodgeBladeTrail->DeactivateSystem();
				}
		// 只放大提示轮廓，武器和命中检测范围保持不变
		if (DodgeGhostMaterial)
			if (UStaticMeshComponent* StaticBlade = Cast<UStaticMeshComponent>(Blade.Get()))
			{
				DodgeBladeGhost =
				    NewObject<UStaticMeshComponent>(GetOwner(), TEXT("DodgeBladePressureGhost"));
				DodgeBladeGhost->SetupAttachment(StaticBlade);
				DodgeBladeGhost->SetStaticMesh(StaticBlade->GetStaticMesh());
				DodgeBladeGhost->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				DodgeBladeGhost->SetGenerateOverlapEvents(false);
				DodgeBladeGhost->SetCastShadow(false);
				for (int32 Slot = 0; Slot < DodgeBladeGhost->GetNumMaterials(); ++Slot)
					DodgeBladeGhost->SetMaterial(Slot, DodgeGhostMaterial);
				DodgeBladeGhost->RegisterComponent();
				DodgeBladeGhost->SetVisibility(false);
			}
		BladeCueLight = NewObject<UPointLightComponent>(GetOwner());
		BladeCueLight->SetCastShadows(false);
		BladeCueLight->SetIntensity(0.f);
		BladeCueLight->SetAttenuationRadius(150.f);
		BladeCueLight->RegisterComponent();
	}
}

void UAttackReadabilityComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	const auto* Combo = GetOwner()->FindComponentByClass<UBeatSyncedComboComponent>();
	if (!Combo)
		return;
	FAttackRecognitionInfo Info;
	FVector Position;
	float Progress = 0.f;
	double Key = 0.;
	bool bCue = Combo->GetRecognitionCue(Info, Position, Progress, Key);
	if (!bCue)
	{
		TArray<FBossOpening> Openings;
		Combo->GetOpenings(Openings);
		const auto* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
		const double Now = Clock ? Clock->GetAudioNowSeconds() : 0.;
		for (const FBossOpening& Opening : Openings)
		{
			if (Opening.Kind != EBossOpeningKind::Sidestep || !Opening.Contains(Now))
				continue;
			Info.Shape = EAttackRecognitionShape::ShortGlint;
			Info.Color = FLinearColor(.08f, .85f, 1.f);
			Info.BladeCueScale = .9f;
			Info.BladeCueIntensity = .7f;
			Position = Blade.IsValid() ? Blade->Bounds.Origin + FVector(0, 0, 14)
			                           : GetOwner()->GetActorLocation() + FVector(0, 0, 80);
			Progress = FMath::Clamp(
			    float((Now - Opening.StartAudio) / (Opening.EndAudio - Opening.StartAudio)), 0.f, 1.f);
			Key = Opening.StartAudio;
			bCue = true;
			break;
		}
	}
	const float Transition = Combo->GetPhaseTransitionProgress();
	if (Transition >= 0.f)
	{
		bCue = false; // 阶段演出期间隐藏攻击提示
	}
	SymbolCueOpacity = FMath::FInterpTo(SymbolCueOpacity, bCue && Info.bShowSymbol ? 1.f : 0.f, DeltaTime,
	                                    bCue ? 18.f : 22.f);
	Symbol->SetVisibility(SymbolCueOpacity > .015f && SymbolMaterial);
	if (SymbolMaterial)
		SymbolMaterial->SetScalarParameterValue(TEXT("RenderOpacity"), SymbolCueOpacity);
	BladeCueOpacity =
	    FMath::FInterpTo(BladeCueOpacity, bCue && Transition < 0.f ? 1.f : 0.f, DeltaTime, 24.f);
	if (Blade.IsValid() && BladeMaterial)
	{
		if (BladeCueOpacity > .01f)
		{
			Blade->SetOverlayMaterial(BladeMaterial);
			const float Pulse =
			    bCue ? Info.bDodgeOnlyCue ? .88f + .58f * FMath::SmoothStep(.15f, 1.f, Progress) +
				                                .08f * FMath::Sin(Progress * UE_TWO_PI * 2.f)
				       : Info.bCounterCue ? .90f + .10f * FMath::Sin(Progress * UE_TWO_PI * 2.f)
				                          : .70f + .08f * FMath::Sin(Progress * UE_TWO_PI * 2.f)
				     : 1.f;
			BladeMaterial->SetScalarParameterValue(
			    TEXT("CueStrength"), FMath::Min(BladeCueOpacity * Info.BladeCueIntensity * Pulse,
				                                Info.bDodgeOnlyCue ? 2.15f : 1.2f));
			if (bCue && Transition < 0.f)
				BladeMaterial->SetVectorParameterValue(TEXT("CueColor"), Info.Color);
		}
		else if (Blade->GetOverlayMaterial() == BladeMaterial)
			Blade->SetOverlayMaterial(OriginalBladeOverlay);
	}
	if (DodgeBladeGhost)
	{
		DodgeBladeGhost->SetVisibility(bCue && Info.bDodgeOnlyCue && BladeCueOpacity > .04f);
		if (bCue && Info.bDodgeOnlyCue)
		{
			DodgeBladeGhost->SetRelativeScale3D(
			    FVector(FMath::Lerp(1.08f, 1.32f, Progress))); // 随起手进度放大刀刃轮廓
			DodgeGhostMaterial->SetVectorParameterValue(TEXT("CueColor"), Info.Color);
			DodgeGhostMaterial->SetScalarParameterValue(
			    TEXT("CueStrength"), FMath::Min(.75f * BladeCueOpacity * Info.BladeCueIntensity, .9f));
		}
	}
	if (DodgeBladeTrail)
	{
		if (bCue && Info.bDodgeOnlyCue)
			DodgeTrailImpactAudioSeconds = Key;
		const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
		const double UntilImpact =
		    Clock ? DodgeTrailImpactAudioSeconds - Clock->GetAudioNowSeconds() : -1000.;
		// 刀光从前摇开始，接近命中时加宽，并始终跟随实际武器
		const bool bTrailWindow =
		    Combo->IsComboQueuedOrPlaying() && Transition < 0.f && UntilImpact <= .95 && UntilImpact >= -.2;
		const float TrailPressure = 1.f - FMath::Clamp(float((UntilImpact + .04) / .99), 0.f, 1.f);
		const float TrailWidth =
		    FMath::Lerp(2.2f, 5.6f, FMath::SmoothStep(0.f, 1.f, TrailPressure)); // 接近命中时加宽刀光
		if (bTrailWindow && !bDodgeTrailPlaying)
		{
			DodgeBladeTrail->BeginTrails(TEXT("hand_joint"), TEXT("joint10"), ETrailWidthMode_FromCentre,
			                             TrailWidth);
			bDodgeTrailPlaying = true;
		}
		else if (bTrailWindow && bDodgeTrailPlaying)
		{
			DodgeBladeTrail->SetTrailSourceData(TEXT("hand_joint"), TEXT("joint10"),
			                                    ETrailWidthMode_FromCentre, TrailWidth);
		}
		else if (!bTrailWindow && bDodgeTrailPlaying)
		{
			DodgeBladeTrail->EndTrails();
			bDodgeTrailPlaying = false;
		}
	}
	if (BladeCueLight && Blade.IsValid())
	{
		BladeCueLight->SetWorldLocation(Blade->Bounds.Origin);
		BladeCueLight->SetLightColor(bCue && Info.bDodgeOnlyCue ? FLinearColor(.42f, .003f, .001f)
		                             : bCue                     ? Info.Color
		                                                        : FLinearColor::White);
		BladeCueLight->SetAttenuationRadius(Info.bDodgeOnlyCue
		                                        ? 320.f + 280.f * FMath::SmoothStep(.2f, 1.f, Progress)
												: 110.f + 90.f * Info.BladeCueScale);
		BladeCueLight->SetIntensity(1700.f * BladeCueOpacity * Info.BladeCueIntensity *
		                            (Info.bDodgeOnlyCue ? 1.1f + 1.5f * FMath::SmoothStep(.18f, 1.f, Progress)
		                             : Info.bCounterCue ? 1.65f
		                                                : 1.f));
	}
	if (bCue)
	{
		if (auto* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			Symbol->SetWorldLocation(Position);
			Symbol->SetWorldRotation(
			    FRotationMatrix::MakeFromZ(Camera->GetCameraLocation() - Position).Rotator());
			const float ShapeSize = Info.Shape == EAttackRecognitionShape::Cross          ? 1.4f
			                        : Info.Shape == EAttackRecognitionShape::WideArc      ? 1.5f
			                        : Info.Shape == EAttackRecognitionShape::SustainedArc ? 1.25f
			                                                                              : 1.05f;
			Symbol->SetWorldScale3D(
			    FVector(Transition >= 0.f
			                ? 1.1f + .45f * Transition
							: ShapeSize * Info.BladeCueScale *
							      (Info.bDodgeOnlyCue ? .9f + .35f * Progress : 1.12f - .12f * Progress)));
		}
		if (SymbolMaterial)
		{
			SymbolMaterial->SetVectorParameterValue(TEXT("CueColor"), Info.Color);
			SymbolMaterial->SetScalarParameterValue(TEXT("Shape"), static_cast<float>(Info.Shape));
			SymbolMaterial->SetScalarParameterValue(TEXT("Progress"), Progress);
			SymbolMaterial->SetScalarParameterValue(TEXT("UseIcon"), Info.Symbol ? 1.f : 0.f);
			if (Info.Symbol)
				SymbolMaterial->SetTextureParameterValue(TEXT("Icon"), Info.Symbol);
		}
		if (Key != LastCue)
		{
			LastCue = Key;
			// 只播放攻击资源配置的提示音，避免额外鼓点干扰节拍
			USoundBase* Sound = Info.bCounterCue ? nullptr : Info.CueSound.Get();
			if (Sound)
				UGameplayStatics::PlaySoundAtLocation(this, Sound, Position, Info.bCounterCue ? .8f : .65f);
		}
	}
	FVector PathStart, PathDirection;
	float PathLength = 0.f;
	const bool bDodgePath =
	    bCue && Info.bDodgeOnlyCue && Combo->GetDodgeOnlyPath(Key, PathStart, PathDirection, PathLength);
	DodgePathOpacity =
	    FMath::FInterpTo(DodgePathOpacity, bDodgePath ? 1.f : 0.f, DeltaTime, bDodgePath ? 12.f : 18.f);
	DodgePathWarning->SetVisibility(DodgePathMaterial && DodgePathOpacity > .02f);
	if (bDodgePath)
	{
		DodgePathWarning->SetWorldLocation(PathStart + PathDirection * (PathLength * .5f) +
		                                   FVector(0, 0, 45));
		DodgePathWarning->SetWorldRotation(FRotator(-90.f, PathDirection.Rotation().Yaw, 0.f));
		// 提示带沿已确定的出剑方向摆放，表示剑刃经过的区域
		DodgePathWarning->DecalSize = FVector(110.f, 150.f, PathLength * .5f); // wider danger corridor
	}
	if (DodgePathMaterial)
	{
		const float Warning = FMath::SmoothStep(0.f, 1.f, Progress);
		DodgePathMaterial->SetScalarParameterValue(TEXT("Progress"), Progress);
		DodgePathMaterial->SetScalarParameterValue(TEXT("Burst"), 0.f);
		DodgePathMaterial->SetScalarParameterValue(TEXT("RenderOpacity"),
		                                           DodgePathOpacity * (.32f + .48f * Warning));
		DodgePathMaterial->SetVectorParameterValue(TEXT("CueColor"), FLinearColor(.55f, .008f, .045f));
		DodgePathMaterial->SetVectorParameterValue(TEXT("SigilBodyColor"), FLinearColor(.025f, .0f, .004f));
		DodgePathMaterial->SetScalarParameterValue(TEXT("SigilOpacity"), .08f);
		DodgePathMaterial->SetScalarParameterValue(TEXT("SigilEmission"), .08f + .32f * Warning);
	}
	float Radius = 0.f;
	const bool bGround = Combo->GetGroundBurstPreview(Position, Radius, Progress);
	if (bGround)
	{
		const float Beats = Combo->GetGroundBurstRemainingBeats();
		const int32 Count = FMath::CeilToInt(Beats);
		if (Count != LastWarningCount)
		{
			LastWarningCount = Count;
			if (WarningPulseSound)
				UGameplayStatics::PlaySound2D(this, WarningPulseSound, Count <= 1 ? .65f : .4f,
				                              Count <= 1 ? .9f : .75f);
		}
		GroundWarning->SetWorldLocation(Position + FVector(0, 0, 35));
		GroundWarning->SetWorldRotation(FRotator(-90, 0, 0));
		GroundWarning->DecalSize = FVector(120, Radius, Radius);
		BurstRemaining = 0.f;
	}
	else
		LastWarningCount = INDEX_NONE;
	// 只有已结算的范围攻击才显示爆炸，取消攻击只撤掉预警
	if (bGroundWasVisible && !bGround && Combo->HasResolvedGroundBurst())
	{
		BurstRemaining = .45f;
		BurstSparks.RemoveAll(
		    [](const auto& Spark)
		    {
			    return !Spark.IsValid();
		    });
		if (auto* Sparks =
		        LoadObject<UParticleSystem>(nullptr, TEXT("/Game/SwordAnim/demo/FX/P_Spark.P_Spark")))
		{
			const FVector Center = GroundWarning->GetComponentLocation();
			const float SparkRadius = GroundWarning->DecalSize.Y * .72f;
			for (int32 I = 0; I < 8; ++I)
			{
				const float Angle = I * UE_TWO_PI / 8.f;
				const FVector Offset(FMath::Cos(Angle) * SparkRadius, FMath::Sin(Angle) * SparkRadius, -20.f);
				auto* Spark = UGameplayStatics::SpawnEmitterAtLocation(
				    GetWorld(), Sparks, Center + Offset, FRotator::ZeroRotator, FVector(.65f), true);
				RetireCombatFX(Spark, .06f, .35f);
				BurstSparks.Add(Spark);
			}
		}
	}
	bGroundWasVisible = bGround;
	BurstRemaining = FMath::Max(0.f, BurstRemaining - DeltaTime);
	GroundWarning->SetVisibility(GroundMaterial && (bGround || BurstRemaining > 0.f));
	if (GroundMaterial)
	{
		GroundMaterial->SetScalarParameterValue(TEXT("Progress"), bGround ? Progress : 1.f);
		const float Tail = BurstRemaining / .45f;
		GroundMaterial->SetScalarParameterValue(TEXT("Burst"), Tail);
		GroundMaterial->SetScalarParameterValue(TEXT("RenderOpacity"),
		                                        bGround ? 1.f : FMath::SmoothStep(0.f, .55f, Tail));
		const float Remaining = Combo->GetGroundBurstRemainingBeats();
		const float Pulse =
		    Remaining >= 0.f ? FMath::Exp(-(FMath::CeilToFloat(Remaining) - Remaining) * 7.f) : 0.f;
		// 颜色随命中时刻临近加深，边缘亮度随剩余节拍起伏
		GroundMaterial->SetVectorParameterValue(TEXT("CueColor"), FLinearColor(.30f, .018f, .008f));
		GroundMaterial->SetVectorParameterValue(TEXT("SigilBodyColor"), FLinearColor(.012f, .0f, .0f));
		GroundMaterial->SetScalarParameterValue(TEXT("SigilOpacity"), .35f);
		GroundMaterial->SetScalarParameterValue(
		    TEXT("SigilEmission"),
		    bGround ? .06f + .16f * Progress + Pulse * (Remaining < 1.f ? .5f : .18f) : .7f + Tail * 1.1f);
	}
#if !UE_BUILD_SHIPPING
	if (!bBladeCaptureRequested && bCue && Info.Shape == EAttackRecognitionShape::ShortGlint &&
	    Progress > .7f && GetWorld()->GetTimeSeconds() > 3.f &&
	    FParse::Param(FCommandLine::Get(), TEXT("BladeReadabilityCapture")))
	{
		FScreenshotRequest::RequestScreenshot(
		    FPaths::ProjectSavedDir() / TEXT("Screenshots/BladeReadabilityRefined.png"), false, false);
		bBladeCaptureRequested = true;
	}
	if (!bTransitionCaptureRequested && Transition > .3f &&
	    FParse::Param(FCommandLine::Get(), TEXT("PhaseTransitionTest")))
	{
		FScreenshotRequest::RequestScreenshot(
		    FPaths::ProjectSavedDir() / TEXT("Screenshots/PhaseTwoTransition.png"), false, false);
		bTransitionCaptureRequested = true;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("PhaseTwoVisualTest")))
	{
		const bool bCapture = (VisualTestCapture == 0 && bGround && Progress > .15f) ||
		                      (VisualTestCapture == 1 && bGround && Progress > .75f) ||
		                      (VisualTestCapture == 2 && BurstRemaining > .25f);
		if (bCapture)
		{
			FScreenshotRequest::RequestScreenshot(
			    FPaths::ProjectSavedDir() /
			        FString::Printf(TEXT("Screenshots/PhaseTwoWarning_%d.png"), VisualTestCapture),
			    false, false);
			++VisualTestCapture;
		}
	}
#endif
}

void UAttackReadabilityComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Symbol)
		Symbol->DestroyComponent();
	if (DodgeBladeTrail)
	{
		if (bDodgeTrailPlaying)
			DodgeBladeTrail->EndTrails();
		DodgeBladeTrail->DestroyComponent();
	}
	if (DodgeBladeGhost)
		DodgeBladeGhost->DestroyComponent();
	if (GroundWarning)
		GroundWarning->DestroyComponent();
	if (DodgePathWarning)
		DodgePathWarning->DestroyComponent();
	if (Blade.IsValid() && Blade->GetOverlayMaterial() == BladeMaterial)
		Blade->SetOverlayMaterial(OriginalBladeOverlay);
	for (const auto& Spark : BurstSparks)
		if (Spark.IsValid())
			Spark->DestroyComponent();
	Super::EndPlay(Reason);
}

bool UBeatSyncedComboComponent::GetDodgeOnlyPath(double ImpactAudioSeconds, FVector& Start,
                                                 FVector& Direction, float& Length) const
{
	if (!AttackTarget.IsValid())
		return false;
	TArray<FUpcomingImpactCue> Cues;
	GetUpcomingImpactCues(Cues, 8);
	for (const FUpcomingImpactCue& Cue : Cues)
	{
		if (FMath::Abs(Cue.ImpactAudioSeconds - ImpactAudioSeconds) > .001 ||
		    Cue.RequiredResponse != ERhythmResponseType::Dodge ||
		    GetImpactMarker(Cue.ImpactIndex).DamageShape != EBossAttackDamageShape::Blade)
			continue;
		Start = GetOwner()->GetActorLocation();
		const FVector Target = bStrikeCommitted && ActiveWarpWindowIndex == Cue.ImpactIndex
		                           ? CommittedPlayerLocation
		                           : AttackTarget->GetActorLocation();
		Direction = (Target - Start).GetSafeNormal2D();
		if (Direction.IsNearlyZero())
			Direction = GetOwner()->GetActorForwardVector().GetSafeNormal2D();
		Length = FMath::Clamp(FVector::Dist2D(Target, Start) + 45.f, 180.f, 320.f);
		return true;
	}
	return false;
}

bool UBeatSyncedComboComponent::GetRecognitionCue(FAttackRecognitionInfo& Info, FVector& Location,
                                                  float& Progress, double& Key) const
{
	if ((!bQueued && !bPlaying) || !AttackData)
		return false;
	const auto* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	if (!Clock || !Clock->IsRunning())
		return false;
	TArray<FUpcomingImpactCue> Cues;
	GetUpcomingImpactCues(Cues, 8);
	for (const FUpcomingImpactCue& Cue : Cues)
	{
		const auto& Marker = GetImpactMarker(Cue.ImpactIndex);
		Info = Marker.Recognition;
		// 单次命中未配置的声音和图形沿用整招默认值，强度与默认倍率相乘
		const auto& Default = AttackData->DefaultRecognition;
		if (!Info.CueSound)
			Info.CueSound = Default.CueSound;
		if (!Info.Symbol)
			Info.Symbol = Default.Symbol;
		Info.BladeCueScale *= Default.BladeCueScale;
		Info.BladeCueIntensity *= Default.BladeCueIntensity;
		if (Info.Shape == EAttackRecognitionShape::Auto)
			Info.Shape = Default.Shape;
		if (!Info.bOverrideColor && Default.bOverrideColor)
		{
			Info.Color = Default.Color;
			Info.bOverrideColor = true;
		}
		const double Until = Cue.ImpactAudioSeconds - Cue.IdealDodgeLeadSeconds - Clock->GetAudioNowSeconds();
		// 闪避、反击和持续格挡需要更长识别时间，提前量至少满足对应下限
		const bool bNeedsLongRead = Cue.RequiredResponse == ERhythmResponseType::Dodge ||
		                            Cue.RequiredResponse == ERhythmResponseType::HoldBlock ||
		                            Cue.RequiredResponse == ERhythmResponseType::Counter;
		const float MinimumLead = bNeedsLongRead                                   ? 1.5f
		                          : Info.Shape == EAttackRecognitionShape::WideArc ? 1.25f
		                                                                           : 1.f;
		const double Lead = FMath::Max(MinimumLead, Info.LeadBeats) * Clock->GetSecondsPerBeat();
		if (Until < -.08 || Until > Lead)
			continue;
		if (Info.Shape == EAttackRecognitionShape::Auto)
		{
			Info.Shape = Cue.RequiredResponse == ERhythmResponseType::Dodge ? EAttackRecognitionShape::WideArc
			             : Cue.RequiredResponse == ERhythmResponseType::Counter
			                 ? EAttackRecognitionShape::Cross
			             : Cue.RequiredResponse == ERhythmResponseType::HoldBlock
			                 ? EAttackRecognitionShape::SustainedArc
			                 : EAttackRecognitionShape::ShortGlint;
		}
		if (Marker.DamageShape == EBossAttackDamageShape::GroundBurst)
			Info.Shape = EAttackRecognitionShape::GroundWarning;
		else if (Cue.RequiredResponse == ERhythmResponseType::Counter)
		{
			Info.bCounterCue = true;
			Info.Shape = EAttackRecognitionShape::Cross;
			Info.Color = FLinearColor(1.f, .7f, .08f);
			Info.bOverrideColor = true;
			Info.BladeCueScale *= 1.2f;
			Info.BladeCueIntensity *= 1.55f;
			Info.Symbol = nullptr;
			Info.bShowSymbol = false; // 反击图标由界面绘制，避免重复显示
		}
		else if (Cue.RequiredResponse == ERhythmResponseType::Dodge)
		{
			// 红色刀刃表示必须闪避，与可格挡的普通挥刀区分
			Info.bDodgeOnlyCue = true;
			Info.Shape = EAttackRecognitionShape::WideArc;
			Info.Color = FLinearColor(.30f, .002f, .001f);
			Info.bOverrideColor = true;
			Info.BladeCueScale *= 1.5f;
			Info.BladeCueIntensity *= 1.15f;
		}
		else if (Cue.RequiredResponse == ERhythmResponseType::DodgeOrBlock &&
		         Marker.Recognition.Shape == EAttackRecognitionShape::Auto)
		{
			// 两种防御都有效时使用中性提示，不指定必须按哪一种操作
			Info.Shape = EAttackRecognitionShape::ShortGlint;
			Info.Symbol = nullptr;
			Info.bOverrideColor = true;
			Info.Color = FLinearColor(.8f, .9f, 1.f);
		}
		if (!Info.bOverrideColor)
			Info.Color = Info.Shape == EAttackRecognitionShape::WideArc        ? FLinearColor(.08f, .8f, 1.f)
			             : Info.Shape == EAttackRecognitionShape::Cross        ? FLinearColor(1.f, .65f, .08f)
			             : Info.Shape == EAttackRecognitionShape::SustainedArc ? FLinearColor(.32f, .52f, 1.f)
			             : Info.Shape == EAttackRecognitionShape::GroundWarning
			                 ? FLinearColor(1.f, .15f, .04f)
			                 : FLinearColor(.8f, .9f, 1.f);
		if (Marker.DamageShape == EBossAttackDamageShape::Blade)
		{
			// 刀击用武器发光提示，不再额外显示悬浮图形
			Info.bShowSymbol = false;
			// 根据伤害增强刀刃亮度和大小，让重击更容易辨认
			const float Pressure = Marker.Damage < 18.f ? .82f : Marker.Damage < 28.f ? 1.08f : 1.4f;
			Info.BladeCueScale *= Pressure;
			Info.BladeCueIntensity *= Pressure;
		}
		Location = GetOwner()->GetActorLocation() + FVector(0, 0, 80);
		if (Marker.DamageShape != EBossAttackDamageShape::GroundBurst)
			if (UMeshComponent* Blade = FindBladeMesh())
				Location = Blade->Bounds.Origin + FVector(0, 0, 14);
		Progress = 1.f - FMath::Clamp(static_cast<float>(Until / Lead), 0.f, 1.f);
		Key = Cue.ImpactAudioSeconds;
		return true;
	}
	return false;
}
