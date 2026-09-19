#pragma once

#include "Components/ActorComponent.h"
#include "Combat/PlayerRhythmCombatComponent.h"
#include "PerfectDodgeFeedbackComponent.generated.h"

class UMaterialInstanceDynamic;
class UPerfectDodgeFeedbackConfig;
class UPoseableMeshComponent;
class UStaticMeshComponent;

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class HEARBLADE_API UPerfectDodgeFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPerfectDodgeFeedbackComponent();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Perfect Dodge") TObjectPtr<UPerfectDodgeFeedbackConfig> Config;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
	UFUNCTION() void HandleRhythmFeedback(const FRhythmSuccessFeedbackData& Feedback);
	void PlayPerfectFeedback(const FPerfectDodgeFeedbackData& Feedback, float TierScale, bool bKeyPerfect);
	void StartImpactFrame();
	void RestoreImpactFrame();
	void SpawnRing(const FPerfectDodgeFeedbackData& Feedback, float DelaySeconds);
	void SpawnGhosts(float Strength);
	void ClearTransientVisuals();
	struct FGhostVisual { TWeakObjectPtr<UPoseableMeshComponent> Mesh; TWeakObjectPtr<UMaterialInstanceDynamic> Material; float Delay=0.f; };
	struct FRingVisual { TWeakObjectPtr<UStaticMeshComponent> Mesh; TWeakObjectPtr<UMaterialInstanceDynamic> Material; double Start=0.0; };
	TArray<FGhostVisual> Ghosts;
	TArray<FRingVisual> Rings;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> PostProcessMID;
	double EffectStartWorldSeconds = -1000.0;
	float ActiveDuration = 0.15f;
	float ActiveStrength = 1.0f;
	double LastImpactFrameWorldSeconds = -1000.0;
	double ImpactFrameEndWorldSeconds = -1000.0;
	FVector4 SavedSaturation = FVector4(1,1,1,1);
	FVector4 SavedContrast = FVector4(1,1,1,1);
	bool bSavedSaturationOverride = false;
	bool bSavedContrastOverride = false;
	bool bImpactFrameActive = false;
};
