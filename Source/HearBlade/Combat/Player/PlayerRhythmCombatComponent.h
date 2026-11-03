// 管理玩家生命、防御输入、节奏评价及对应反馈状态
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/Data/AttackData.h"
#include "PlayerRhythmCombatComponent.generated.h"

UENUM(BlueprintType)
enum class ERhythmDodgeJudgment : uint8
{
	None,
	Perfect,
	Good,
	Bad,
	Miss,
	HoldBreak,
	GoodHold,
	PerfectHold
};

USTRUCT(BlueprintType)

struct FPerfectDodgeFeedbackData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FVector BladeWorldLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) FVector AttackDirection = FVector::ForwardVector;
	UPROPERTY(BlueprintReadOnly) float SignedErrorMs = 0.0f;
	UPROPERTY(BlueprintReadOnly) int32 ComboCount = 0;
	UPROPERTY(BlueprintReadOnly) float ResonanceFraction = 0.0f;
	UPROPERTY(BlueprintReadOnly) bool bStrongBeat = false;
};

USTRUCT(BlueprintType)

struct FRhythmSuccessFeedbackData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bPlayerAttack = false;
	UPROPERTY(BlueprintReadOnly) ERhythmResponseType Response = ERhythmResponseType::Dodge;
	UPROPERTY(BlueprintReadOnly) ERhythmHoldMarkerType HoldMarker = ERhythmHoldMarkerType::None;
	UPROPERTY(BlueprintReadOnly) ERhythmDodgeJudgment Grade = ERhythmDodgeJudgment::None;
	UPROPERTY(BlueprintReadOnly) int32 PreviousCombo = 0;
	UPROPERTY(BlueprintReadOnly) int32 ComboCount = 0;
	UPROPERTY(BlueprintReadOnly) int32 FeedbackTier = 0;
	UPROPERTY(BlueprintReadOnly) bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly) bool bPerfect = false;
	UPROPERTY(BlueprintReadOnly) bool bKeyPerfect = false;
	UPROPERTY(BlueprintReadOnly) bool bStrongBeat = false;
	UPROPERTY(BlueprintReadOnly) FVector WorldLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) FVector AttackDirection = FVector::ForwardVector;
	UPROPERTY(BlueprintReadOnly) int64 EventId = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerHealthChanged, float, Health, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDodgeJudged, ERhythmDodgeJudgment, Grade, float,
                                               SignedErrorMs, int32, ComboCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPerfectDodge, const FPerfectDodgeFeedbackData&, Feedback);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRhythmSuccessFeedback, const FRhythmSuccessFeedbackData&,
                                            Feedback);

// 处理闪避、格挡、反击、受击与防御输入缓存
// 使用原始按键时间评级，使用动作状态决定何时允许执行
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))

class HEARBLADE_API UPlayerRhythmCombatComponent : public UActorComponent
{
	GENERATED_BODY()

  public:
	UPlayerRhythmCombatComponent();
	// 根据玩家攻击评级增加共鸣，并记录本次收益
	void AwardPlayerAttack(bool bPerfect, float BaseGain);
	// 切换演出状态，进入时清理未完成的攻击与防御操作
	void SetPhaseCinematic(bool bEnabled);

	bool IsPhaseCinematic() const
	{
		return bPhaseCinematic;
	}

