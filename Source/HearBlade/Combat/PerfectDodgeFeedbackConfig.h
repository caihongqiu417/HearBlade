#pragma once

#include "Engine/DataAsset.h"
#include "PerfectDodgeFeedbackConfig.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class USoundBase;

UENUM(BlueprintType)
enum class EPerfectDodgeAccessibility : uint8
{
	Full,
	ReducedFlash,
	NoFullscreen,
	NoCameraShake
};

UCLASS(BlueprintType)
class HEARBLADE_API UPerfectDodgeFeedbackConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|素材") TObjectPtr<UMaterialInterface> PostProcessMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|素材") TObjectPtr<UMaterialInterface> GhostMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|素材") TObjectPtr<UMaterialInterface> RingMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|素材") TObjectPtr<UNiagaraSystem> RingParticles;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|声音") TObjectPtr<USoundBase> GoodSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|声音") TObjectPtr<USoundBase> PerfectSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|声音") TObjectPtr<USoundBase> TierUpSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|时序", meta=(ClampMin="0.11", ClampMax="0.18")) float DurationSeconds = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|时序", meta=(ClampMin="0.05", ClampMax="0.4")) float GhostLifetimeSeconds = 0.22f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|外观", meta=(ClampMin="1", ClampMax="5")) int32 GhostCount = 3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|外观", meta=(ClampMin="0.1", ClampMax="3.0")) float StrongBeatMultiplier = 1.35f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|镜头", meta=(ClampMin="0.0", ClampMax="5.0")) float FOVExpansionDegrees = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|镜头", meta=(ClampMin="0.0", ClampMax="0.5")) float CameraShakeDegrees = 0.18f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|镜头", meta=(ClampMin="0.0", ClampMax="30.0")) float CameraLateralShiftCm = 14.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|镜头", meta=(ClampMin="0.0", ClampMax="20.0")) float CameraDollyShiftCm = 7.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|镜头", meta=(ClampMin="0.0", ClampMax="0.35")) float CameraYawKickDegrees = 0.32f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|镜头", meta=(ClampMin="-0.35", ClampMax="0.35")) float CameraPitchKickDegrees = -0.12f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|镜头", meta=(ClampMin="0.0", ClampMax="2.0")) float CameraReboundStrength = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|镜头", meta=(ClampMin="0.0", ClampMax="1.0")) float ObstructedStrength = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|无障碍") EPerfectDodgeAccessibility Accessibility = EPerfectDodgeAccessibility::Full;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|连击") int32 Tier1Combo = 4;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|连击") int32 Tier2Combo = 8;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|连击") int32 Tier3Combo = 16;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|连击") FVector4 TierStrengths = FVector4(1.f, 1.15f, 1.30f, 1.45f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|冲击帧", meta=(ClampMin="0.01", ClampMax="0.08")) float ImpactFrameSeconds = .035f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|冲击帧", meta=(ClampMin="0.1", ClampMax="2")) float ImpactFrameCooldown = .5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|持续格挡", meta=(ClampMin="0", ClampMax="0.2")) float HoldShakeDegrees = .05f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|持续格挡", meta=(ClampMin="0", ClampMax="0.35")) float HoldImpactShakeDegrees = .15f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|持续格挡", meta=(ClampMin="0.02", ClampMax="0.15")) float HoldStopSeconds = .06f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|UI", meta=(ClampMin="0", ClampMax="0.02")) float RegularBeatUIScale = .005f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|UI", meta=(ClampMin="0", ClampMax="0.02")) float DownbeatUIScale = .009f;
};
