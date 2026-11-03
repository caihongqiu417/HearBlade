#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/Data/AttackData.h"
#include "Engine/TimerHandle.h"
#include "BeatSyncedComboComponent.generated.h"

class UAttackDataAsset;
class USkeletalMeshComponent;
class UBossMotionWarpingComponent;
class UNiagaraSystem;
class UAnimInstance;
class UAnimSequence;
class UNiagaraComponent;
class UBeatClockSubsystem;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UAudioComponent;
class USoundBase;
class UMaterialInterface;
class UMeshComponent;
class UMusicEmphasisData;

USTRUCT(BlueprintType)

struct FBeatBlinkSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "闪现", meta = (ClampMin = "0.0"))
	float LandingDistance = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "闪现",
	          meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float SideAngleDegrees = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "闪现", meta = (ClampMin = "0.0"))
	float WarningBeats = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "闪现")
	TObjectPtr<UNiagaraSystem> WarningEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "闪现")
	TObjectPtr<UNiagaraSystem> OriginEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "闪现")
	TObjectPtr<UNiagaraSystem> LandingEffect;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnComboImpact, int32, ImpactIndex, double, TargetBeat, float,
                                               BeatErrorSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnComboStarted, int32, FirstImpactBeat);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnComboFinished);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBossBlinkWarning, FVector, LandingLocation, double,
                                             TargetAudioSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBossBlinkSucceeded, FVector, LandingLocation, double,
                                             TargetAudioSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBossBlinkCancelled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBossConfirmedHit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBossCinematicAccent, float, Strength, bool, bStrongBeat);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnBossCinematicClash, float, Strength, bool, bStrongBeat,
                                               FVector, ClashLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBossCinematicAccentEnded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBossPhaseChanged, int32, Phase);

// 界面读取的命中提示，与连招共用音乐时钟，不另设界面计时器

struct FUpcomingImpactCue
{
	EBossAttackDamageShape DamageShape = EBossAttackDamageShape::Blade;
	double ImpactAudioSeconds = 0.0;
	float IdealDodgeLeadSeconds = 0.14f;
	float PromptLeadBeats = 3.0f;
	int32 ImpactIndex = 0;
	bool bStrongBeat = false;
	ERhythmResponseType RequiredResponse = ERhythmResponseType::Dodge;
	ERhythmHoldMarkerType HoldMarker = ERhythmHoldMarkerType::None;
	FName HoldGroup;
	TObjectPtr<UTexture2D> IconOverride = nullptr;
	float IconScale = 1.0f;
	FLinearColor IconColor = FLinearColor::White;
};

// 玩家可以追击的时间段，Gap 表示连招两击之间的空隙，Tail 表示连招结束或硬直期间的破绽


enum class EBossOpeningKind : uint8
{
	None,
	Gap,
	Tail,
	Sidestep
};

struct FBossOpening
{
	EBossOpeningKind Kind = EBossOpeningKind::None;
	double StartAudio = 0.0, EndAudio = 0.0;

	bool Contains(double T) const
	{
		return Kind != EBossOpeningKind::None && T >= StartAudio - .02 && T <= EndAudio + .02;
	}
};

// 行为树触发攻击后，将第一击安排在下一可用的指定小节拍位
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))

class HEARBLADE_API UBeatSyncedComboComponent : public UActorComponent
{
	GENERATED_BODY()

  public:
	// 按时间顺序返回当前连招附近的破绽
	void GetOpenings(TArray<FBossOpening>& Out) const;
	// 查找指定音乐时刻是否落在可追击的破绽内
	bool GetOpeningAt(double AudioSeconds, FBossOpening& Out) const;
	// 统一接收玩家命中，处理生命、韧性、挑飞和受击反馈
	void ReceivePlayerStrike(float Damage, float PoiseDamage, AActor* Source, bool bHeavy,
	                         FVector ImpactLocation, bool bUppercut = false);
	// 普通命中的表现包括护甲闪光、短距离后退和动画停顿，强度由节奏评级和招式决定


