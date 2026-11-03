// 管理移动动画与全身动作混合，输入判定由战斗组件维护
#pragma once

#include "Animation/AnimInstance.h"
#include "PlayerCombatAnimInstance.generated.h"

// 行走与全身动作槽统一混合，输入判定仍由战斗组件负责

UCLASS(Transient)

class HEARBLADE_API UPlayerCombatAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
  public:
	UAnimMontage* GetDefenseMontage() const
	{
		return VisualAction;
	}

	UPlayerCombatAnimInstance();
	UPROPERTY(Transient) TObjectPtr<class UBlendSpace> Locomotion;
	UPROPERTY(Transient) TObjectPtr<class UBlendSpace> DirectionalHit;
	FVector LocomotionPosition = FVector::ZeroVector;
	// 节拍姿态只改变动画显示，不移动胶囊或修改角色移动速度


	float LocomotionPlayRate = 1.0f;
	// 角色每帧传入实际速度与动画参考速度的比例
	float SpeedMatchPlayRate = 1.0f;
	float BeatPoseAlpha = 0.0f;
	FVector BeatPelvisTranslation = FVector::ZeroVector;
	FRotator BeatSpineRotation = FRotator::ZeroRotator;
	// 播放闪避或防御动作，设置循环和衔接时间
	void PlayCombatAction(class UAnimSequence* Sequence, bool bLooping, float Duration,
	                      float ChainGrace = .10f);
	// 创建并播放攻击蒙太奇，让攻击组件接管动画位置
	class UAnimMontage* BeginBeatAttack(class UAnimSequence* Sequence, float StartPosition,
	                                    float BlendInSeconds = .10f);
	// 淡出指定攻击蒙太奇，释放攻击姿态
	void EndBeatAttack(class UAnimMontage* Montage, float BlendOutSeconds = .08f);
	// 提前淡出防御或闪避动作，回到行走姿态
	void ReleaseCombatAction(float BlendOutSeconds);
	// 记录受击方向和时间，启动方向姿态混合
	void StartDirectionalHit(FVector2D ReactionDirection, float Duration);
	// 结束受击姿态，回到当前基础动作
	void EndDirectionalHit();

	bool IsDirectionalHitActive() const
	{
		return bDirectionalHitActive;
	}

	// 更新本帧移动参数和战斗姿态混合状态
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

  protected:
	// 创建行走、战斗动作槽与受击混合的原生动画图
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	// 释放原生动画图代理
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

  private:
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> VisualAction;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> BeatAttackMontage;
	double ReleaseAtWorldSeconds = -1.0;
	FVector HitBlendPosition = FVector(1.0, 0.0, 0.0);
	float HitBlendAlpha = 0.0f;
	int32 HitReactionSerial = 0;
	bool bDirectionalHitActive = false;
	double HitReactionEndWorldSeconds = -1.0;
	float SmoothedBeatPoseAlpha = 0.0f;
	float SmoothedLocomotionPlayRate = 1.0f;
	friend struct FPlayerCombatAnimProxy;
};
