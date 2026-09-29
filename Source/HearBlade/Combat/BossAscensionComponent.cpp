#include "Combat/BossAscensionComponent.h"
#include "Combat/BeatSyncedComboComponent.h"
#include "Combat/BossCombatCameraComponent.h"
#include "Combat/BossLocomotionAnimInstance.h"
#include "Combat/PlayerRhythmCombatComponent.h"
#include "Combat/PlayerCombatAnimInstance.h"
#include "Combat/TransientCombatFX.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Particles/ParticleSystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Environment/SoundstageBeatController.h"
#include "EngineUtils.h"

UBossAscensionComponent::UBossAscensionComponent()
{
	PrimaryComponentTick.bCanEverTick=true;
	PrimaryComponentTick.TickGroup=TG_PostUpdateWork;
}

bool UBossAscensionComponent::BeginAscension(ACharacter* Player,FVector Contact,bool bFromClash)
{
	auto* Clock=GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	auto* Motion=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/Anim/Ascension_Taunt.Ascension_Taunt"));
	if(bPresenting || !Player || !Clock || !Clock->IsRunning() || !Motion) return false;
	PlayerActor=Player; ContactPoint=Contact; bClash=bFromClash;
	StartBeat=Clock->GetCurrentBeat();
	const double Bar=FMath::Max(1,Clock->GetBeatsPerBar());
	// Release on the nearest upcoming downbeat about six beats into the shot.
	EndBeat=FMath::RoundToDouble((StartBeat+6.)/Bar)*Bar+2.;
	StartWorld=GetWorld()->GetTimeSeconds();
	bPresenting=true; bMotionStarted=bBurstPlayed=false; CaptureIndex=0;Reveal=0.f;
	InitialBossScale=GetOwner()->GetActorScale3D(); AppliedScale=1.f; AllowedScale=1.f;
	if(auto* Capsule=Cast<UCapsuleComponent>(GetOwner()->GetRootComponent()))
	{
		const float Half=Capsule->GetScaledCapsuleHalfHeight(),Radius=Capsule->GetScaledCapsuleRadius();
		FCollisionQueryParams Q(SCENE_QUERY_STAT(AscensionGrowth),false,GetOwner());
		const FVector Center=GetOwner()->GetActorLocation()+FVector(0,0,Half*(BodyScaleMultiplier-1.f)+2.f);
		if(!GetWorld()->OverlapBlockingTestByChannel(Center,FQuat::Identity,ECC_Pawn,
			FCollisionShape::MakeCapsule(Radius*BodyScaleMultiplier,Half*BodyScaleMultiplier-1.f),Q)) AllowedScale=BodyScaleMultiplier;
	}
	if(auto* Combat=Player->FindComponentByClass<UPlayerRhythmCombatComponent>()) Combat->SetPhaseCinematic(true);
	if(auto* CombatCamera=Player->FindComponentByClass<UBossCombatCameraComponent>()) CombatCamera->SetPhasePresentation(true);
	if(auto* Anim=Cast<UPlayerCombatAnimInstance>(Player->GetMesh()->GetAnimInstance()))
		Anim->PlayCombatAction(LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Defense01_Loop.Defense01_Loop")),true,0.f);
	if(auto* PC=Cast<APlayerController>(Player->GetController()); PC && PC->PlayerCameraManager)
	{
		PreviousView=PC->GetViewTarget();
		InitialCamera=PC->PlayerCameraManager->GetCameraLocation(); InitialCameraRotation=PC->PlayerCameraManager->GetCameraRotation();
		InitialCameraFOV=PC->PlayerCameraManager->GetFOVAngle();
		Camera=GetWorld()->SpawnActor<ACameraActor>(InitialCamera,InitialCameraRotation);
		Camera->GetCameraComponent()->SetFieldOfView(InitialCameraFOV);
		PC->SetViewTarget(Camera);
	}
	CreateForm();
	PlayAscensionMotion();
	UE_LOG(LogTemp,Display,TEXT("BossAscension: BEGIN source=%s beat=%.3f end=%.3f grow=%.2f musicRate=%.3f"),
		bClash?TEXT("real-contact"):TEXT("no-clash-fallback"),StartBeat,EndBeat,AllowedScale,Clock->GetMusicalPlaybackRate());
	return true;
}

float UBossAscensionComponent::GetProgress() const
{
	const auto* Clock=GetWorld()?GetWorld()->GetSubsystem<UBeatClockSubsystem>():nullptr;
	return bPresenting && Clock?FMath::Clamp(float((Clock->GetCurrentBeat()-StartBeat)/(EndBeat-StartBeat)),0.f,1.f):-1.f;
}

void UBossAscensionComponent::CreateForm()
{
	if(!HaloParts.IsEmpty()) return;
	// Reuse the knight's detailed weapon rather than architectural ring blocks.
	auto* Asset=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/MESHES/SWORD/SM_Dragon_knight_sword.SM_Dragon_knight_sword"));
	for(int32 I=0;Asset && I<5;++I)
	{
		auto* Part=NewObject<UStaticMeshComponent>(GetOwner());
		Part->SetStaticMesh(Asset);Part->SetupAttachment(GetOwner()->GetRootComponent());
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetCastShadow(false);
		Part->SetVisibility(false);Part->RegisterComponent();HaloParts.Add(Part);
	}
	USkeletalMeshComponent* Body=nullptr;
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(GetOwner());
	for(auto* Mesh:Meshes) if(Mesh->DoesSocketExist(TEXT("spine_03")) && Cast<UBossLocomotionAnimInstance>(Mesh->GetAnimInstance())) {Body=Mesh;break;}
	auto* Fire=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/VFX/NS_Fire_local_System.NS_Fire_local_System"));
	for(const FName Socket:{FName(TEXT("clavicle_l")),FName(TEXT("clavicle_r")),FName(TEXT("spine_03"))})
		if(Body && Fire && Body->DoesSocketExist(Socket))
			if(auto* FX=UNiagaraFunctionLibrary::SpawnSystemAttached(Fire,Body,Socket,FVector::ZeroVector,FRotator::ZeroRotator,EAttachLocation::SnapToTarget,false,false))
			{FX->SetRelativeScale3D(FVector(.01f));FX->Activate();Flames.Add(FX);}
}

void UBossAscensionComponent::PlayAscensionMotion()
{
	bMotionStarted=true;
	auto* Clock=GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	auto* Motion=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/Anim/Ascension_Taunt.Ascension_Taunt"));
	if(!Motion || !Clock) return;
	const float Duration=FMath::Max(.5f,float((EndBeat-Clock->GetCurrentBeat())*Clock->GetSecondsPerBeat()));
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(GetOwner());
	for(auto* Mesh:Meshes)
		if(auto* Anim=Mesh->GetAnimInstance(); Anim && Mesh->GetSkeletalMeshAsset() && Mesh->GetSkeletalMeshAsset()->GetSkeleton()==Motion->GetSkeleton())
		{
			Mesh->GlobalAnimRateScale=1.f;
			if(auto* Locomotion=Cast<UBossLocomotionAnimInstance>(Anim)) Locomotion->SetLocomotion(0.f,FVector::ZeroVector,0.f);
			if(auto* Montage=Anim->PlaySlotAnimationAsDynamicMontage(Motion,TEXT("DefaultSlot"),.16f,.28f,Motion->GetPlayLength()/Duration)) PresentationMontages.Add(Montage);
		}
	UE_LOG(LogTemp,Display,TEXT("BossAscension: full Taunt motion meshes=%d duration=%.3f"),PresentationMontages.Num(),Duration);
}

void UBossAscensionComponent::UpdateForm(float DeltaTime,double Beat)
{
	const float P=GetProgress();
	const float Target=bTransformed?1.f:FMath::SmoothStep(.16f,.72f,P);
	Reveal=FMath::FInterpTo(Reveal,Target,DeltaTime,10.f);
	const float Pulse=FMath::Exp(-float(FMath::Frac(Beat))*7.f);
	const FVector Forward=GetOwner()->GetActorForwardVector(),Right=GetOwner()->GetActorRightVector();
	const auto* CrownCapsule=Cast<UCapsuleComponent>(GetOwner()->GetRootComponent());
	const float Half=CrownCapsule?CrownCapsule->GetScaledCapsuleHalfHeight():110.f;
	const FVector CrownCenter=GetOwner()->GetActorLocation()+FVector(0,0,Half*.86f)-Forward*65.f;
	for(int32 I=0;I<HaloParts.Num();++I)
	{
		auto* Part=HaloParts[I].Get();
		const float Lane=float(I-2),Spread=FMath::Lerp(.3f,1.f,Reveal);
		const float Angle=FMath::DegreesToRadians(-Lane*23.f*Spread);
		const FQuat Rotation=GetOwner()->GetActorQuat()*FQuat(FVector::ForwardVector,Angle)*FQuat(FVector::UpVector,UE_HALF_PI);
		const float Length=(I==2?135.f:110.f)*FMath::Max(.01f,Reveal);
		const float S=Length/(Part->GetStaticMesh()->GetBounds().BoxExtent.Z*2.f);
		const FVector Center=CrownCenter+Right*(Lane*32.f*Spread)+FVector(0,0,30.f-FMath::Abs(Lane)*13.f+FMath::Sin(float(Beat)*UE_PI+I)*1.5f);
		Part->SetWorldScale3D(FVector(S));Part->SetWorldRotation(Rotation);
		Part->SetWorldLocation(Center-Rotation.RotateVector(Part->GetStaticMesh()->GetBounds().Origin*S));
		Part->SetVisibility(Reveal>.02f);
	}
	for(UNiagaraComponent* FX:Flames) if(IsValid(FX)) FX->SetRelativeScale3D(FVector(FMath::Max(.01f,Reveal)*(.38f+Pulse*.06f)));
	if(bPresenting && AllowedScale>1.f)
	{
		const float NewScale=FMath::Lerp(1.f,AllowedScale,Reveal);
		if(auto* Capsule=Cast<UCapsuleComponent>(GetOwner()->GetRootComponent()))
		{
			const float OldHalf=Capsule->GetScaledCapsuleHalfHeight();
			GetOwner()->SetActorScale3D(InitialBossScale*NewScale);
			GetOwner()->AddActorWorldOffset(FVector(0,0,Capsule->GetScaledCapsuleHalfHeight()-OldHalf),false);
		}
		AppliedScale=NewScale;
	}
}

void UBossAscensionComponent::TickComponent(float DeltaTime,ELevelTick Type,FActorComponentTickFunction* Tick)
{
	Super::TickComponent(DeltaTime,Type,Tick);
	auto* Combo=GetOwner()->FindComponentByClass<UBeatSyncedComboComponent>();
	auto* Clock=GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	if(!Combo || Combo->BossHealth<=0.f)
	{
		const bool bWasPresenting=bPresenting;FinishPresentation(false);DestroyForm();
		if(Combo && bWasPresenting) Combo->FinishPhaseAscension(false);
		if(Camera)
		{
			if(auto* Player=PlayerActor.Get()) if(auto* PC=Cast<APlayerController>(Player->GetController())) PC->SetViewTarget(PreviousView.IsValid()?PreviousView.Get():Player);
			Camera->Destroy();Camera=nullptr;bReturning=false;
		}
		return;
	}
	if(bReturning)
	{
		ReturnElapsed+=DeltaTime;
		if(ReturnElapsed>CameraReturnSeconds+.1f) {if(Camera) Camera->Destroy();Camera=nullptr;bReturning=false;}
	}
	if(!bPresenting) {if(bTransformed && Clock) UpdateForm(DeltaTime,Clock->GetCurrentBeat());return;}
	auto* Player=PlayerActor.Get();
	const auto* Combat=Player?Player->FindComponentByClass<UPlayerRhythmCombatComponent>():nullptr;
	if(!Clock || !Clock->IsRunning() || !Player || !Combat || Combat->Health<=0.f || GetWorld()->GetTimeSeconds()-StartWorld>12.f)
	{FinishPresentation(false);Combo->FinishPhaseAscension(false);return;}
	const float P=GetProgress(); const double Beat=Clock->GetCurrentBeat();
	if(!bMotionStarted && GetWorld()->GetTimeSeconds()-StartWorld>.22f) PlayAscensionMotion();
	UpdateForm(DeltaTime,Beat);
	// Orient over several frames from the actual contact, without teleporting either actor.
	const FVector Toward=(Player->GetActorLocation()-GetOwner()->GetActorLocation()).GetSafeNormal2D();
	if(P<.16f)
	{
		GetOwner()->SetActorRotation(FMath::RInterpTo(GetOwner()->GetActorRotation(),Toward.Rotation(),DeltaTime,9.f));
		Player->SetActorRotation(FMath::RInterpTo(Player->GetActorRotation(),(-Toward).Rotation(),DeltaTime,9.f));
	}
	if(Camera)
	{
		const FVector Side=FVector::CrossProduct(FVector::UpVector,Toward);
		const FVector BossChest=GetOwner()->GetActorLocation()+FVector(0,0,60);
		const FVector Mid=(Player->GetActorLocation()+GetOwner()->GetActorLocation())*.5f+FVector(0,0,45);
		const float RevealT=FMath::SmoothStep(.14f,.70f,P);
		const FVector Focus=bClash?FMath::Lerp(ContactPoint,BossChest,RevealT):BossChest;
		const FVector Close=(bClash?Mid:BossChest)+Side*340.f+Toward*110.f+FVector(0,0,95);
		const FVector Wide=BossChest+Side*320.f+Toward*460.f+FVector(0,0,95);
		FVector Wanted=FMath::Lerp(Close,Wide,RevealT);
		const float Enter=FMath::SmoothStep(0.f,.12f,P);
		Wanted=FMath::Lerp(InitialCamera,Wanted,Enter);
		FCollisionQueryParams Q(SCENE_QUERY_STAT(AscensionCamera),false,GetOwner()); Q.AddIgnoredActor(Player);
		FHitResult Hit;
		if(GetWorld()->SweepSingleByChannel(Hit,Focus,Wanted,FQuat::Identity,ECC_Camera,FCollisionShape::MakeSphere(15.f),Q)) Wanted=Hit.Location+Hit.Normal*8.f;
		Camera->SetActorLocation(Wanted);
		Camera->SetActorRotation(FQuat::Slerp(InitialCameraRotation.Quaternion(),(Focus-Wanted).Rotation().Quaternion(),Enter));
		Camera->GetCameraComponent()->SetFieldOfView(FMath::Lerp(InitialCameraFOV,FMath::Lerp(54.f,65.f,RevealT),Enter));
	}
	if(!bBurstPlayed && Beat>=EndBeat-2.)
	{
		bBurstPlayed=true;
		for(TActorIterator<ASoundstageBeatController> It(GetWorld());It;++It) It->TriggerAscensionAccent(EndBeat-2.);
		if(auto* Sound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/SFX/SW_BossDrumKick.SW_BossDrumKick"))) UGameplayStatics::PlaySound2D(this,Sound,.85f);
		if(auto* Spark=LoadObject<UParticleSystem>(nullptr,TEXT("/Game/SwordAnim/demo/FX/P_Spark.P_Spark")))
			RetireCombatFX(UGameplayStatics::SpawnEmitterAtLocation(GetWorld(),Spark,GetOwner()->GetActorLocation()+FVector(0,0,90),FRotator::ZeroRotator,FVector(1.3f)),.08f,.5f);
		UE_LOG(LogTemp,Display,TEXT("BossAscension: RELEASE beat=%.3f target=%.0f errorMs=%.1f"),Beat,EndBeat-2.,(Beat-(EndBeat-2.))*Clock->GetSecondsPerBeat()*1000.);
	}
#if !UE_BUILD_SHIPPING
	if(P>.5f && FParse::Param(FCommandLine::Get(),TEXT("PhaseTransitionAbortTest")))
	{UE_LOG(LogTemp,Display,TEXT("BossAscension: test-only target destruction injected"));GetOwner()->Destroy();return;}
	if(FParse::Param(FCommandLine::Get(),TEXT("PhaseTransitionTest")) && CaptureIndex<3 && P>(CaptureIndex==0?.12f:CaptureIndex==1?.5f:.82f))
	{
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/Ascension_%d.png"),CaptureIndex++),false,false);
	}
#endif
	if(Beat>=EndBeat) {FinishPresentation(true);Combo->FinishPhaseAscension(true);}
}

void UBossAscensionComponent::FinishPresentation(bool bCompleted)
{
	if(!bPresenting) return;
	bPresenting=false; bTransformed=bCompleted;
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(GetOwner());
	for(auto* Mesh:Meshes) if(auto* Anim=Mesh->GetAnimInstance())
		for(UAnimMontage* Montage:PresentationMontages) Anim->Montage_Stop(.25f,Montage);
	PresentationMontages.Reset();
	if(auto* Player=PlayerActor.Get())
	{
		if(auto* Combat=Player->FindComponentByClass<UPlayerRhythmCombatComponent>()) Combat->SetPhaseCinematic(false);
		if(auto* CombatCamera=Player->FindComponentByClass<UBossCombatCameraComponent>()) CombatCamera->SetPhasePresentation(false);
		if(auto* PC=Cast<APlayerController>(Player->GetController()))
		{
			if(Camera && PC->GetViewTarget()==Camera) PC->SetViewTargetWithBlend(PreviousView.IsValid()?PreviousView.Get():Player,CameraReturnSeconds,VTBlend_Cubic);
		}
	}
	bReturning=true;ReturnElapsed=0.f;
	if(!bCompleted) DestroyForm();
	UE_LOG(LogTemp,Display,TEXT("BossAscension: END complete=%d input released, camera returning, no music rate writes"),bCompleted);
}

void UBossAscensionComponent::DestroyForm()
{
	for(UStaticMeshComponent* Part:HaloParts) if(IsValid(Part)) Part->DestroyComponent(); HaloParts.Reset();
	for(UNiagaraComponent* FX:Flames) if(IsValid(FX)) FX->DestroyComponent(); Flames.Reset();
	if(AppliedScale>1.f && IsValid(GetOwner()))
		if(auto* Capsule=Cast<UCapsuleComponent>(GetOwner()->GetRootComponent()))
		{
			const float OldHalf=Capsule->GetScaledCapsuleHalfHeight(); GetOwner()->SetActorScale3D(InitialBossScale);
			GetOwner()->AddActorWorldOffset(FVector(0,0,Capsule->GetScaledCapsuleHalfHeight()-OldHalf),false);
		}
	AppliedScale=1.f;bTransformed=false;
}

void UBossAscensionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	FinishPresentation(false);
	if(Camera)
	{
		if(auto* Player=PlayerActor.Get()) if(auto* PC=Cast<APlayerController>(Player->GetController()))
			PC->SetViewTarget(PreviousView.IsValid()?PreviousView.Get():Player);
		Camera->Destroy();Camera=nullptr;
	}
	DestroyForm(); Super::EndPlay(Reason);
}
