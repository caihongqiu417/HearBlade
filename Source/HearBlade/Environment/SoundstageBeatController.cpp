// 根据节拍更新已导入的舞台物体，不生成或修改网格几何
#include "Environment/SoundstageBeatController.h"
#include "Combat/Debug/RhythmCaptureComponent.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "EngineUtils.h"
#include "Components/SceneComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "UObject/UObjectIterator.h"

ASoundstageBeatController::ASoundstageBeatController()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("StageRoot"));
}

void ASoundstageBeatController::BeginPlay()
{
	Super::BeginPlay();
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("PhaseTwoVideo")))
	{
		auto* Capture = NewObject<URhythmCaptureComponent>(this);
		Capture->RegisterComponent();
	}
#endif
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("HB_RhythmSky")))
			if (UMeshComponent* Mesh = It->FindComponentByClass<UMeshComponent>())
				if (auto* MID = Mesh->CreateDynamicMaterialInstance(0))
					SkyMaterials.Add(MID);
		if (!It->ActorHasTag(TEXT("HB_HarmonicCourt")))
			continue;
		for (const FName Mode :
		     {FName("BeatCrown"), FName("BeatSpeaker"), FName("BeatHalo"), FName("BeatShard"),
		      FName("BeatPendulum"), FName("BeatGimbal"), FName("BeatDisc")})
		{
			if (!It->ActorHasTag(Mode))
				continue;
			if (It->GetRootComponent())
				It->GetRootComponent()->SetMobility(EComponentMobility::Movable);
			Props.Add({*It, It->GetActorTransform(), Mode});
			break;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("HarmonicCourt: %d imported Blender props follow the music clock"),
	       Props.Num());
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("HarmonicCourtCapture")) ||
	    FParse::Param(FCommandLine::Get(), TEXT("HarmonicCourtLongCapture")))
	{
		const bool bLong = FParse::Param(FCommandLine::Get(), TEXT("HarmonicCourtLongCapture"));
		const TArray<int32> CaptureTimes = bLong ? TArray<int32>{5, 25, 50, 80} : TArray<int32>{5, 12};
		for (int32 Second : CaptureTimes)
		{
			FTimerHandle Handle;
			GetWorldTimerManager().SetTimer(
			    Handle,
			    FTimerDelegate::CreateWeakLambda(
			        this,
			        [this, Second]()
			        {
				        if (Second == 5)
				        {
					        for (const FVector2D Offset :
							     {FVector2D(0, 0), FVector2D(1800, 0), FVector2D(-1800, 0),
							      FVector2D(0, 1800), FVector2D(0, -1800)})
					        {
						        const FVector Point = GetActorLocation() + FVector(Offset.X, Offset.Y, 0);
						        FHitResult Hit;
						        const bool bHit = GetWorld()->LineTraceSingleByChannel(
						            Hit, Point + FVector(0, 0, 100), Point - FVector(0, 0, 200),
						            ECC_Visibility);
						        UE_LOG(LogTemp, Display, TEXT("HarmonicCourtFloor: offset=%s hit=%d z=%.2f"),
								       *Offset.ToString(), bHit, Hit.ImpactPoint.Z);
					        }
				        }
				        FScreenshotRequest::RequestScreenshot(
				            FPaths::ProjectSavedDir() /
				                FString::Printf(TEXT("Screenshots/HarmonicCourt_%02d.png"), Second),
				            false, false);
				        for (TObjectIterator<UNiagaraComponent> It; It; ++It)
					        if (It->GetWorld() == GetWorld() && It->IsRegistered())
						        UE_LOG(
						            LogTemp, Display,
						            TEXT(
						                "CourtParticleAudit: t=%d component=%s effect=%s active=%d visible=%d at=%s"),
						            Second, *It->GetPathName(), *GetPathNameSafe(It->GetAsset()),
						            It->IsActive(), It->IsVisible(), *It->GetComponentLocation().ToString());
				        for (TObjectIterator<UParticleSystemComponent> It; It; ++It)
					        if (It->GetWorld() == GetWorld() && It->IsRegistered())
						        UE_LOG(LogTemp, Display,
								       TEXT("CourtCascadeAudit: t=%d component=%s effect=%s active=%d"),
								       Second, *It->GetPathName(), *GetPathNameSafe(It->Template),
								       It->IsActive());
				        for (const FMovingProp& Prop : Props)
					        if (Prop.Actor.IsValid())
						        UE_LOG(LogTemp, Display, TEXT("HarmonicCourtVerify: %s offset=%s"),
								       *Prop.Mode.ToString(),
								       *(Prop.Actor->GetActorLocation() - Prop.RestTransform.GetLocation())
								            .ToString());
			        }),
			    float(Second), false);
		}
		FTimerHandle ExitHandle;
		GetWorldTimerManager().SetTimer(
		    ExitHandle,
		    []()
		    {
			    FPlatformMisc::RequestExit(false);
		    },
		    bLong ? 85.f : 16.f, false);
	}