	void PlayPlayerHitFeedback(AActor* Source, float Strength, float FreezeSeconds);
	// 命中停顿期间沿受击方向抖动身体，随后逐渐恢复

	FTimerHandle HitShakeTimer;
	FVector HitShakeBase = FVector::ZeroVector, HitShakeDir = FVector::ZeroVector;
	float HitShakeElapsed = 0.f, HitShakeDuration = 0.f, HitShakeAmplitude = 0.f;
	bool bHitShaking = false;
	// 更新受击抖动，并在停顿结束后恢复网格速度和位置
	void TickHitShake();
	void StartUppercutFeedback(AActor* Source);
	// 推进网格的挑飞曲线，保持碰撞身体在地面
	void TickUppercutFeedback();
	// 还原挑飞修改的网格位置和旋转
	void EndUppercutFeedback();
	FTimerHandle UppercutTimer;
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FVector> UppercutMeshBases;
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FRotator> UppercutMeshRots;
	double UppercutStartWorldSeconds = -1000.;
	// 进入玩家共鸣配合流程，记录拍点与目标
	void BeginPlayerResonance(double StartBeat, AActor* Player);
	// 结束共鸣配合并恢复 Boss 正常战斗状态
	void EndPlayerResonance();

	bool IsPlayerResonanceActive() const
	{
		return bPlayerResonance;
	}

