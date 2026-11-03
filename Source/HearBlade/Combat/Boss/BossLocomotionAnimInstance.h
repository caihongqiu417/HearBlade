// 更新敌人移动动画和方向性受击表现
#pragma once

#include "Animation/AnimInstance.h"
#include "BossLocomotionAnimInstance.generated.h"

// 供动画图混合待机与行走的地面速度
UCLASS(Transient, Blueprintable)

class HEARBLADE_API UBossLocomotionAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

  public:
	// 触发不打断整套攻击的轻微护甲受击姿态
	void PlayArmorFlinch(FVector2D Direction, float Strength);
	FRotator ArmorFlinchRotation = FRotator::ZeroRotator;
	double ArmorFlinchStart = -1000.;
	FVector2D ArmorFlinchDirection = FVector2D::ZeroVector;
	float ArmorFlinchStrength = 0.f;
	UBossLocomotionAnimInstance();

	UPROPERTY(Transient)
	TObjectPtr<class UBlendSpace> Locomotion;

	UPROPERTY(Transient)
	TObjectPtr<class UBlendSpace> DirectionalHit;

	UPROPERTY(BlueprintReadOnly, Category = "Boss Locomotion")
	float GroundSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Boss Locomotion")
	bool bIsMoving = false;

	// 八方向混合空间使用的角色局部移动方向
	UPROPERTY(BlueprintReadOnly, Category = "Boss Locomotion")
	float MoveForward = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Boss Locomotion")
	float MoveRight = 0.0f;

	// 正前方为零，向右每隔 45 度划分一个方向
	UPROPERTY(BlueprintReadOnly, Category = "Boss Locomotion")
	int32 MoveDirectionIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Boss Locomotion")
	bool bDirectionBit0 = false;

	UPROPERTY(BlueprintReadOnly, Category = "Boss Locomotion")
	bool bDirectionBit1 = false;

	UPROPERTY(BlueprintReadOnly, Category = "Boss Locomotion")
	bool bDirectionBit2 = false;

	void SetLocomotion(float NewSpeed, const FVector& LocalDirection, float DeltaSeconds)
	{
		GroundSpeed = FMath::Max(0.0f, NewSpeed);
		bIsMoving = GroundSpeed > 5.0f;
		const FVector Desired = bIsMoving ? LocalDirection.GetSafeNormal2D() : FVector::ZeroVector;
		const FVector Current(MoveForward, MoveRight, 0.0f);
		const FVector Smoothed =
		    bIsMoving ? FMath::VInterpTo(Current, Desired, DeltaSeconds, 8.0f) : FVector::ZeroVector;
		MoveForward = Smoothed.X;
		MoveRight = Smoothed.Y;
		const float Angle = FMath::RadiansToDegrees(FMath::Atan2(MoveRight, MoveForward));
		MoveDirectionIndex = (FMath::RoundToInt(Angle / 45.0f) + 8) % 8;
		bDirectionBit0 = (MoveDirectionIndex & 1) != 0;
		bDirectionBit1 = (MoveDirectionIndex & 2) != 0;
		bDirectionBit2 = (MoveDirectionIndex & 4) != 0;
	}

	// 保存受击方向与时长，开始混合受击姿态
	void StartDirectionalHit(const FVector2D& ReactionDirection, float Duration);
	// 结束方向受击，让姿态回到基础动作
	void EndDirectionalHit();

	bool IsDirectionalHitActive() const
	{
		return bDirectionalHitActive;
	}

	// 更新行走与受击混合使用的参数
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

  protected:
	// 创建原生动画图，连接行走、攻击与受击姿态
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	// 释放原生动画图代理
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

  private:
	FVector HitBlendPosition = FVector(-1.0, 0.0, 0.0);
	float HitBlendAlpha = 0.0f;
	int32 HitReactionSerial = 0;
	bool bDirectionalHitActive = false;
	double HitReactionEndWorldSeconds = -1.0;
	friend struct FBossCombatAnimProxy;
};