#endif
}

void ASoundstageBeatController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const auto* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	const bool bRunning = Clock && Clock->IsRunning();
	const double Beat = bRunning ? Clock->GetCurrentBeat() : 0.0;
	const float Phase = static_cast<float>(Beat - FMath::FloorToDouble(Beat));
	// 直接按当前拍内位置计算起伏，跳播或掉帧后仍跟随音乐
	const float D = FMath::Min(Phase, 1.f - Phase);
	const float Pulse = bRunning ? FMath::Exp(-FMath::Square(D / .16f)) : 0.f;
	const float AscensionAccent = bRunning && Beat >= AscensionAccentBeat
	                                  ? .45f * FMath::Exp(-float(Beat - AscensionAccentBeat) * 3.f)
	                                  : 0.f;
	const float Accent = (bRunning && Clock->GetCurrentBeatInBar() == 0 ? 1.35f : 1.f) + AscensionAccent;
	Envelope = FMath::FInterpTo(Envelope, Pulse * Accent, DeltaSeconds, 24.f);
	const float Amount = FMath::Clamp(MotionIntensity, 0.f, 2.f) * (bReducedMotion ? .2f : 1.f);
	const float SkyBreath =
	    bRunning
	        ? .5f + .5f * FMath::Sin(static_cast<float>(Beat * 2.0 * PI / (Clock->GetBeatsPerBar() * 4.0)))
	        : .5f;
	for (UMaterialInstanceDynamic* MID : SkyMaterials)
		if (MID)
		{
			MID->SetScalarParameterValue(TEXT("BeatLift"),
			                             SkyBreath * SkyPulseStrength * (bReducedMotion ? .2f : 1.f));
			MID->SetScalarParameterValue(TEXT("MusicPhase"), static_cast<float>(Beat * .002));
		}
	for (int32 I = 0; I < Props.Num(); ++I)
	{
		const FMovingProp& Prop = Props[I];
		if (!Prop.Actor.IsValid())
			continue;
		// 每帧从初始变换计算效果，避免逐帧累加导致物体不断偏移
		FTransform Transform = Prop.RestTransform;
		FVector Position = Transform.GetLocation();
		FVector Scale = Transform.GetScale3D();
		if (Prop.Mode == TEXT("BeatCrown"))
		{
			Position.Z += Envelope * CrownBounceCm * Amount;
			Scale.Z *= 1.f + Envelope * .05f * Amount;
		}
		else if (Prop.Mode == TEXT("BeatSpeaker"))
		{
			FVector Inward = GetActorLocation() - Position;
			Inward.Z = 0.f;
			Position += Inward.GetSafeNormal() * (Envelope * 8.f * Amount);
			Scale *= 1.f + Envelope * .045f * Amount;
		}
		else if (Prop.Mode == TEXT("BeatHalo"))
		{
			// 模型轴心位于圆环中心，整体放大时各部分向外展开
			Scale *= 1.f + Envelope * .025f * Amount;
		}
		else if (Prop.Mode == TEXT("BeatPendulum"))
		{
			const float Swing =
			    bRunning ? FMath::Sin(static_cast<float>(Beat * PI)) * PendulumAngleDegrees * Amount : 0.f;
			Transform.SetRotation(Transform.GetRotation() *
			                      FQuat(FVector::RightVector, FMath::DegreesToRadians(Swing)));
		}
		else if (Prop.Mode == TEXT("BeatGimbal"))
		{
			const float Swing =
			    bRunning ? FMath::Sin(static_cast<float>(Beat * PI * .5)) * .25f * Amount : 0.f;
			Transform.SetRotation(Transform.GetRotation() * FQuat(FVector::UpVector, Swing));
		}
		else if (Prop.Mode == TEXT("BeatDisc"))
		{
			Position.Z += Envelope * 9.f * Amount;
			Scale.X *= 1.f + Envelope * .07f * Amount;
			Scale.Y *= 1.f + Envelope * .07f * Amount;
		}
		else
		{
			Position.Z += (Envelope * 30.f +
			               (bRunning ? FMath::Sin(static_cast<float>(Beat * PI * .5) + I) * 9.f : 0.f)) *
			              Amount;
			Transform.SetRotation(Transform.GetRotation() *
			                      FQuat(FVector::UpVector, Envelope * .08f * Amount));
		}
		Transform.SetLocation(Position);
		Transform.SetScale3D(Scale);
		Prop.Actor->SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void ASoundstageBeatController::EndPlay(const EEndPlayReason::Type Reason)
{
	for (const FMovingProp& Prop : Props)
		if (Prop.Actor.IsValid())
			Prop.Actor->SetActorTransform(Prop.RestTransform);
	Props.Reset();
	SkyMaterials.Reset();
	Super::EndPlay(Reason);
}
