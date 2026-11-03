// 管理生命、防御输入、闪避与每次攻击的应对结果
#include "Combat/Player/PlayerRhythmCombatComponent.h"
#include "Combat/Player/PlayerBeatAttackComponent.h"
#include "Components/CapsuleComponent.h"
#include "Combat/Feedback/TransientCombatFX.h"
#include "Combat/Player/PlayerCombatAnimInstance.h"
#include "HearBladeCharacter.h"
#include "Combat/Boss/BeatSyncedComboComponent.h"
#include "Combat/Camera/BossCombatCameraComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Particles/ParticleSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

UPlayerRhythmCombatComponent::UPlayerRhythmCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	DodgeForwardAnimation =
	    LoadObject<UAnimSequence>(nullptr, TEXT("/Game/SwordAnim/Animation/UE5/roll_front.roll_front"));
	DodgeBackwardAnimation =
	    LoadObject<UAnimSequence>(nullptr, TEXT("/Game/SwordAnim/Animation/UE5/roll_back.roll_back"));
	DodgeLeftAnimation =
	    LoadObject<UAnimSequence>(nullptr, TEXT("/Game/SwordAnim/Animation/UE5/roll_left.roll_left"));
	DodgeRightAnimation =
	    LoadObject<UAnimSequence>(nullptr, TEXT("/Game/SwordAnim/Animation/UE5/roll_right.roll_right"));
	BlockStartAnimation = LoadObject<UAnimSequence>(
	    nullptr,
	    TEXT("/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Defense01_Start.Defense01_Start"));
	BlockLoopAnimation = LoadObject<UAnimSequence>(
	    nullptr,
	    TEXT("/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Defense01_Loop.Defense01_Loop"));
	BlockEndAnimation = LoadObject<UAnimSequence>(
	    nullptr,
	    TEXT("/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Defense01_End.Defense01_End"));
	CounterAnimation = LoadObject<UAnimSequence>(
	    nullptr,
	    TEXT(
	        "/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Defense01_CounterAttack.Defense01_CounterAttack"));
	BlockHitReactAnimation = LoadObject<UAnimSequence>(
	    nullptr,
	    TEXT(
	        "/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Anim_DKM_Hit_Alert_Fwd.Anim_DKM_Hit_Alert_Fwd"));
	BlockImpactEffect = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/SwordAnim/demo/FX/P_Spark.P_Spark"));
	CounterImpactEffect = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/SwordAnim/demo/FX/P_Hit.P_Hit"));
	PerfectHoldShockEffect = LoadObject<UNiagaraSystem>(
	    nullptr, TEXT("/Game/Combat/Feedback/NS_PerfectDodge_Ring.NS_PerfectDodge_Ring"));
	BloodHitEffect = LoadObject<UNiagaraSystem>(
	    nullptr, TEXT("/Game/Blood_VFX_Pack/Particles/Systems/P_Splash_Hit_01.P_Splash_Hit_01"));
	BloodPoiseBreakEffect = LoadObject<UNiagaraSystem>(
	    nullptr,
	    TEXT("/Game/Blood_VFX_Pack/Particles/Systems/P_SplashWithBurst_Hit_01.P_SplashWithBurst_Hit_01"));
}

