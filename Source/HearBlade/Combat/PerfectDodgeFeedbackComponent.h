#pragma once

#include "Components/ActorComponent.h"
#include "Combat/PlayerRhythmCombatComponent.h"
#include "PerfectDodgeFeedbackComponent.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPerfectDodgeFeedbackConfig;
class UPoseableMeshComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class HEARBLADE_API UPerfectDodgeFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPerfectDodgeFeedbackComponent();
	void PlayResonanceAttack(bool bBurst);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Perfect Dodge") TObjectPtr<UPerfectDodgeFeedbackConfig> Config;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
	// Punch-family defense sounds (same synthesized set as the attacks, /Game/Audio/SFX/HitRush).
	UPROPERTY(Transient) TObjectPtr<class USoundBase> DodgeHitSound;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> BlockHitSound;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> CounterHitSound;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> HoldHitSound;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> TierUpHitSound;
	UFUNCTION() void HandleRhythmFeedback(const FRhythmSuccessFeedbackData& Feedback);
	void PlayPerfectFeedback(const FPerfectDodgeFeedbackData& Feedback, float TierScale, bool bKeyPerfect);
	void StartImpactFrame();
	void RestoreImpactFrame();
	void SpawnRing(const FPerfectDodgeFeedbackData& Feedback, float DelaySeconds);
	void SpawnGhosts(float Strength);
	void ClearTransientVisuals();
	void UpdateHoldArmorVisual(float DeltaTime);
	void CreateHoldArmorVisual();
	void DestroyHoldArmorVisual();
	struct FGhostVisual { TWeakObjectPtr<UPoseableMeshComponent> Mesh; TWeakObjectPtr<UMaterialInstanceDynamic> Material; float Delay=0.f; };
	struct FRingVisual { TWeakObjectPtr<UStaticMeshComponent> Mesh; TWeakObjectPtr<UMaterialInstanceDynamic> Material; double Start=0.0; };
	TArray<FGhostVisual> Ghosts;
	// Ghost meshes are created once and reused: building skinned meshes + render
	// state at the moment of a perfect dodge caused a visible frame hitch.
	TArray<FGhostVisual> GhostPool;
	void EnsureGhostPool();
	TArray<FRingVisual> Rings;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> PostProcessMID;
	// The Dark Knight body and cloak are separate skeletal components.  Keep one
	// synchronized armor shell per source component so cloth never drops out of
	// the sustained-guard silhouette.
	struct FHoldArmorVisual
	{
		TWeakObjectPtr<USkeletalMeshComponent> Source;
		TWeakObjectPtr<USkeletalMeshComponent> Mesh;
		TWeakObjectPtr<UMaterialInstanceDynamic> Material;
		TWeakObjectPtr<UMaterialInterface> PreviousOverlay;
	};
	TArray<FHoldArmorVisual> HoldArmorVisuals;
	double HoldArmorImpactWorldSeconds = -1000.0;
	bool bHoldArmorWasActive = false;
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
