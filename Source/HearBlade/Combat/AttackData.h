// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimMontage.h"
#include "Engine/DataAsset.h"
#include "AttackData.generated.h"

// 完整连招中的一次命中。BeatOffset 相对第一击，第一击填 0。
USTRUCT(BlueprintType)
struct FAttackImpactMarker
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据", meta = (ClampMin = "0.0"))
	float AnimationTimeSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据", meta = (ClampMin = "0"))
	double BeatOffset = 0.0;
};

// ============================================================
// Boss 攻击数据资产
// ============================================================
// 保存 Boss 攻击动画和动画原本的命中时间。
// 本类只保存数据，不播放动画，不检测伤害。
UCLASS(BlueprintType)
class HEARBLADE_API UAttackDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Boss 要播放的攻击动画
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据")
	TObjectPtr<UAnimMontage> AttackMontage = nullptr;

	// 动画开始到命中的时间
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据", meta = (ClampMin = "0.0"))
	float AuthoredImpactTime = 0.0f;

	// 按动画时间升序排列；第一个标记决定整段 Montage 的启动时间。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据")
	TArray<FAttackImpactMarker> ImpactMarkers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据", meta = (ClampMin = "0.01"))
	float PlayRate = 1.0f;
};