// 推进闪避和受击，恢复韧性，再尝试执行缓存的防御输入
void UPlayerRhythmCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                 FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bPhaseCinematic)
		return;
	if (GetWorld())
	{
		const double WorldNow = GetWorld()->GetTimeSeconds();
		if (bRecoveryStrikePending && (!bCountering || bHitReacting || bDodging || Health <= 0.f))
			bRecoveryStrikePending = false;
		if (bRecoveryStrikePending && WorldNow - CounterStartWorldSeconds >= .20)
		{
			bRecoveryStrikePending = false;
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
				if (auto* Boss = It->FindComponentByClass<UBeatSyncedComboComponent>();
				    Boss && Boss->TryRecoveryStrike(GetOwner()))
				{
					Resonance = FMath::Min(MaxResonance, Resonance + 8.f);
					LastResonanceGainWorldSeconds = WorldNow;
					// 反击接触声由 PlayResponseFeedback 统一播放，避免同一音效叠加两次

					PlayResponseFeedback(ERhythmResponseType::Counter, ERhythmDodgeJudgment::Good,
					                     It->GetActorLocation(), GetOwner()->GetActorForwardVector(), false);
					break;
				}
		}
		if (bHitReacting)
		{
			if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
			{
				const float TurnSpeed =
				    HitTurnSeconds > KINDA_SMALL_NUMBER ? 1.0f / HitTurnSeconds * 5.0f : 1000.0f;
				Character->SetActorRotation(
				    FMath::RInterpTo(Character->GetActorRotation(), HitFacingRotation, DeltaTime, TurnSpeed));
				if (bHitTurnPending && WorldNow >= HitTurnEndWorldSeconds)
				{
					Character->SetActorRotation(HitFacingRotation);
					bHitTurnPending = false;
					if (UPlayerCombatAnimInstance* Animation =
					        Cast<UPlayerCombatAnimInstance>(Character->GetMesh()->GetAnimInstance()))
						// 前方受击样本让身体向后退
						// 后方受击样本让身体向前倾
						Animation->StartDirectionalHit(
						    FVector2D(1.0f, 0.0f),
						    FMath::Max(0.10f, static_cast<float>(HitReactionEndWorldSeconds - WorldNow)));
				}
			}
			if (WorldNow >= HitReactionEndWorldSeconds)
				FinishHitReaction();
		}
		// 松键先等待最短起手时间，再播放收手动画，避免按得很快时姿态瞬间跳变
		if (bBlockReleasePending && WorldNow - DefenseAnimationStartWorldSeconds >= .22)
		{
			bBlockReleasePending = false;
			DefenseAnimationStartWorldSeconds = WorldNow;
			PlayDefenseAnimation(BlockEndAnimation, false);
		}
		if (bBlocking && BlockStartAnimation &&
		    WorldNow - DefenseAnimationStartWorldSeconds >= BlockStartAnimation->GetPlayLength())
		{
			PlayDefenseAnimation(BlockLoopAnimation, true);
			DefenseAnimationStartWorldSeconds = BIG_NUMBER;
		}
		if (bBlockRecovering && !bBlockReleasePending &&
		    WorldNow - DefenseAnimationStartWorldSeconds >= DefenseAnimationDurationSeconds)
			bBlockRecovering = false;
		if (bCountering && WorldNow - CounterStartWorldSeconds >= CounterDurationSeconds)
			bCountering = false;
		if (bPoiseBroken && WorldNow - PoiseBreakStartWorldSeconds >= PoiseBreakDurationSeconds)
			EndPoiseBreak();
		else if (!bPoiseBroken && Poise < MaxPoise &&
		         WorldNow - LastPoiseDamageWorldSeconds >= PoiseRecoveryDelaySeconds)
			Poise = FMath::Min(MaxPoise, Poise + PoiseRecoveryPerSecond * DeltaTime);
		const auto* Attacks = GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>();
		if (Resonance > 0.0f && Resonance < MaxResonance &&
		    (!Attacks || Attacks->ResonanceState == EPlayerResonanceState::Filling) &&
		    WorldNow - LastResonanceGainWorldSeconds >= ResonanceDecayDelaySeconds)
			Resonance = FMath::Max(0.0f, Resonance - ResonanceDecayPerSecond * DeltaTime);
	}
	// 先更新受击和动作锁定，再尝试消费缓冲输入
	ProcessBufferedInput();
	if (bHUDPreviewMode && GetWorld() && HUDPreviewStep < 6 &&
	    GetWorld()->GetTimeSeconds() >= NextHUDPreviewWorldSeconds)
	{
		static const ERhythmDodgeJudgment PreviewGrades[] = {
		    ERhythmDodgeJudgment::Perfect, ERhythmDodgeJudgment::Good, ERhythmDodgeJudgment::Bad,
		    ERhythmDodgeJudgment::Perfect, ERhythmDodgeJudgment::Good, ERhythmDodgeJudgment::Miss};
		bSubmittingHUDPreviewJudgment = true;
		SubmitDodgeJudgment(PreviewGrades[HUDPreviewStep], -18.0f + HUDPreviewStep * 7.0f,
		                    HUDPreviewStep % 4 == 0);
		bSubmittingHUDPreviewJudgment = false;
		++HUDPreviewStep;
		NextHUDPreviewWorldSeconds = GetWorld()->GetTimeSeconds() + 0.24;
	}
	if (bPerfectFeedbackPreviewMode && PerfectFeedbackPreviewStep < 2 && GetWorld() && GetOwner() &&
	    GetWorld()->GetTimeSeconds() >= NextPerfectFeedbackPreviewWorldSeconds)
	{
		const bool bStrong = PerfectFeedbackPreviewStep % 2 == 1;
		TryDodge(GetOwner()->GetActorRightVector() * (bStrong ? -1.f : 1.f));
		SubmitDodgeJudgment(ERhythmDodgeJudgment::Perfect, bStrong ? -9.f : 14.f, bStrong);
		BroadcastPerfectFeedback(bStrong ? -9.f : 14.f, bStrong,
		                         GetOwner()->GetActorLocation() +
		                             GetOwner()->GetActorForwardVector() * 110.f + FVector(0, 0, 90),
		                         GetOwner()->GetActorRightVector());
		++PerfectFeedbackPreviewStep;
		NextPerfectFeedbackPreviewWorldSeconds = GetWorld()->GetTimeSeconds() + 1.4;
	}
	if (bAutomationTestMode && GetWorld() && GetOwner())
	{
		if (const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>())
		{
			if (bBlocking && AutomationReleaseAudioSeconds > 0.0 &&
			    Clock->GetAudioNowSeconds() >= AutomationReleaseAudioSeconds)
			{
				EndBlock();
				AutomationReleaseAudioSeconds = -1000.0;
			}
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			{
				UBeatSyncedComboComponent* Combo = It->FindComponentByClass<UBeatSyncedComboComponent>();
				TArray<FUpcomingImpactCue> Cues;
				if (!Combo)
					continue;
#if !UE_BUILD_SHIPPING
				if (FParse::Param(FCommandLine::Get(), TEXT("GroundBurstPursuitTest")) &&
				    Combo->IsGroundBurstRecovery())
				{
					if (auto* Character = Cast<ACharacter>(GetOwner()); Character && !IsPerformingAction())
					{
						const FVector Toward =
						    (It->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal2D();
						Character->SetActorRotation(Toward.Rotation());
						if (FVector::Dist2D(It->GetActorLocation(), GetOwner()->GetActorLocation()) > 250.f)
							Character->AddMovementInput(Toward, 1.f, true);
						else
							TryCounter();
					}
					continue;
				}
#endif
				Combo->GetUpcomingImpactCues(Cues, 1);
				if (Cues.IsEmpty())
					continue;
				const FUpcomingImpactCue& Cue = Cues[0];
				// 范围爆炸要求离开危险区域，不能只自动按一次防御键
				if (Cue.DamageShape == EBossAttackDamageShape::GroundBurst)
				{
#if !UE_BUILD_SHIPPING
					const bool bEarlyEscape =
					    FParse::Param(FCommandLine::Get(), TEXT("GroundBurstEarlyTest"));
					const bool bReenter = FParse::Param(FCommandLine::Get(), TEXT("GroundBurstReenterTest"));
					if (FParse::Param(FCommandLine::Get(), TEXT("GroundBurstEscapeTest")) || bEarlyEscape ||
					    bReenter)
					{
						FVector Center;
						float Radius, Progress;
						if (auto* Character = Cast<ACharacter>(GetOwner());
						    Character && Combo->GetGroundBurstPreview(Center, Radius, Progress))
						{
							auto* Movement = Character->GetCharacterMovement();
							const float Boundary =
							    Radius + Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
							if (bReenter &&
							    FVector::Dist2D(Center, Character->GetActorLocation()) > Boundary + 2.f)
								bGroundBurstTestExited = true;
							const float Distance = FMath::Max(
							    0.f, Boundary - FVector::Dist2D(Center, Character->GetActorLocation()));
							const float Speed = FMath::Max(1.f, Movement->MaxWalkSpeed);
							const double Until = Cue.ImpactAudioSeconds - Clock->GetAudioNowSeconds();
							if (FParse::Param(FCommandLine::Get(), TEXT("GroundBurstWalkDodgeTest")) &&
							    Until <= DodgeDurationSeconds + .1 && Until > .1 && Distance > 20.f &&
							    LastDodgeInputAudioSeconds < Cue.ImpactAudioSeconds - 1.)
								TryDodge((Character->GetActorLocation() - Center).GetSafeNormal2D());
							const double Travel = Distance / Speed +
							                      .5 * Speed / FMath::Max(1.f, Movement->MaxAcceleration) +
							                      .08;
							if (Until <= Travel || bEarlyEscape || bReenter)
							{
								const float Direction = bReenter && bGroundBurstTestExited ? -1.f : 1.f;
								if (Distance > 0.f || Direction < 0.f || !bEarlyEscape)
									Character->AddMovementInput(
									    (Character->GetActorLocation() - Center).GetSafeNormal2D(), Direction,
									    true);
							}
						}
					}
#endif
					continue;
				}
				const double SecondsUntilImpact = Cue.ImpactAudioSeconds - Clock->GetAudioNowSeconds();
				const bool bNewImpact = Cue.ImpactAudioSeconds > LastAutomatedImpactAudioSeconds + 0.001;
				if (Cue.RequiredResponse == ERhythmResponseType::HoldBlock &&
				    Cue.HoldMarker == ERhythmHoldMarkerType::End && bBlocking && bNewImpact &&
				    SecondsUntilImpact <= .040 && SecondsUntilImpact >= -.010)
				{
					EndBlock();
					LastAutomatedImpactAudioSeconds = Cue.ImpactAudioSeconds;
					UE_LOG(LogTemp, Display, TEXT("RhythmCombatAutomation: hold release leadMs=%.1f"),
					       SecondsUntilImpact * 1000.0);
					break;
				}
				const double InputLead = Cue.IdealDodgeLeadSeconds;
				if (!bNewImpact || SecondsUntilImpact > InputLead + .055 ||
				    SecondsUntilImpact < InputLead - .055)
					continue;
				bool bIssued = false;
				const ERhythmResponseType TestResponse =
				    Cue.RequiredResponse == ERhythmResponseType::DodgeOrBlock
				        ? (Cue.ImpactIndex % 2 == 0 ? ERhythmResponseType::Block : ERhythmResponseType::Dodge)
				        : Cue.RequiredResponse;
				switch (TestResponse)
				{
				case ERhythmResponseType::Dodge:
				{
					if (bAutomationFailureTestMode && !bAutomationWrongActionIssued)
					{
						bIssued = TryBlockStart();
						if (bIssued)
						{
							bAutomationWrongActionIssued = true;
							AutomationReleaseAudioSeconds = Cue.ImpactAudioSeconds + .035;
							UE_LOG(
							    LogTemp, Display,
							    TEXT("RhythmCombatFailureAutomation: deliberately issued Block for Dodge"));
						}
						break;
					}
					const FVector ToBoss =
					    (It->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal2D();
					bIssued = TryDodge(FVector::CrossProduct(FVector::UpVector, ToBoss));
					break;
				}
				case ERhythmResponseType::Block:
					bIssued = TryBlockStart();
					if (bIssued)
						AutomationReleaseAudioSeconds = Cue.ImpactAudioSeconds + .035;
					break;
				case ERhythmResponseType::Counter:
					bIssued = TryCounter();
					break;
				case ERhythmResponseType::HoldBlock:
					if (Cue.HoldMarker == ERhythmHoldMarkerType::Start)
					{
						bIssued = TryBlockStart();
						if (bIssued && bAutomationFailureTestMode)
						{
							AutomationReleaseAudioSeconds = Cue.ImpactAudioSeconds + .12;
							UE_LOG(LogTemp, Display,
							       TEXT("RhythmCombatFailureAutomation: scheduled early hold release"));
						}
					}
					break;
				}
				if (bIssued)
				{
					LastAutomatedImpactAudioSeconds = Cue.ImpactAudioSeconds;
					UE_LOG(LogTemp, Display,
					       TEXT("RhythmCombatAutomation: response=%s leadMs=%.1f impactAudio=%.3f"),
					       *UEnum::GetValueAsString(Cue.RequiredResponse), SecondsUntilImpact * 1000.0,
					       Cue.ImpactAudioSeconds);
					break;
				}
			}
		}
	}
	if (!bDodging || !GetWorld() || !GetOwner())
		return;
	const float Elapsed = static_cast<float>(GetWorld()->GetTimeSeconds() - DodgeStartWorldSeconds);
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		const float Alpha = FMath::Clamp(Elapsed / DodgeDurationSeconds, 0.0f, 1.0f);
		const float EasedAlpha = 1.0f - FMath::Pow(1.0f - Alpha, 2.4f);
		const float TargetDistance = DodgeDistanceCm * EasedAlpha;
		const float Step = FMath::Max(0.0f, TargetDistance - DodgeDistanceApplied);
		if (Step > KINDA_SMALL_NUMBER)
		{
			FHitResult Hit;
			Character->AddActorWorldOffset(DodgeDirection * Step, true, &Hit);
			DodgeDistanceApplied += Step * (Hit.IsValidBlockingHit() ? Hit.Time : 1.0f);
		}
	}
	// 持续输入方向时，翻滚后段可以提前接跑步，避免动作结束后突然停住
	const AHearBladeCharacter* Player = Cast<AHearBladeCharacter>(GetOwner());
	const FVector Intent = Player ? Player->GetWorldMoveIntent() : FVector::ZeroVector;
	const bool bRunOut = Intent.SizeSquared() > .09f && Elapsed >= DodgeDurationSeconds * .7f;
	if (Elapsed >= DodgeDurationSeconds || bRunOut)
	{
		bDodging = false;
		if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
		{
			// 恢复走路模式，但根位移仍由当前动作与移动动画交接，避免身体位置跳变
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			{
				Movement->SetMovementMode(MOVE_Walking);
				if (Intent.SizeSquared() > .09f)
				{
					const FVector Dir = Intent.GetSafeNormal2D();
					Movement->Velocity = Dir * Movement->MaxWalkSpeed *
					                     (FVector::DotProduct(Dir, DodgeDirection) > .3f ? .85f : .55f);
					if (auto* Instance =
					        Cast<UPlayerCombatAnimInstance>(Character->GetMesh()->GetAnimInstance()))
						Instance->ReleaseCombatAction(.16f);
				}
			}
			UE_LOG(LogTemp, Display,
			       TEXT("RhythmDodgeMotion: completed distance=%.1fcm meshRelative=(%.1f,%.1f,%.1f)"),
			       FVector::Dist2D(DodgeStartLocation, Character->GetActorLocation()),
			       Character->GetMesh()->GetRelativeLocation().X,
			       Character->GetMesh()->GetRelativeLocation().Y,
			       Character->GetMesh()->GetRelativeLocation().Z);
		}
		return;
	}
}

