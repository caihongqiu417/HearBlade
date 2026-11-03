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

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))

class HEARBLADE_API UPlayerRhythmCombatComponent : public UActorComponent
{
	GENERATED_BODY()

  public:
	UPlayerRhythmCombatComponent();
	void AwardPlayerAttack(bool bPerfect, float BaseGain);
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

	// Resonance starts falling after the player stops landing successful rhythm
	// responses. The short grace period keeps individual gains readable.
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

	// Hidden combat resource. The HUD intentionally does not draw this value.
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

	UFUNCTION(BlueprintCallable, Category = "生命")
	void ApplyCombatDamage(float Damage);
	void ApplyGroundBurstDamage(float Damage, const FVector& Center);
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

	UFUNCTION(BlueprintCallable, Category = "生命")
	void RestoreHealth(float Amount);

	UFUNCTION(BlueprintCallable, Category = "节奏闪避")
	void SubmitDodgeJudgment(ERhythmDodgeJudgment Grade, float SignedErrorMs, bool bStrongBeat = false);

	UFUNCTION(BlueprintCallable, Category = "节奏闪避")
	void ResetCombatState();

	UFUNCTION(BlueprintCallable, Category = "节奏闪避")
	bool TryDodge(FVector WorldDirection);

	UFUNCTION(BlueprintCallable, Category = "节奏防御") bool TryBlockStart();
	UFUNCTION(BlueprintCallable, Category = "节奏防御") void EndBlock();
	UFUNCTION(BlueprintCallable, Category = "节奏防御") bool TryCounter();
	// Ordinary attacks never write defense timing histories or counter state.
	bool PrepareForOrdinaryAttack();
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

	// The visual starts after a very short sustained press, then remains active
	// through authored HoldBlock markers. A tap block therefore stays clean while
	// a player who deliberately holds guard gets immediate readable feedback.
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

	// Called only when the real blade volume reaches the player capsule.
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

	UFUNCTION(BlueprintPure, Category = "节奏反馈") static int32 GetFeedbackTierForCombo(int32 Combo);

  protected:
	virtual void BeginPlay() override;
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
	float PoiseBreakDurationSeconds = 0.62f;

	UPROPERTY(EditAnywhere, Category = "受击", meta = (ClampMin = "0.05", ClampMax = "0.5"))
	float LightHitDurationSeconds = 0.34f;
	UPROPERTY(EditAnywhere, Category = "受击", meta = (ClampMin = "0.0", ClampMax = "0.2"))
	float HitTurnSeconds = 0.07f;
	UPROPERTY(EditAnywhere, Category = "受击", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	// 普通受击的输入锁定时间，与受击动画总长度分开
	float LightHitInputLockSeconds = 0.22f;

	UPROPERTY(EditAnywhere, Category = "韧性", meta = (ClampMin = "0.0", ClampMax = "5.0"))
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
	float DodgeDistanceApplied = 0.0f;
	bool bRecoveryStrikePending = false;
	double GetCurrentInputAudioSeconds() const;
	void BufferInput(EBufferedRhythmAction Action, double InputAudioSeconds,
	                 FVector Direction = FVector::ZeroVector);
	void ProcessBufferedInput();
	double ConsumeClosestInput(TArray<double>& History, double IdealAudioSeconds);
	void CancelDodgeForTransition();
	void ApplyPoiseDamage(float Amount, FVector ImpactLocation, FVector AttackDirection,
	                      FVector AttackerWorldLocation = FVector::ZeroVector);
	void StartPoiseBreak(FVector ImpactLocation, FVector AttackDirection,
	                     FVector AttackerWorldLocation = FVector::ZeroVector);
	void EndPoiseBreak();
	void StartHitReaction(FVector ImpactLocation, FVector AttackDirection, bool bHeavy,
	                      FVector AttackerWorldLocation = FVector::ZeroVector);
	void FinishHitReaction();
	void SpawnBloodFeedback(FVector ImpactLocation, FVector AttackDirection, bool bPoiseBreak) const;
	bool BufferWhilePoiseLocked(EBufferedRhythmAction Action, double InputAudioSeconds,
	                            FVector Direction = FVector::ZeroVector);
	bool StartDodge(FVector WorldDirection, double InputAudioSeconds);
	bool StartBlock(double InputAudioSeconds);
	bool StartCounter(double InputAudioSeconds);
	ERhythmDodgeJudgment JudgeTimedInput(double InputAudioSeconds, double IdealAudioSeconds) const;
	ERhythmDodgeJudgment JudgeCounterInput(double InputAudioSeconds, double IdealAudioSeconds) const;
	bool ResolveResponse(double ImpactAudioSeconds, double TargetBeat, float Damage, float IdealLeadSeconds,
	                     bool bStrongBeat, FVector BladeWorldLocation, FVector AttackDirection,
	                     ERhythmResponseType RequiredResponse, ERhythmHoldMarkerType HoldMarker,
	                     FName HoldGroup, bool bPhysicalContact,
	                     FVector AttackerWorldLocation = FVector::ZeroVector);
	void PlayDefenseAnimation(class UAnimSequence* Animation, bool bLooping, float DesiredDuration = 0.0f);
	void PlayResponseFeedback(ERhythmResponseType Response, ERhythmDodgeJudgment Grade, FVector WorldLocation,
	                          FVector AttackDirection, bool bStrongBeat);
	void BroadcastPerfectFeedback(float ErrorMs, bool bStrongBeat, FVector BladeWorldLocation,
	                              FVector AttackDirection);
};
