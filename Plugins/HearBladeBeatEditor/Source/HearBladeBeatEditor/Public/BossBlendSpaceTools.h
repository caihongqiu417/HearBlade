// 提供骨架关联与混合空间重建的编辑器工具
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "BossBlendSpaceTools.generated.h"

class UBlendSpace;

// 提供编辑器脚本调用的骨架修正、混合空间重建与动画校准工具
UCLASS()

class HEARBLADEBEATEDITOR_API UBossBlendSpaceTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

  public:
	// 脚本添加采样点后，重建混合空间的三角形连接
	UFUNCTION(BlueprintCallable, Category = "HearBlade|Boss Animation")
	static int32 RebuildBlendSpace(UBlendSpace* BlendSpace);
	// 为骨骼结构兼容的动画指定目标骨架
	UFUNCTION(BlueprintCallable, Category = "HearBlade|Boss Animation")
	static bool AssignIdenticalSkeleton(class UAnimSequence* Sequence, class USkeleton* Target);
	// 保留动画旋转，将非根骨骼平移校正为参考骨架长度
	UFUNCTION(BlueprintCallable, Category = "HearBlade|Player Animation")
	static bool NormalizePlayerAttackBoneLengths(class UAnimSequence* Sequence);
};
