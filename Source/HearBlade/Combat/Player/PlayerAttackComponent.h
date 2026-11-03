// 管理主角攻击输入、连招状态、动画推进与接触检测
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/Data/PlayerAttackSet.h"
#include "PlayerAttackComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPlayerAttackJudged, EPlayerAttackGrade, Grade, float, ErrorMs,
                                               FName, Attack);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPlayerAttackHit, AActor*, Target, float, Damage,
                                               EPlayerAttackGrade, Grade);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPlayerResonanceChanged, EPlayerResonanceState, State);

// 记录玩家在 Boss 破绽期间的命中，供界面显示
struct FOpeningHitMark
{
	double Audio = 0.;
	bool bGap = false;
};

/**
 * 玩家攻击：轻重击、连招、攻击位移、命中和共鸣
 * RequestAttack 接收按键，StartAttack 安排拍点，TickAttack 推进动作，SweepBlade 检查命中
 * 连招缓存保留第一次选择，普通起手缓存等待恢复；防御请求由玩家防御组件处理
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent, DisplayName = "玩家攻击"))

class HEARBLADE_API UPlayerAttackComponent : public UActorComponent
{
	GENERATED_BODY()
  public:
	UPlayerAttackComponent();

	// 攻击配置
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player Attack")
	TObjectPtr<UPlayerAttackSet> AttackSet;

	// 控制普通重击是否可用，右键在防御窗口内仍用于格挡
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Attack") bool bHeavyAttacksEnabled = true;

	// 界面状态：当前招式、评级与共鸣
	UPROPERTY(BlueprintReadOnly) EPlayerResonanceState ResonanceState = EPlayerResonanceState::Filling;
	UPROPERTY(BlueprintReadOnly) EPlayerAttackGrade LastGrade = EPlayerAttackGrade::Bad;
	UPROPERTY(BlueprintReadOnly) float LastErrorMs = 0.f;
	UPROPERTY(BlueprintReadOnly) FString InputSequence;
	UPROPERTY(BlueprintReadOnly) FName CurrentAttack;
	UPROPERTY(BlueprintReadOnly) double LastJudgedWorldSeconds = -1000.;
	UPROPERTY(BlueprintReadOnly) float LastResonanceGain = 0.f;
	UPROPERTY(BlueprintReadOnly) double LastHitWorldSeconds = -1000.;

	// 命中表现数据
	// 供界面读取的命中表现，使用游戏时间控制显示时长
	FVector ImpactFxWorld = FVector::ZeroVector;
	double ImpactFxWorldTime = -1000.;
	FString ImpactFxWord;
	float ImpactFxPower = 1.f;
	bool bImpactFxHeavy = false, bImpactFxFrame = false, bImpactFxPerfect = false;
	bool bImpactFxLaunch = false; // 第三段重击 HHH 触发挑飞表现
	// 记录破绽命中与填满破绽的表现
	// 伤害倍率由接触时的破绽类型决定，不修改普通攻击的节奏评级
	TArray<FOpeningHitMark> OpeningHitMarks;
	double FillWorldTime = -1000.;
	double FillAudio = 0.;
	bool bFillGap = false;
	double LastOpeningHitWorldTime = -1000.;
	bool bLastOpeningHitGap = false;
	// 剑尖命中爆发的位置与显示时间
	FVector TipBurstWorld = FVector::ZeroVector;
	double TipBurstWorldTime = -1000.;

	// 对外事件：评级、命中与共鸣变化
	UPROPERTY(BlueprintAssignable) FPlayerAttackJudged OnAttackJudged;
	UPROPERTY(BlueprintAssignable) FPlayerAttackHit OnAttackHit;
	UPROPERTY(BlueprintAssignable) FPlayerResonanceChanged OnResonanceChanged;

	// 攻击与共鸣操作
	// 请求轻击或重击，返回 true 表示已出招或已缓存，不代表已经命中
	UFUNCTION(BlueprintCallable, Category = "玩家攻击|输入", meta = (DisplayName = "请求攻击"))
	bool RequestAttack(EPlayerAttackInput Input);
	// 请求共鸣，条件满足后等待指定拍点启动，返回 true 不代表此刻已生效
	UFUNCTION(BlueprintCallable, Category = "玩家攻击|共鸣", meta = (DisplayName = "请求共鸣"))
	bool RequestResonance();

	UFUNCTION(BlueprintPure)
	bool IsAttacking() const
	{
		return !CurrentAttack.IsNone();
	}

	bool IsPlayingAttackMotion() const
	{
		return IsAttacking() && !bAttackVisualEnded;
	}

	UFUNCTION(BlueprintPure)
	bool IsResonanceActive() const
	{
		return ResonanceState == EPlayerResonanceState::Active;
	}

	// 返回当前共鸣剩余拍数
	UFUNCTION(BlueprintPure)
	float GetResonanceRemainingBeats() const;
	// 返回攻击期间允许的移动比例
	float GetAttackMovementScale() const;
	// 根据当前连招节点生成下一步按键提示
	FString GetComboPrompt() const;
	// 防御窗口内可取消攻击；窗口外按招式阶段和是否闪避判断，返回 false 时保留攻击
	bool TryCancelAttackForDefense(bool bDodge);
	// 停止本次攻击的动画、轨迹和输入缓存
	void CancelAttack();
	// 重置攻击、缓存和共鸣状态
	void Reset();
	// 受击等外部中断发生时清理攻击状态
	void InterruptAttack();
	// 记录攻击收益，更新共鸣与命中反馈状态
	void ApplyAttackReward(EPlayerAttackGrade Grade, float Amount, bool bHit);

	// 组件生命周期
	// 缓存战斗组件，加载音效并校验攻击配置
	virtual void BeginPlay() override;
	// 退出关卡时清空攻击与共鸣状态
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float Delta, ELevelTick TickType,
	                           FActorComponentTickFunction* TickFunction) override;

  private:
	// 关联组件与动作资源
	TWeakObjectPtr<class UPlayerDefenseComponent> DefenseComponent;
	TWeakObjectPtr<class UBeatSyncedComboComponent> ResonanceBoss;

	UPROPERTY(Transient) TObjectPtr<class UAnimMontage> AttackMontage;

	// 音效资源
	UPROPERTY(Transient) TObjectPtr<class USoundBase> HitSound;
	UPROPERTY(EditDefaultsOnly, Category = "Attack|Audio") TObjectPtr<class USoundBase> LightSwingSound;
	UPROPERTY(EditDefaultsOnly, Category = "Attack|Audio") TObjectPtr<class USoundBase> HeavySwingSound;
	UPROPERTY(EditDefaultsOnly, Category = "Attack|Audio") TObjectPtr<class USoundBase> ContactSound;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> StartSound;
	// 轻击命中的节奏音效
	UPROPERTY(Transient) TObjectPtr<class USoundBase> BeatKickSound;
	// 不同连段使用的命中音效资源
	UPROPERTY(Transient) TArray<TObjectPtr<class USoundBase>> LightHitSounds;
	UPROPERTY(Transient) TArray<TObjectPtr<class USoundBase>> HeavyHitSounds;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> FinisherHitSound;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> KickHitSound;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> OffBeatHitSound;
	// 加载各连段的命中音效，排除缺失资源
	void LoadHitRushSounds();

	UPROPERTY(Transient) TObjectPtr<class USoundBase> GoodHitSound;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> SnareSound;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> FinisherSound;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> FinisherPerfectSound;
	UPROPERTY(Transient) TObjectPtr<class UNiagaraSystem> PerfectBurstEffect;

	// 攻击时间安排
	// 命中停顿与镜头反馈
	// 命中停顿使用音乐时间，恢复时追上原定收招结束时刻，不推迟下一击
	double HitStopStartAudio = -1000.;
	float HitStopSeconds = 0.f;
	float HitStopEase = 2.f; // 命中停顿后恢复动画的曲线指数
	// 镜头后处理的短促命中效果，使用游戏时间恢复
	double PunchStartWorld = -1000.;
	float PunchStrength = 0.f;
	float BaseFringe = -1.f, BaseVignette = -1.f;
	// 衰减命中造成的色散与暗角，结束后恢复原值
	void TickScreenPunch();
	UPROPERTY(Transient) TObjectPtr<class UParticleSystem> HitEffect;
	UPROPERTY(Transient) TObjectPtr<class UParticleSystem> TrailEffect;
	UPROPERTY(Transient) TObjectPtr<class UParticleSystemComponent> Trail;
	TWeakObjectPtr<AActor> AttackTarget;

	// 本次命中的目标记录
	// 每次出招清空，只记录这一击已结算过的目标
	TSet<TWeakObjectPtr<AActor>> HitActors;

	// 连招缓存：出招期间使用，保存第一次按键和原始音乐时间
	// 连招缓存保留本轮第一次按键，后续按键只计数，不覆盖方向选择
	EPlayerAttackInput BufferedInput = EPlayerAttackInput::Light;
	bool bHasBufferedInput = false;
	// 普通起手缓存：空闲但尚未解锁时使用，保留第一次按键，最多等待 1 秒游戏时间
	bool bHasPendingAttack = false;
	EPlayerAttackInput PendingAttack = EPlayerAttackInput::Light;
	// 前者是原始音乐时间，用于评级；后者是游戏时间截止点，用于过期清理
	double PendingAttackAudio = 0., PendingAttackDeadline = 0.;
	bool bImpactPresented = false;
	bool bSwingPresented = false;
	bool bRewarded = false;
	// 动画淡出与连招窗口关闭分别记录，不能用动画是否播放判断能否接招
	bool bAttackVisualEnded = false;
	double ComboInputAudio = 0.;
	// 连招接收截止基准，最终截止时刻为 LatestCueAudio + ChainLateSeconds()
	// 这是按键窗口，与 SweepBlade 使用的命中窗口分开
	double LatestCueAudio = 0.;
	double NextStartAllowedAudio = -1000.;
	double BufferedInputTime = 0.;
	// 当前一轮累计的按键次数，大于一次就降低评级，不增加出招次数
	int32 PressCountSinceCommit = 0;
	// 当前攻击的起手、命中、结束和上一帧时间，均使用音乐秒数
	double StartAudio = 0., ImpactAudio = 0., EndAudio = 0., LastAudio = 0.;

	// 共鸣起止拍数
	// 共鸣等待与生效的拍号，不是秒数
	double ArmedBeat = 0., ActiveStartBeat = 0.;
	double LastChainAudio = -1000.;
	double LastCommittedImpactBeat = -1000.;
	// 普通起手最早允许恢复的音乐时刻
	double RecoveryUntilAudio = -1000.;
	float AttackPlayRate = 1.15f;
	double SwingStartAudio = 0.;
	double LinkAudio = 0.;
	// 上一招的节点标识，用于选择轻击或重击后继分支
	FName ChainNode;
	// 每次成功起手递增，回调返回后可据此判断是否仍在处理同一击
	uint64 AttackSerial = 0;

	// 动画位移与武器轨迹
	// 已经提取根位移的动画位置，避免同一段位移重复应用
	float LastRootMotionPosition = 0.f;
	FVector RootProbeStart = FVector::ZeroVector;
	FVector LastBladeBase = FVector::ZeroVector, LastBladeTip = FVector::ZeroVector;
	FVector LastLegA[2], LastLegB[2]; // 左右腿从小腿到前脚掌的检测端点
	void SampleLegs(FVector OutA[2], FVector OutB[2]) const;
	// 记录破绽中的有效命中，并检查是否达到填满条件
	void RegisterOpeningHit(const struct FBossOpening& Opening, bool bGap);
	// 按破绽起始时间区分不同破绽，分别累计命中次数
	double OpeningKey = -1.;
	int32 OpeningHits = 0;
	bool bOpeningFilled = false;
	// 切换共鸣状态并通知监听者
	void SetResonanceState(EPlayerResonanceState State);
	// 结束共鸣并通知配合的 Boss 恢复状态
	void EndResonance();

	// 内部出招与接续
	// bChained 表示消费已有连招缓存，保留原分支
	// TargetImpactAudio 是下一击的目标音乐时刻，负数表示按普通起手规则安排
	bool StartAttack(EPlayerAttackInput Input, double InputAudio, bool bChained = false,
	                 double TargetImpactAudio = -1.);
	// 从起手姿态到接触姿态需要预留的时间
	static double StrikeLead(const FPlayerAttackNode& Node);
	// 返回连招允许迟按的时间宽限
	double ChainLateSeconds() const;
	// 备用重击蓄力入口，当前实现关闭蓄力，统一返回 0
	static double WindupBeats(const FPlayerAttackNode& Node, bool bResonance);
	bool bWindupAttack = false;
	double ChamberStartAudio = 0., ChamberEndAudio = 0., SnapStartAudio = 0.;
	float HoldPosition = 0.f;
	// 备用蓄力连段的理想按键时间，普通连段使用 ComboInputAudio
	double HeavyPressAudio = 0.;
	double NextChargeCueAudio = -1.;
	// 根据蓄力进度计算应该显示的动画位置
	float WindupPosition(double Now, double SampleNow) const;
	// 实际播放的动画区间，连续片段可延伸到下一招的起手姿态
	float ActiveClipStart = 0.f, ActiveClipEnd = 0.f;
	// 按音乐时间推进动作、接触检测与连招接续
	void TickAttack(double Now, float Delta);
	void SweepBlade(double Now);
	// 按命中结果和评级组合动作停顿、声音、特效与镜头
	void PresentImpact(bool bHit, FVector Location, AActor* Target = nullptr);
	// 优先使用镜头锁定目标，否则寻找前方附近的存活 Boss
	AActor* FindTarget() const;
	// 取得当前世界的节拍时钟
	class UBeatClockSubsystem* Clock() const;

	// 运行验证入口
	// 按命令行参数驱动攻击验证流程
	void TickRuntimeProbe();
	// 按预设顺序输入指令，记录实际攻击状态
	void TickInputProbe();
	// 通过实际刀刃接触验证闪避、格挡和反击，以及失败后的恢复
	void TickDefenseContactProbe();
	// 检查侧移后武器接触与攻击判定的变化
	void TickSpatialSidestepProbe();
	// 在真实运行流程中检查连招路线、输入缓存和连续按键
	void TickComboProbe();
	// 运行时验证使用的状态
	uint64 ComboProbeObservedSerial = 0;
	bool bComboProbeFailed = false;
	int32 DefenseProbeMask = 0;
	double DefenseProbeLastJudgment = -1., DefenseProbeRelease = -1.;
	UPROPERTY(Transient) TObjectPtr<class UAttackDataAsset> DefenseProbeAttack;
	int32 InputProbeStep = 0;
	double InputProbeStart = -1.;
	bool bProbeStarted = false;
	int32 ProbeStage = 0, ProbeInputs = 0;
	double ProbeNextInputBeat = 0., ProbeStartWorld = 0.;
	float ProbeInitialBossHealth = 0.f;
};