	bool bPhaseCinematic = false;
	uint8 PhaseSavedMovementMode = 1;
	uint8 PhaseSavedCustomMode = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "生命", meta = (ClampMin = "1.0"))
	float MaxHealth = 500.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "生命")
	float Health = 500.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "节奏闪避")
	float Resonance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "节奏闪避", meta = (ClampMin = "1.0"))
	float MaxResonance = 100.0f;

	// 成功防御后的宽限期结束后，共鸣值才开始下降

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节奏闪避|共鸣衰减", meta = (ClampMin = "0.0"))
	float ResonanceDecayDelaySeconds = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节奏闪避|共鸣衰减", meta = (ClampMin = "0.0"))
	float ResonanceDecayPerSecond = 6.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "节奏闪避")
	int32 ComboCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "节奏闪避")
	ERhythmDodgeJudgment LastJudgment = ERhythmDodgeJudgment::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "节奏防御")
	ERhythmResponseType LastResponse = ERhythmResponseType::Dodge;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "节奏闪避")
	float LastJudgmentErrorMs = 0.0f;

	// 内部韧性数值，不显示在界面上
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "韧性", meta = (ClampMin = "1.0"))
	float MaxPoise = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "韧性")
	float Poise = 100.0f;

	UPROPERTY(BlueprintAssignable, Category = "生命")
	FOnPlayerHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "节奏闪避")
	FOnDodgeJudged OnDodgeJudged;

	UPROPERTY(BlueprintAssignable, Category = "节奏闪避")
	FOnPerfectDodge OnPerfectDodge;
	UPROPERTY(BlueprintAssignable, Category = "节奏反馈")
	FOnRhythmSuccessFeedback OnRhythmSuccessFeedback;

	// 扣除生命并限制数值范围
	UFUNCTION(BlueprintCallable, Category = "生命")
	void ApplyCombatDamage(float Damage);
	// 结算范围攻击伤害，并按爆发位置产生受击反应
	void ApplyGroundBurstDamage(float Damage, const FVector& Center);
	// 记录踩拍逃离奖励并增加共鸣
	void AwardRhythmicEscape(float Amount, float ErrorSeconds);

	float GetEscapeDodgeBonus(float WalkingSpeed, double AvailableSeconds) const
	{
		return AvailableSeconds > DodgeDurationSeconds + .55
		           ? FMath::Max(0.f, DodgeDistanceCm - WalkingSpeed * DodgeDurationSeconds)
				   : 0.f;
	}

	UPROPERTY(BlueprintReadOnly, Category = "范围攻击") double LastRhythmicEscapeWorldSeconds = -1000.0;
	UPROPERTY(BlueprintReadOnly, Category = "范围攻击") float LastRhythmicEscapeGain = 0.f;
	bool bGroundBurstTestExited = false;

	// 恢复生命，不超过生命上限
	UFUNCTION(BlueprintCallable, Category = "生命")
	void RestoreHealth(float Amount);

	// 保存评级结果，更新连击与共鸣，并通知反馈组件
	UFUNCTION(BlueprintCallable, Category = "节奏闪避")
	void SubmitDodgeJudgment(ERhythmDodgeJudgment Grade, float SignedErrorMs, bool bStrongBeat = false);

	// 清空本轮防御、受击、评级和输入历史
	UFUNCTION(BlueprintCallable, Category = "节奏闪避")
	void ResetCombatState();

	// 检查当前状态，决定立即闪避还是保留输入等待解锁
	UFUNCTION(BlueprintCallable, Category = "节奏闪避")
	bool TryDodge(FVector WorldDirection);

	// 检查格挡条件，锁定期间先缓存按下操作
	// 记录松键时刻，处理仍在缓存中的格挡以及持续格挡结算
	UFUNCTION(BlueprintCallable, Category = "节奏防御") bool TryBlockStart();
	// 检查反击条件，必要时缓存按键而不立即播放动作
	UFUNCTION(BlueprintCallable, Category = "节奏防御") void EndBlock();
	UFUNCTION(BlueprintCallable, Category = "节奏防御") bool TryCounter();
	// 普通攻击不修改防御按键历史或反击状态
	bool PrepareForOrdinaryAttack();
	// 判断当前鼠标输入是否应该解释为防御
	// 根据临近的敌人攻击生成鼠标防御提示
	UFUNCTION(BlueprintPure, Category = "节奏防御") bool IsMouseDefenseWindow() const;
	FString GetMouseDefensePrompt() const;

	double GetLastCounterInputAudioSeconds() const
	{
		return LastCounterInputAudioSeconds;
	}

	UFUNCTION(BlueprintPure, Category = "节奏防御") bool IsBlocking() const
	{
		return bBlocking;
	}

	// 短按不显示持续格挡外壳，持续按住后显示，并跟随持续格挡状态更新


	UFUNCTION(BlueprintPure, Category = "节奏防御") bool IsSustainedHoldActive() const
	{
		return bBlocking && !bActiveHoldBroken && GetWorld() &&
		       GetWorld()->GetTimeSeconds() - BlockStartWorldSeconds >= HoldArmorActivationSeconds;
	}

	UFUNCTION(BlueprintPure, Category = "节奏防御") bool IsInDefensePose() const
	{
		return bPhaseCinematic || bBlocking || bBlockRecovering || bCountering;
	}

	UFUNCTION(BlueprintPure, Category = "节奏防御") bool IsPerformingAction() const
	{
		return bPhaseCinematic || bDodging || bBlocking || bCountering || bBlockRecovering || bPoiseBroken ||
		       bHitReacting;
	}

	UFUNCTION(BlueprintPure, Category = "韧性") bool IsPoiseBroken() const
	{
		return bPoiseBroken;
	}

	UFUNCTION(BlueprintPure, Category = "受击") bool IsHitReacting() const
	{
		return bHitReacting || bPoiseBroken;
	}

	UFUNCTION(BlueprintPure, Category = "韧性") float GetPoiseFraction() const
	{
		return MaxPoise > 0.0f ? Poise / MaxPoise : 0.0f;
	}

	// 实际刀刃检测接触玩家胶囊时调用
	bool ResolveBossContact(double ContactAudioSeconds, double TargetBeat, float Damage,
	                        float IdealDodgeLeadSeconds, bool bStrongBeat, FVector BladeWorldLocation,
	                        FVector AttackDirection, ERhythmResponseType RequiredResponse,
	                        ERhythmHoldMarkerType HoldMarker, FName HoldGroup, FVector AttackerWorldLocation);

	// 刀刃窗口结束但未接触玩家时，只结算躲避反馈，不扣生命
	bool ResolveBossAvoidance(double ImpactAudioSeconds, double TargetBeat, float IdealDodgeLeadSeconds,
	                          bool bStrongBeat, FVector BladeWorldLocation, FVector AttackDirection,
	                          ERhythmResponseType RequiredResponse, ERhythmHoldMarkerType HoldMarker,
	                          FName HoldGroup);

	UFUNCTION(BlueprintPure, Category = "节奏闪避")

	bool IsDodging() const
	{
		return bDodging;
	}

	double GetLastDodgeInputAudioSeconds() const
	{
		return LastDodgeInputAudioSeconds;
	}

	// 判断当前是否处于闪避无敌时间内
	UFUNCTION(BlueprintPure, Category = "节奏闪避")
	bool IsInvulnerable() const;

	UFUNCTION(BlueprintPure, Category = "生命")

	float GetHealthFraction() const
	{
		return MaxHealth > 0.0f ? Health / MaxHealth : 0.0f;
	}

	UFUNCTION(BlueprintPure, Category = "节奏闪避")

	float GetResonanceFraction() const
	{
		return MaxResonance > 0.0f ? Resonance / MaxResonance : 0.0f;
	}

	double GetLastJudgmentWorldSeconds() const
	{
		return LastJudgmentWorldSeconds;
	}

	FVector GetDodgeStartLocation() const
	{
		return DodgeStartLocation;
	}

	FVector GetDodgeDirection() const
	{
		return DodgeDirection;
	}

	float GetPerfectWindowSeconds() const
	{
		return PerfectWindowMs / 1000.0f;
	}

	float GetGoodWindowSeconds() const
	{
		return GoodWindowMs / 1000.0f;
	}

	// 根据连续成功次数返回反馈档位
	UFUNCTION(BlueprintPure, Category = "节奏反馈") static int32 GetFeedbackTierForCombo(int32 Combo);

  protected:
	// 缓存角色与反馈组件，准备防御资源和初始战斗状态
	virtual void BeginPlay() override;
	// 推进闪避和受击，恢复韧性，再尝试执行缓存的防御输入
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

  private:
	double LastJudgmentWorldSeconds = -1000.0;
	double LastResonanceGainWorldSeconds = -1000.0;
	// 按键的音乐时间用于无敌与评级，动作开始的游戏时间用于动画和移动
	double LastDodgeInputAudioSeconds = -1000.0;
	double DodgeStartWorldSeconds = -1000.0;
	FVector DodgeDirection = FVector::ForwardVector;
	FVector DodgeStartLocation = FVector::ZeroVector;
	bool bDodging = false;
	bool bBlocking = false;
	bool bBlockRecovering = false;
	bool bBlockReleasePending = false;
	// 格挡仍在缓存中时记录松键，执行后需要立即补上释放
	bool bBufferedBlockReleased = false;
	double BufferedBlockReleaseAudioSeconds = -1000.0;
	// 最近的防御按键历史，每次命中消费最接近理想时刻的一次
	TArray<double> BlockInputHistory;
	TArray<double> CounterInputHistory;
	bool bCountering = false;
	bool bPoiseBroken = false;
	bool bHitReacting = false;
	bool bHitTurnPending = false;
	FRotator HitFacingRotation = FRotator::ZeroRotator;
	double HitTurnEndWorldSeconds = -1000.0;
	double HitReactionEndWorldSeconds = -1000.0;
	// 受击动画可以尚未结束，但到达此时刻后允许切换到新动作
	double HitInputUnlockWorldSeconds = -1000.0;
	double PoiseBreakStartWorldSeconds = -1000.0;
	double LastPoiseDamageWorldSeconds = -1000.0;
	double LastBlockStartAudioSeconds = -1000.0;
	double BlockStartWorldSeconds = -1000.0;
	double LastBlockReleaseAudioSeconds = -1000.0;
	double LastCounterInputAudioSeconds = -1000.0;
	double DefenseAnimationStartWorldSeconds = -1000.0;
	float DefenseAnimationDurationSeconds = 0.0f;
	double CounterStartWorldSeconds = -1000.0;
	// 持续格挡组的状态，中断只提示一次，后续实际伤害仍逐击处理
	FName ActiveHoldGroup;
	bool bActiveHoldBroken = false;
	TSet<FName> ReportedHoldBreakGroups;
	// 有限长度的命中去重记录，当前键只包含量化后的音乐时间
	TSet<int64> ResolvedImpactKeys;
	uint32 DodgeSequenceId = 0;
	uint32 LastResolvedDodgeSequenceId = MAX_uint32;
	friend class FRhythmInputTransitionsTest;
	bool bAutomationTestMode = false;
	bool bAutomationFailureTestMode = false;
	bool bHUDPreviewMode = false;
	bool bPerfectFeedbackPreviewMode = false;
	int32 PerfectFeedbackPreviewStep = 0;
	double NextPerfectFeedbackPreviewWorldSeconds = 0.0;
	int32 HUDPreviewStep = 0;
	bool bSubmittingHUDPreviewJudgment = false;
	double NextHUDPreviewWorldSeconds = 0.0;
	double LastAutomatedImpactAudioSeconds = -1000.0;
	double AutomationReleaseAudioSeconds = -1000.0;
	bool bAutomationWrongActionIssued = false;
	int64 FeedbackEventSerial = 0;
	enum class EBufferedRhythmAction : uint8
	{
		None,
		Dodge,
		Block,
		Counter
	};
	// 单槽防御缓存，新操作覆盖旧操作，闪避同时保存方向
	EBufferedRhythmAction BufferedAction = EBufferedRhythmAction::None;
	FVector BufferedDodgeDirection = FVector::ForwardVector;
	// 音乐时间保存按键时刻，游戏时间用于计算等待是否超时
	double BufferedInputAudioSeconds = -1000.0;
	double BufferedInputWorldSeconds = -1000.0;

	UPROPERTY(EditAnywhere, Category = "节奏闪避|动作", meta = (ClampMin = "0.1"))
	float DodgeDurationSeconds = 0.62f;

	// 翻滚达到允许接续的时刻后，新闪避可以替换后段动作
	UPROPERTY(EditAnywhere, Category = "节奏闪避|动作", meta = (ClampMin = "0.0"))
	float DodgeChainStartSeconds = 0.12f;

	// 防御缓存的基础有效期，受击和破防锁定时会延长到解锁后
	// 执行延迟不会改变用于节奏评级的原始按键时间
	UPROPERTY(EditAnywhere, Category = "节奏输入", meta = (ClampMin = "0.05", ClampMax = "0.4"))
	float InputBufferSeconds = 0.26f;

	UPROPERTY(EditAnywhere, Category = "节奏输入", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	// 反击开始后至少经过这段游戏时间，才允许缓存动作接管
	float CounterCancelStartSeconds = 0.12f;

	// 旧翻滚根位移配置，当前 StartDodge 关闭动画根位移，实际距离由 DodgeDistanceCm 推进
	UPROPERTY(EditAnywhere, Category = "节奏闪避|动作", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float DodgeRootMotionTranslationScale = 0.32f;

	UPROPERTY(EditAnywhere, Category = "节奏闪避|动作", meta = (ClampMin = "0.0", ClampMax = "400.0"))
	// 翻滚的目标水平距离，实际位移仍受碰撞阻挡
	float DodgeDistanceCm = 185.0f;

	UPROPERTY(EditAnywhere, Category = "节奏闪避|无敌帧", meta = (ClampMin = "0.0"))
	// 相对原始闪避按键的音乐时间，不是动画帧编号
	float InvulnerabilityStartSeconds = 0.04f;

	UPROPERTY(EditAnywhere, Category = "节奏闪避|无敌帧", meta = (ClampMin = "0.0"))
	// 闪避无敌结束时间，相对原始闪避按键的音乐时刻
	float InvulnerabilityEndSeconds = 0.42f;

	UPROPERTY(EditAnywhere, Category = "节奏闪避|判定", meta = (ClampMin = "1.0"))
	// 与理想按键时刻的双向误差，单位为毫秒
	float PerfectWindowMs = 80.0f;

	UPROPERTY(EditAnywhere, Category = "节奏闪避|判定", meta = (ClampMin = "1.0"))
	// 较宽的成功评级范围，开局至少使用 160 毫秒
	float GoodWindowMs = 160.0f;

	UPROPERTY(EditAnywhere, Category = "节奏防御|动作", meta = (ClampMin = "0.25", ClampMax = "1.5"))
	float CounterDurationSeconds = 0.62f;

	UPROPERTY(EditAnywhere, Category = "节奏防御|霸体视觉", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float HoldArmorActivationSeconds = 0.14f;

	UPROPERTY(EditAnywhere, Category = "韧性", meta = (ClampMin = "1.0"))
	float BadPoiseDamage = 38.0f;

	UPROPERTY(EditAnywhere, Category = "韧性", meta = (ClampMin = "1.0"))
	float DirectHitPoiseDamage = 55.0f;

	UPROPERTY(EditAnywhere, Category = "韧性", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	// 破防后这段时间只缓存操作，结束后才允许执行
	float PoiseBreakInputLockSeconds = 0.28f;

	UPROPERTY(EditAnywhere, Category = "韧性", meta = (ClampMin = "0.1", ClampMax = "1.5"))
	// 破防表现的总时长，不等于输入锁定时长
	float PoiseBreakDurationSeconds = 0.62f;

	UPROPERTY(EditAnywhere, Category = "受击", meta = (ClampMin = "0.05", ClampMax = "0.5"))
	float LightHitDurationSeconds = 0.34f;
	UPROPERTY(EditAnywhere, Category = "受击", meta = (ClampMin = "0.0", ClampMax = "0.2"))
	float HitTurnSeconds = 0.07f;
	UPROPERTY(EditAnywhere, Category = "受击", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	// 普通受击的输入锁定时间，与受击动画总长度分开
	float LightHitInputLockSeconds = 0.22f;

	UPROPERTY(EditAnywhere, Category = "韧性", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	// 受到韧性伤害后，等待这段时间才开始恢复
	float PoiseRecoveryDelaySeconds = 1.10f;

	UPROPERTY(EditAnywhere, Category = "韧性", meta = (ClampMin = "1.0", ClampMax = "200.0"))
	float PoiseRecoveryPerSecond = 40.0f;

	UPROPERTY(EditAnywhere, Category = "受击|血液特效")
	TObjectPtr<class UNiagaraSystem> BloodHitEffect;

	UPROPERTY(EditAnywhere, Category = "受击|血液特效")
	TObjectPtr<class UNiagaraSystem> BloodPoiseBreakEffect;

	UPROPERTY(EditAnywhere, Category = "受击|血液特效", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float BloodHitScale = 0.65f;

	UPROPERTY(EditAnywhere, Category = "受击|血液特效", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float BloodPoiseBreakScale = 0.9f;

	// 闪避与防御动画资源
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> DodgeForwardAnimation;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> DodgeBackwardAnimation;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> DodgeLeftAnimation;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> DodgeRightAnimation;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> BlockStartAnimation;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> BlockLoopAnimation;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> BlockEndAnimation;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> CounterAnimation;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> BlockHitReactAnimation;
	UPROPERTY(Transient) TObjectPtr<class UParticleSystem> BlockImpactEffect;
	UPROPERTY(Transient) TObjectPtr<class UParticleSystem> CounterImpactEffect;
	// 防御动作使用的音效资源，分别用于弹反起手、举盾和接触反馈
	UPROPERTY(Transient) TObjectPtr<class USoundBase> DodgeSwishSound;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> CounterSwishSound;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> GuardUpSound;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> CounterStrikeSound;
	UPROPERTY(Transient) TObjectPtr<class UNiagaraSystem> PerfectHoldShockEffect;
	// 本次翻滚已应用的距离，每帧只补上新增部分
	float DodgeDistanceApplied = 0.0f;
	bool bRecoveryStrikePending = false;
	// 取得本次输入用于节奏判定的音乐时间
	double GetCurrentInputAudioSeconds() const;
	// 保存待执行的防御类型、原始时间和闪避方向
	void BufferInput(EBufferedRhythmAction Action, double InputAudioSeconds,
	                 FVector Direction = FVector::ZeroVector);
	// 等待动作解锁后重试缓存输入，过期或失效时清除
	void ProcessBufferedInput();
	// 取出最接近理想时刻的一次按键，避免同一输入重复参与判定
	double ConsumeClosestInput(TArray<double>& History, double IdealAudioSeconds);
	// 状态切换时结束闪避位移与动作
	void CancelDodgeForTransition();
	// 减少韧性，耗尽时进入破防
	void ApplyPoiseDamage(float Amount, FVector ImpactLocation, FVector AttackDirection,
	                      FVector AttackerWorldLocation = FVector::ZeroVector);
	// 清理冲突动作，启动破防硬直和输入锁定
	void StartPoiseBreak(FVector ImpactLocation, FVector AttackDirection,
	                     FVector AttackerWorldLocation = FVector::ZeroVector);
	// 结束破防锁定并恢复允许执行动作的状态
	void EndPoiseBreak();
	// 根据攻击方向启动受击姿态，单独设置输入解锁时间
	void StartHitReaction(FVector ImpactLocation, FVector AttackDirection, bool bHeavy,
	                      FVector AttackerWorldLocation = FVector::ZeroVector);
	// 结束受击动作并恢复移动状态
	void FinishHitReaction();
	// 按接触位置和受击方向生成血液效果
	void SpawnBloodFeedback(FVector ImpactLocation, FVector AttackDirection, bool bPoiseBreak) const;
	// 破防锁定期间保留操作，并将有效期延长到允许执行之后
	bool BufferWhilePoiseLocked(EBufferedRhythmAction Action, double InputAudioSeconds,
	                            FVector Direction = FVector::ZeroVector);
	// 记录原始按键时间，启动翻滚动画与带碰撞的位移
	bool StartDodge(FVector WorldDirection, double InputAudioSeconds);
	// 记录格挡按键时刻并进入举盾姿态
	bool StartBlock(double InputAudioSeconds);
	// 保留原始输入时刻，启动反击姿态
	bool StartCounter(double InputAudioSeconds);
	// 按输入与理想时刻的误差划分完美、成功和失败
	ERhythmDodgeJudgment JudgeTimedInput(double InputAudioSeconds, double IdealAudioSeconds) const;
	// 使用反击窗口判断按键误差
	ERhythmDodgeJudgment JudgeCounterInput(double InputAudioSeconds, double IdealAudioSeconds) const;
	// 统一处理真实接触与未接触的防御判定，按命中键去重
	bool ResolveResponse(double ImpactAudioSeconds, double TargetBeat, float Damage, float IdealLeadSeconds,
	                     bool bStrongBeat, FVector BladeWorldLocation, FVector AttackDirection,
	                     ERhythmResponseType RequiredResponse, ERhythmHoldMarkerType HoldMarker,
	                     FName HoldGroup, bool bPhysicalContact,
	                     FVector AttackerWorldLocation = FVector::ZeroVector);
	// 统一播放防御动画，并设置循环和动作衔接时间
	void PlayDefenseAnimation(class UAnimSequence* Animation, bool bLooping, float DesiredDuration = 0.0f);
	// 按实际防御结果播放声音与反馈，不替换已开始的新动作
	void PlayResponseFeedback(ERhythmResponseType Response, ERhythmDodgeJudgment Grade, FVector WorldLocation,
	                          FVector AttackDirection, bool bStrongBeat);
	// 组装完美防御数据并广播给表现组件
	void BroadcastPerfectFeedback(float ErrorMs, bool bStrongBeat, FVector BladeWorldLocation,
	                              FVector AttackDirection);
};
