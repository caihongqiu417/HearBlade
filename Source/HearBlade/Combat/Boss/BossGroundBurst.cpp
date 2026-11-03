// 管理地面范围攻击的预警、逃离空间检查、命中与收招状态
#include "Combat/Boss/BeatSyncedComboComponent.h"
#include "Combat/Player/PlayerRhythmCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "Misc/CommandLine.h"
#include "Components/BoxComponent.h"

bool UBeatSyncedComboComponent::IsGroundBurstRecovery() const
{
	const auto* Clock = GetWorld() ? GetWorld()->GetSubsystem<UBeatClockSubsystem>() : nullptr;
	return BossHealth > 0.f && Clock && Clock->IsRunning() &&
	       Clock->GetAudioNowSeconds() < GroundBurstRecoveryEndAudio;
}

float UBeatSyncedComboComponent::GetGroundBurstRemainingBeats() const
{
	const auto* Clock = GetWorld() ? GetWorld()->GetSubsystem<UBeatClockSubsystem>() : nullptr;
	return Clock && GroundBurstImpact != INDEX_NONE && !bGroundBurstResolved && !bGroundBurstHarmless
	           ? FMath::Max(0., (GroundBurstTargetAudio - Clock->GetAudioNowSeconds()) /
			                        Clock->GetSecondsPerBeat())
			   : -1.f;
}

// 收招反击需同时满足时间间隔、距离、朝向与无遮挡条件
bool UBeatSyncedComboComponent::TryRecoveryStrike(AActor* Player)
{
	if (!Player || !IsGroundBurstRecovery())
	{
		return false;
	}

	const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	const double CurrentMusicTime = Clock->GetAudioNowSeconds();
	const double StrikeInterval = Clock->GetSecondsPerBeat() * .75;
	if (CurrentMusicTime - LastRecoveryStrikeAudio < StrikeInterval)
	{
		return false;
	}

	const FVector OffsetToBoss = GetOwner()->GetActorLocation() - Player->GetActorLocation();
	// 近距离反击同时限制水平距离和高度差，避免隔层命中
	if (OffsetToBoss.Size2D() > 300.f || FMath::Abs(OffsetToBoss.Z) > 160.f)
	{
		return false;
	}

	const float FacingAlignment =
	    FVector::DotProduct(Player->GetActorForwardVector(), OffsetToBoss.GetSafeNormal2D());
	if (FacingAlignment < .35f)
	{
		return false;
	}

	// 忽略攻击双方，只检查两者之间是否存在其他遮挡
	FHitResult ObstructionHit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RecoveryStrike), false, Player);
	QueryParams.AddIgnoredActor(GetOwner());
	if (GetWorld()->LineTraceSingleByChannel(ObstructionHit, Player->GetActorLocation(),
	                                         GetOwner()->GetActorLocation(), ECC_Visibility, QueryParams))
	{
		return false;
	}

	LastRecoveryStrikeAudio = CurrentMusicTime;
	ApplyBossDamageFrom(CounterDamage * .5f, Player);
	UE_LOG(LogTemp, Display, TEXT("GroundBurstRecovery: strike distance=%.1f damage=%.1f resonance=8"),
	       OffsetToBoss.Size2D(), CounterDamage * .5f);
	return true;
}

