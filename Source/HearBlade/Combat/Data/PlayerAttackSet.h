// 定义主角轻重攻击节点、连招分支和节拍时间计算接口
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PlayerAttackSet.generated.h"

UENUM(BlueprintType)
enum class EPlayerAttackInput : uint8
{
	Light,
	Heavy
};
UENUM(BlueprintType)
enum class EPlayerAttackGrade : uint8
{
	Perfect,
	Good,
	Bad
};
UENUM(BlueprintType)
enum class EPlayerResonanceState : uint8
{
	Filling,
	Ready,
	Armed,
	Active
};

USTRUCT(BlueprintType)

struct FPlayerAttackNode
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EPlayerAttackInput Input = EPlayerAttackInput::Light;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<class UAnimSequence> Animation;
	// 以下片段位置均为源动画秒数；与音乐时间及 DurationBeats 的拍数分开
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ClipStart = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ImpactTime = .2f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StrikeStart = .1f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float LinkTime = .35f;
	// 挥击段的目标播放时长，单位为秒
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StrikeSeconds = .12f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ClipEnd = .4f;
	// 节点标称拍数，目前用于长重击迟到宽限等判断
	// 普通连招实际间隔由 StartAttack 固定为一拍，共鸣期间为半拍
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float DurationBeats = .5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Damage = 16.f;
	// 对敌人韧性造成的损耗，与生命伤害分开结算
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float PoiseDamage = 12.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float AdvanceCm = 40.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bFinisher = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bResonanceOnly = false;
	// 踢击额外检测双腿经过的轨迹，剑的轨迹仍参与检测
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bKick = false;
	// 数据中的挑飞标记；当前命中逻辑还按 HHH 节点名称判断挑飞
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bUppercut = false;
	// 轻、重输入对应的后继节点标识；空标识表示没有该分支
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName NextLight;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName NextHeavy;
};

UCLASS(BlueprintType)

class HEARBLADE_API UPlayerAttackSet : public UDataAsset
{
	GENERATED_BODY()
  public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FPlayerAttackNode> Nodes;
	const FPlayerAttackNode* Find(FName Id) const;
	bool Validate(FString& Error) const;
};

namespace PlayerAttackTiming
{
// 普通接续判断的提前和迟到范围，单位为秒
// 出招期间的第一条缓存输入可以更早进入，实际执行仍等待接续拍点
inline constexpr double ComboQueueEarlySeconds = .30;
inline constexpr double ComboQueueLateSeconds = .18;
HEARBLADE_API EPlayerAttackGrade Judge(double ErrorSeconds);
HEARBLADE_API double CadenceBeats(EPlayerAttackInput Input, bool Resonance);
HEARBLADE_API double ScheduleImpact(double CandidateBeat, double PreviousImpactBeat, double Cadence);
HEARBLADE_API bool CanQueue(double InputAudio, double ImpactAudio, bool AlreadyQueued);
HEARBLADE_API float ClipPosition(double AudioNow, double ImpactAudio, float ClipStart, float ImpactTime,
                                 float ClipEnd, float PlayRate);
HEARBLADE_API float ContinuousComboPosition(double AudioNow, double StartAudio, double ImpactAudio,
                                            double EndAudio, const FPlayerAttackNode& Node);
HEARBLADE_API bool IsDefenseInputWindow(double SecondsUntilImpact, double IdealLeadSeconds);
// 拍点安排与原始输入评级分开，辅助函数不替代 StartAttack 的实际起手规则
HEARBLADE_API double NextImpactBeat(double CurrentBeat, double SecondsPerBeat, float Windup, bool Heavy);
}