// 缓存角色与反馈组件，准备防御资源和初始战斗状态
void UPlayerRhythmCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	auto LoadHit = [](const TCHAR* Name)
	{
		return LoadObject<USoundBase>(nullptr,
		                              *FString::Printf(TEXT("/Game/Audio/SFX/HitRush/%s.%s"), Name, Name));
	};
	DodgeSwishSound = LoadHit(TEXT("SW_HitRush_WhooshLight"));
	CounterSwishSound = LoadHit(TEXT("SW_HitRush_WhooshHeavy"));
	GuardUpSound = LoadHit(TEXT("SW_HitRush_HoldHit"));
	CounterStrikeSound = LoadHit(TEXT("SW_HitRush_Counter"));
	// 兼容旧蓝图保存的生命值配置，开局至少使用 500 上限并恢复满血
	MaxHealth = FMath::Max(MaxHealth, 500.f);
	Health = MaxHealth;
	// 旧蓝图可能保留旧默认值，在开局统一限制取消窗口和缓存时长
	// 这里会覆盖超出限制的配置，编辑器里填写的数值不一定原样生效
	DodgeChainStartSeconds = FMath::Min(DodgeChainStartSeconds, .12f);
	CounterCancelStartSeconds = FMath::Min(CounterCancelStartSeconds, .12f);
	InputBufferSeconds = FMath::Max(InputBufferSeconds, .26f);
	PerfectWindowMs = FMath::Max(PerfectWindowMs, 80.f);
	GoodWindowMs = FMath::Max(GoodWindowMs, 160.f);
	Health = FMath::Clamp(Health, 0.0f, MaxHealth);
	Poise = FMath::Clamp(Poise, 0.0f, MaxPoise);
	LastResonanceGainWorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	bAutomationFailureTestMode = FParse::Param(FCommandLine::Get(), TEXT("RhythmCombatFailureTest"));
	bAutomationTestMode =
	    FParse::Param(FCommandLine::Get(), TEXT("RhythmCombatAutoTest")) || bAutomationFailureTestMode;
	if (bAutomationTestMode)
		UE_LOG(LogTemp, Display, TEXT("RhythmCombatAutomation: enabled for %s"), *GetNameSafe(GetOwner()));
	if (FParse::Param(FCommandLine::Get(), TEXT("RhythmHUDPreview")))
	{
		Health = MaxHealth * 0.72f;
		bHUDPreviewMode = true;
		NextHUDPreviewWorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() + 0.5 : 0.5;
		OnHealthChanged.Broadcast(Health, MaxHealth);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("PerfectDodgePreview")))
	{
		bPerfectFeedbackPreviewMode = true;
		NextPerfectFeedbackPreviewWorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() + 1.0 : 1.0;
	}
}

// 生命与伤害

// 扣除生命并限制数值范围
void UPlayerRhythmCombatComponent::ApplyCombatDamage(float Damage)
{
	if (Damage <= 0.0f || Health <= 0.0f)
		return;
	Health = FMath::Clamp(Health - Damage, 0.0f, MaxHealth);
	UE_LOG(LogTemp, Display, TEXT("PlayerHealth: %.1f/%.1f%s"), Health, MaxHealth,
	       Health <= 0.f ? TEXT(" DEFEATED - Enter to retry") : TEXT(""));
	ComboCount = 0;
	OnHealthChanged.Broadcast(Health, MaxHealth);
}

// 恢复生命，不超过生命上限
void UPlayerRhythmCombatComponent::RestoreHealth(float Amount)
{
	if (Amount <= 0.0f)
		return;
	Health = FMath::Clamp(Health + Amount, 0.0f, MaxHealth);
	OnHealthChanged.Broadcast(Health, MaxHealth);
}

// 结算范围攻击伤害，并按爆发位置产生受击反应
void UPlayerRhythmCombatComponent::ApplyGroundBurstDamage(float Damage, const FVector& Center)
{
	if (Health <= 0.f || Damage <= 0.f)
		return;
	ApplyCombatDamage(Damage);
	const FVector Direction = (GetOwner()->GetActorLocation() - Center).GetSafeNormal2D();
	StartHitReaction(GetOwner()->GetActorLocation(), Direction, false, Center);
	UE_LOG(LogTemp, Display, TEXT("GroundBurst: player damaged %.1f (area requires spatial exit)"), Damage);
}

// 记录踩拍逃离奖励并增加共鸣
void UPlayerRhythmCombatComponent::AwardRhythmicEscape(float Amount, float ErrorSeconds)
{
	if (Health <= 0.f || !GetWorld() || !GetOwner())
		return;
	const float PreviousResonance = Resonance;
	Resonance = FMath::Clamp(Resonance + Amount, 0.f, MaxResonance);
	LastResonanceGainWorldSeconds = GetWorld()->GetTimeSeconds();
	LastRhythmicEscapeWorldSeconds = LastResonanceGainWorldSeconds;
	LastRhythmicEscapeGain = Resonance - PreviousResonance;
	FRhythmSuccessFeedbackData Feedback;
	Feedback.Response = ERhythmResponseType::Dodge;
	Feedback.Grade = ERhythmDodgeJudgment::Perfect;
	Feedback.PreviousCombo = Feedback.ComboCount = ComboCount;
	Feedback.FeedbackTier = GetFeedbackTierForCombo(ComboCount);
	// 范围攻击的踩拍逃脱使用强化表现，作为独立的空间躲避奖励
	Feedback.bSuccess = Feedback.bPerfect = true;
	Feedback.bStrongBeat = true;
	Feedback.bKeyPerfect = true;
	Feedback.WorldLocation = GetOwner()->GetActorLocation();
	Feedback.AttackDirection = GetOwner()->GetActorForwardVector();
	Feedback.EventId = ++FeedbackEventSerial;
	OnRhythmSuccessFeedback.Broadcast(Feedback);
	PlayResponseFeedback(ERhythmResponseType::Dodge, ERhythmDodgeJudgment::Perfect, Feedback.WorldLocation,
	                     Feedback.AttackDirection, true);
	// 空间逃脱不再额外提交一次防御评级，也不重复增加连击数
	UE_LOG(LogTemp, Display, TEXT("GroundBurst: rhythmic escape reward=%.1f errorMs=%.1f"), Amount,
	       ErrorSeconds * 1000.f);
}

// 防御评级与连击

// 保存评级结果，更新连击与共鸣，并通知反馈组件
void UPlayerRhythmCombatComponent::SubmitDodgeJudgment(ERhythmDodgeJudgment Grade, float SignedErrorMs,
                                                       bool bStrongBeat)
{
	if (bHUDPreviewMode && HUDPreviewStep >= 6 && !bSubmittingHUDPreviewJudgment)
		return;
	LastJudgment = Grade;
	LastJudgmentErrorMs = SignedErrorMs;
	LastJudgmentWorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Grade == ERhythmDodgeJudgment::Perfect || Grade == ERhythmDodgeJudgment::Good ||
	    Grade == ERhythmDodgeJudgment::GoodHold)
	{
		++ComboCount;
		const float BaseGain = Grade == ERhythmDodgeJudgment::Perfect
		                           ? (LastResponse == ERhythmResponseType::Counter ? 8.f : 4.f)
		                           : 2.f;
		const float ComboMultiplier = ComboCount >= 8 ? 1.5f : (ComboCount >= 4 ? 1.25f : 1.0f);
		Resonance = FMath::Clamp(Resonance + BaseGain * ComboMultiplier * (bStrongBeat ? 1.25f : 1.0f), 0.0f,
		                         MaxResonance);
		LastResonanceGainWorldSeconds = LastJudgmentWorldSeconds;
	}
	else if (Grade == ERhythmDodgeJudgment::Bad || Grade == ERhythmDodgeJudgment::Miss)
	{
		ComboCount = 0;
	}
	else if (Grade == ERhythmDodgeJudgment::PerfectHold)
	{
		++ComboCount;
		Resonance = FMath::Clamp(Resonance + 4.0f, 0.0f, MaxResonance);
		LastResonanceGainWorldSeconds = LastJudgmentWorldSeconds;
	}
	else if (Grade == ERhythmDodgeJudgment::HoldBreak)
	{
		ComboCount = 0;
	}
	OnDodgeJudged.Broadcast(Grade, SignedErrorMs, ComboCount);
}

// 根据连续成功次数返回反馈档位
int32 UPlayerRhythmCombatComponent::GetFeedbackTierForCombo(int32 Combo)
{
	return Combo >= 16 ? 3 : Combo >= 8 ? 2 : Combo >= 4 ? 1 : 0;
}

// 清空本轮防御、受击、评级和输入历史
void UPlayerRhythmCombatComponent::ResetCombatState()
{
	if (auto* Attacks = GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>())
		Attacks->Reset();
	Health = MaxHealth;
	Resonance = 0.0f;
	ComboCount = 0;
	LastJudgment = ERhythmDodgeJudgment::None;
	LastJudgmentErrorMs = 0.0f;
	LastJudgmentWorldSeconds = -1000.0;
	LastResonanceGainWorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	ActiveHoldGroup = NAME_None;
	bActiveHoldBroken = false;
	ReportedHoldBreakGroups.Reset();
	BufferedAction = EBufferedRhythmAction::None;
	bPoiseBroken = false;
	FinishHitReaction();
	Poise = MaxPoise;
	OnHealthChanged.Broadcast(Health, MaxHealth);
	BlockInputHistory.Reset();
	CounterInputHistory.Reset();
}