// 检查目标能否在预警时间内离开范围，并考虑角色体积和路径碰撞
bool UBeatSyncedComboComponent::HasSafeAreaExit(const FVector& Center, float Radius, const AActor* Target,
                                                double AvailableSeconds) const
{
	if (!Target || !GetWorld())
		return false;
	const UCapsuleComponent* Capsule = Target->FindComponentByClass<UCapsuleComponent>();
	const float R = Capsule ? Capsule->GetScaledCapsuleRadius() : 42.f;
	const float H = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.f;
	if (FVector::DistSquared2D(Target->GetActorLocation(), Center) > FMath::Square(Radius + R + 5.f))
		return true;
	const ACharacter* Character = Cast<ACharacter>(Target);
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	// 防御可能暂时关闭移动模式，逃离距离仍按恢复后的步行速度估算
	const float Speed = Movement ? Movement->MaxWalkSpeed : 200.f;
	if (AvailableSeconds < 0.)
	{
		const auto* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
		AvailableSeconds = Clock ? Clock->GetSecondsPerBeat() * 4. : 2.;
	}
	// 先预留反应时间，再估算步行与闪避能否到达安全区
	const auto* Combat = Target->FindComponentByClass<UPlayerRhythmCombatComponent>();
	// 先给玩家留出反应时间，再估算剩余时间内走路和闪避能到达的距离
	// 从预警时长中预留反应时间，再计算步行和闪避最多能移动多远
	const double TravelBudget = FMath::Max(0., AvailableSeconds - .55) * Speed +
	                            (Combat ? Combat->GetEscapeDodgeBonus(Speed, AvailableSeconds) : 0.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GroundBurstExit), false, Target);
	Params.AddIgnoredActor(GetOwner());
	int32 TimeRejected = 0, GroundRejected = 0, ObstacleRejected = 0;
	for (int32 I = 0; I < 16; ++I)
	{
		const float A = I * UE_TWO_PI / 16.f;
		const FVector End = Center + FVector(FMath::Cos(A), FMath::Sin(A), 0) * (Radius + R + 60.f);
		const float Distance = FVector::Dist2D(Target->GetActorLocation(), End);
		if (Distance > TravelBudget)
		{
			++TimeRejected;
			continue;
		}
		// 沿逃离路线逐段检查地面和胶囊通行空间，出口能站立不代表途中能通过
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(Distance / 60.f));
		FVector Previous = Target->GetActorLocation();
		bool bSafe = true;
		for (int32 Step = 1; Step <= Steps; ++Step)
		{
			const FVector Sample = FMath::Lerp(Target->GetActorLocation(), End, float(Step) / Steps);
			FHitResult Ground, Obstacle;
			const float FloorZ = Previous.Z - H;
			const FVector Top(Sample.X, Sample.Y, FloorZ + 100.f), Bottom(Sample.X, Sample.Y, FloorZ - 100.f);
			if (!GetWorld()->LineTraceSingleByChannel(Ground, Top, Bottom, ECC_Visibility, Params) ||
			    Ground.ImpactNormal.Z < (Movement ? Movement->GetWalkableFloorZ() : .7f) ||
			    FMath::Abs(Ground.ImpactPoint.Z - FloorZ) > (Movement ? Movement->MaxStepHeight : 45.f))
			{
				++GroundRejected;
				bSafe = false;
				break;
			}
			const FVector Safe = Ground.ImpactPoint + FVector(0, 0, H + 3.f);
			if (GetWorld()->SweepSingleByChannel(Obstacle, Previous, Safe, FQuat::Identity, ECC_Pawn,
			                                     FCollisionShape::MakeCapsule(R, H - 2.f), Params))
			{
				++ObstacleRejected;
				bSafe = false;
				break;
			}
			Previous = Safe;
		}
		if (bSafe)
			return true;
	}
	UE_LOG(
	    LogTemp, Display,
	    TEXT(
	        "GroundBurstExit: radius=%.0f seconds=%.3f speed=%.1f budget=%.1f time=%d ground=%d obstacle=%d"),
	    Radius, AvailableSeconds, Speed, TravelBudget, TimeRejected, GroundRejected, ObstacleRejected);
	return false;
}

void UBeatSyncedComboComponent::ClearGroundBurst()
{
	GroundBurstImpact = INDEX_NONE;
	bGroundBurstResolved = false;
	GroundBurstExitAudio = -1.0;
	bGroundBurstWasInside = false;
	GroundBurstBoundaryCrossings = 0;
	ActiveGroundBurstRadius = 0.f;
	bGroundBurstHarmless = false;
}

bool UBeatSyncedComboComponent::GetGroundBurstPreview(FVector& Center, float& Radius, float& Progress) const
{
	if (GroundBurstImpact == INDEX_NONE || bGroundBurstResolved || !AttackData)
		return false;
	Center = GroundBurstCenter;
	Radius = ActiveGroundBurstRadius;
	if (bGroundBurstHarmless)
		return false;
	Progress =
	    FMath::Clamp(static_cast<float>((GroundBurstPreviousAudio - GroundBurstStartAudio) /
		                                FMath::Max(.01, GroundBurstTargetAudio - GroundBurstStartAudio)),
		             0.f, 1.f);
	return true;
}

