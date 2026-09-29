#include "Combat/AttackReadabilityComponent.h"
#include "Combat/BeatSyncedComboComponent.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Combat/TransientCombatFX.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Sound/SoundBase.h"

UAttackReadabilityComponent::UAttackReadabilityComponent() { PrimaryComponentTick.bCanEverTick=true; }

void UAttackReadabilityComponent::BeginPlay()
{
	Super::BeginPlay();
	WarningPulseSound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/SFX/SW_BossDrumKick.SW_BossDrumKick"));
	Symbol=NewObject<UStaticMeshComponent>(GetOwner());
	Symbol->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
	Symbol->SetCollisionEnabled(ECollisionEnabled::NoCollision); Symbol->SetCastShadow(false);
	Symbol->SetMobility(EComponentMobility::Movable); Symbol->RegisterComponent();
	if (auto* Mat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Combat/Readability/M_AttackRecognition.M_AttackRecognition")))
	{ SymbolMaterial=UMaterialInstanceDynamic::Create(Mat,this); Symbol->SetMaterial(0,SymbolMaterial); }
	GroundWarning=NewObject<UDecalComponent>(GetOwner()); GroundWarning->RegisterComponent();
	if (auto* Mat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Combat/Readability/M_GroundBurstWarning.M_GroundBurstWarning")))
	{ GroundMaterial=UMaterialInstanceDynamic::Create(Mat,this); GroundWarning->SetDecalMaterial(GroundMaterial); }
	Symbol->SetVisibility(false);GroundWarning->SetVisibility(false);
	TInlineComponentArray<UMeshComponent*> Meshes(GetOwner());
	for(auto* Mesh:Meshes)
		if(Mesh->GetName().Contains(TEXT("Sword"),ESearchCase::IgnoreCase)||Mesh->GetName().Contains(TEXT("Weapon"),ESearchCase::IgnoreCase)||Mesh->ComponentHasTag(TEXT("DamageBlade")))
		{Blade=Mesh;OriginalBladeOverlay=Mesh->GetOverlayMaterial();break;}
	if(auto* Mat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Combat/Readability/M_BladeRecognition.M_BladeRecognition")))
		BladeMaterial=UMaterialInstanceDynamic::Create(Mat,this);
}

void UAttackReadabilityComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime,TickType,TickFunction);
	const auto* Combo=GetOwner()->FindComponentByClass<UBeatSyncedComboComponent>();
	if (!Combo) return;
	FAttackRecognitionInfo Info;FVector Position;float Progress=0.f;double Key=0.;
	bool bCue=Combo->GetRecognitionCue(Info,Position,Progress,Key);
	const float Transition=Combo->GetPhaseTransitionProgress();
	if(Transition>=0.f)
	{
		bCue=false; // The ascension has actual geometry; no floating attack symbol over its camera shot.
	}
	SymbolCueOpacity=FMath::FInterpTo(SymbolCueOpacity,bCue&&Info.bShowSymbol?1.f:0.f,DeltaTime,bCue?18.f:22.f);
	Symbol->SetVisibility(SymbolCueOpacity>.015f && SymbolMaterial);
	if(SymbolMaterial) SymbolMaterial->SetScalarParameterValue(TEXT("RenderOpacity"),SymbolCueOpacity);
	BladeCueOpacity=FMath::FInterpTo(BladeCueOpacity,bCue&&Transition<0.f?1.f:0.f,DeltaTime,24.f);
	if(Blade.IsValid()&&BladeMaterial)
	{
		if(BladeCueOpacity>.01f)
		{
			Blade->SetOverlayMaterial(BladeMaterial);
			BladeMaterial->SetScalarParameterValue(TEXT("CueStrength"),BladeCueOpacity);
			if(bCue&&Transition<0.f) BladeMaterial->SetVectorParameterValue(TEXT("CueColor"),Info.Color);
		}
		else if(Blade->GetOverlayMaterial()==BladeMaterial) Blade->SetOverlayMaterial(OriginalBladeOverlay);
	}
	if (bCue)
	{
		if (auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0))
		{
			Symbol->SetWorldLocation(Position);
			Symbol->SetWorldRotation(FRotationMatrix::MakeFromZ(Camera->GetCameraLocation()-Position).Rotator());
			const float ShapeSize=Info.Shape==EAttackRecognitionShape::WideArc?1.2f:.82f;
			Symbol->SetWorldScale3D(FVector(Transition>=0.f?1.1f+.45f*Transition:ShapeSize*(1.12f-.12f*Progress)));
		}
		if (SymbolMaterial)
		{
			SymbolMaterial->SetVectorParameterValue(TEXT("CueColor"),Info.Color);
			SymbolMaterial->SetScalarParameterValue(TEXT("Shape"),static_cast<float>(Info.Shape));
			SymbolMaterial->SetScalarParameterValue(TEXT("Progress"),Progress);
			SymbolMaterial->SetScalarParameterValue(TEXT("UseIcon"),Info.Symbol?1.f:0.f);
			if (Info.Symbol) SymbolMaterial->SetTextureParameterValue(TEXT("Icon"),Info.Symbol);
		}
		if (Key!=LastCue)
		{
			LastCue=Key;
			if (Info.CueSound) UGameplayStatics::PlaySoundAtLocation(this,Info.CueSound,Position,.65f);
		}
	}
	float Radius=0.f;
	const bool bGround=Combo->GetGroundBurstPreview(Position,Radius,Progress);
	if (bGround)
	{
		const float Beats=Combo->GetGroundBurstRemainingBeats();
		const int32 Count=FMath::CeilToInt(Beats);
		if(Count!=LastWarningCount)
		{
			LastWarningCount=Count;
			if(WarningPulseSound) UGameplayStatics::PlaySound2D(this,WarningPulseSound,Count<=1?.65f:.4f,Count<=1?.9f:.75f);
		}
		GroundWarning->SetWorldLocation(Position+FVector(0,0,35));
		GroundWarning->SetWorldRotation(FRotator(-90,0,0));
		GroundWarning->DecalSize=FVector(120,Radius,Radius);
		BurstRemaining=0.f;
	}
	else LastWarningCount=INDEX_NONE;
	// A cancelled attack must never look like an explosion.
	if (bGroundWasVisible && !bGround && Combo->HasResolvedGroundBurst())
	{
		BurstRemaining=.45f;
		BurstSparks.RemoveAll([](const auto& Spark){ return !Spark.IsValid(); });
		if (auto* Sparks=LoadObject<UParticleSystem>(nullptr,TEXT("/Game/SwordAnim/demo/FX/P_Spark.P_Spark")))
		{
			const FVector Center=GroundWarning->GetComponentLocation();
			const float SparkRadius=GroundWarning->DecalSize.Y*.72f;
			for (int32 I=0;I<8;++I)
			{
				const float Angle=I*UE_TWO_PI/8.f;
				const FVector Offset(FMath::Cos(Angle)*SparkRadius,FMath::Sin(Angle)*SparkRadius,-20.f);
				auto* Spark=UGameplayStatics::SpawnEmitterAtLocation(GetWorld(),Sparks,Center+Offset,FRotator::ZeroRotator,FVector(.65f),true);
				RetireCombatFX(Spark,.06f,.35f);BurstSparks.Add(Spark);
			}
		}
	}
	bGroundWasVisible=bGround;
	BurstRemaining=FMath::Max(0.f,BurstRemaining-DeltaTime);
	GroundWarning->SetVisibility(GroundMaterial && (bGround || BurstRemaining>0.f));
	if (GroundMaterial)
	{
		GroundMaterial->SetScalarParameterValue(TEXT("Progress"),bGround?Progress:1.f);
		const float Tail=BurstRemaining/.45f;
		GroundMaterial->SetScalarParameterValue(TEXT("Burst"),Tail);
		GroundMaterial->SetScalarParameterValue(TEXT("RenderOpacity"),bGround?1.f:FMath::SmoothStep(0.f,.55f,Tail));
		const float Remaining=Combo->GetGroundBurstRemainingBeats();
		const float Pulse=Remaining>=0.f?FMath::Exp(-(FMath::CeilToFloat(Remaining)-Remaining)*7.f):0.f;
		// Oppressive, not bright: a dark crimson pool that deepens toward the hit,
		// dim ember rim, and a low heartbeat pulse on each remaining beat.
		GroundMaterial->SetVectorParameterValue(TEXT("CueColor"),FLinearColor(.30f,.018f,.008f));
		GroundMaterial->SetVectorParameterValue(TEXT("SigilBodyColor"),FLinearColor(.012f,.0f,.0f));
		GroundMaterial->SetScalarParameterValue(TEXT("SigilOpacity"),.35f);
		GroundMaterial->SetScalarParameterValue(TEXT("SigilEmission"),bGround?.06f+.16f*Progress+Pulse*(Remaining<1.f?.5f:.18f):.7f+Tail*1.1f);
	}
#if !UE_BUILD_SHIPPING
	if(!bBladeCaptureRequested && bCue && Info.Shape==EAttackRecognitionShape::ShortGlint && Progress>.7f && GetWorld()->GetTimeSeconds()>3.f && FParse::Param(FCommandLine::Get(),TEXT("BladeReadabilityCapture")))
	{
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/BladeReadabilityRefined.png"),false,false);
		bBladeCaptureRequested=true;
	}
	if(!bTransitionCaptureRequested && Transition>.3f && FParse::Param(FCommandLine::Get(),TEXT("PhaseTransitionTest")))
	{
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/PhaseTwoTransition.png"),false,false);
		bTransitionCaptureRequested=true;
	}
	if (FParse::Param(FCommandLine::Get(),TEXT("PhaseTwoVisualTest")))
	{
		const bool bCapture=(VisualTestCapture==0&&bGround&&Progress>.15f)||
			(VisualTestCapture==1&&bGround&&Progress>.75f)||
			(VisualTestCapture==2&&BurstRemaining>.25f);
		if (bCapture)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/PhaseTwoWarning_%d.png"),VisualTestCapture),false,false);
			++VisualTestCapture;
		}
	}
