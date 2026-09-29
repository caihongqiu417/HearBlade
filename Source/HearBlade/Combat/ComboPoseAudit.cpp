#include "Combat/BeatSyncedComboComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "NiagaraComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

bool UBeatSyncedComboComponent::TickComboPoseAudit()
{
#if !UE_BUILD_SHIPPING
	if(!FParse::Param(FCommandLine::Get(),TEXT("ComboPoseAudit"))) return false;
	const float Time=GetWorld()->GetTimeSeconds();
	if(Time>19.f) {FPlatformMisc::RequestExit(false);return true;}
	if(!PoseAuditCamera.IsValid())
	{
		const FVector Focus=GetOwner()->GetActorLocation()+FVector(0,0,40);
		const FVector Position=Focus+FVector(420,550,110);
		auto* Camera=GetWorld()->SpawnActor<ACameraActor>(Position,(Focus-Position).Rotation());
		Camera->GetCameraComponent()->SetFieldOfView(48.f);PoseAuditCamera=Camera;
		TInlineComponentArray<UNiagaraComponent*> Flames(GetOwner());
		for(auto* Flame:Flames) Flame->SetVisibility(false);
	}
	if(auto* PC=UGameplayStatics::GetPlayerController(this,0))
	{
		PC->SetViewTarget(PoseAuditCamera.Get());
		if(PC->GetPawn()) PC->GetPawn()->SetActorHiddenInGame(true);
	}
	const int32 Frames[]={20,33,60,70,82,94,150,172,175};
	const int32 Index=FMath::Clamp(FMath::FloorToInt(Time/2.f),0,8);
	auto* Sequence=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/Montage/Combo03_BeatRootMotion.Combo03_BeatRootMotion"));
	if(!Sequence) return true;
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(GetOwner());
	for(auto* Mesh:Meshes)
	{
		if(!Mesh->GetSkeletalMeshAsset() || Mesh->GetSkeletalMeshAsset()->GetSkeleton()!=Sequence->GetSkeleton()) continue;
		if(Mesh->GetAnimationMode()!=EAnimationMode::AnimationSingleNode) {Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);Mesh->SetAnimation(Sequence);}
		Mesh->SetPlayRate(0.f);Mesh->SetPosition(Frames[Index]/30.f,false);
		Mesh->TickAnimation(0.f,false);Mesh->RefreshBoneTransforms();
	}
	if(PoseAuditCaptured!=Index && FMath::Fmod(Time,2.f)>.6f)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/Combo03Pose_%03d.png"),Frames[Index]),false,false);
		PoseAuditCaptured=Index;
		UE_LOG(LogTemp,Display,TEXT("ComboPoseAudit: frame=%d seconds=%.3f"),Frames[Index],Frames[Index]/30.f);
	}
	return true;
#else
	return false;
#endif
}
