#pragma once

#include "Engine/DataAsset.h"
#include "BossCameraConfig.generated.h"

UCLASS(BlueprintType)
class HEARBLADE_API UBossCameraConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lock") float AcquireDistance = 2000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lock") float ReleaseDistance = 2500.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lock") float ReleaseDelay = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lock") float OcclusionGrace = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing") float FreeArmLength = 400.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing") float MaxCombatArmLength = 650.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing") float FreeFOV = 90.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing") float MaxCombatFOV = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing") float RotationSpeed = 6.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing") float DistanceSpeed = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing") float WarningLandingWeight = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing") float CombatPivotHeight = 75.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing") float CombatShoulderOffset = 55.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing") float CombatArmLengthAtCloseRange = 440.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing") float CloseRangeFOV = 90.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing") float BossFocusWeight = 0.52f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing") float MaxCloseRangeUpwardPitch = 6.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Feedback") float BlinkFOVKick = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Feedback") float BlinkPushSeconds = 0.08f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Feedback") float BlinkHoldSeconds = 0.05f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Feedback") float BlinkRecoverSeconds = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Feedback") float BlinkShakeDegrees = 0.55f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Feedback") float HitShakeDegrees = 1.1f;
};
