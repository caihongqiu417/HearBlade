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

// A read-only snapshot used by player-facing HUD. Times come from the same audio clock
// that drives the combo, so the cue never invents a separate visual rhythm timer.
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

// An attack opening in the boss's rhythm (audio-clock seconds). Gap = the rest
// between two notes of one boss combo (the one we most want players to hit);
// Tail = after the combo's last note, or while the boss is staggered.
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
	// Attack openings around the current combo (see FBossOpening), oldest first.
	void GetOpenings(TArray<FBossOpening>& Out) const;
	bool GetOpeningAt(double AudioSeconds, FBossOpening& Out) const;
	void ReceivePlayerStrike(float Damage, float PoiseDamage, AActor* Source, bool bHeavy,
	                         FVector ImpactLocation, bool bUppercut = false);
	// Presentation-only feedback for an ordinary player hit: armour flash, a short
	// shove away from the player and a brief animation freeze (hit-stop).
	// Strength ~0.5 (off-beat) .. 1.6 (on-beat finisher).
	void PlayPlayerHitFeedback(AActor* Source, float Strength, float FreezeSeconds);
	// Hit shudder (Sakurai-style hitstop shake): the struck body vibrates along the
	// hit direction while frozen, decaying to rest.
	FTimerHandle HitShakeTimer;
	FVector HitShakeBase = FVector::ZeroVector, HitShakeDir = FVector::ZeroVector;
	float HitShakeElapsed = 0.f, HitShakeDuration = 0.f, HitShakeAmplitude = 0.f;
	bool bHitShaking = false;
	void TickHitShake();
	void StartUppercutFeedback(AActor* Source);
	void TickUppercutFeedback();
	void EndUppercutFeedback();
	FTimerHandle UppercutTimer;
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FVector> UppercutMeshBases;
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FRotator> UppercutMeshRots;
	double UppercutStartWorldSeconds = -1000.;
	void BeginPlayerResonance(double StartBeat, AActor* Player);
	void EndPlayerResonance();

	bool IsPlayerResonanceActive() const
	{
		return bPlayerResonance;
	}

	void TickPlayerResonance();
	bool bPlayerResonance = false;
	double PlayerResonanceStartBeat = 0.;
	int32 PlayerResonanceAttempt = 0;
	UPROPERTY(Transient) TObjectPtr<UAttackDataAsset> ResonanceCounterAttack;
	// Tutorial sparring: one slow single-note attack asking for Response, landing on TargetBeat.
	bool QueuePracticeAttack(ERhythmResponseType Response, double TargetBeat, AActor* Target);
	UPROPERTY(Transient) TObjectPtr<UAttackDataAsset> PracticeAttack;
	// Tutorial practice note in flight: always judged, however far away the player stands.
	bool bPracticeNote = false;
	// Set by the tutorial while sparring: hits react but deal no health damage.
	bool bTutorialNoDamage = false;
	// Beat backstep after a finished combo (see BossBackstep.cpp).
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
	void TickBackstep(float DeltaTime);
	// Light-combo kick: shove the boss back a step (eased, collision-swept).
	void StartKickKnockback(const AActor* Source, float Distance, float Seconds);
	void TickKnockback(float DeltaTime);
	// Small hit shader: a short, dim pulse on the weak beat overlay channel.
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
	void UpdateGroundBurst(double AudioNow);
	void ResolveGroundBurst(int32 ImpactIndex, double AudioNow);
	void ClearGroundBurst();
	bool GetGroundBurstPreview(FVector& OutCenter, float& OutRadius, float& OutProgress) const;

	bool HasResolvedGroundBurst() const
	{
		return bGroundBurstResolved;
	}

	bool IsGroundBurstRecovery() const;
	float GetGroundBurstRemainingBeats() const;
	bool TryRecoveryStrike(AActor* Player);
	UPROPERTY(EditAnywhere, Category = "二阶段|破绽") float GroundBurstRecoveryBeats = 2.f;
	double GroundBurstRecoveryEndAudio = -1.;
	double LastRecoveryStrikeAudio = -100.;
	float ActiveGroundBurstRadius = 0.f;
	bool bGroundBurstHarmless = false;
	float GetPhaseTransitionProgress() const;
	void RecordPhaseTwoAttackOutcome(bool bCompleted);
	bool bPhaseTwoAttackCounted = false;
	bool TickComboPoseAudit();
	int32 PoseAuditCaptured = INDEX_NONE;
	TWeakObjectPtr<AActor> PoseAuditCamera;
	bool bGroundBurstTestInjected = false;
	int32 GroundBurstBoundaryCrossings = 0;
	bool GetRecognitionCue(FAttackRecognitionInfo& Info, FVector& Location, float& Progress,
	                       double& Key) const;
	bool GetDodgeOnlyPath(double ImpactAudioSeconds, FVector& Start, FVector& Direction, float& Length) const;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss生命", meta = (ClampMin = "1"))
	float MaxBossHealth = 2700.f;
	// Area attacks are escaped by dodging on the detonation beat, not by outrunning
	// a small circle: the circle may exceed walking reach, a dodge pressed in
	// [-0.35s, +LateGrace] around the hit passes through it.
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

	UFUNCTION(BlueprintCallable, Category = "Boss生命") void ApplyBossDamage(float Damage);
	UFUNCTION()
	void ReceiveBossDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
	                       AController* InstigatedBy, AActor* DamageCauser);

	// Hidden poise prevents light attacks from stun-locking the Boss. Idle hits react
	// immediately; attacks are interrupted only by a counter or a poise break.
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

	// Optional animation graph used for idle/walk and montage blending.
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

	// Call this from actual damage confirmation, not from an animation impact marker.
	UFUNCTION(BlueprintCallable, Category = "节拍连招|相机") void NotifyConfirmedHit()
	{
		OnConfirmedHit.Broadcast();
	}

	UFUNCTION(BlueprintCallable, Category = "节拍连招")
	bool QueueCombo();

	// 行为树或蓝图在决定攻击时调用；使用传入的独立动作数据，并对齐下一可用拍
	UFUNCTION(BlueprintCallable, Category = "节拍连招")
	bool QueueAttack(UAttackDataAsset* InAttackData);

	// Must be called before QueueAttack. The first impact and the blink then share one clock timestamp.
	void ConfigureNextBlink(const FBeatBlinkSettings& Settings);

	double GetScheduledFirstImpactAudioSeconds() const
	{
		return ScheduledFirstImpactAudioSeconds;
	}

	double GetScheduledStartAudioSeconds() const
	{
		return ScheduledStartAudioSeconds;
	}

	// Runtime timing for the next unresolved blade contact. Used by player-facing rhythm cues and tests.
	bool GetNextImpactDodgeTiming(double& OutImpactAudioSeconds, float& OutIdealLeadSeconds) const;
	void GetUpcomingImpactCues(TArray<FUpcomingImpactCue>& OutCues, int32 MaxCount = 4,
	                           bool bIncludeRecent = false) const;

	UFUNCTION(BlueprintCallable, Category = "节拍连招")
	void SetAttackTarget(AActor* InTarget);

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

	float GetWalkClipLengthSeconds() const;

	// If no marker is explicitly authored, selected final hits can still receive
	// an occasional accent. Disable this to use marker settings exclusively.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|关键慢放")
	bool bEnableOccasionalFinalImpactCinematic = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|关键慢放",
	          meta = (ClampMin = "0.0", ClampMax = "1.0",
	                  EditCondition = "bEnableOccasionalFinalImpactCinematic"))
	float FinalImpactCinematicChance = .35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|关键慢放",
	          meta = (EditCondition = "bEnableOccasionalFinalImpactCinematic"))
	FImpactCinematicSettings FinalImpactCinematic;

	// The existing fire effects breathe on the music beat, including while idle.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|节奏感",
	          meta = (ClampMin = "0.0", ClampMax = "0.3"))
	float FireBeatPulseStrength = 0.10f;

	// The first strike of a bar draws the fire into the blade before it lands.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|强拍",
	          meta = (ClampMin = "0.25", ClampMax = "2.0"))
	float StrongBeatChargeBeats = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|强拍",
	          meta = (ClampMin = "1.0", ClampMax = "6.0"))
	float StrongBeatSwordGlowMultiplier = 3.5f;

	// Additive outline and moving energy bands drawn on top of the armor.
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

	// Extra gain for short wind sounds against the full-band music mix.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|强弱拍",
	          meta = (ClampMin = "0.5", ClampMax = "4.0"))
	float BladeSfxGain = .72f;

	// Start the air sweep before the animation's blade impact marker.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|强弱拍",
	          meta = (ClampMin = "0.0", ClampMax = "0.15"))
	float BladeWhooshLeadSeconds = 0.15f;

	// Skip the quiet head of the selected user sound. Combined with the lead,
	// its main air-slice transient lands just before the impact marker.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|强弱拍",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BladeSoundStartTimeSeconds = 0.15f;

	// Brief background music reduction leaves room for the blade transient.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|强拍",
	          meta = (ClampMin = "0.2", ClampMax = "1.0"))
	float StrongBeatMusicGain = .38f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍连招|强拍",
	          meta = (ClampMin = "0.2", ClampMax = "1.0"))
	float WeakBeatMusicGain = .58f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

  private:
	int32 GroundBurstImpact = INDEX_NONE;
	bool bGroundBurstResolved = false;
	FVector GroundBurstCenter = FVector::ZeroVector;
	FVector GroundBurstPreviousLocation = FVector::ZeroVector;
	double GroundBurstPreviousAudio = 0.0;
	double GroundBurstTargetAudio = 0.0;
	double GroundBurstStartAudio = 0.0;
	double GroundBurstExitAudio = -1.0;
	bool bGroundBurstWasInside = false;
	bool bPhaseChangePending = false;
	bool bPhaseTransitionActive = false;
	double PhaseTransitionStartBeat = -1.0;
	double PhaseRequestWorldSeconds = -1.0;
	int32 CompletedPhaseTwoMelee = 0;
	int32 NextAreaAfterMelee = 2;
	void SelectRuntimeResponsePattern();
	const FAttackImpactMarker& GetImpactMarker(int32 Index) const;
	float GetImpactTime(int32 Index) const;
	int32 GetImpactCount() const;
	bool StartPlayback(double AudioNowSeconds);
	void UpdateWarpTarget(float AnimationPosition);
	void ClearWarping();
	bool LockBlinkDestination();
	void ClearBlink();
	void UpdateBeatFirePulse(const UBeatClockSubsystem* Clock);
	void UpdateStrongBeatPresentation(const UBeatClockSubsystem* Clock);
	void ResetStrongBeatPresentation();
	void UpdateBeatOverlay(const UBeatClockSubsystem* Clock);
	void TriggerBeatOverlay(bool bStrong, double AudioNow);
	void UpdateDamageSweep(double AudioNow);
	void ResolveUncontactedDamageWindow(int32 ImpactIndex);
	UMeshComponent* FindBladeMesh() const;
	void ApplyBossDamageFrom(float Damage, const AActor* DamageSource, bool bForceHeavyBlood = false,
	                         float ExplicitPoise = -1.f, FVector ImpactLocation = FVector::ZeroVector);
	void ApplyBossPoiseDamage(float Amount, const AActor* DamageSource);
	void StartBossHitReaction(const AActor* DamageSource, float Duration, bool bInterruptAttack);
	void EndBossHitReaction();
	void SpawnBossBlood(const AActor* DamageSource, bool bHeavy, FVector ImpactLocation) const;
	void SelectRuntimeCinematicAccents();
	void UpdateCinematicAccent(UBeatClockSubsystem* Clock, double AudioNow);
	void EndCinematicAccent(bool bImmediate);
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
	mutable FBossOpening CachedTail; // keeps the last combo's tail alive after it ends
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
