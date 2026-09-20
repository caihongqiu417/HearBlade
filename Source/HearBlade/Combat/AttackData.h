// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimMontage.h"
#include "Engine/DataAsset.h"
#include "AttackData.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class ERhythmResponseType : uint8
{
	Dodge UMETA(DisplayName="闪避"),
	Block UMETA(DisplayName="格挡"),
	Counter UMETA(DisplayName="反击"),
	HoldBlock UMETA(DisplayName="持续格挡")
};

UENUM(BlueprintType)
enum class ERhythmHoldMarkerType : uint8
{
	None UMETA(DisplayName="非长按"),
	Start UMETA(DisplayName="长按开始"),
	Continue UMETA(DisplayName="长按持续"),
	End UMETA(DisplayName="长按结束")
};

USTRUCT(BlueprintType)
struct FImpactCinematicSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="关键慢放")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="关键慢放", meta=(ClampMin="0.35", ClampMax="1.0", EditCondition="bEnabled"))
	float MusicAndBossRate = .88f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="关键慢放", meta=(ClampMin="0.0", ClampMax="2.0", EditCondition="bEnabled"))
	float LeadBeats = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="关键慢放", meta=(ClampMin="0.0", ClampMax="1.0", EditCondition="bEnabled"))
	float HoldAfterImpactBeats = .16f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="关键慢放", meta=(ClampMin="0.02", ClampMax="1.0", EditCondition="bEnabled"))
	float RecoverySeconds = .14f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="关键慢放", meta=(ClampMin="0.0", ClampMax="1.0", EditCondition="bEnabled"))
	float TriggerChance = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="关键慢放", meta=(ClampMin="0.25", ClampMax="2.0", EditCondition="bEnabled"))
	float CameraStrength = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="关键慢放", meta=(ClampMin="0.2", ClampMax="1.0", EditCondition="bEnabled"))
	float ClashActorRate = .48f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="关键慢放", meta=(ClampMin="0.03", ClampMax="0.3", EditCondition="bEnabled"))
	float ClashActorSlowSeconds = .11f;
};

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|防御指令")
	ERhythmResponseType RequiredResponse = ERhythmResponseType::Dodge;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|防御指令", meta=(ClampMin="0.0", ClampMax="8.0"))
	float PromptLeadBeats = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|防御指令")
	bool bForceStrongAccent = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|防御指令")
	TObjectPtr<UTexture2D> PromptIconOverride = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|防御指令", meta=(ClampMin="0.4", ClampMax="2.0", EditCondition="PromptIconOverride != nullptr"))
	float PromptIconScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|防御指令", meta=(EditCondition="PromptIconOverride != nullptr"))
	FLinearColor PromptIconColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|持续格挡", meta=(EditCondition="RequiredResponse == ERhythmResponseType::HoldBlock"))
	FName HoldGroup = TEXT("HoldA");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|持续格挡", meta=(EditCondition="RequiredResponse == ERhythmResponseType::HoldBlock"))
	ERhythmHoldMarkerType HoldMarker = ERhythmHoldMarkerType::None;

	// Optional authored accent for selected impacts. Timing still uses BeatOffset;
	// slowing the musical timeline cannot move this hit away from its target beat.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|关键慢放")
	FImpactCinematicSettings Cinematic;
};

// One runtime instruction used by a response pattern. Timing and damage still
// come from the authored impact marker; only the player's required response is
// replaced when this pattern is selected.
USTRUCT(BlueprintType)
struct FAttackResponseInstruction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|随机指令")
	ERhythmResponseType RequiredResponse = ERhythmResponseType::Block;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|随机指令",
		meta=(EditCondition="RequiredResponse == ERhythmResponseType::HoldBlock"))
	ERhythmHoldMarkerType HoldMarker = ERhythmHoldMarkerType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|随机指令",
		meta=(EditCondition="RequiredResponse == ERhythmResponseType::HoldBlock"))
	FName HoldGroup = TEXT("HoldA");
};

// A weighted command sequence selected once when an attack is queued. This
// keeps the HUD prompt and the damage judgment on the same locked sequence.
USTRUCT(BlueprintType)
struct FAttackResponsePattern
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|随机指令")
	FName PatternName = TEXT("Pattern");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|随机指令", meta=(ClampMin="0.0"))
	float Weight = 1.0f;

	// Zero based index of the first impact replaced by Instructions.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|随机指令", meta=(ClampMin="0"))
	int32 FirstImpactIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|随机指令")
	TArray<FAttackResponseInstruction> Instructions;
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

	// Empty keeps the authored marker responses. Otherwise one valid pattern is
	// selected per attack using Weight. Animation timing is never changed.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss攻击数据|随机指令")
	TArray<FAttackResponsePattern> ResponsePatterns;

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