// 闪避请求与动作

// 检查当前状态，决定立即闪避还是保留输入等待解锁
bool UPlayerRhythmCombatComponent::TryDodge(FVector WorldDirection)
{
	if (Health <= 0.f)
		return false;
	if (auto* Attacks = GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>();
	    Attacks && !Attacks->CancelForDefense(true))
		return false;
	if (bPhaseCinematic)
		return false;
	if (!GetWorld() || !GetOwner())
		return false;
	const double InputAudioSeconds = GetCurrentInputAudioSeconds();
	if (BufferWhilePoiseLocked(EBufferedRhythmAction::Dodge, InputAudioSeconds, WorldDirection))
		return true;
	if (bHitReacting)
		FinishHitReaction();
	if (bPoiseBroken)
		EndPoiseBreak();
	if (bBlocking)
		EndBlock();
	if (bCountering)
	{
		const double Elapsed = GetWorld()->GetTimeSeconds() - CounterStartWorldSeconds;
		if (Elapsed < CounterCancelStartSeconds)
		{
			BufferInput(EBufferedRhythmAction::Dodge, InputAudioSeconds, WorldDirection);
			return true;
		}
		bCountering = false;
	}
	if (bDodging)
	{
		const double Elapsed = GetWorld()->GetTimeSeconds() - DodgeStartWorldSeconds;
		if (Elapsed < DodgeChainStartSeconds)
		{
			BufferInput(EBufferedRhythmAction::Dodge, InputAudioSeconds, WorldDirection);
			return true;
		}
		UE_LOG(LogTemp, Verbose, TEXT("RhythmDodgeMotion: chained after %.3fs"), Elapsed);
	}
	return StartDodge(WorldDirection, InputAudioSeconds);
}

// 记录原始按键时间，启动翻滚动画与带碰撞的位移
bool UPlayerRhythmCombatComponent::StartDodge(FVector WorldDirection, double InputAudioSeconds)
{
	if (!GetWorld() || !GetOwner())
		return false;
	if (auto* Attack = GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>();
	    Attack && !Attack->CancelForDefense(true))
		return false;
	if (bPoiseBroken)
		EndPoiseBreak();
	// 翻滚动画已有音效，这里不再叠加另一声起手音效
	BufferedAction = EBufferedRhythmAction::None;
	bBlockRecovering = false;
	bBlockReleasePending = false;
	WorldDirection.Z = 0.0f;
	if (!WorldDirection.Normalize())
		WorldDirection = GetOwner()->GetActorForwardVector().GetSafeNormal2D();
	DodgeDirection = WorldDirection;
	DodgeStartLocation = GetOwner()->GetActorLocation();
	bDodging = true;
	DodgeDistanceApplied = 0.0f;
	++DodgeSequenceId;
	DodgeStartWorldSeconds = GetWorld()->GetTimeSeconds();
	LastDodgeInputAudioSeconds = InputAudioSeconds;

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
		}
		// 翻滚位移由胶囊检测推进，关闭动画根位移，避免同一次移动计算两遍
		Character->SetAnimRootMotionTranslationScale(0.0f);
		const FVector Local = Character->GetActorTransform().InverseTransformVectorNoScale(DodgeDirection);
		UAnimSequence* Chosen = DodgeForwardAnimation;
		if (FMath::Abs(Local.Y) > FMath::Abs(Local.X))
			Chosen = Local.Y >= 0.0f ? DodgeRightAnimation : DodgeLeftAnimation;
		else if (Local.X < 0.0f)
			Chosen = DodgeBackwardAnimation;
		if (Chosen && Character->GetMesh())
		{
			if (auto* Instance = Cast<UPlayerCombatAnimInstance>(Character->GetMesh()->GetAnimInstance()))
				Instance->PlayCombatAction(Chosen, false, DodgeDurationSeconds, 0.f);
		}
	}
	return true;
}

// 统一播放防御动画，并设置循环和动作衔接时间
void UPlayerRhythmCombatComponent::PlayDefenseAnimation(UAnimSequence* Animation, bool bLooping,
                                                        float DesiredDuration)
{
	// 防御已接受输入后，先停止攻击组件写动画位置，再启动防御动画
	if (auto* Attack = GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>())
		Attack->NotifyInterrupted();
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Animation || !Character || !Character->GetMesh())
		return;
	DefenseAnimationDurationSeconds = DesiredDuration > 0.0f ? DesiredDuration : Animation->GetPlayLength();
	Character->SetAnimRootMotionTranslationScale(0.0f);
	Character->ConsumeMovementInputVector();
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		Movement->StopMovementImmediately();
	if (auto* Instance = Cast<UPlayerCombatAnimInstance>(Character->GetMesh()->GetAnimInstance()))
		Instance->PlayCombatAction(Animation, bLooping, DefenseAnimationDurationSeconds);
}

// 格挡开始与结束

// 检查格挡条件，锁定期间先缓存按下操作
bool UPlayerRhythmCombatComponent::TryBlockStart()
{
	if (Health <= 0.f || bPhaseCinematic)
		return false;
	if (auto* Attacks = GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>();
	    Attacks && !Attacks->CancelForDefense(false))
		return false;
	if (bPhaseCinematic)
		return false;
	if (!GetWorld() || bBlocking)
		return false;
	const double InputAudioSeconds = GetCurrentInputAudioSeconds();
	// 按键历史保存原始时间，结算时选择最接近理想时刻的一次
	// 这份历史与等待执行的动作缓存不同，不能互相代替
	BlockInputHistory.Add(InputAudioSeconds);
	if (BlockInputHistory.Num() > 16)
		BlockInputHistory.RemoveAt(0);
	if (BufferWhilePoiseLocked(EBufferedRhythmAction::Block, InputAudioSeconds))
		return true;
	if (bHitReacting)
		FinishHitReaction();
	if (bPoiseBroken)
		EndPoiseBreak();
	if (bDodging)
	{
		const double Elapsed = GetWorld()->GetTimeSeconds() - DodgeStartWorldSeconds;
		if (Elapsed < DodgeChainStartSeconds)
		{
			BufferInput(EBufferedRhythmAction::Block, InputAudioSeconds);
			return true;
		}
		CancelDodgeForTransition();
	}
	if (bCountering)
	{
		const double Elapsed = GetWorld()->GetTimeSeconds() - CounterStartWorldSeconds;
		if (Elapsed < CounterCancelStartSeconds)
		{
			BufferInput(EBufferedRhythmAction::Block, InputAudioSeconds);
			return true;
		}
		bCountering = false;
	}
	return StartBlock(InputAudioSeconds);
}

// 记录格挡按键时刻并进入举盾姿态
bool UPlayerRhythmCombatComponent::StartBlock(double InputAudioSeconds)
{
	if (auto* Attack = GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>();
	    Attack && !Attack->CancelForDefense(false))
		return false;
	BufferedAction = EBufferedRhythmAction::None;
	bBlocking = true;
	bBlockRecovering = false;
	bBlockReleasePending = false;
	bActiveHoldBroken = false;
	DefenseAnimationStartWorldSeconds = GetWorld()->GetTimeSeconds();
	LastBlockStartAudioSeconds = InputAudioSeconds;
	BlockStartWorldSeconds = GetWorld()->GetTimeSeconds();
	if (GuardUpSound)
		UGameplayStatics::PlaySound2D(this, GuardUpSound, .3f, 1.35f); // 轻柔的举盾声音
	PlayDefenseAnimation(BlockStartAnimation, false);
	return true;
}

// 记录松键时刻，处理仍在缓存中的格挡以及持续格挡结算
void UPlayerRhythmCombatComponent::EndBlock()
{
	if (bPhaseCinematic)
		return;
	if (!GetWorld())
		return;
	// 格挡还在缓存中就松键时，连同松开时间一起保存
	// 执行缓存时补上松开，避免短按变成长时间举盾
	if (!bBlocking && BufferedAction == EBufferedRhythmAction::Block)
	{
		bBufferedBlockReleased = true;
		BufferedBlockReleaseAudioSeconds = GetCurrentInputAudioSeconds();
		LastBlockReleaseAudioSeconds = BufferedBlockReleaseAudioSeconds;
		return;
	}
	if (!bBlocking)
		return;
	bBlocking = false;
	if (UBossCombatCameraComponent* Camera = GetOwner()->FindComponentByClass<UBossCombatCameraComponent>())
		Camera->StopHoldFeedback(.06f);
	bBlockRecovering = true;
	const double ReleaseWorldSeconds = GetWorld()->GetTimeSeconds();
	const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	LastBlockReleaseAudioSeconds = Clock ? Clock->GetAudioNowSeconds() : ReleaseWorldSeconds;
	// 格挡循环会改写动画计时标记，松键判断必须使用真实按下时间
	// 不能拿循环动画的标记判断恢复期，否则可能一直无法收盾
	bBlockReleasePending = ReleaseWorldSeconds - BlockStartWorldSeconds < .22;
	if (!bBlockReleasePending)
	{
		DefenseAnimationStartWorldSeconds = ReleaseWorldSeconds;
		PlayDefenseAnimation(BlockEndAnimation, false);
	}
}

// 攻击与防御切换

