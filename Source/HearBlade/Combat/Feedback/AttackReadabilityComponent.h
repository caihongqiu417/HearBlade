// 显示攻击识别提示、目标路径和应对信息
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AttackReadabilityComponent.generated.h"

UCLASS(ClassGroup = (Combat))

class HEARBLADE_API UAttackReadabilityComponent : public UActorComponent
{
	GENERATED_BODY()
  public:
	UAttackReadabilityComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* TickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

  private:
	// 提示控件与动态材质，仅用于显示，不参与伤害判定
	UPROPERTY() TObjectPtr<class UStaticMeshComponent> Symbol;
	UPROPERTY() TObjectPtr<class UDecalComponent> GroundWarning;
	UPROPERTY() TObjectPtr<class UDecalComponent> DodgePathWarning;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> SymbolMaterial;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> GroundMaterial;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> DodgePathMaterial;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> BladeMaterial;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> DodgeGhostMaterial;
	UPROPERTY() TObjectPtr<class UStaticMeshComponent> DodgeBladeGhost;
	UPROPERTY() TObjectPtr<class UPointLightComponent> BladeCueLight;
	UPROPERTY() TObjectPtr<class UParticleSystemComponent> DodgeBladeTrail;
	// 保存原武器材质，撤销提示时恢复
	UPROPERTY() TObjectPtr<class UMaterialInterface> OriginalBladeOverlay;
	TWeakObjectPtr<class UMeshComponent> Blade;
	float BladeCueOpacity = 0.f;
	float SymbolCueOpacity = 0.f;
	float DodgePathOpacity = 0.f;
	bool bDodgeTrailPlaying = false;
	double DodgeTrailImpactAudioSeconds = -1.0;
	// 已显示的攻击时刻，避免每帧重复播放提示音
	double LastCue = -1.0;
	bool bGroundWasVisible = false;
	int32 LastWarningCount = INDEX_NONE;
	UPROPERTY() TObjectPtr<class USoundBase> WarningPulseSound;
	UPROPERTY() TObjectPtr<class USoundBase> CounterCueSound;
	float BurstRemaining = 0.f;
	TArray<TWeakObjectPtr<class UParticleSystemComponent>> BurstSparks;
	int32 VisualTestCapture = 0;
	bool bTransitionCaptureRequested = false;
	bool bBladeCaptureRequested = false;
};
