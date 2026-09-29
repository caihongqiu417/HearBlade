#pragma once

#include "Components/ActorComponent.h"
#include "BossAscensionComponent.generated.h"

class UStaticMeshComponent;
class UNiagaraComponent;
class UAnimMontage;
class ACameraActor;
class ACharacter;

// Presentation owns a temporary camera and animation only. BeatClock remains the authority.
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class HEARBLADE_API UBossAscensionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UBossAscensionComponent();
	bool BeginAscension(ACharacter* Player, FVector Contact, bool bFromClash);
	void FinishPresentation(bool bCompleted);
	virtual void TickComponent(float DeltaTime,ELevelTick Type,FActorComponentTickFunction* Tick) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UPROPERTY(EditAnywhere,Category="Ascension") float BodyScaleMultiplier=1.10f;
	UPROPERTY(EditAnywhere,Category="Ascension") float CameraReturnSeconds=.45f;
	bool IsPresenting() const { return bPresenting; }
	float GetProgress() const;
private:
	void CreateForm();
	void UpdateForm(float DeltaTime, double Beat);
	void DestroyForm();
	void PlayAscensionMotion();
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> HaloParts;
	UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraComponent>> Flames;
	UPROPERTY(Transient) TArray<TObjectPtr<UAnimMontage>> PresentationMontages;
	UPROPERTY(Transient) TObjectPtr<ACameraActor> Camera;
	TWeakObjectPtr<ACharacter> PlayerActor;
	TWeakObjectPtr<AActor> PreviousView;
	FVector ContactPoint=FVector::ZeroVector;
	FVector InitialCamera=FVector::ZeroVector;
	FRotator InitialCameraRotation=FRotator::ZeroRotator;
	FVector InitialBossScale=FVector::OneVector;
	float InitialCameraFOV=90.f;
	float AppliedScale=1.f;
	float AllowedScale=1.f;
	float Reveal=0.f;
	float ReturnElapsed=0.f;
	bool bPresenting=false,bReturning=false,bTransformed=false,bMotionStarted=false,bBurstPlayed=false,bClash=false;
	double StartBeat=0.,EndBeat=0.,StartWorld=0.;
	int32 CaptureIndex=0;
};