// 结束允许被攻击替换的防御姿态，为普通攻击腾出动作控制
bool UPlayerRhythmCombatComponent::PrepareForOrdinaryAttack()
{
	if (!GetWorld() || Health <= 0.f || bPhaseCinematic || bDodging || bCountering || bPoiseBroken ||
	    bHitReacting)
		return false;
	// 主动攻击直接清除格挡和待播放的收盾状态
	// 不安排稍后播放的收盾动画，避免它覆盖刚开始的攻击
	bBlocking = false;
	bBlockRecovering = false;
	bBlockReleasePending = false;
	BufferedAction = EBufferedRhythmAction::None;
	bBufferedBlockReleased = false;
	if (auto* Camera = GetOwner()->FindComponentByClass<UBossCombatCameraComponent>())
		Camera->StopHoldFeedback(.06f);
	return true;
}

// 判断当前鼠标输入是否应该解释为防御
bool UPlayerRhythmCombatComponent::IsMouseDefenseWindow() const
{
	return !GetMouseDefensePrompt().IsEmpty();
}

// 根据临近的敌人攻击生成鼠标防御提示
FString UPlayerRhythmCombatComponent::GetMouseDefensePrompt() const
{
	if (!GetWorld() || Health <= 0.f || bPhaseCinematic)
		return FString();
	const auto* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	if (!Clock || !Clock->IsRunning())
		return FString();
	// 这层窗口只决定鼠标左右键要做什么，不代表防御已经成功
	// 只考虑附近仍存活的 Boss，并排除必须离开范围的地面爆炸
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const auto* Boss = It->FindComponentByClass<UBeatSyncedComboComponent>();
		if (!Boss || Boss->BossHealth <= 0.f || Boss->IsPhaseTransitionActive() ||
		    FVector::DistSquared2D(It->GetActorLocation(), GetOwner()->GetActorLocation()) >
		        FMath::Square(600.f))
			continue;
		TArray<FUpcomingImpactCue> Cues;
		Boss->GetUpcomingImpactCues(Cues, 3, true);
		for (const auto& Cue : Cues)
		{
			if (Cue.DamageShape == EBossAttackDamageShape::GroundBurst)
				continue;
			const double Remaining = Cue.ImpactAudioSeconds - Clock->GetAudioNowSeconds();
			if (!PlayerAttackTiming::IsDefenseInputWindow(Remaining, Cue.IdealDodgeLeadSeconds))
				continue;
			switch (Cue.RequiredResponse)
			{
			case ERhythmResponseType::Block:
				return TEXT("RMB  BLOCK");
			case ERhythmResponseType::Counter:
				return TEXT("LMB  COUNTER");
			case ERhythmResponseType::HoldBlock:
				return Cue.HoldMarker == ERhythmHoldMarkerType::End ? TEXT("RELEASE RMB") : TEXT("HOLD RMB");
			case ERhythmResponseType::DodgeOrBlock:
				return TEXT("RMB BLOCK / SHIFT DODGE");
			default:
				return TEXT("SHIFT  DODGE");
			}
		}
	}
	return FString();
}

// 反击输入

// 检查反击条件，必要时缓存按键而不立即播放动作
bool UPlayerRhythmCombatComponent::TryCounter()
{
	if (Health <= 0.f || bPhaseCinematic)
		return false;
	if (auto* Attacks = GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>();
	    Attacks && !Attacks->CancelForDefense(false))
		return false;
	if (bPhaseCinematic)
		return false;
	if (!GetWorld())
		return false;
	const double InputAudioSeconds = GetCurrentInputAudioSeconds();
	// 保留原始按键时间供命中时评级，最多保存最近十六次
	CounterInputHistory.Add(InputAudioSeconds);
	if (CounterInputHistory.Num() > 16)
		CounterInputHistory.RemoveAt(0);
	if (BufferWhilePoiseLocked(EBufferedRhythmAction::Counter, InputAudioSeconds))
		return true;
	if (bHitReacting)
		FinishHitReaction();
	if (bPoiseBroken)
		EndPoiseBreak();
	// 弹反可以主动退出格挡，不等待格挡的松键事件
	// 保留玩家实际按下弹反的时间，避免动画交接改变节奏评级
	if (bBlocking)
	{
		EndBlock();
		bBlockRecovering = false;
	}
	if (bDodging)
	{
		const double Elapsed = GetWorld()->GetTimeSeconds() - DodgeStartWorldSeconds;
		if (Elapsed < DodgeChainStartSeconds)
		{
			BufferInput(EBufferedRhythmAction::Counter, InputAudioSeconds);
			return true;
		}
		CancelDodgeForTransition();
	}
	if (bCountering)
	{
		const double Elapsed = GetWorld()->GetTimeSeconds() - CounterStartWorldSeconds;
		if (Elapsed < CounterCancelStartSeconds)
		{
			BufferInput(EBufferedRhythmAction::Counter, InputAudioSeconds);
			return true;
		}
		bCountering = false;
	}
	return StartCounter(InputAudioSeconds);
}

// 保留原始输入时刻，启动反击姿态
bool UPlayerRhythmCombatComponent::StartCounter(double InputAudioSeconds)
{
	if (auto* Attack = GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>();
	    Attack && !Attack->CancelForDefense(false))
		return false;
	BufferedAction = EBufferedRhythmAction::None;
	bCountering = true;
	bRecoveryStrikePending = false; // 追击伤害统一由武器接触检测处理
	bBlockRecovering = false;
	bBlockReleasePending = false;
	CounterStartWorldSeconds = GetWorld()->GetTimeSeconds();
	LastCounterInputAudioSeconds = InputAudioSeconds;
	if (CounterSwishSound)
		UGameplayStatics::PlaySound2D(this, CounterSwishSound, .5f, .92f);
	PlayDefenseAnimation(CounterAnimation, false, CounterDurationSeconds);
	return true;
}

// 取得本次输入用于节奏判定的音乐时间
double UPlayerRhythmCombatComponent::GetCurrentInputAudioSeconds() const
{
	if (!GetWorld())
		return -1000.0;
	if (const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>())
		return Clock->GetAudioNowSeconds();
	return GetWorld()->GetTimeSeconds();
}

// 防御输入缓存

// 保存待执行的防御类型、原始时间和闪避方向
void UPlayerRhythmCombatComponent::BufferInput(EBufferedRhythmAction Action, double InputAudioSeconds,
                                               FVector Direction)
{
	// 防御缓存只保留一个操作，新的操作覆盖旧操作
	// 音乐时间用于评级，游戏时间用于过期，两个时间不能混用
	BufferedAction = Action;
	bBufferedBlockReleased = false;
	BufferedInputAudioSeconds = InputAudioSeconds;
	BufferedInputWorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : -1000.0;
	if (Action == EBufferedRhythmAction::Dodge)
		BufferedDodgeDirection = Direction;
	UE_LOG(LogTemp, Verbose, TEXT("RhythmInputBuffer: queued action=%d audio=%.3f"),
	       static_cast<int32>(Action), InputAudioSeconds);
}

// 状态切换时结束闪避位移与动作
void UPlayerRhythmCombatComponent::CancelDodgeForTransition()
{
	if (!bDodging)
		return;
	bDodging = false;
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			Movement->SetMovementMode(MOVE_Walking);
	}
}

// 破防锁定期间保留操作，并将有效期延长到允许执行之后
bool UPlayerRhythmCombatComponent::BufferWhilePoiseLocked(EBufferedRhythmAction Action,
                                                          double InputAudioSeconds, FVector Direction)
{
	if (!GetWorld())
		return false;
	const double Now = GetWorld()->GetTimeSeconds();
	const bool bPoiseLocked = bPoiseBroken && Now - PoiseBreakStartWorldSeconds < PoiseBreakInputLockSeconds;
	const bool bHitLocked = bHitReacting && Now < HitInputUnlockWorldSeconds;
	if (!bPoiseLocked && !bHitLocked)
		return false;
	BufferInput(Action, InputAudioSeconds, Direction);
	return true;
}

// 韧性与受击恢复

// 减少韧性，耗尽时进入破防
void UPlayerRhythmCombatComponent::ApplyPoiseDamage(float Amount, FVector ImpactLocation,
                                                    FVector AttackDirection, FVector AttackerWorldLocation)
{
	if (Amount <= 0.0f || bPoiseBroken || !GetWorld())
		return;
	Poise = FMath::Max(0.0f, Poise - Amount);
	LastPoiseDamageWorldSeconds = GetWorld()->GetTimeSeconds();
	UE_LOG(LogTemp, Display, TEXT("PlayerPoise: damage=%.1f remaining=%.1f/%.1f"), Amount, Poise, MaxPoise);
	if (Poise <= KINDA_SMALL_NUMBER)
		StartPoiseBreak(ImpactLocation, AttackDirection, AttackerWorldLocation);
}

// 按接触位置和受击方向生成血液效果
void UPlayerRhythmCombatComponent::SpawnBloodFeedback(FVector ImpactLocation, FVector AttackDirection,
                                                      bool bPoiseBreak) const
{
	if (!GetWorld() || !GetOwner())
		return;
	UNiagaraSystem* Effect = bPoiseBreak ? BloodPoiseBreakEffect.Get() : BloodHitEffect.Get();
	if (!Effect)
		return;
	const FVector Torso = GetOwner()->GetActorLocation() + FVector(0, 0, 85.0f);
	if (ImpactLocation.IsNearlyZero() || FVector::DistSquared(ImpactLocation, Torso) > FMath::Square(190.0f))
		ImpactLocation = Torso;
	if (!AttackDirection.Normalize())
		AttackDirection = -GetOwner()->GetActorForwardVector();
	const float Scale = bPoiseBreak ? BloodPoiseBreakScale : BloodHitScale;
	RetireCombatFX(UNiagaraFunctionLibrary::SpawnSystemAtLocation(
	                   GetWorld(), Effect, ImpactLocation, AttackDirection.Rotation(), FVector(Scale * .65f)),
	               .12f, .65f);
	UE_LOG(LogTemp, Display, TEXT("PlayerBloodVFX: heavy=%d effect=%s location=%s"), bPoiseBreak ? 1 : 0,
	       *GetNameSafe(Effect), *ImpactLocation.ToString());
}