void UBeatSyncedComboComponent::UpdateGroundBurst(double AudioNow)
{
	if (!AttackData || !AttackTarget.IsValid())
		return;
	if (GroundBurstImpact == INDEX_NONE)
	{
		for (int32 I = 0; I < GetImpactCount(); ++I)
		{
			if (GetImpactMarker(I).DamageShape != EBossAttackDamageShape::GroundBurst)
				continue;
			const double Start = ScheduledStartAudioSeconds +
			                     AttackData->GetElapsedAtAnimationTime(I > 0 ? GetImpactTime(I - 1)
			                                                                 : AttackData->ClipStartSeconds) -
			                     AttackData->GetElapsedAtAnimationTime(AttackData->ClipStartSeconds);
			if (AudioNow < Start)
				return;
			GroundBurstCenter = GetOwner()->GetActorLocation();
			if (const UCapsuleComponent* C = GetOwner()->FindComponentByClass<UCapsuleComponent>())
				GroundBurstCenter.Z -= C->GetScaledCapsuleHalfHeight();
			const double TargetAudio = ScheduledStartAudioSeconds +
			                           AttackData->GetElapsedAtAnimationTime(GetImpactTime(I)) -
			                           AttackData->GetElapsedAtAnimationTime(AttackData->ClipStartSeconds);
			const auto* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
			const double RequiredWarning =
			    Clock ? Clock->GetSecondsPerBeat() * GetImpactMarker(I).GroundBurst.WarningBeats : 0.;
#if !UE_BUILD_SHIPPING
			if (!bGroundBurstTestInjected &&
			    FParse::Param(FCommandLine::Get(), TEXT("GroundBurstBlockedTest")))
			{
				bGroundBurstTestInjected = true;
				for (int32 Wall = 0; Wall < 4; ++Wall)
				{
					auto* Barrier = GetWorld()->SpawnActor<AActor>();
					auto* Box = NewObject<UBoxComponent>(Barrier);
					Barrier->SetRootComponent(Box);
					Box->SetBoxExtent(Wall < 2 ? FVector(10, 130, 180) : FVector(130, 10, 180));
					Box->SetCollisionProfileName(TEXT("BlockAll"));
					Box->RegisterComponent();
					const FVector Offset = Wall < 2 ? FVector(Wall == 0 ? 110 : -110, 0, 0)
					                                : FVector(0, Wall == 2 ? 110 : -110, 0);
					Barrier->SetActorLocation(AttackTarget->GetActorLocation() + Offset);
					Barrier->SetLifeSpan(12.f);
				}
				UE_LOG(LogTemp, Display, TEXT("GroundBurstTest: temporary escape blockers injected"));
			}
#endif
			ActiveGroundBurstRadius = GetImpactMarker(I).GroundBurst.RadiusCm;
			const bool bEnoughWarning = TargetAudio - Start + .001 >= RequiredWarning;
			// 开启范围内闪避通过规则时，不因缺少步行出口缩小范围
			bool bExit =
			    bGroundBurstDodgeThrough || HasSafeAreaExit(GroundBurstCenter, ActiveGroundBurstRadius,
				                                            AttackTarget.Get(), TargetAudio - AudioNow);
			// 找不到安全出口时逐步缩小范围，仍无出口或预警不足就改为无伤结束
			while (!bExit && ActiveGroundBurstRadius > 300.f)
			{
				ActiveGroundBurstRadius = FMath::Max(300.f, ActiveGroundBurstRadius - 40.f);
				bExit = HasSafeAreaExit(GroundBurstCenter, ActiveGroundBurstRadius, AttackTarget.Get(),
				                        TargetAudio - AudioNow);
			}
			if (!bEnoughWarning || !bExit)
			{
				bGroundBurstHarmless = true;
				ActiveGroundBurstRadius = 0.f;
				UE_LOG(LogTemp, Warning,
				       TEXT("GroundBurst: harmless finish warningSeconds=%.3f required=%.3f safeExit=%d"),
				       TargetAudio - Start, RequiredWarning, bExit);
			}
			GroundBurstImpact = I;
			GroundBurstStartAudio = Start;
			GroundBurstTargetAudio = ScheduledStartAudioSeconds +
			                         AttackData->GetElapsedAtAnimationTime(GetImpactTime(I)) -
			                         AttackData->GetElapsedAtAnimationTime(AttackData->ClipStartSeconds);
			GroundBurstPreviousLocation = AttackTarget->GetActorLocation();
			GroundBurstPreviousAudio = AudioNow;
			const UCapsuleComponent* InitialCapsule = AttackTarget->FindComponentByClass<UCapsuleComponent>();
			const float InitialBoundary =
			    ActiveGroundBurstRadius + (InitialCapsule ? InitialCapsule->GetScaledCapsuleRadius() : 42.f);
			bGroundBurstWasInside = FVector::DistSquared2D(GroundBurstPreviousLocation, GroundBurstCenter) <=
			                        FMath::Square(InitialBoundary);
			ClearWarping();
			UE_LOG(
			    LogTemp, Display,
			    TEXT(
			        "GroundBurst: warning center=%s radius=%.0f authoredRadius=%.0f seconds=%.3f targetAudio=%.3f harmless=%d"),
			    *GroundBurstCenter.ToString(), ActiveGroundBurstRadius,
			    GetImpactMarker(I).GroundBurst.RadiusCm, TargetAudio - Start, GroundBurstTargetAudio,
			    bGroundBurstHarmless);
			break;
		}
	}
	if (GroundBurstImpact == INDEX_NONE || bGroundBurstResolved)
		return;
#if !UE_BUILD_SHIPPING
	if (!bGroundBurstTestInjected && AudioNow - GroundBurstStartAudio > 1. &&
	    FParse::Param(FCommandLine::Get(), TEXT("GroundBurstDeathTest")))
	{
		bGroundBurstTestInjected = true;
		ApplyBossDamage(MaxBossHealth * 2.f);
		UE_LOG(LogTemp, Display,
		       TEXT("GroundBurstTest: killed boss during warning, activeArea=%d playing=%d"),
		       GroundBurstImpact, bPlaying);
		return;
	}
	if (!bGroundBurstTestInjected && GroundBurstTargetAudio - AudioNow < .1 &&
	    GroundBurstTargetAudio > AudioNow && FParse::Param(FCommandLine::Get(), TEXT("GroundBurstHitchTest")))
	{
		bGroundBurstTestInjected = true;
		UE_LOG(LogTemp, Display, TEXT("GroundBurstTest: injecting 250ms game-thread hitch before impact"));
		FPlatformProcess::Sleep(.25f);
	}
#endif
	const FVector Current = AttackTarget->GetActorLocation();
	const UCapsuleComponent* Capsule = AttackTarget->FindComponentByClass<UCapsuleComponent>();
	const float Boundary = ActiveGroundBurstRadius + (Capsule ? Capsule->GetScaledCapsuleRadius() : 42.f);
	const FVector2D From(GroundBurstPreviousLocation - GroundBurstCenter),
	    Delta(Current - GroundBurstPreviousLocation);
	const bool Inside = FVector::DistSquared2D(Current, GroundBurstCenter) <= FMath::Square(Boundary);
	const bool WasInside = From.SizeSquared() <= FMath::Square(Boundary);
	if (Inside != WasInside)
		++GroundBurstBoundaryCrossings;
	if (WasInside && !Inside && Delta.SizeSquared() > UE_SMALL_NUMBER)
	{
		// 求移动线段与危险圈边界的交点，再插值得到离开时刻
		// 这样不用把离开时间一律算在当前帧末尾
		const double A = Delta.SizeSquared(), B = 2. * FVector2D::DotProduct(From, Delta),
		             C = From.SizeSquared() - FMath::Square(Boundary);
		const double T =
		    FMath::Clamp((-B + FMath::Sqrt(FMath::Max(0., B * B - 4. * A * C))) / (2. * A), 0., 1.);
		GroundBurstExitAudio = FMath::Lerp(GroundBurstPreviousAudio, AudioNow, T);
	}
	// 重新进入危险区后清除上一次离开时间，防止先出去再回来仍获得安全判定
	if (Inside)
		GroundBurstExitAudio = -1.0;
	// 只有预警开始时在危险区内的玩家才有逃离奖励
	// 跨过爆炸时刻的这一帧保留前后位置，供结算还原当时的位置
	if (AudioNow < GroundBurstTargetAudio)
	{
		GroundBurstPreviousLocation = Current;
		GroundBurstPreviousAudio = AudioNow;
	}
}