#endif
}

void UAttackReadabilityComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Symbol) Symbol->DestroyComponent();
	if (GroundWarning) GroundWarning->DestroyComponent();
	if(Blade.IsValid()&&Blade->GetOverlayMaterial()==BladeMaterial) Blade->SetOverlayMaterial(OriginalBladeOverlay);
	for (const auto& Spark:BurstSparks) if (Spark.IsValid()) Spark->DestroyComponent();
	Super::EndPlay(Reason);
}

bool UBeatSyncedComboComponent::GetRecognitionCue(FAttackRecognitionInfo& Info,FVector& Location,float& Progress,double& Key) const
{
	if ((!bQueued&&!bPlaying)||!AttackData) return false;
	const auto* Clock=GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	if (!Clock||!Clock->IsRunning()) return false;
	TArray<FUpcomingImpactCue> Cues;GetUpcomingImpactCues(Cues,8);
	for (const FUpcomingImpactCue& Cue:Cues)
	{
		const auto& Marker=GetImpactMarker(Cue.ImpactIndex);
		Info=Marker.Recognition;
		const auto& Default=AttackData->DefaultRecognition;
		if (!Info.CueSound) Info.CueSound=Default.CueSound;
		if (!Info.Symbol) Info.Symbol=Default.Symbol;
		if (Info.Shape==EAttackRecognitionShape::Auto) Info.Shape=Default.Shape;
		if (!Info.bOverrideColor && Default.bOverrideColor) {Info.Color=Default.Color;Info.bOverrideColor=true;}
		const double Until=Cue.ImpactAudioSeconds-Cue.IdealDodgeLeadSeconds-Clock->GetAudioNowSeconds();
		const double Lead=FMath::Max(.25f,Info.LeadBeats)*Clock->GetSecondsPerBeat();
		if (Until<-.08 || Until>Lead) continue;
		if (Info.Shape==EAttackRecognitionShape::Auto)
		{
			Info.Shape=Cue.RequiredResponse==ERhythmResponseType::Dodge?EAttackRecognitionShape::WideArc:
				Cue.RequiredResponse==ERhythmResponseType::Counter?EAttackRecognitionShape::Cross:
				Cue.RequiredResponse==ERhythmResponseType::HoldBlock?EAttackRecognitionShape::SustainedArc:EAttackRecognitionShape::ShortGlint;
		}
		if (Marker.DamageShape==EBossAttackDamageShape::GroundBurst) Info.Shape=EAttackRecognitionShape::GroundWarning;
		else if(Cue.RequiredResponse==ERhythmResponseType::DodgeOrBlock)
		{
			// One neutral melee cue: authored Dodge/Counter shapes no longer imply
			// a mandatory response after the phase-two rules change.
			Info.Shape=EAttackRecognitionShape::ShortGlint;
			Info.Symbol=nullptr;Info.bOverrideColor=true;
			Info.Color=FLinearColor(.8f,.9f,1.f);
		}
		if (!Info.bOverrideColor)
			Info.Color=Info.Shape==EAttackRecognitionShape::WideArc?FLinearColor(.08f,.8f,1.f):
				Info.Shape==EAttackRecognitionShape::Cross?FLinearColor(1.f,.65f,.08f):
				Info.Shape==EAttackRecognitionShape::GroundWarning?FLinearColor(1.f,.15f,.04f):FLinearColor(.8f,.9f,1.f);
		Location=GetOwner()->GetActorLocation()+FVector(0,0,80);
		if(Marker.DamageShape!=EBossAttackDamageShape::GroundBurst)
			if(UMeshComponent* Blade=FindBladeMesh()) Location=Blade->Bounds.Origin+FVector(0,0,14);
		Progress=1.f-FMath::Clamp(static_cast<float>(Until/Lead),0.f,1.f); Key=Cue.ImpactAudioSeconds;
		return true;
	}
	return false;
}