// 清理冲突动作，启动破防硬直和输入锁定
void UPlayerRhythmCombatComponent::StartPoiseBreak(FVector ImpactLocation, FVector AttackDirection,
                                                   FVector AttackerWorldLocation)
{
	if (!GetWorld() || !GetOwner())
		return;
	bPoiseBroken = true;
	Poise = 0.0f;
	PoiseBreakStartWorldSeconds = GetWorld()->GetTimeSeconds();
	bActiveHoldBroken = true;
	StartHitReaction(ImpactLocation, AttackDirection, true, AttackerWorldLocation);
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !Character->GetMesh())
		return;
	FVector ReactionDirection = -HitFacingRotation.Vector();
	// 破防只做短距离带碰撞的位移，避免玩家被推得过远

	FHitResult PushHit;
	Character->AddActorWorldOffset(ReactionDirection * 34.0f, true, &PushHit);
	UE_LOG(LogTemp, Display, TEXT("PlayerPoise: BROKEN facingYaw=%.1f lock=%.2fs duration=%.2fs"),
	       HitFacingRotation.Yaw, PoiseBreakInputLockSeconds, PoiseBreakDurationSeconds);
}

// 结束破防锁定并恢复允许执行动作的状态
void UPlayerRhythmCombatComponent::EndPoiseBreak()
{
	if (!bPoiseBroken)
		return;
	bPoiseBroken = false;
	Poise = MaxPoise;
	FinishHitReaction();
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			Movement->SetMovementMode(MOVE_Walking);
	}
	UE_LOG(LogTemp, Display, TEXT("PlayerPoise: recovered %.1f/%.1f"), Poise, MaxPoise);
}

// 根据攻击方向启动受击姿态，单独设置输入解锁时间
void UPlayerRhythmCombatComponent::StartHitReaction(FVector ImpactLocation, FVector AttackDirection,
                                                    bool bHeavy, FVector AttackerWorldLocation)
{
	if (auto* Attacks = GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>())
		Attacks->NotifyInterrupted();
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!GetWorld() || !Character || !Character->GetMesh())
		return;
	FVector TowardAttacker = AttackerWorldLocation.IsNearlyZero()
	                             ? FVector::ZeroVector
	                             : (AttackerWorldLocation - Character->GetActorLocation()).GetSafeNormal2D();
	if (TowardAttacker.IsNearlyZero())
		TowardAttacker = (ImpactLocation - Character->GetActorLocation()).GetSafeNormal2D();
	if (TowardAttacker.IsNearlyZero())
		TowardAttacker = -AttackDirection.GetSafeNormal2D();
	if (TowardAttacker.IsNearlyZero())
		TowardAttacker = Character->GetActorForwardVector().GetSafeNormal2D();
	HitFacingRotation = FRotator(0.0f, TowardAttacker.Rotation().Yaw, 0.0f);
	const double Now = GetWorld()->GetTimeSeconds();
	const float Duration = bHeavy ? PoiseBreakDurationSeconds : LightHitDurationSeconds;
	bHitReacting = true;
	bHitTurnPending = true;
	HitTurnEndWorldSeconds = Now + HitTurnSeconds;
	HitReactionEndWorldSeconds = Now + HitTurnSeconds + Duration;
	// 输入解锁与受击动画结束分开计时
	HitInputUnlockWorldSeconds = Now + (bHeavy ? PoiseBreakInputLockSeconds : LightHitInputLockSeconds);
	BufferedAction = EBufferedRhythmAction::None;
	bBufferedBlockReleased = false;
	bDodging = false;
	bBlocking = false;
	bBlockRecovering = false;
	bBlockReleasePending = false;
	bCountering = false;
	Character->ConsumeMovementInputVector();
	Character->SetAnimRootMotionTranslationScale(0.0f);
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
		Movement->StopMovementImmediately();
	}
	if (UAnimInstance* Animation = Character->GetMesh()->GetAnimInstance())
		Animation->Montage_Stop(.04f);
	if (UBossCombatCameraComponent* Camera = Character->FindComponentByClass<UBossCombatCameraComponent>())
		Camera->StopHoldFeedback(.04f);
	UE_LOG(LogTemp, Display, TEXT("PlayerHitReaction: heavy=%d turnYaw=%.1f turn=%.2fs duration=%.2fs"),
	       bHeavy ? 1 : 0, HitFacingRotation.Yaw, HitTurnSeconds, Duration);
}

// 结束受击动作并恢复移动状态
void UPlayerRhythmCombatComponent::FinishHitReaction()
{
	if (!bHitReacting)
		return;
	bHitReacting = false;
	bHitTurnPending = false;
	HitReactionEndWorldSeconds = -1000.0;
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
		if (UPlayerCombatAnimInstance* Animation =
		        Cast<UPlayerCombatAnimInstance>(Character->GetMesh()->GetAnimInstance()))
			Animation->EndDirectionalHit();
}

// 消费缓存与时机评级

// 等待动作解锁后重试缓存输入，过期或失效时清除
void UPlayerRhythmCombatComponent::ProcessBufferedInput()
{
	if (Health <= 0.f)
	{
		BufferedAction = EBufferedRhythmAction::None;
		return;
	}
	if (BufferedAction == EBufferedRhythmAction::None || !GetWorld())
		return;
	const double Now = GetWorld()->GetTimeSeconds();
	// 受击或破防锁定会延长缓冲有效期，让提前按下的操作有机会在解锁后执行
	double BufferDeadline = BufferedInputWorldSeconds + InputBufferSeconds;
	if (bHitReacting)
		BufferDeadline = FMath::Max(BufferDeadline, HitInputUnlockWorldSeconds + .05);
	if (bPoiseBroken)
		BufferDeadline =
		    FMath::Max(BufferDeadline, PoiseBreakStartWorldSeconds + PoiseBreakInputLockSeconds + .05);
	if (Now > BufferDeadline)
	{
		UE_LOG(LogTemp, Verbose, TEXT("RhythmInputBuffer: expired action=%d"),
		       static_cast<int32>(BufferedAction));
		BufferedAction = EBufferedRhythmAction::None;
		return;
	}
	// 动作尚未到允许取消的时刻就继续等待，不重新记录按键时间
	if (bDodging && Now - DodgeStartWorldSeconds < DodgeChainStartSeconds)
		return;
	if (bCountering && Now - CounterStartWorldSeconds < CounterCancelStartSeconds)
		return;
	if (bPoiseBroken && Now - PoiseBreakStartWorldSeconds < PoiseBreakInputLockSeconds)
		return;
	if (bHitReacting && Now < HitInputUnlockWorldSeconds)
		return;
	if (bPoiseBroken)
		EndPoiseBreak();
	if (bHitReacting)
		FinishHitReaction();

	// 执行前复制待处理数据，后续清空缓存不会丢失原始按键和松键时间
	const EBufferedRhythmAction Action = BufferedAction;
	// 执行可以延后，节奏评级仍使用玩家实际按键时的音乐时间
	const double OriginalInputAudioSeconds = BufferedInputAudioSeconds;
	const FVector OriginalDirection = BufferedDodgeDirection;
	const bool bReleasedBlock = bBufferedBlockReleased;
	const double OriginalReleaseAudioSeconds = BufferedBlockReleaseAudioSeconds;
	// 调用动作前清除缓冲，避免动作回调再次消费同一输入
	BufferedAction = EBufferedRhythmAction::None;
	if (bDodging)
		CancelDodgeForTransition();
	if (bCountering)
		bCountering = false;
	bool bStarted = false;
	switch (Action)
	{
	case EBufferedRhythmAction::Dodge:
		bStarted = StartDodge(OriginalDirection, OriginalInputAudioSeconds);
		break;
	case EBufferedRhythmAction::Block:
		bStarted = !bBlocking && StartBlock(OriginalInputAudioSeconds);
		// 缓冲期间已经松键时，补发松开状态，防止格挡一直保持
		if (bStarted && bReleasedBlock)
		{
			EndBlock();
			LastBlockReleaseAudioSeconds = OriginalReleaseAudioSeconds;
		}
		break;
	case EBufferedRhythmAction::Counter:
		if (bBlocking)
		{
			EndBlock();
			bBlockRecovering = false;
		}
		bStarted = StartCounter(OriginalInputAudioSeconds);
		break;
	default:
		break;
	}
	UE_LOG(LogTemp, Display,
	       TEXT("RhythmInputBuffer: dispatched action=%d accepted=%d delayMs=%.1f originalAudio=%.3f"),
	       static_cast<int32>(Action), bStarted ? 1 : 0,
	       static_cast<float>((Now - BufferedInputWorldSeconds) * 1000.0), OriginalInputAudioSeconds);
}

