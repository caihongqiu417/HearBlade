#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AttackReadabilityComponent.generated.h"

UCLASS(ClassGroup=(Combat))
class HEARBLADE_API UAttackReadabilityComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UAttackReadabilityComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UPROPERTY() TObjectPtr<class UStaticMeshComponent> Symbol;
	UPROPERTY() TObjectPtr<class UDecalComponent> GroundWarning;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> SymbolMaterial;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> GroundMaterial;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> BladeMaterial;
	UPROPERTY() TObjectPtr<class UMaterialInterface> OriginalBladeOverlay;
	TWeakObjectPtr<class UMeshComponent> Blade;
	float BladeCueOpacity = 0.f;
	float SymbolCueOpacity = 0.f;
	double LastCue = -1.0;
	bool bGroundWasVisible = false;
	int32 LastWarningCount = INDEX_NONE;
	UPROPERTY() TObjectPtr<class USoundBase> WarningPulseSound;
	float BurstRemaining = 0.f;
	TArray<TWeakObjectPtr<class UParticleSystemComponent>> BurstSparks;
	int32 VisualTestCapture = 0;
	bool bTransitionCaptureRequested = false;
	bool bBladeCaptureRequested = false;
};
