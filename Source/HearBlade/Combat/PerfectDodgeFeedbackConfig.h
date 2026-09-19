#pragma once

#include "Engine/DataAsset.h"
#include "PerfectDodgeFeedbackConfig.generated.h"

class UMaterialInterface;
class UNiagaraSystem;

UENUM(BlueprintType)
enum class EPerfectDodgeAccessibility : uint8
{
	Full,
	ReducedFlash,
	NoFullscreen
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|时序", meta=(ClampMin="0.11", ClampMax="0.18")) float DurationSeconds = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|时序", meta=(ClampMin="0.05", ClampMax="0.4")) float GhostLifetimeSeconds = 0.22f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|外观", meta=(ClampMin="1", ClampMax="5")) int32 GhostCount = 3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|外观", meta=(ClampMin="0.1", ClampMax="3.0")) float StrongBeatMultiplier = 1.35f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|镜头", meta=(ClampMin="0.0", ClampMax="8.0")) float FOVExpansionDegrees = 3.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|镜头", meta=(ClampMin="0.0", ClampMax="0.5")) float CameraShakeDegrees = 0.18f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|镜头", meta=(ClampMin="0.0", ClampMax="30.0")) float CameraLateralShiftCm = 14.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|镜头", meta=(ClampMin="0.0", ClampMax="20.0")) float CameraDollyShiftCm = 7.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|镜头", meta=(ClampMin="0.0", ClampMax="2.0")) float CameraYawKickDegrees = 0.72f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|镜头", meta=(ClampMin="-1.0", ClampMax="1.0")) float CameraPitchKickDegrees = -0.22f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|镜头", meta=(ClampMin="0.0", ClampMax="0.35")) float CameraReboundStrength = 0.16f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|镜头", meta=(ClampMin="0.0", ClampMax="1.0")) float ObstructedStrength = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="节拍裂闪|无障碍") EPerfectDodgeAccessibility Accessibility = EPerfectDodgeAccessibility::Full;
};