// 取出最接近理想时刻的一次按键，避免同一输入重复参与判定
double UPlayerRhythmCombatComponent::ConsumeClosestInput(TArray<double>& History, double IdealAudioSeconds)
{
	// 先丢弃明显过早的按键，再从剩余历史中选择时间最接近的一次
	History.RemoveAll(
	    [&](double Time)
	    {
		    return Time < IdealAudioSeconds - .5;
	    });
	int32 Best = INDEX_NONE;
	double Error = .5;
	for (int32 Index = 0; Index < History.Num(); ++Index)
	{
		const double Candidate = FMath::Abs(History[Index] - IdealAudioSeconds);
		if (Candidate < Error)
		{
			Best = Index;
			Error = Candidate;
		}
	}
	if (Best == INDEX_NONE)
		return -1000.0;
	const double Result = History[Best];
	// 消费后移除这次按键，后续命中不能再次使用它评级
	History.RemoveAt(Best);
	return Result;
}

// 按输入与理想时刻的误差划分完美、成功和失败
ERhythmDodgeJudgment UPlayerRhythmCombatComponent::JudgeTimedInput(double InputAudioSeconds,
                                                                   double IdealAudioSeconds) const
{
	const float ErrorMs = static_cast<float>(FMath::Abs(InputAudioSeconds - IdealAudioSeconds) * 1000.0);
	if (ErrorMs <= PerfectWindowMs)
		return ERhythmDodgeJudgment::Perfect;
	if (ErrorMs <= GoodWindowMs)
		return ERhythmDodgeJudgment::Good;
	return ERhythmDodgeJudgment::Bad;
}

// 使用反击窗口判断按键误差
ERhythmDodgeJudgment UPlayerRhythmCombatComponent::JudgeCounterInput(double InputAudioSeconds,
                                                                     double IdealAudioSeconds) const
{
	// 反击使用与界面相同的评级窗口，避免画面提示和实际判定不一致
	return JudgeTimedInput(InputAudioSeconds, IdealAudioSeconds);
}

// 判断当前是否处于闪避无敌时间内
bool UPlayerRhythmCombatComponent::IsInvulnerable() const
{
	if (!bDodging || !GetWorld())
		return false;
	// 无敌区间按原始按键的音乐时间判断，不因缓存延后而重新计时
	// 若缓存等待已超过无敌结束点，开始播放翻滚也不会获得新的一整段无敌
	double Elapsed = GetWorld()->GetTimeSeconds() - DodgeStartWorldSeconds;
	if (const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	    Clock && LastDodgeInputAudioSeconds > -900.0)
	{
		Elapsed = Clock->GetAudioNowSeconds() - LastDodgeInputAudioSeconds;
	}
	return Elapsed >= InvulnerabilityStartSeconds && Elapsed <= InvulnerabilityEndSeconds;
}

// 实际接触与防御结果

// 真实刀刃接触时，进入防御与伤害结算
bool UPlayerRhythmCombatComponent::ResolveBossContact(double ContactAudioSeconds, double TargetBeat,
                                                      float Damage, float IdealDodgeLeadSeconds,
                                                      bool bStrongBeat, FVector BladeWorldLocation,
                                                      FVector AttackDirection,
                                                      ERhythmResponseType RequiredResponse,
                                                      ERhythmHoldMarkerType HoldMarker, FName HoldGroup,
                                                      FVector AttackerWorldLocation)
{
	return ResolveResponse(ContactAudioSeconds, TargetBeat, Damage, IdealDodgeLeadSeconds, bStrongBeat,
	                       BladeWorldLocation, AttackDirection, RequiredResponse, HoldMarker, HoldGroup, true,
	                       AttackerWorldLocation);
}

// 刀刃未接触时，进入躲避或练习判定
bool UPlayerRhythmCombatComponent::ResolveBossAvoidance(double ImpactAudioSeconds, double TargetBeat,
                                                        float IdealDodgeLeadSeconds, bool bStrongBeat,
                                                        FVector BladeWorldLocation, FVector AttackDirection,
                                                        ERhythmResponseType RequiredResponse,
                                                        ERhythmHoldMarkerType HoldMarker, FName HoldGroup)
{
	return ResolveResponse(ImpactAudioSeconds, TargetBeat, 0.0f, IdealDodgeLeadSeconds, bStrongBeat,
	                       BladeWorldLocation, AttackDirection, RequiredResponse, HoldMarker, HoldGroup,
	                       false);
}

