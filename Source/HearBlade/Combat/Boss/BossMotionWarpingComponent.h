// 为非 Character 敌人提供根位移修正所需的对象适配
#pragma once

#include "CoreMinimal.h"
#include "MotionWarpingAdapter.h"
#include "MotionWarpingComponent.h"
#include "BossMotionWarpingComponent.generated.h"

class USkeletalMeshComponent;

// Boss 不是 Character，通过适配器向动作对齐组件提供角色和网格信息
UCLASS()

class HEARBLADE_API UBossMotionWarpingAdapter : public UMotionWarpingBaseAdapter
{
	GENERATED_BODY()

  public:
	// 向动作对齐适配器提供 Boss 与骨骼网格
	void SetBoss(AActor* InBoss, USkeletalMeshComponent* InMesh);
	virtual AActor* GetActor() const override;
	virtual USkeletalMeshComponent* GetMesh() const override;
	// 返回网格根部位置，作为动作对齐的视觉参考
	virtual FVector GetVisualRootLocation() const override;
	// 提供网格相对角色的基础位置偏移
	virtual FVector GetBaseVisualTranslationOffset() const override;
	// 提供网格相对角色的基础旋转偏移
	virtual FQuat GetBaseVisualRotationOffset() const override;

  private:
	TWeakObjectPtr<AActor> Boss;
	TWeakObjectPtr<USkeletalMeshComponent> Mesh;
};

// 接收每帧提取的根位移，并按攻击目标修正移动方向和距离
UCLASS()

class HEARBLADE_API UBossMotionWarpingComponent : public UMotionWarpingComponent
{
	GENERATED_BODY()

  public:
	// 把动画根位移交给动作对齐处理，返回修正后的位移
	FTransform WarpRootMotion(const FTransform& LocalDelta, float DeltaSeconds, const UAnimMontage* Montage,
	                          float PreviousPosition, float CurrentPosition, float PlayRate);
};
