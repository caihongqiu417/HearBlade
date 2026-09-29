#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/PlayerAttackSet.h"
#include "PlayerBeatAttackComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPlayerAttackJudged, EPlayerAttackGrade, Grade, float, ErrorMs, FName, Attack);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPlayerAttackHit, AActor*, Target, float, Damage, EPlayerAttackGrade, Grade);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPlayerResonanceChanged, EPlayerResonanceState, State);

// One of your hits that landed inside a boss opening (drawn on the Blade Score).
struct FOpeningHitMark { double Audio = 0.; bool bGap = false; };

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class HEARBLADE_API UPlayerBeatAttackComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPlayerBeatAttackComponent();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player Attack") TObjectPtr<UPlayerAttackSet> AttackSet;
	// Heavy (RMB / gamepad top) ordinary attacks: the 4-hit Katana combo route.
	// RMB still blocks inside defense windows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Attack") bool bHeavyAttacksEnabled = true;
	UPROPERTY(BlueprintReadOnly) EPlayerResonanceState ResonanceState = EPlayerResonanceState::Filling;
	UPROPERTY(BlueprintReadOnly) EPlayerAttackGrade LastGrade = EPlayerAttackGrade::Bad;
	UPROPERTY(BlueprintReadOnly) float LastErrorMs = 0.f;
	UPROPERTY(BlueprintReadOnly) FString InputSequence;
	UPROPERTY(BlueprintReadOnly) FName CurrentAttack;
	UPROPERTY(BlueprintReadOnly) double LastJudgedWorldSeconds = -1000.;
	UPROPERTY(BlueprintReadOnly) float LastResonanceGain = 0.f;
	UPROPERTY(BlueprintReadOnly) double LastHitWorldSeconds = -1000.;
	// Comic impact presentation read by the HUD (Hi-Fi Rush onomatopoeia, DaD impact frame).
	FVector ImpactFxWorld = FVector::ZeroVector;
	double ImpactFxWorldTime = -1000.;
	FString ImpactFxWord;
	float ImpactFxPower = 1.f;
	bool bImpactFxHeavy = false, bImpactFxFrame = false, bImpactFxPerfect = false;
	bool bImpactFxLaunch = false; // heavy 3rd hit (HHH) launches the boss
	// Openings (Blade Score): hits inside a boss opening, and the last FILL.
	// Gap (between two notes of a boss combo) x2 damage/resonance, +50% poise;
	// Tail (after the combo / stagger) x1.2. Nothing is ever reduced outside.
	TArray<FOpeningHitMark> OpeningHitMarks;
	double FillWorldTime = -1000.;
	double FillAudio = 0.;
	bool bFillGap = false;
	double LastOpeningHitWorldTime = -1000.;
	bool bLastOpeningHitGap = false;
	// Comic explosion at the blade tip on heavy / finisher contact.
	FVector TipBurstWorld = FVector::ZeroVector;
	double TipBurstWorldTime = -1000.;
	UPROPERTY(BlueprintAssignable) FPlayerAttackJudged OnAttackJudged;
	UPROPERTY(BlueprintAssignable) FPlayerAttackHit OnAttackHit;
	UPROPERTY(BlueprintAssignable) FPlayerResonanceChanged OnResonanceChanged;
	UFUNCTION(BlueprintCallable) bool TryAttack(EPlayerAttackInput Input);
	UFUNCTION(BlueprintCallable) bool TryActivateResonance();
	UFUNCTION(BlueprintPure) bool IsAttacking() const { return !CurrentAttack.IsNone(); }
	bool IsPlayingAttackMotion() const { return IsAttacking() && !bAttackVisualEnded; }
	UFUNCTION(BlueprintPure) bool IsResonanceActive() const { return ResonanceState == EPlayerResonanceState::Active; }
	UFUNCTION(BlueprintPure) float GetRemainingBeats() const;
	float GetMovementScale() const;
	FString GetComboPrompt() const;
	bool CancelForDefense(bool bDodge);
	void CancelAttack();
	void Reset();
	void NotifyInterrupted();
	void ReceiveReward(EPlayerAttackGrade Grade, float Amount, bool bHit);
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
private:
	TWeakObjectPtr<class UPlayerRhythmCombatComponent> Combat;
	TWeakObjectPtr<class UBeatSyncedComboComponent> ResonanceBoss;
	UPROPERTY(Transient) TObjectPtr<class UAnimMontage> AttackMontage;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> HitSound;
	UPROPERTY(EditDefaultsOnly, Category="Attack|Audio") TObjectPtr<class USoundBase> LightSwingSound;
	UPROPERTY(EditDefaultsOnly, Category="Attack|Audio") TObjectPtr<class USoundBase> HeavySwingSound;
	UPROPERTY(EditDefaultsOnly, Category="Attack|Audio") TObjectPtr<class USoundBase> ContactSound;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> StartSound;
	// Light-hit feedback layers (Dead-as-Disco style: every connecting hit is a drum hit).
	UPROPERTY(Transient) TObjectPtr<class USoundBase> BeatKickSound;
	// Hi-Fi-Rush-style per-step hit drums (synthesized, /Game/Audio/SFX/HitRush).
	UPROPERTY(Transient) TArray<TObjectPtr<class USoundBase>> LightHitSounds;
	UPROPERTY(Transient) TArray<TObjectPtr<class USoundBase>> HeavyHitSounds;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> FinisherHitSound;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> KickHitSound;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> OffBeatHitSound;
	void LoadHitRushSounds();
	UPROPERTY(Transient) TObjectPtr<class USoundBase> GoodHitSound;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> SnareSound;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> FinisherSound;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> FinisherPerfectSound;
	UPROPERTY(Transient) TObjectPtr<class UNiagaraSystem> PerfectBurstEffect;
	// Hit-stop on the player's pose, measured on the music clock. The recovery
	// after the freeze is compressed so the next beat cue is never delayed.
	double HitStopStartAudio = -1000.;
	float HitStopSeconds = 0.f;
	float HitStopEase = 2.f; // exponent of the post-contact ease-in
	// Short full-screen punch (chromatic split + vignette) on the follow camera.
	double PunchStartWorld = -1000.;
	float PunchStrength = 0.f;
	float BaseFringe = -1.f, BaseVignette = -1.f;
	void TickScreenPunch();
	UPROPERTY(Transient) TObjectPtr<class UParticleSystem> HitEffect;
	UPROPERTY(Transient) TObjectPtr<class UParticleSystem> TrailEffect;
	UPROPERTY(Transient) TObjectPtr<class UParticleSystemComponent> Trail;
	TWeakObjectPtr<AActor> AttackTarget;
	TSet<TWeakObjectPtr<AActor>> HitActors;
	EPlayerAttackInput BufferedInput = EPlayerAttackInput::Light;
	bool bHasBufferedInput = false;
	bool bHasPendingAttack = false;
	EPlayerAttackInput PendingAttack = EPlayerAttackInput::Light;
	double PendingAttackAudio = 0., PendingAttackDeadline = 0.;
	bool bImpactPresented = false;
	bool bSwingPresented = false;
	bool bRewarded = false;
	bool bAttackVisualEnded = false;
	double ComboInputAudio = 0.;
	// Chain window: the next hit may land on any half-beat slot from the earliest
	// cue (ComboInputAudio) up to LatestCueAudio (the next whole beat, >= 1 beat
	// after this hit), so pressing on every beat keeps the combo going too.
	double LatestCueAudio = 0.;
	double NextStartAllowedAudio = -1000.;
	double BufferedInputTime = 0.;
	// Presses since the last committed attack. >1 = mashing: the swing still
	// fires on the beat, but it is judged Bad (no resonance, no on-beat FX).
	int32 PressCountSinceCommit = 0;
	double StartAudio = 0., ImpactAudio = 0., EndAudio = 0., LastAudio = 0.;
	double ArmedBeat = 0., ActiveStartBeat = 0.;
	double LastChainAudio = -1000.;
	double LastCommittedImpactBeat = -1000.;
	double RecoveryUntilAudio = -1000.;
	float AttackPlayRate = 1.15f;
	double SwingStartAudio = 0.;
	double LinkAudio = 0.;
	FName ChainNode;
	uint64 AttackSerial = 0;
	float LastRootMotionPosition = 0.f;
	FVector RootProbeStart = FVector::ZeroVector;
	FVector LastBladeBase = FVector::ZeroVector, LastBladeTip = FVector::ZeroVector;
	FVector LastLegA[2], LastLegB[2]; // calf -> ball, left/right (kick hit test)
	void SampleLegs(FVector OutA[2], FVector OutB[2]) const;
	void RegisterOpeningHit(const struct FBossOpening& Opening, bool bGap);
	double OpeningKey = -1.;
	int32 OpeningHits = 0;
	bool bOpeningFilled = false;
	void SetResonanceState(EPlayerResonanceState State);
	void EndResonance();
	// bChained: consume the buffered press as the next step of the current route.
	// TargetImpactAudio: music time the blade must connect (a beat). <0 = root press:
	// connect on the coming beat if the press was slightly early, else ~1 frame later.
	bool StartAttack(EPlayerAttackInput Input, double InputAudio, bool bChained = false, double TargetImpactAudio = -1.);
	// Seconds from a node's strike pose to contact (the only part played after the press).
	static double StrikeLead(const FPlayerAttackNode& Node);
	double ChainLateSeconds() const;
	// Hi-Fi-Rush heavy: the beats are spent BEFORE the hit. Press on a beat, snap
	// into the chamber pose, hold it (charging, a cue on each beat), then a very
	// fast strike connects WindupBeats later. The next heavy is pressed on the hit.
	static double WindupBeats(const FPlayerAttackNode& Node, bool bResonance);
	bool bWindupAttack = false;
	double ChamberStartAudio = 0., ChamberEndAudio = 0., SnapStartAudio = 0.;
	float HoldPosition = 0.f;
	// Ideal press time for a heavy follow-up (the current hit for heavy->heavy).
	double HeavyPressAudio = 0.;
	double NextChargeCueAudio = -1.;
	float WindupPosition(double Now, double SampleNow) const;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> ChargeSound;
	// Sampled slice of the node's clip: starts at the strike pose, and ends in the
	// next slice's strike pose when the combo flows on in the same clip.
	float ActiveClipStart = 0.f, ActiveClipEnd = 0.f;
	void TickAttack(double Now, float Delta);
	void SweepBlade(double Now);
	void PresentImpact(bool bHit, FVector Location, AActor* Target = nullptr);
	AActor* FindTarget() const;
	class UBeatClockSubsystem* Clock() const;
	void TickRuntimeProbe();
	void TickInputProbe();
	void TickDefenseContactProbe();
	void TickComboProbe();
	uint64 ComboProbeObservedSerial=0;
	bool bComboProbeFailed=false;
	int32 DefenseProbeMask=0;
	double DefenseProbeLastJudgment=-1., DefenseProbeRelease=-1.;
	UPROPERTY(Transient) TObjectPtr<class UAttackDataAsset> DefenseProbeAttack;
	int32 InputProbeStep=0;
	double InputProbeStart=-1.;
	bool bProbeStarted=false;
	int32 ProbeStage=0, ProbeInputs=0;
	double ProbeNextInputBeat=0., ProbeStartWorld=0.;
	float ProbeInitialBossHealth=0.f;
};