// 应对结算：先用命中时间去重，再按要求选择闪避、格挡、反击或持续格挡
// 理想输入时间等于命中时间减去提前量；输入偏差用于评级，物理接触与防御状态决定后续伤害分支
bool UPlayerRhythmCombatComponent::ResolveResponse(double ImpactAudioSeconds, double TargetBeat, float Damage,
                                                   float IdealLeadSeconds, bool bStrongBeat,
                                                   FVector BladeWorldLocation, FVector AttackDirection,
                                                   ERhythmResponseType RequiredResponse,
                                                   ERhythmHoldMarkerType HoldMarker, FName HoldGroup,
                                                   bool bPhysicalContact, FVector AttackerWorldLocation)
{
	// 将命中音乐时间量化为 0.1 毫秒单位的整数键，用于过滤同一命中的重复结算
	const int64 ImpactKey = FMath::RoundToInt64(ImpactAudioSeconds * 10000.0);
	if (Health <= 0.f)
		return false;
	if (ResolvedImpactKeys.Contains(ImpactKey))
		return true;
	// 当前只用音乐时间去重，适用于本项目单 Boss 的攻击流程
	// 如果扩展到多个攻击者同刻命中，需要把攻击者和命中编号也纳入键
	ResolvedImpactKeys.Add(ImpactKey);
	// 这是有限长度的历史去重，清空后不再记得更早的命中
	if (ResolvedImpactKeys.Num() > 64)
		ResolvedImpactKeys.Reset();

	// 从命中时刻减去提前量，得到玩家应当按键的时刻
	const double IdealInput = ImpactAudioSeconds - IdealLeadSeconds;
	if (RequiredResponse == ERhythmResponseType::DodgeOrBlock)
	{
		// 比较闪避和格挡的按键误差，只结算更合适的一种防御
		double BlockError = 1000.;
		for (double Time : BlockInputHistory)
			BlockError = FMath::Min(BlockError, FMath::Abs(Time - IdealInput));
		const bool CanDodge = IsInvulnerable() || (!bPhysicalContact && DodgeSequenceId > 0 &&
		                                           ImpactAudioSeconds - LastDodgeInputAudioSeconds >= -.10 &&
		                                           ImpactAudioSeconds - LastDodgeInputAudioSeconds <= .55);
		const double DodgeError = FMath::Abs(LastDodgeInputAudioSeconds - IdealInput);
		const bool GoodBlock = BlockError * 1000. <= GoodWindowMs;
		RequiredResponse = CanDodge && (!GoodBlock || DodgeError < BlockError) ? ERhythmResponseType::Dodge
		                                                                       : ERhythmResponseType::Block;
	}
	LastResponse = RequiredResponse;
	double JudgmentTargetAudioSeconds = IdealInput;
	// 负值表示尚未取得有效输入，不作为实际音乐时间参与正常评级
	double InputTime = -1000.0;
	bool bDefended = false;
	// 防御不完全成功时，按这个比例计算剩余伤害
	float DamageMultiplier = 1.0f;
	ERhythmDodgeJudgment Grade = ERhythmDodgeJudgment::Miss;

	switch (RequiredResponse)
	{
	case ERhythmResponseType::Dodge:
		InputTime = LastDodgeInputAudioSeconds;
		bDefended = IsInvulnerable() || (!bPhysicalContact && DodgeSequenceId > 0 &&
		                                 ImpactAudioSeconds - LastDodgeInputAudioSeconds >= -0.10 &&
		                                 ImpactAudioSeconds - LastDodgeInputAudioSeconds <= 0.55);
		Grade = bDefended ? JudgeTimedInput(InputTime, IdealInput) : ERhythmDodgeJudgment::Miss;
		if (bDodging)
			LastResolvedDodgeSequenceId = DodgeSequenceId;
		break;
	// 普通格挡与弹反按键需要逐次消费，不能用一次按键抵挡后续所有命中
	case ERhythmResponseType::Block:
		InputTime = ConsumeClosestInput(BlockInputHistory, IdealInput);
		Grade = InputTime > -900.0 ? JudgeTimedInput(InputTime, IdealInput) : ERhythmDodgeJudgment::Miss;
		bDefended = Grade == ERhythmDodgeJudgment::Perfect || Grade == ERhythmDodgeJudgment::Good;
		if (Grade == ERhythmDodgeJudgment::Bad)
			DamageMultiplier = .35f;
		break;
	case ERhythmResponseType::Counter:
		InputTime = ConsumeClosestInput(CounterInputHistory, IdealInput);
		Grade = JudgeCounterInput(InputTime, IdealInput);
		bDefended = Grade == ERhythmDodgeJudgment::Perfect || Grade == ERhythmDodgeJudgment::Good;
		if (Grade == ERhythmDodgeJudgment::Bad)
			DamageMultiplier = .60f;
		if (!bDefended && InputTime < 0.0)
			Grade = ERhythmDodgeJudgment::Miss;
		break;
	// 持续格挡用组名连接开始、维持和结束，途中断开后不能靠最后一次松键补回成功
	case ERhythmResponseType::HoldBlock:
		if (HoldMarker == ERhythmHoldMarkerType::Start)
		{
			ActiveHoldGroup = HoldGroup;
			ReportedHoldBreakGroups.Remove(HoldGroup);
			bActiveHoldBroken = !bBlocking;
			InputTime = LastBlockStartAudioSeconds;
			bDefended = bBlocking;
			Grade = bDefended ? JudgeTimedInput(InputTime, IdealInput) : ERhythmDodgeJudgment::HoldBreak;
		}
		else if (HoldMarker == ERhythmHoldMarkerType::End)
		{
			JudgmentTargetAudioSeconds = ImpactAudioSeconds;
			// 持续格挡结束必须属于同一组且未中断，避免沿用其他攻击组的格挡状态
			const bool bSameGroup = ActiveHoldGroup == HoldGroup && !bActiveHoldBroken;
			if (bBlocking && bSameGroup)
			{
				bDefended = true;
				Grade = ERhythmDodgeJudgment::Good; // 持续按住仍可防住，但没有准时松键奖励
			}
			else if (bSameGroup && LastBlockReleaseAudioSeconds > -900.0)
			{
				InputTime = LastBlockReleaseAudioSeconds;
				const float ReleaseErrorMs =
				    FMath::Abs(static_cast<float>((InputTime - ImpactAudioSeconds) * 1000.0));
				bDefended = ReleaseErrorMs <= GoodWindowMs;
				Grade = ReleaseErrorMs <= PerfectWindowMs ? ERhythmDodgeJudgment::PerfectHold
				        : ReleaseErrorMs <= GoodWindowMs  ? ERhythmDodgeJudgment::GoodHold
				                                          : ERhythmDodgeJudgment::HoldBreak;
			}
			else
				Grade = ERhythmDodgeJudgment::HoldBreak;
			ActiveHoldGroup = NAME_None;
		}
		else
		{
			bDefended = bBlocking && !bActiveHoldBroken && ActiveHoldGroup == HoldGroup;
			Grade = bDefended ? ERhythmDodgeJudgment::Good : ERhythmDodgeJudgment::HoldBreak;
			if (!bDefended)
				bActiveHoldBroken = true;
		}
		break;
	}

	const float ErrorMs =
	    InputTime > -900.0 ? static_cast<float>((InputTime - JudgmentTargetAudioSeconds) * 1000.0) : 0.0f;
	// 同一组持续格挡中断只提示一次，后续接触仍照常结算伤害
	const bool bDuplicateHoldBreak =
	    Grade == ERhythmDodgeJudgment::HoldBreak && ReportedHoldBreakGroups.Contains(HoldGroup);
	const int32 PreviousCombo = ComboCount;
	if (!bDuplicateHoldBreak)
	{
		SubmitDodgeJudgment(Grade, ErrorMs, bStrongBeat);
		if (Grade == ERhythmDodgeJudgment::HoldBreak && !HoldGroup.IsNone())
			ReportedHoldBreakGroups.Add(HoldGroup);
	}
	if (!bDuplicateHoldBreak)
	{
		FRhythmSuccessFeedbackData Feedback;
		Feedback.Response = RequiredResponse;
		Feedback.HoldMarker = HoldMarker;
		Feedback.Grade = Grade;
		Feedback.PreviousCombo = PreviousCombo;
		Feedback.ComboCount = ComboCount;
		Feedback.FeedbackTier = GetFeedbackTierForCombo(ComboCount);
		Feedback.bSuccess = Grade == ERhythmDodgeJudgment::Perfect || Grade == ERhythmDodgeJudgment::Good ||
		                    Grade == ERhythmDodgeJudgment::GoodHold ||
		                    Grade == ERhythmDodgeJudgment::PerfectHold;
		Feedback.bPerfect =
		    Grade == ERhythmDodgeJudgment::Perfect || Grade == ERhythmDodgeJudgment::PerfectHold;
		const bool bTierCrossed =
		    GetFeedbackTierForCombo(ComboCount) > GetFeedbackTierForCombo(PreviousCombo);
		Feedback.bKeyPerfect =
		    Feedback.bPerfect && (RequiredResponse == ERhythmResponseType::Counter ||
			                      Grade == ERhythmDodgeJudgment::PerfectHold || bTierCrossed);
		Feedback.bStrongBeat = bStrongBeat;
		Feedback.WorldLocation = BladeWorldLocation;
		Feedback.AttackDirection = AttackDirection;
		Feedback.EventId = ++FeedbackEventSerial;
		OnRhythmSuccessFeedback.Broadcast(Feedback);
	}
	PlayResponseFeedback(RequiredResponse, Grade, BladeWorldLocation, AttackDirection, bStrongBeat);
	if (RequiredResponse == ERhythmResponseType::Dodge && Grade == ERhythmDodgeJudgment::Perfect)
		BroadcastPerfectFeedback(ErrorMs, bStrongBeat, BladeWorldLocation, AttackDirection);
	// 没有物理接触时传入伤害为 0，可以有节奏反馈但不能凭拍点扣血
	if (!bDefended && Damage > 0.0f)
		ApplyCombatDamage(Damage * DamageMultiplier);
	if (!bDuplicateHoldBreak)
	{
		if (Grade == ERhythmDodgeJudgment::Bad)
		{
			if (bPhysicalContact && !bDefended && Damage > 0.0f)
				SpawnBloodFeedback(BladeWorldLocation, AttackDirection, Poise <= BadPoiseDamage);
			ApplyPoiseDamage(BadPoiseDamage, BladeWorldLocation, AttackDirection, AttackerWorldLocation);
		}
		else if (bPhysicalContact && !bDefended && Damage > 0.0f)
		{
			SpawnBloodFeedback(BladeWorldLocation, AttackDirection, Poise <= DirectHitPoiseDamage);
			ApplyPoiseDamage(DirectHitPoiseDamage, BladeWorldLocation, AttackDirection,
			                 AttackerWorldLocation);
		}
	}
	// 实际受伤时播放受击动作，破防已播放更强反应时不再重复
	if (bPhysicalContact && !bDefended && Damage > 0.0f && !bPoiseBroken)
		StartHitReaction(BladeWorldLocation, AttackDirection, false, AttackerWorldLocation);
	UE_LOG(LogTemp, Display,
	       TEXT("RhythmResponse: type=%s hold=%s grade=%s errorMs=%.1f defended=%d damage=%.1f beat=%.2f"),
	       *UEnum::GetValueAsString(RequiredResponse), *UEnum::GetValueAsString(HoldMarker),
	       *UEnum::GetValueAsString(Grade), ErrorMs, bDefended ? 1 : 0, bDefended ? 0.0f : Damage,
	       TargetBeat);
	return bDefended;
}

// 防御反馈与事件通知

// 按实际防御结果播放声音与反馈，不替换已开始的新动作
void UPlayerRhythmCombatComponent::PlayResponseFeedback(ERhythmResponseType Response,
                                                        ERhythmDodgeJudgment Grade, FVector WorldLocation,
                                                        FVector AttackDirection, bool bStrongBeat)
{
	if (!GetWorld() || Grade == ERhythmDodgeJudgment::Miss || Grade == ERhythmDodgeJudgment::HoldBreak)
		return;
	if (WorldLocation.IsNearlyZero() && GetOwner())
		WorldLocation = GetOwner()->GetActorLocation() + FVector(0, 0, 90);
	const FRotator Rotation =
	    AttackDirection.IsNearlyZero() ? FRotator::ZeroRotator : AttackDirection.Rotation();
	UParticleSystem* Effect =
	    Response == ERhythmResponseType::Counter ? CounterImpactEffect.Get() : BlockImpactEffect.Get();
	if (Effect)
	{
		const float GradeScale =
		    Grade == ERhythmDodgeJudgment::Perfect || Grade == ERhythmDodgeJudgment::PerfectHold ? 1.65f
			: (Grade == ERhythmDodgeJudgment::Good || Grade == ERhythmDodgeJudgment::GoodHold)   ? 1.15f
			                                                                                     : .75f;
		RetireCombatFX(UGameplayStatics::SpawnEmitterAtLocation(
		                   GetWorld(), Effect, WorldLocation, Rotation,
		                   FVector(GradeScale * (bStrongBeat ? 1.25f : 1.0f)), true),
		               .15f, .45f);
	}
	// 延迟到达的评级反馈不替换当前动作，避免旧判定打断新操作

	if (Grade == ERhythmDodgeJudgment::PerfectHold && PerfectHoldShockEffect && GetOwner())
	{
		SpawnStandalonePerfectSparks(GetWorld(), PerfectHoldShockEffect,
		                             GetOwner()->GetActorLocation() + FVector(0, 0, 70),
		                             FRotator::ZeroRotator, 1.85f);
	}
}

// 组装完美防御数据并广播给表现组件
void UPlayerRhythmCombatComponent::BroadcastPerfectFeedback(float ErrorMs, bool bStrongBeat,
                                                            FVector BladeWorldLocation,
                                                            FVector AttackDirection)
{
	if (!GetOwner())
		return;
	if (BladeWorldLocation.IsNearlyZero())
		BladeWorldLocation = GetOwner()->GetActorLocation() + FVector(0, 0, 80.0f);
	if (!AttackDirection.Normalize())
		AttackDirection = -GetOwner()->GetActorForwardVector();
	FPerfectDodgeFeedbackData Feedback;
	Feedback.BladeWorldLocation = BladeWorldLocation;
	Feedback.AttackDirection = AttackDirection;
	Feedback.SignedErrorMs = ErrorMs;
	Feedback.ComboCount = ComboCount;
	Feedback.ResonanceFraction = GetResonanceFraction();
	Feedback.bStrongBeat = bStrongBeat;
	OnPerfectDodge.Broadcast(Feedback);
	UE_LOG(LogTemp, Display,
	       TEXT("PerfectDodgeFeedback: strong=%d errorMs=%.1f combo=%d resonance=%.2f location=%s"),
	       bStrongBeat ? 1 : 0, ErrorMs, ComboCount, Feedback.ResonanceFraction,
	       *BladeWorldLocation.ToCompactString());
}