void UBeatSyncedComboComponent::ResolveGroundBurst(int32 ImpactIndex, double AudioNow)
{
	if (GroundBurstImpact != ImpactIndex || bGroundBurstResolved || !AttackTarget.IsValid())
		return;
	// 先标记本次爆炸已结算，再发奖励或伤害，避免回调重复处理
	bGroundBurstResolved = true;
	const auto* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	GroundBurstRecoveryEndAudio =
	    FMath::Max(AudioNow, ScheduledStartAudioSeconds +
		                         AttackData->GetElapsedAtAnimationTime(AttackData->GetClipEndSeconds()) -
		                         AttackData->GetElapsedAtAnimationTime(AttackData->ClipStartSeconds)) +
	    (Clock ? Clock->GetSecondsPerBeat() : .577) * GroundBurstRecoveryBeats;
	UE_LOG(LogTemp, Display, TEXT("GroundBurstRecovery: open untilAudio=%.3f"), GroundBurstRecoveryEndAudio);
	if (bGroundBurstHarmless)
		return;
	const FAttackImpactMarker& Marker = GetImpactMarker(ImpactIndex);
	const UCapsuleComponent* Capsule = AttackTarget->FindComponentByClass<UCapsuleComponent>();
	const float Boundary = ActiveGroundBurstRadius + (Capsule ? Capsule->GetScaledCapsuleRadius() : 42.f);
	// 用爆炸前后的位置插值还原拍点处的位置，避免卡顿后只看最新位置
	const double T = FMath::Clamp((GroundBurstTargetAudio - GroundBurstPreviousAudio) /
	                                  FMath::Max(.0001, AudioNow - GroundBurstPreviousAudio),
	                              0., 1.);
	const FVector AtImpact = FMath::Lerp(GroundBurstPreviousLocation, AttackTarget->GetActorLocation(), T);
	const bool Inside = FVector::DistSquared2D(AtImpact, GroundBurstCenter) <= FMath::Square(Boundary);
	if (auto* Player = AttackTarget->FindComponentByClass<UPlayerRhythmCombatComponent>())
	{
		const double DodgeError = Player->GetLastDodgeInputAudioSeconds() - GroundBurstTargetAudio;
		const bool bDodgedThrough = bGroundBurstDodgeThrough && Inside && DodgeError >= -.35 &&
		                            DodgeError <= GroundBurstLateGraceSeconds + .001;
		if (bDodgedThrough)
		{
			// 接近爆炸拍点闪避获得完整奖励，其他有效闪避只获部分奖励
			const bool bOnBeat = FMath::Abs(DodgeError) <= Marker.GroundBurst.EscapeWindowSeconds;
			Player->AwardRhythmicEscape(bOnBeat ? Marker.GroundBurst.EscapeReward
			                                    : Marker.GroundBurst.EscapeReward * .4f,
			                            static_cast<float>(DodgeError));
			UE_LOG(LogTemp, Display, TEXT("GroundBurst: dodged through errorMs=%.1f onBeat=%d"),
			       DodgeError * 1000., bOnBeat ? 1 : 0);
		}
		else if (Inside)
			Player->ApplyGroundBurstDamage(Marker.Damage, GroundBurstCenter);
		else if (bGroundBurstWasInside)
		{
			// 预警时在圈内、爆炸时在圈外才算真实逃离
			// 只穿过边界一次且接近拍点，才能获得完整奖励
			const bool bTimed =
			    GroundBurstBoundaryCrossings == 1 &&
			    GroundBurstExitAudio >= GroundBurstTargetAudio - Marker.GroundBurst.EscapeWindowSeconds &&
			    GroundBurstExitAudio <= GroundBurstTargetAudio;
			Player->AwardRhythmicEscape(
			    bTimed ? Marker.GroundBurst.EscapeReward : Marker.GroundBurst.EscapeReward * .4f,
			    GroundBurstExitAudio >= 0. ? static_cast<float>(GroundBurstExitAudio - GroundBurstTargetAudio)
				                           : 0.f);
		}
	}
	UE_LOG(LogTemp, Display,
	       TEXT("GroundBurst: resolved index=%d inside=%d errorMs=%.1f crossings=%d exitErrorMs=%.1f"),
	       ImpactIndex, Inside, (AudioNow - GroundBurstTargetAudio) * 1000., GroundBurstBoundaryCrossings,
	       GroundBurstExitAudio >= 0. ? (GroundBurstExitAudio - GroundBurstTargetAudio) * 1000. : -9999.);
}
