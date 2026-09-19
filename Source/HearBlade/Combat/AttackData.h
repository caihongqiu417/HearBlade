// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimMontage.h"
#include "Engine/DataAsset.h"
#include "AttackData.generated.h"

// 一段独立攻击中的一次命中。BeatOffset 相对第一击，第一击填 0。
USTRUCT(BlueprintType)
struct FAttackImpactMarker
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据", meta = (ClampMin = "0.0"))
	float AnimationTimeSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据", meta = (ClampMin = "0"))
	double BeatOffset = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|伤害", meta = (ClampMin = "0.0"))
	float Damage = 18.0f;

	// The intended dodge input happens this many seconds before blade contact.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|闪避判定", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float IdealDodgeLeadSeconds = 0.14f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|伤害", meta = (ClampMin = "0.01", ClampMax = "0.3"))
	float DamageWindowBeforeSeconds = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|伤害", meta = (ClampMin = "0.01", ClampMax = "0.3"))
	float DamageWindowAfterSeconds = 0.10f;
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
	// 行为树可独立调度的一段攻击动画
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据")
	TObjectPtr<UAnimMontage> AttackMontage = nullptr;

	// 独立动作的有效片段。End=0 表示播放到 Montage 末尾，可去掉多余的收剑段。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据", meta = (ClampMin = "0.0"))
	float ClipStartSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据", meta = (ClampMin = "0.0"))
	float ClipEndSeconds = 0.0f;

	float GetClipEndSeconds() const;

	// 动画开始到命中的时间
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据", meta = (ClampMin = "0.0"))
	float AuthoredImpactTime = 0.0f;

	// 按动画时间升序排列；第一个标记决定这一段动作的启动时间。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据")
	TArray<FAttackImpactMarker> ImpactMarkers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据", meta = (ClampMin = "0.01"))
	float PlayRate = 1.0f;

	// 可选：起手→第一击、各相邻命中、最后一击→收招的逐段速度。留空时整段使用 PlayRate。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据")
	TArray<float> SegmentPlayRates;

	// Use UE Motion Warping during the lead-in to each configured impact.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|Motion Warping")
	bool bWarpToTarget = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|Motion Warping", meta=(ClampMin="0.05"))
	float WarpLeadAnimationSeconds = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|Motion Warping", meta=(ClampMin="0.0"))
	float WarpStopDistance = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|Motion Warping", meta=(ClampMin="0.0"))
	float MaxWarpTravelPerStrike = 250.0f;

	float GetRateAtAnimationTime(float AnimationTime) const;
	float GetElapsedAtAnimationTime(float AnimationTime) const;
	float GetAnimationTimeAtElapsed(float ElapsedSeconds) const;

	// 音乐拍线参数由编辑器写入，测试音乐时钟也从这里读取，保证预览与运行时一致。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "节拍编辑器", meta = (ClampMin = "1.0"))
	double ReferenceBPM = 104.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "节拍编辑器")
	double ReferenceBeatPhaseSeconds = 0.204;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "节拍编辑器", meta = (ClampMin = "0.0"))
	double ReferenceMusicStartSeconds = 4.0;

	// 0 = any beat; 1 = first beat of each bar. The runtime uses the music clock's beats-per-bar.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "节拍编辑器|调度", meta = (ClampMin = "0"))
	int32 FirstImpactBeatInBar = 1;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "节拍编辑器")
	FString MusicSourceWavPath;

	UPROPERTY(EditAnywhere, Category = "节拍编辑器", meta = (ClampMin = "0.01"))
	float MinimumPlayRate = 1.0f;

	UPROPERTY(EditAnywhere, Category = "节拍编辑器", meta = (ClampMin = "0.01"))
	float MaximumPlayRate = 1.15f;
#endif
};
