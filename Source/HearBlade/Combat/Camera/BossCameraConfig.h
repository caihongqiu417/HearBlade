// 配置锁定距离、镜头构图与战斗反馈参数
#pragma once

#include "Engine/DataAsset.h"
#include "BossCameraConfig.generated.h"

// 保存锁定、构图和战斗镜头反馈参数，供镜头组件读取
UCLASS(BlueprintType)

class HEARBLADE_API UBossCameraConfig : public UDataAsset
{
	GENERATED_BODY()
  public:
	// 锁定距离以厘米为单位；释放距离大于获取距离，用于减少边界反复切换
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lock") float AcquireDistance = 2000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lock") float ReleaseDistance = 2500.f;
	// 释放等待时间与遮挡容忍时间均以秒为单位
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lock") float ReleaseDelay = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lock") float OcclusionGrace = 1.f;
	// 镜头臂长度与构图偏移以厘米为单位，视野和俯仰角以度为单位
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Framing") float FreeArmLength = 400.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Framing") float MaxCombatArmLength = 650.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Framing") float FreeFOV = 90.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Framing") float MaxCombatFOV = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Framing") float RotationSpeed = 6.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Framing") float DistanceSpeed = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Framing") float WarningLandingWeight = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Framing") float CombatPivotHeight = 75.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Framing") float CombatShoulderOffset = 55.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Framing")
	float CombatArmLengthAtCloseRange = 440.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Framing") float CloseRangeFOV = 90.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Framing") float BossFocusWeight = 0.52f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Framing") float MaxCloseRangeUpwardPitch = 6.f;
	// 瞬移与命中的镜头反馈，时长为秒，视角变化为度
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback") float BlinkFOVKick = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback") float BlinkPushSeconds = 0.08f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback") float BlinkHoldSeconds = 0.05f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback") float BlinkRecoverSeconds = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback") float BlinkShakeDegrees = 0.55f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback") float HitShakeDegrees = 1.1f;
};
