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

	// 招式身份与动画
	// 节点身份与使用的动画资源
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EPlayerAttackInput Input = EPlayerAttackInput::Light;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<class UAnimSequence> Animation;

	// 源动画中的时间位置
	// 以下片段位置均为源动画秒数；与音乐时间及 DurationBeats 的拍数分开
	// 源动画片段起点，单位为秒，实际起手可能由连续动作衔接调整
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ClipStart = 0.f;
	// 源动画中的命中姿态位置，攻击组件把这个姿态对齐目标音乐拍点
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ImpactTime = .2f;
	// 源动画中开始快速挥击的位置，用于计算起手预留时间
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StrikeStart = .1f;
	// 当前用于配置合法性检查，实际接招时刻由攻击组件安排
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float LinkTime = .35f;
	// 当前只检查该值是否有效，实际挥击时长由攻击组件计算
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StrikeSeconds = .12f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ClipEnd = .4f;
	// 节点标称拍数，目前用于长重击迟到宽限等判断
	// 普通连招实际间隔由 StartAttack 固定为一拍，共鸣期间为半拍
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float DurationBeats = .5f;

	// 命中数值与招式标记
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Damage = 16.f;
	// 对敌人韧性造成的损耗，与生命伤害分开结算
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float PoiseDamage = 12.f;
	// 保留的前进距离配置，当前攻击位移使用动画提取的根位移
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float AdvanceCm = 40.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bFinisher = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bResonanceOnly = false;
	// 踢击额外检测双腿经过的轨迹，剑的轨迹仍参与检测
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bKick = false;
	// 数据中的挑飞标记；当前命中逻辑还按 HHH 节点名称判断挑飞
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bUppercut = false;

	// 轻重击后继分支
	// 轻、重输入对应的后继节点标识；空标识表示没有该分支
	// 轻击接续节点的 Id，例如 L 的下一次轻击可指向 LL
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName NextLight;
	// 重击接续节点的 Id，例如 L 的下一次重击可指向 LH
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName NextHeavy;
};

// 保存玩家攻击节点与轻重击分支，提供节点查询和配置检查
UCLASS(BlueprintType)

class HEARBLADE_API UPlayerAttackSet : public UDataAsset
{
	GENERATED_BODY()
  public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FPlayerAttackNode> Nodes;
	// 按标识查找节点，找不到时返回空指针
	const FPlayerAttackNode* Find(FName Id) const;
	// 检查节点、动画片段和分支引用，错误原因写入 Error
	bool Validate(FString& Error) const;
};

namespace PlayerAttackTiming
{
// 普通接续判断的提前和迟到范围，单位为秒
// 出招期间的第一条缓存输入可以更早进入，实际执行仍等待接续拍点
inline constexpr double ComboQueueEarlySeconds = .30;
inline constexpr double ComboQueueLateSeconds = .18;
// 按输入时间误差返回攻击评级
HEARBLADE_API EPlayerAttackGrade Judge(double ErrorSeconds);
HEARBLADE_API double CadenceBeats(EPlayerAttackInput Input, bool Resonance);
HEARBLADE_API double ScheduleImpact(double CandidateBeat, double PreviousImpactBeat, double Cadence);
HEARBLADE_API bool CanQueue(double InputAudio, double ImpactAudio, bool AlreadyQueued);
HEARBLADE_API float ClipPosition(double AudioNow, double ImpactAudio, float ClipStart, float ImpactTime,
                                 float ClipEnd, float PlayRate);
// 把起手、命中和收招时间映射到动画片段，保证关键姿态按时到达
HEARBLADE_API float ContinuousComboPosition(double AudioNow, double StartAudio, double ImpactAudio,
                                            double EndAudio, const FPlayerAttackNode& Node);
HEARBLADE_API bool IsDefenseInputWindow(double SecondsUntilImpact, double IdealLeadSeconds);
// 拍点安排与原始输入评级分开，辅助函数不替代 StartAttack 的实际起手规则
HEARBLADE_API double NextImpactBeat(double CurrentBeat, double SecondsPerBeat, float Windup, bool Heavy);
}