	// 推进共鸣期间的攻击与动作安排
	void TickPlayerResonance();
	bool bPlayerResonance = false;
	double PlayerResonanceStartBeat = 0.;
	int32 PlayerResonanceAttempt = 0;
	UPROPERTY(Transient) TObjectPtr<UAttackDataAsset> ResonanceCounterAttack;
	// 教程使用的单次攻击，在指定拍点命中，并要求玩家执行指定防御操作
	bool QueuePracticeAttack(ERhythmResponseType Response, double TargetBeat, AActor* Target);
	UPROPERTY(Transient) TObjectPtr<UAttackDataAsset> PracticeAttack;
	// 教程攻击即使未接触玩家也会进行练习判定
	bool bPracticeNote = false;
	// 教程练习期间保留受击表现，但不扣生命
	bool bTutorialNoDamage = false;
	// 连招结束后的踩拍后跳，实现在 BossBackstep.cpp
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss后跳",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BackstepChance = .85f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss后跳", meta = (ClampMin = "50.0"))
	float BackstepDistance = 380.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss后跳", meta = (ClampMin = "50.0"))
	float BackstepTriggerDistance = 600.f;

	bool IsBackstepping() const
	{
		return bBackstepPending || bBackstepActive;
	}

	void TryScheduleBackstep();
	// 等待后跳拍点，推进带碰撞的位移，结束或中断时恢复行为树
	void TickBackstep(float DeltaTime);
	// 踢击造成的短距离击退，移动时检查碰撞
	void StartKickKnockback(const AActor* Source, float Distance, float Seconds);
	// 按累计目标距离推进击退，碰到阻挡后停止
	void TickKnockback(float DeltaTime);
	// 受击时短暂增强弱拍材质效果
	double LastHitOverlayAudioSeconds = -1000.;
	float HitOverlayStrength = 0.f;
	FVector KnockDir = FVector::ZeroVector;
	float KnockDistance = 0.f, KnockSeconds = 0.f, KnockElapsed = 0.f, KnockTravelled = 0.f;
	bool bBackstepPending = false, bBackstepActive = false;
	int32 ConsecutiveBacksteps = 0;
	double BackstepStartAudio = 0., BackstepEndAudio = 0.;
	FVector BackstepDir = FVector::ZeroVector;
	float BackstepTravelled = 0.f;
	TArray<TWeakObjectPtr<class UAnimInstance>> BackstepAnims;
	UPROPERTY(Transient) TObjectPtr<UAttackDataAsset> PreResonanceAttack;
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, float> ResonanceSavedRates;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "音乐强调点")
	TSoftObjectPtr<UMusicEmphasisData> MusicEmphasisData = TSoftObjectPtr<UMusicEmphasisData>(
	    FSoftObjectPath(TEXT("/Game/Audio/Music/DA_AintItFun_Emphasis.DA_AintItFun_Emphasis")));
	UPROPERTY(Transient) TObjectPtr<UMusicEmphasisData> LoadedMusicEmphasis;
	// 在允许等待的拍数内选择音乐重音，兼顾攻击等待时间
	int32 SelectMusicEmphasisBeat(const UAttackDataAsset* Data, float& OutPriority) const;
	UBeatSyncedComboComponent();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss阶段") bool bEnableSecondPhase = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss阶段",
	          meta = (ClampMin = "0.01", ClampMax = "0.99"))
	float SecondPhaseHealthThreshold = .6f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss阶段") int32 BossPhase = 1;
	UPROPERTY(BlueprintAssignable, Category = "Boss阶段") FOnBossPhaseChanged OnPhaseChanged;

	UFUNCTION(BlueprintPure, Category = "Boss阶段") bool IsPhaseTransitionBlocking() const
	{
		return bPhaseTransitionActive;
	}

	// 检查阶段转换条件，通过演出组件启动升阶
	bool TryBeginPhaseAscension(AActor* Player, FVector Contact, bool bFromClash);
	void FinishPhaseAscension(bool bCompleted);

	UFUNCTION(BlueprintPure, Category = "Boss阶段") bool IsPhaseTransitionActive() const
	{
		return bPhaseTransitionActive;
	}

	bool IsAreaAttackDue() const
	{
		return BossPhase == 2 && CompletedPhaseTwoMelee >= NextAreaAfterMelee;
	}

	void UpdateBossPhase(const UBeatClockSubsystem* Clock);
	bool HasSafeAreaExit(const FVector& Center, float Radius, const AActor* Target,
	                     double AvailableSeconds = -1.0) const;
	// 按攻击时间更新危险圈预警和爆发状态
	void UpdateGroundBurst(double AudioNow);
	// 结算范围内玩家的伤害或踩拍闪避，并处理攻击后的破绽
	void ResolveGroundBurst(int32 ImpactIndex, double AudioNow);
	// 清除危险圈与本次范围攻击状态
	void ClearGroundBurst();
	// 提供危险圈的位置、半径和预警进度
	bool GetGroundBurstPreview(FVector& OutCenter, float& OutRadius, float& OutProgress) const;

	bool HasResolvedGroundBurst() const
	{
		return bGroundBurstResolved;
	}

	// 判断是否处于范围攻击后的追击机会
	bool IsGroundBurstRecovery() const;
	// 返回范围爆发前的剩余拍数，未预警时返回无效值
	float GetGroundBurstRemainingBeats() const;
	bool TryRecoveryStrike(AActor* Player);
	UPROPERTY(EditAnywhere, Category = "二阶段|破绽") float GroundBurstRecoveryBeats = 2.f;
	double GroundBurstRecoveryEndAudio = -1.;
	double LastRecoveryStrikeAudio = -100.;
	float ActiveGroundBurstRadius = 0.f;
	bool bGroundBurstHarmless = false;
	// 返回阶段转换的进度，未转换时返回无效值
	float GetPhaseTransitionProgress() const;
	void RecordPhaseTwoAttackOutcome(bool bCompleted);
	bool bPhaseTwoAttackCounted = false;
	// 按测试参数采样连招姿态，输出动画衔接检查结果
	bool TickComboPoseAudit();
	int32 PoseAuditCaptured = INDEX_NONE;
	TWeakObjectPtr<AActor> PoseAuditCamera;
	bool bGroundBurstTestInjected = false;
	int32 GroundBurstBoundaryCrossings = 0;
	// 提供当前攻击的应对类型、位置和预警进度
	bool GetRecognitionCue(FAttackRecognitionInfo& Info, FVector& Location, float& Progress,
	                       double& Key) const;
	// 提供只能闪避攻击的危险路径
	bool GetDodgeOnlyPath(double ImpactAudioSeconds, FVector& Start, FVector& Direction, float& Length) const;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss生命", meta = (ClampMin = "1"))
	float MaxBossHealth = 2700.f;
	// 范围攻击允许在爆发附近的闪避窗口内躲过，不要求玩家必须跑出整个危险圈


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "范围攻击") bool bGroundBurstDodgeThrough = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "范围攻击",
	          meta = (ClampMin = "0", ClampMax = "0.15"))
	float GroundBurstLateGraceSeconds = .08f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss生命") float BossHealth = 1200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss生命")
	FText BossDisplayName = FText::FromString(TEXT("焰铠龙骑士"));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss生命", meta = (ClampMin = "0"))
	float CounterDamage = 80.f;

	UFUNCTION(BlueprintPure, Category = "Boss生命") float GetBossHealthFraction() const
	{
		return MaxBossHealth > 0.f ? FMath::Clamp(BossHealth / MaxBossHealth, 0.f, 1.f) : 0.f;
	}

	// 处理没有明确来源的伤害
	UFUNCTION(BlueprintCallable, Category = "Boss生命") void ApplyBossDamage(float Damage);
	// 接收引擎伤害事件，转入 Boss 的伤害处理
	UFUNCTION()
	void ReceiveBossDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
	                       AController* InstigatedBy, AActor* DamageCauser);

	// 韧性控制攻击能否被打断，普通受击与破防分开处理

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss受击|韧性", meta = (ClampMin = "1.0"))
	float MaxBossPoise = 320.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss受击|韧性")
	float BossPoise = 160.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss受击|韧性", meta = (ClampMin = "0.0"))
	float PoiseDamagePerHealthDamage = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss受击|韧性", meta = (ClampMin = "0.0"))
	float BossPoiseRecoveryDelay = 1.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss受击|韧性", meta = (ClampMin = "1.0"))
	float BossPoiseRecoveryPerSecond = 64.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss受击|动作",
	          meta = (ClampMin = "0.1", ClampMax = "1.5"))
	float LightHitReactionSeconds = 0.34f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss受击|动作",
	          meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float HeavyHitReactionSeconds = 0.58f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss受击|血液特效")
	TObjectPtr<UNiagaraSystem> BloodHitMetalEffect;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss受击|血液特效")
	TObjectPtr<UNiagaraSystem> BloodHeavyMetalEffect;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss受击|血液特效",
	          meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float BloodHitScale = 0.75f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss受击|血液特效",
	          meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float BloodHeavyScale = 1.05f;

	UFUNCTION(BlueprintPure, Category = "Boss受击") bool IsHitReacting() const
	{
		return bBossHitReacting;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招")
	TObjectPtr<UAttackDataAsset> AttackData;

	// 行为树触发攻击后，留给动画起手的额外时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招", meta = (ClampMin = "0.0"))
	float SchedulingLeadSeconds = 0.1f;

	// 空标签：播放 Owner 上所有与 Montage 骨架相同的 SkeletalMeshComponent
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招")
	FName MeshComponentTag;

	// 用于混合待机、行走和攻击蒙太奇的动画类
	UPROPERTY(EditAnywhere, Category = "节拍连招|移动")
	TSoftClassPtr<UAnimInstance> LocomotionAnimClass;

	UPROPERTY(BlueprintAssignable, Category = "节拍连招")
	FOnComboStarted OnComboStarted;

	UPROPERTY(BlueprintAssignable, Category = "节拍连招")
	FOnComboImpact OnImpact;

	UPROPERTY(BlueprintAssignable, Category = "节拍连招")
	FOnComboFinished OnComboFinished;
	UPROPERTY(BlueprintAssignable, Category = "节拍连招|相机") FOnBossBlinkWarning OnBlinkWarning;
	UPROPERTY(BlueprintAssignable, Category = "节拍连招|相机") FOnBossBlinkSucceeded OnBlinkSucceeded;
	UPROPERTY(BlueprintAssignable, Category = "节拍连招|相机") FOnBossBlinkCancelled OnBlinkCancelled;
	UPROPERTY(BlueprintAssignable, Category = "节拍连招|相机") FOnBossConfirmedHit OnConfirmedHit;
	UPROPERTY(BlueprintAssignable, Category = "节拍连招|关键慢放")
	FOnBossCinematicAccent OnCinematicAccentStarted;
	UPROPERTY(BlueprintAssignable, Category = "节拍连招|关键慢放")
	FOnBossCinematicClash OnCinematicAccentImpact;
	UPROPERTY(BlueprintAssignable, Category = "节拍连招|关键慢放")
	FOnBossCinematicAccentEnded OnCinematicAccentEnded;

	// 实际确认命中后调用，不能只凭动画到达命中标记就调用
	UFUNCTION(BlueprintCallable, Category = "节拍连招|相机") void NotifyConfirmedHit()
	{
		OnConfirmedHit.Broadcast();
	}

	UFUNCTION(BlueprintCallable, Category = "节拍连招")
	bool QueueCombo();

	// 行为树或蓝图在决定攻击时调用；使用传入的独立动作数据，并对齐下一可用拍
	UFUNCTION(BlueprintCallable, Category = "节拍连招")
	bool QueueAttack(UAttackDataAsset* InAttackData);

	// 在 QueueAttack 前设置，让瞬移和第一击使用同一个目标时间
	void ConfigureNextBlink(const FBeatBlinkSettings& Settings);

	double GetScheduledFirstImpactAudioSeconds() const
	{
		return ScheduledFirstImpactAudioSeconds;
	}

	double GetScheduledStartAudioSeconds() const
	{
		return ScheduledStartAudioSeconds;
	}

	// 返回下一次尚未结算的刀刃命中时间，供提示和测试使用
	bool GetNextImpactDodgeTiming(double& OutImpactAudioSeconds, float& OutIdealLeadSeconds) const;
	void GetUpcomingImpactCues(TArray<FUpcomingImpactCue>& OutCues, int32 MaxCount = 4,
	                           bool bIncludeRecent = false) const;

	// 记录攻击目标，供追踪和动作对齐使用
	UFUNCTION(BlueprintCallable, Category = "节拍连招")
	void SetAttackTarget(AActor* InTarget);

	// 取消排队或正在播放的连招，清理窗口、移动对齐和临时表现
	UFUNCTION(BlueprintCallable, Category = "节拍连招")
	void CancelCombo();

	UFUNCTION(BlueprintPure, Category = "节拍连招")

	bool IsComboQueuedOrPlaying() const
	{
		return bQueued || bPlaying;
	}
	UFUNCTION(BlueprintPure, Category = "节拍连招")

	bool IsComboQueued() const
	{
		return bQueued;
	}
	UFUNCTION(BlueprintPure, Category = "节拍连招")

	bool IsComboPlaying() const
	{
		return bPlaying;
	}

	UFUNCTION(BlueprintPure, Category = "节拍连招")

	bool WasLastAttackCompleted() const
	{
		return bLastAttackCompleted;
	}

	float GetLastLocomotionSpeed() const
	{
		return LastLocomotionSpeed;
	}

	bool WasLastLocomotionBlocked() const
	{
		return bLastLocomotionBlocked;
	}

	// 读取行走动画时长，供步伐与音乐拍点对齐
	float GetWalkClipLengthSeconds() const;

	// 允许为未配置慢放的收尾攻击补充慢放表现，关闭后只使用资源中的配置

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|关键慢放")
	bool bEnableOccasionalFinalImpactCinematic = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|关键慢放",
	          meta = (ClampMin = "0.0", ClampMax = "1.0",
	                  EditCondition = "bEnableOccasionalFinalImpactCinematic"))
	float FinalImpactCinematicChance = .35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|关键慢放",
	          meta = (EditCondition = "bEnableOccasionalFinalImpactCinematic"))
	FImpactCinematicSettings FinalImpactCinematic;

	// 待机和战斗期间都让火焰随音乐节拍变化
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|节奏感",
	          meta = (ClampMin = "0.0", ClampMax = "0.3"))
	float FireBeatPulseStrength = 0.10f;

	// 小节第一击落下前，先让火焰向刀刃聚拢
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|强拍",
	          meta = (ClampMin = "0.25", ClampMax = "2.0"))
	float StrongBeatChargeBeats = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|强拍",
	          meta = (ClampMin = "1.0", ClampMax = "6.0"))
	float StrongBeatSwordGlowMultiplier = 3.5f;

	// 叠加在护甲上的轮廓光与流动光带
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|强弱拍")
	TObjectPtr<UMaterialInterface> BeatOverlayMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|强弱拍",
	          meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float StrongOverlaySeconds = 0.42f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|强弱拍",
	          meta = (ClampMin = "0.05", ClampMax = "0.6"))
	float WeakOverlaySeconds = 0.23f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|强拍")
	TObjectPtr<USoundBase> StrongBeatSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|强拍")
	TObjectPtr<USoundBase> WeakBladeSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|强拍",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StrongBladeVolume = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|强拍",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WeakBladeVolume = .9f;

	// 挥刀风声的额外音量，避免被背景音乐盖住
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|强弱拍",
	          meta = (ClampMin = "0.5", ClampMax = "4.0"))
	float BladeSfxGain = .72f;

	// 挥刀声音比动画命中标记提前播放
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|强弱拍",
	          meta = (ClampMin = "0.0", ClampMax = "0.15"))
	float BladeWhooshLeadSeconds = 0.15f;

	// 跳过音效开头的静音部分，让主要挥刀声出现在命中之前

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|强弱拍",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BladeSoundStartTimeSeconds = 0.15f;

	// 命中时短暂降低背景音乐音量，突出刀刃声音
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|强拍",
	          meta = (ClampMin = "0.2", ClampMax = "1.0"))
	float StrongBeatMusicGain = .38f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|强拍",
	          meta = (ClampMin = "0.2", ClampMax = "1.0"))
	float WeakBeatMusicGain = .58f;

	// 按音乐时间推进瞬移、动画、命中窗口和收尾，并处理阶段演出
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	// 绑定伤害事件，检查身体和动画配置，准备战斗使用的组件
	virtual void BeginPlay() override;
	// 停止连招与演出，清理临时效果和事件绑定
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

  private:
	// 范围爆发：当前标记、预警区域、玩家离开时间与结算状态
	int32 GroundBurstImpact = INDEX_NONE;
	bool bGroundBurstResolved = false;
	FVector GroundBurstCenter = FVector::ZeroVector;
	FVector GroundBurstPreviousLocation = FVector::ZeroVector;
	double GroundBurstPreviousAudio = 0.0;
	double GroundBurstTargetAudio = 0.0;
	double GroundBurstStartAudio = 0.0;
	double GroundBurstExitAudio = -1.0;
	bool bGroundBurstWasInside = false;
	// 阶段转换：请求等待、演出执行和后续攻击计数
	bool bPhaseChangePending = false;
	bool bPhaseTransitionActive = false;
	double PhaseTransitionStartBeat = -1.0;
	double PhaseRequestWorldSeconds = -1.0;
	int32 CompletedPhaseTwoMelee = 0;
	int32 NextAreaAfterMelee = 2;
	// 只在本次攻击开始时选择防御指令组合，后续提示与判定共用结果
	void SelectRuntimeResponsePattern();
	const FAttackImpactMarker& GetImpactMarker(int32 Index) const;
	float GetImpactTime(int32 Index) const;
	int32 GetImpactCount() const;
	// 启动攻击蒙太奇并恢复到音乐时间对应的动画位置
	bool StartPlayback(double AudioNowSeconds);
	// 更新追踪目标，出刀锁定后保留既定方向
	void UpdateWarpTarget(float AnimationPosition);
	// 移除攻击使用的动作对齐目标
	void ClearWarping();
	// 确定瞬移落点，后续按已锁定的位置执行
	bool LockBlinkDestination();
	// 清除本次瞬移的目标、预警和执行状态
	void ClearBlink();
	// 根据音乐拍点调整角色火焰强度
	void UpdateBeatFirePulse(const UBeatClockSubsystem* Clock);
	// 更新强拍攻击的蓄力火焰与身体表现
	void UpdateStrongBeatPresentation(const UBeatClockSubsystem* Clock);
	// 恢复强拍演出修改的身体与特效状态
	void ResetStrongBeatPresentation();
	// 更新并衰减护甲材质的拍点与受击效果
	void UpdateBeatOverlay(const UBeatClockSubsystem* Clock);
	// 记录一次材质闪光的强度和时间
	void TriggerBeatOverlay(bool bStrong, double AudioNow);
	// 在命中窗口内检查刀刃路径，先记录提前接触，再按命中时刻结算
	void UpdateDamageSweep(double AudioNow);
	// 窗口结束仍未接触时进行躲避或教程判定，不按真实接触扣血
	void ResolveUncontactedDamageWindow(int32 ImpactIndex);
	// 查找用于刀刃采样的武器网格
	UMeshComponent* FindBladeMesh() const;
	// 扣除生命并更新受击表现，死亡时停止当前连招
	void ApplyBossDamageFrom(float Damage, const AActor* DamageSource, bool bForceHeavyBlood = false,
	                         float ExplicitPoise = -1.f, FVector ImpactLocation = FVector::ZeroVector);
	// 减少韧性，达到破防条件时打断攻击并进入硬直
	void ApplyBossPoiseDamage(float Amount, const AActor* DamageSource);
	// 记录受击方向和持续时间，启动方向受击动画
	void StartBossHitReaction(const AActor* DamageSource, float Duration, bool bInterruptAttack);
	// 结束方向受击并恢复行走动画状态
	void EndBossHitReaction();
	// 根据攻击来源确定方向，在命中位置生成血液效果
	void SpawnBossBlood(const AActor* DamageSource, bool bHeavy, FVector ImpactLocation) const;
	// 确定本次连招哪些命中使用慢放表现
	void SelectRuntimeCinematicAccents();
	// 到达配置时刻后启动慢放，并在结束条件满足时恢复
	void UpdateCinematicAccent(UBeatClockSubsystem* Clock, double AudioNow);
	// 恢复音乐和角色速度，再结束本次慢放
	void EndCinematicAccent(bool bImmediate);
	// 记录实际交锋位置并通知镜头表现
	void TriggerCinematicClash(int32 ImpactIndex, const FVector& ClashLocation, bool bStrongBeat);

	// 排队表示已经确定攻击拍点但尚未播放，与正在播放分开
	bool bQueued = false;
	bool bPlaying = false;
	bool bLastAttackCompleted = false;
	TArray<FAttackImpactMarker> RuntimeImpactMarkers;
	TArray<bool> RuntimeCinematicEnabled;
	int32 ActiveCinematicImpactIndex = INDEX_NONE;
	bool bCinematicAccentActive = false;
	TWeakObjectPtr<AActor> CinematicPlayerActor;
	float SavedBossCustomTimeDilation = 1.f;
	float SavedPlayerCustomTimeDilation = 1.f;
	double CinematicActorSlowEndWorldSeconds = -1000.0;
	double CinematicAccentEndWorldSeconds = -1000.0;
	FName ActiveResponsePattern = NAME_None;
	int32 FirstImpactBeat = 0;
	int32 NextImpactIndex = 0;
	int32 NextBladeSoundIndex = 0;
	TWeakObjectPtr<UAudioComponent> ActiveBladeVoice;
	double ScheduledStartAudioSeconds = 0.0;
	mutable FBossOpening CachedTail; // 连招结束后仍保留最后一段追击破绽
	double ScheduledFirstImpactAudioSeconds = 0.0;
	FBeatBlinkSettings BlinkSettings;
	bool bBlinkRequested = false;
	bool bBlinkWarningShown = false;
	bool bBlinkExecuted = false;
	bool bStrongBeatThisAttack = false;
	bool bStrongBeatChargeStarted = false;
	FVector LockedBlinkLocation = FVector::ZeroVector;
	float LockedBlinkGroundZ = 0.0f;
	FRotator LockedBlinkRotation = FRotator::ZeroRotator;
	bool bNextBlinkOnRight = false;
	float LastRootMotionPosition = 0.0f;
	TWeakObjectPtr<USkeletalMeshComponent> RootMotionMesh;
	TWeakObjectPtr<USkeletalMeshComponent> LocomotionMesh;
	float LastLocomotionSpeed = 0.0f;
	bool bLastLocomotionBlocked = false;
	double LocomotionResumeWorldSeconds = 0.0;
	double LastLocomotionLogWorldSeconds = -1.0e9;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimSequence>> DirectionalWalkAnimations;
	float LocomotionAnimationTime = 0.0f;
	TWeakObjectPtr<AActor> AttackTarget;
	bool bBossHitReacting = false;
	double BossHitReactionEndWorldSeconds = -1.0;
	double LastBossPoiseDamageWorldSeconds = -1000.0;

	struct FFirePulseTarget
	{
		TWeakObjectPtr<UNiagaraComponent> Component;
		FVector BaseScale = FVector::OneVector;
	};

	TArray<FFirePulseTarget> FirePulseTargets;

	struct FSwordGlowTarget
	{
		TWeakObjectPtr<UMaterialInstanceDynamic> Material;
		float BaseIntensity = 20.f;
	};

	TArray<FSwordGlowTarget> SwordGlowTargets;
	TArray<TWeakObjectPtr<UMeshComponent>> BeatOverlayMeshes;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BeatOverlayInstance;
	double LastStrongOverlayAudioSeconds = -1.0e9;
	double LastWeakOverlayAudioSeconds = -1.0e9;
	TWeakObjectPtr<UPointLightComponent> SwordChargeLight;
	TWeakObjectPtr<UNiagaraComponent> OriginWarningFX;
	TWeakObjectPtr<UNiagaraComponent> LandingWarningFX;
	UPROPERTY(Transient)
	TObjectPtr<UBossMotionWarpingComponent> WarpComponent;
	// 动作对齐：当前窗口与本次出刀锁定的位置
	int32 ActiveWarpWindowIndex = INDEX_NONE;
	FVector WarpAnchorLocation = FVector::ZeroVector;
	FTransform CommittedWarpTarget = FTransform::Identity;
	FVector CommittedPlayerLocation = FVector::ZeroVector;
	// 出刀方向一旦锁定，不再跟随玩家横移旋转整个拳路
	FVector CommittedStrikeDirection = FVector::ZeroVector;
	bool bStrikeCommitted = false;
	FBossOpening SidestepOpening;
	// 当前伤害窗口与待处理窗口分别记录，掉帧时可以逐个补完窗口
	int32 ActiveDamageImpactIndex = INDEX_NONE;
	int32 NextDamageWindowIndex = 0;
	// 同一窗口只结算一次，调用玩家伤害逻辑前先置位
	bool bActiveDamageResolved = false;
	bool bActiveDamageThreatened = false;
	// 提前接触先记下目标位置，到拍点才结算；目标已离开原位置时撤销记录
	bool bContactAtMarker = false;
	FVector EarlyContactTargetLocation = FVector::ZeroVector;
	// 上一帧刀刃位置用于连续轨迹检测，不只是检测当前帧的静态位置
	FVector PreviousBladeTraceLocation = FVector::ZeroVector;
	FVector ClosestBladeFeedbackLocation = FVector::ZeroVector;
	FVector ClosestBladeAttackDirection = FVector::ForwardVector;
	float ClosestBladeFeedbackDistance = BIG_NUMBER;
	TWeakObjectPtr<UMeshComponent> BladeTraceMesh;
};
