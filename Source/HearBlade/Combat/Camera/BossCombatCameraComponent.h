// 维护战斗目标锁定、镜头构图及攻击反馈
#pragma once

#include "Components/ActorComponent.h"
#include "BossCombatCameraComponent.generated.h"

class UBossCameraConfig;
class UBossCameraModifier;
class UBeatSyncedComboComponent;
class ABossBeatDirector;

// 管理目标锁定和战斗构图，监听攻击事件并驱动镜头反馈
UCLASS(ClassGroup = (Camera), meta = (BlueprintSpawnableComponent))

class HEARBLADE_API UBossCombatCameraComponent : public UActorComponent
{
	GENERATED_BODY()
  public:
	UBossCombatCameraComponent();

	void SetPhasePresentation(bool bEnabled)
	{
		bPhasePresentation = bEnabled;
		CancelFeedback();
	}

	bool bPhasePresentation = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Camera")
	TObjectPtr<UBossCameraConfig> Config;
	// 切换锁定目标，并绑定新目标的战斗事件
	// 取消目标锁定并解除敌人事件绑定
	UFUNCTION(BlueprintCallable, Category = "Combat Camera") void SetCombatTarget(AActor* NewTarget);
	// 已有目标时取消锁定，否则寻找可锁定敌人
	UFUNCTION(BlueprintCallable, Category = "Combat Camera") void ClearCombatTarget();
	UFUNCTION(BlueprintCallable, Category = "Combat Camera") void ToggleLockOn();

	UFUNCTION(BlueprintPure, Category = "Combat Camera") bool IsLockedOn() const
	{
		return Target.IsValid();
	}

	UFUNCTION(BlueprintPure, Category = "Combat Camera") AActor* GetCombatTarget() const
	{
		return Target.Get();
	}

	// 把视角输入交给当前自由观察或锁定镜头流程
	void AddLookInput(float Yaw, float Pitch);
	// 根据闪避结果和攻击方向设置镜头冲击参数
	void TriggerPerfectDodgeFeedback(float FOVExpansionDegrees, float ShakeDegrees, float Strength,
	                                 FVector BladeWorldLocation, FVector AttackDirection, bool bStrongBeat,
	                                 float LateralShiftCm, float DollyShiftCm, float YawKickDegrees,
	                                 float PitchKickDegrees, float ReboundStrength);
	// 为成功闪避选择镜头反馈强度
	void TriggerDodgeSuccessFeedback(bool bPerfect, FVector BladeWorldLocation, FVector AttackDirection,
	                                 bool bStrongBeat);
	// 为反击成功设置镜头反馈
	void TriggerCounterSuccessFeedback(bool bPerfect, FVector BladeWorldLocation, FVector AttackDirection,
	                                   bool bStrongBeat);
	// 踢击收尾时拉近并倾斜镜头，停顿结束后回弹恢复

	void TriggerKickAccent(float FOVKickDegrees, float RollDegrees, float PitchDipDegrees, float ShakeDegrees,
	                       FVector ContactWorld, float HoldSeconds);
	// 第三段重击挑飞时，让镜头短暂向上响应
	void TriggerUppercutAccent(float Strength);
	// 挑飞时切入侧面近景，跟随身体上升，再平滑回到战斗镜头

	void TriggerLaunchShot(AActor* Subject, float Seconds);
	// 根据漏判或失误减弱、调整当前镜头反馈
	void TriggerFailureFeedback(bool bMiss);
	// 开始持续格挡的轻微镜头震动
	void StartHoldFeedback(float ShakeDegrees);
	// 为持续格挡中的一次接触叠加短促震动
	void TriggerHoldImpact(float ShakeDegrees, float DurationSeconds);
	// 逐渐停止持续格挡震动
	void StopHoldFeedback(float FadeSeconds);
	// 获取玩家镜头并准备战斗镜头修饰器
	virtual void BeginPlay() override;
	// 退出关卡时解除事件并清除镜头反馈
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// 更新锁定方向与距离，再推进镜头反馈和特写
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

  private:
	// 收到攻击预警后更新镜头关注位置
	// 响应 Boss 瞬移，更新镜头反馈
	UFUNCTION() void HandleWarning(FVector Landing, double TargetAudioSeconds);
	// 攻击取消时停止对应镜头表现
	UFUNCTION() void HandleBlink(FVector Landing, double TargetAudioSeconds);
	// 实际命中确认后触发镜头反馈
	UFUNCTION() void HandleCancelled();
	// 进入交锋慢放时调整镜头表现
	UFUNCTION() void HandleConfirmedHit();
	// 在实际交锋位置触发镜头冲击
	UFUNCTION() void HandleCinematicStarted(float Strength, bool bStrongBeat);
	// 交锋演出结束后恢复镜头反馈状态
	UFUNCTION() void HandleCinematicImpact(float Strength, bool bStrongBeat, FVector ClashLocation);
	// 更换敌人事件绑定，接收预警、瞬移和交锋通知
	UFUNCTION() void HandleCinematicEnded();
	void BindCombo(UBeatSyncedComboComponent* Combo);
	// 清除当前镜头冲击和特写
	void CancelFeedback();
	// 优先使用配置资源，没有资源时使用默认配置
	const UBossCameraConfig* Settings() const;
	TWeakObjectPtr<AActor> Target;
	TWeakObjectPtr<ABossBeatDirector> Director;
	TWeakObjectPtr<UBeatSyncedComboComponent> BoundCombo;
	TWeakObjectPtr<UBossCameraModifier> Modifier;
	FVector WarningLanding = FVector::ZeroVector;
	bool bWarning = false;
	bool bManualSuppression = false;
	float OutOfRangeTime = 0.f;
	float OccludedTime = 0.f;
	float ManualYaw = 0.f;
	float ManualPitch = 0.f;
	float LastLookInputTime = -100.f;
	// 跟随挑飞目标更新特写位置，并平滑回到普通镜头
	void TickLaunchShot(float DeltaTime);
	TWeakObjectPtr<AActor> ShotSubject;
	double ShotStartWorld = -1000.0;
	float ShotSeconds = 0.f, ShotSide = 1.f;
	bool bShotInit = false;
	FVector ShotSmoothedLoc = FVector::ZeroVector;
	FRotator ShotSmoothedRot = FRotator::ZeroRotator;
	FVector CinematicFocusLocation = FVector::ZeroVector;
	double CinematicFocusEndWorldSeconds = -1000.0;
};
