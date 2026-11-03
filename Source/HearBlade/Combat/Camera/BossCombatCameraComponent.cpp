// 维护战斗目标锁定、镜头构图及攻击反馈
#include "Combat/Camera/BossCombatCameraComponent.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Combat/Boss/BeatSyncedComboComponent.h"
#include "Combat/Boss/BossBeatDirector.h"
#include "Combat/Camera/BossCameraConfig.h"
#include "Combat/Camera/BossCameraModifier.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HearBladeCharacter.h"
#include "Components/CapsuleComponent.h"

UBossCombatCameraComponent::UBossCombatCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

// 优先使用配置资源，没有资源时使用默认配置
const UBossCameraConfig* UBossCombatCameraComponent::Settings() const
{
	return Config ? Config.Get() : GetDefault<UBossCameraConfig>();
}

// 镜头与战斗事件连接

// 获取玩家镜头并准备战斗镜头修饰器
void UBossCombatCameraComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!Config)
		Config = LoadObject<UBossCameraConfig>(
		    nullptr, TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_BossCamera.DA_BossCamera"));
	if (const AHearBladeCharacter* Player = Cast<AHearBladeCharacter>(GetOwner()))
		if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
			if (PC->PlayerCameraManager)
				Modifier = Cast<UBossCameraModifier>(
				    PC->PlayerCameraManager->AddNewCameraModifier(UBossCameraModifier::StaticClass()));
}

// 退出关卡时解除事件并清除镜头反馈
void UBossCombatCameraComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearCombatTarget();
	if (const AHearBladeCharacter* Player = Cast<AHearBladeCharacter>(GetOwner()))
		if (const APlayerController* PC = Cast<APlayerController>(Player->GetController()))
			if (PC->PlayerCameraManager && Modifier.IsValid())
				PC->PlayerCameraManager->RemoveCameraModifier(Modifier.Get());
	Super::EndPlay(Reason);
}

// 更换敌人事件绑定，接收预警、瞬移和交锋通知
void UBossCombatCameraComponent::BindCombo(UBeatSyncedComboComponent* Combo)
{
	if (UBeatSyncedComboComponent* Old = BoundCombo.Get())
	{
		Old->OnBlinkWarning.RemoveDynamic(this, &ThisClass::HandleWarning);
		Old->OnBlinkSucceeded.RemoveDynamic(this, &ThisClass::HandleBlink);
		Old->OnBlinkCancelled.RemoveDynamic(this, &ThisClass::HandleCancelled);
		Old->OnConfirmedHit.RemoveDynamic(this, &ThisClass::HandleConfirmedHit);
		Old->OnCinematicAccentStarted.RemoveDynamic(this, &ThisClass::HandleCinematicStarted);
		Old->OnCinematicAccentImpact.RemoveDynamic(this, &ThisClass::HandleCinematicImpact);
		Old->OnCinematicAccentEnded.RemoveDynamic(this, &ThisClass::HandleCinematicEnded);
	}
	BoundCombo = Combo;
	if (Combo)
	{
		Combo->OnBlinkWarning.AddUniqueDynamic(this, &ThisClass::HandleWarning);
		Combo->OnBlinkSucceeded.AddUniqueDynamic(this, &ThisClass::HandleBlink);
		Combo->OnBlinkCancelled.AddUniqueDynamic(this, &ThisClass::HandleCancelled);
		Combo->OnConfirmedHit.AddUniqueDynamic(this, &ThisClass::HandleConfirmedHit);
		Combo->OnCinematicAccentStarted.AddUniqueDynamic(this, &ThisClass::HandleCinematicStarted);
		Combo->OnCinematicAccentImpact.AddUniqueDynamic(this, &ThisClass::HandleCinematicImpact);
		Combo->OnCinematicAccentEnded.AddUniqueDynamic(this, &ThisClass::HandleCinematicEnded);
	}
}

// 目标锁定

// 切换锁定目标，并绑定新目标的战斗事件
void UBossCombatCameraComponent::SetCombatTarget(AActor* NewTarget)
{
	if (!IsValid(NewTarget) || NewTarget == GetOwner())
		return;
	if (Target.Get() == NewTarget)
		return;
	ClearCombatTarget();
	Target = NewTarget;
	bManualSuppression = false;
	OutOfRangeTime = OccludedTime = 0.f;
	BindCombo(NewTarget->FindComponentByClass<UBeatSyncedComboComponent>());
	UE_LOG(LogTemp, Display, TEXT("CombatCamera: locked %s"), *GetNameSafe(NewTarget));
}

// 清除当前镜头冲击和特写
void UBossCombatCameraComponent::CancelFeedback()
{
	bWarning = false;
	if (UBossCameraModifier* M = Modifier.Get())
		M->Clear();
}

// 取消目标锁定并解除敌人事件绑定
void UBossCombatCameraComponent::ClearCombatTarget()
{
	BindCombo(nullptr);
	Target.Reset();
	OutOfRangeTime = OccludedTime = 0.f;
	ManualYaw = ManualPitch = 0.f;
	CancelFeedback();
}

// 已有目标时取消锁定，否则寻找可锁定敌人
void UBossCombatCameraComponent::ToggleLockOn()
{
	if (bPhasePresentation)
		return;
	if (Target.IsValid())
	{
		ClearCombatTarget();
		bManualSuppression = true;
		return;
	}
	bManualSuppression = false;
	if (ABossBeatDirector* D = Director.Get())
		if (IsValid(D->BossActor))
			SetCombatTarget(D->BossActor);
}

// 把视角输入交给当前自由观察或锁定镜头流程
void UBossCombatCameraComponent::AddLookInput(float Yaw, float Pitch)
{
	ManualYaw = FMath::Clamp(ManualYaw + Yaw, -22.f, 22.f);
	ManualPitch = FMath::Clamp(ManualPitch + Pitch, -12.f, 12.f);
	LastLookInputTime = GetWorld()->GetTimeSeconds();
}

// 闪避与反击反馈

// 根据闪避结果和攻击方向设置镜头冲击参数
void UBossCombatCameraComponent::TriggerPerfectDodgeFeedback(float FOVExpansionDegrees, float ShakeDegrees,
                                                             float Strength, FVector BladeWorldLocation,
                                                             FVector AttackDirection, bool bStrongBeat,
                                                             float LateralShiftCm, float DollyShiftCm,
                                                             float YawKickDegrees, float PitchKickDegrees,
                                                             float ReboundStrength)
{
	AHearBladeCharacter* Player = Cast<AHearBladeCharacter>(GetOwner());
	if (!Player || !Modifier.IsValid())
		return;
	float ObstructionScale = 1.0f;
	if (USpringArmComponent* Boom = Player->GetCameraBoom())
	{
		const float Compression = Boom->TargetArmLength > 1.f
		                              ? FVector::Dist(Boom->GetComponentLocation(),
		                                              Player->GetFollowCamera()->GetComponentLocation()) /
		                                    Boom->TargetArmLength
		                              : 1.f;
		if (Compression < .72f)
			ObstructionScale = .25f;
	}
	if (Target.IsValid() && FVector::Dist2D(Target->GetActorLocation(), Player->GetActorLocation()) < 170.f)
		ObstructionScale = FMath::Min(ObstructionScale, .25f);
	float SideSign = 1.f;
	if (const UCameraComponent* Camera = Player->GetFollowCamera())
	{
		FVector ToBlade = BladeWorldLocation - Player->GetActorLocation();
		if (ToBlade.IsNearlyZero())
			ToBlade = AttackDirection;
		SideSign =
		    FVector::DotProduct(ToBlade.GetSafeNormal2D(), Camera->GetRightVector().GetSafeNormal2D()) >= 0.f
		        ? 1.f
		        : -1.f;
	}
	const float BeatAccent = bStrongBeat ? 1.12f : 1.f;
	const float FinalStrength = Strength * ObstructionScale * BeatAccent;
	// 镜头先收紧再略微回弹，形成命中后的短暂冲击
	Modifier->TriggerPerfectDodge(FOVExpansionDegrees * FinalStrength, ShakeDegrees * FinalStrength,
	                              LateralShiftCm * FinalStrength, DollyShiftCm * FinalStrength,
	                              YawKickDegrees * FinalStrength, PitchKickDegrees * FinalStrength, SideSign,
	                              ReboundStrength, .032f, .040f, bStrongBeat ? .19f : .16f);
	UE_LOG(LogTemp, Display, TEXT("PerfectDodgeCamera: side=%.0f strong=%d scale=%.2f lateral=%.1f yaw=%.2f"),
	       SideSign, bStrongBeat ? 1 : 0, FinalStrength, LateralShiftCm * FinalStrength,
	       YawKickDegrees * FinalStrength);
}

// 为成功闪避选择镜头反馈强度
void UBossCombatCameraComponent::TriggerDodgeSuccessFeedback(bool bPerfect, FVector BladeWorldLocation,
                                                             FVector AttackDirection, bool bStrongBeat)
{
	// 闪避时拉开视野并横移镜头，让擦身而过的剑留在画面内
	TriggerPerfectDodgeFeedback(bPerfect ? -5.f : -2.5f, bPerfect ? .065f : .035f, 1.f, BladeWorldLocation,
	                            AttackDirection, bStrongBeat, bPerfect ? 17.f : 9.f, bPerfect ? -6.f : -3.f,
	                            bPerfect ? .65f : .35f, .08f, .12f);
}

// 为反击成功设置镜头反馈
void UBossCombatCameraComponent::TriggerCounterSuccessFeedback(bool bPerfect, FVector BladeWorldLocation,
                                                               FVector AttackDirection, bool bStrongBeat)
{
	// 反击时靠近 Boss 并短暂停留，突出双方武器接触
	CinematicFocusLocation = BladeWorldLocation;
	CinematicFocusEndWorldSeconds =
	    GetWorld() ? GetWorld()->GetTimeSeconds() + (bPerfect ? .22 : .13) : -1000.;
	TriggerPerfectDodgeFeedback(bPerfect ? 6.5f : 3.f, bPerfect ? .17f : .075f, 1.f, BladeWorldLocation,
	                            AttackDirection, bStrongBeat, 0.f, bPerfect ? 12.f : 6.f, .24f,
	                            bPerfect ? -.2f : -.1f, .18f);
}

// 触发踢击的拉近、侧倾与短暂震动
void UBossCombatCameraComponent::TriggerKickAccent(float FOVKickDegrees, float RollDegrees,
                                                   float PitchDipDegrees, float ShakeDegrees,
                                                   FVector ContactWorld, float HoldSeconds)
{
	AHearBladeCharacter* Player = Cast<AHearBladeCharacter>(GetOwner());
	UBossCameraModifier* CameraModifier = Modifier.Get();
	if (!Player || !CameraModifier)
		return;
	float SideSign = 1.f;
	if (const UCameraComponent* Camera = Player->GetFollowCamera())
	{
		const FVector ToContact = ContactWorld - Player->GetActorLocation();
		SideSign = FVector::DotProduct(ToContact.GetSafeNormal2D(),
		                               Camera->GetRightVector().GetSafeNormal2D()) >= 0.f
		               ? 1.f
		               : -1.f;
	}
	CameraModifier->TriggerPerfectDodge(FMath::Clamp(FOVKickDegrees, 0.f, 6.f),
	                                    FMath::Clamp(ShakeDegrees, 0.f, .25f), 0.f, 6.f, 0.f,
	                                    FMath::Clamp(PitchDipDegrees, 0.f, .6f), SideSign, .25f, .03f,
	                                    FMath::Clamp(HoldSeconds, .04f, .22f), .32f);
	CameraModifier->RollKick = FMath::Clamp(RollDegrees, 0.f, 4.f);
}

// 触发挑飞时的向上镜头反馈
void UBossCombatCameraComponent::TriggerUppercutAccent(float Strength)
{
	UBossCameraModifier* CameraModifier = Modifier.Get();
	if (!CameraModifier || !GetOwner())
		return;
	const float Power = FMath::Clamp(Strength, 0.f, 1.5f);
	// 近身反击也保留镜头反馈，通过幅度限制避免遮住目标
	CameraModifier->TriggerPerfectDodge(2.1f + 1.1f * Power, .035f + .035f * Power, 0.f, 4.f, 0.f,
	                                    .32f + .28f * Power, 1.f, .22f, .028f, .055f, .25f);
	UE_LOG(LogTemp, Display, TEXT("UppercutCamera: fov=%.2f pitch=%.2f"), 2.1f + 1.1f * Power,
	       .32f + .28f * Power);
}

// 根据漏判或失误减弱、调整当前镜头反馈
void UBossCombatCameraComponent::TriggerFailureFeedback(bool bMiss)
{
	AHearBladeCharacter* Player = Cast<AHearBladeCharacter>(GetOwner());
	UBossCameraModifier* CameraModifier = Modifier.Get();
	if (!Player || !CameraModifier)
		return;

	float ObstructionScale = 1.0f;
	if (const USpringArmComponent* Boom = Player->GetCameraBoom())
	{
		const float ActualArm =
		    FVector::Dist(Boom->GetComponentLocation(), Player->GetFollowCamera()->GetComponentLocation());
		if (Boom->TargetArmLength > 1.0f && ActualArm / Boom->TargetArmLength < .72f)
			ObstructionScale = .25f;
	}
	if (Target.IsValid() && FVector::Dist2D(Target->GetActorLocation(), Player->GetActorLocation()) < 170.f)
		ObstructionScale = FMath::Min(ObstructionScale, .35f);

	// 失败只产生较弱的镜头偏移，不改动玩家输入的视角
	const float Shake = (bMiss ? .18f : .10f) * ObstructionScale;
	const float Dolly = (bMiss ? -3.2f : -1.4f) * ObstructionScale;
	const float Pitch = (bMiss ? -.12f : -.055f) * ObstructionScale;
	CameraModifier->TriggerPerfectDodge(0.0f, Shake, 0.0f, Dolly, 0.0f, Pitch, 1.0f, 0.0f, .018f, .012f,
	                                    bMiss ? .18f : .14f);
	UE_LOG(LogTemp, Verbose, TEXT("RhythmFailureCamera: miss=%d shake=%.2f obstruction=%.2f"), bMiss ? 1 : 0,
	       Shake, ObstructionScale);
}

// 攻击预警与持续格挡反馈

// 收到攻击预警后更新镜头关注位置
void UBossCombatCameraComponent::HandleWarning(FVector Landing, double)
{
	bWarning = true;
	WarningLanding = Landing;
	UE_LOG(LogTemp, Display, TEXT("CombatCamera: warning Landing=%s"), *Landing.ToString());
}

// 开始持续格挡的轻微镜头震动
void UBossCombatCameraComponent::StartHoldFeedback(float ShakeDegrees)
{
	if (UBossCameraModifier* M = Modifier.Get())
		M->StartHoldShake(ShakeDegrees);
}

// 为持续格挡中的一次接触叠加短促震动
void UBossCombatCameraComponent::TriggerHoldImpact(float ShakeDegrees, float DurationSeconds)
{
	if (UBossCameraModifier* M = Modifier.Get())
		M->TriggerHoldImpact(ShakeDegrees, DurationSeconds);
}

// 逐渐停止持续格挡震动
void UBossCombatCameraComponent::StopHoldFeedback(float FadeSeconds)
{
	if (UBossCameraModifier* M = Modifier.Get())
		M->StopHoldShake(FadeSeconds);
}

// 响应 Boss 瞬移，更新镜头反馈
void UBossCombatCameraComponent::HandleBlink(FVector, double)
{
	bWarning = false;
	const AHearBladeCharacter* Player = Cast<AHearBladeCharacter>(GetOwner());
	const USpringArmComponent* Boom = Player ? Player->GetCameraBoom() : nullptr;
	float Strength = 1.f;
	if (Boom && FVector::Dist(Boom->GetSocketTransform(USpringArmComponent::SocketName).GetLocation(),
	                          Boom->GetComponentLocation()) < Boom->TargetArmLength * .55f)
		Strength = .25f;
	if (Player && Target.IsValid() &&
	    FVector::Dist(Player->GetActorLocation(), Target->GetActorLocation()) < 300.f)
		Strength = FMath::Min(Strength, .45f);
	if (const APlayerController* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr)
	{
		int32 Width = 0, Height = 0;
		FVector2D Screen;
		PC->GetViewportSize(Width, Height);
		if (!PC->ProjectWorldLocationToScreen(Target->GetActorLocation() + FVector(0, 0, 130), Screen) ||
		    Screen.X < Width * .12f || Screen.X > Width * .88f || Screen.Y < Height * .1f ||
		    Screen.Y > Height * .9f)
			Strength = FMath::Min(Strength, .25f);
	}
	if (UBossCameraModifier* M = Modifier.Get())
	{
		const UBossCameraConfig* C = Settings();
		M->Trigger(C->BlinkFOVKick * Strength, C->BlinkShakeDegrees * Strength, C->BlinkPushSeconds,
		           C->BlinkHoldSeconds, C->BlinkRecoverSeconds);
		UE_LOG(LogTemp, Display, TEXT("CombatCamera: blink camera kick=%.1f shake=%.2f"),
		       C->BlinkFOVKick * Strength, C->BlinkShakeDegrees * Strength);
	}
}

// 攻击取消时停止对应镜头表现
void UBossCombatCameraComponent::HandleCancelled()
{
	CancelFeedback();
}

// 实际命中确认后触发镜头反馈
void UBossCombatCameraComponent::HandleConfirmedHit()
{
	if (UBossCameraModifier* M = Modifier.Get())
	{
		const UBossCameraConfig* C = Settings();
		M->Trigger(C->BlinkFOVKick, C->HitShakeDegrees, C->BlinkPushSeconds, C->BlinkHoldSeconds,
		           C->BlinkRecoverSeconds);
	}
}

// 关键交锋镜头

// 进入交锋慢放时调整镜头表现
void UBossCombatCameraComponent::HandleCinematicStarted(float Strength, bool bStrongBeat)
{
	if (UBossCameraModifier* M = Modifier.Get())
	{
		const float Accent = FMath::Clamp(Strength, .25f, 2.f) * (bStrongBeat ? 1.12f : 1.f);
		// 蓄力开始时轻微收紧视野，仍保持攻击可见
		M->Trigger(3.4f * Accent, .035f * Accent, .10f, .08f, .42f);
		UE_LOG(LogTemp, Display, TEXT("BeatCinematicCamera: PREP strength=%.2f strong=%d"), Accent,
		       bStrongBeat ? 1 : 0);
	}
}

// 在实际交锋位置触发镜头冲击
void UBossCombatCameraComponent::HandleCinematicImpact(float Strength, bool bStrongBeat,
                                                       FVector ClashLocation)
{
	CinematicFocusLocation = ClashLocation;
	CinematicFocusEndWorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() + .22 : -1000.0;
	if (UBossCameraModifier* M = Modifier.Get())
	{
		const float Accent = FMath::Clamp(Strength, .25f, 2.f) * (bStrongBeat ? 1.18f : 1.f);
		M->TriggerPerfectDodge(4.2f * Accent, .16f * Accent, 5.f * Accent, 7.f * Accent, .14f * Accent,
		                       -.08f * Accent, 1.f, .18f, .025f, .04f, .26f);
		UE_LOG(LogTemp, Display, TEXT("BeatCinematicCamera: IMPACT strength=%.2f strong=%d"), Accent,
		       bStrongBeat ? 1 : 0);
	}
}

// 交锋演出结束后恢复镜头反馈状态
void UBossCombatCameraComponent::HandleCinematicEnded()
{
	UE_LOG(LogTemp, Verbose, TEXT("BeatCinematicCamera: recovery"));
}

// 记录挑飞目标和持续时间，开始侧面特写
void UBossCombatCameraComponent::TriggerLaunchShot(AActor* Subject, float Seconds)
{
	AHearBladeCharacter* Player = Cast<AHearBladeCharacter>(GetOwner());
	if (!Subject || !Player || !GetWorld() || bPhasePresentation)
		return;
	// 优先沿当前镜头所在侧移动，避免突然翻到另一侧
	FVector Axis = Subject->GetActorLocation() - Player->GetActorLocation();
	Axis.Z = 0.f;
	if (!Axis.Normalize())
		Axis = Player->GetActorForwardVector();
	const FVector Side(-Axis.Y, Axis.X, 0.f);
	const FVector Mid = (Player->GetActorLocation() + Subject->GetActorLocation()) * .5f;
	const FVector CameraLoc =
	    Player->GetFollowCamera() ? Player->GetFollowCamera()->GetComponentLocation() : Mid - Axis * 300.f;
	ShotSide = FVector::DotProduct(CameraLoc - Mid, Side) >= 0.f ? 1.f : -1.f;
	// 当前侧被墙挡住时尝试另一侧
	FCollisionQueryParams Q(SCENE_QUERY_STAT(LaunchShot), false, Player);
	Q.AddIgnoredActor(Subject);
	if (GetWorld()->LineTraceTestByChannel(Mid + FVector(0, 0, 60),
	                                       Mid + Side * ShotSide * 380.f + FVector(0, 0, 60), ECC_Camera, Q))
		ShotSide = -ShotSide;
	ShotSubject = Subject;
	ShotStartWorld = GetWorld()->GetTimeSeconds();
	ShotSeconds = FMath::Max(.6f, Seconds);
	bShotInit = false;
	UE_LOG(LogTemp, Display, TEXT("LaunchShot: side=%.0f seconds=%.2f"), ShotSide, ShotSeconds);
}

// 跟随挑飞目标更新特写位置，并平滑回到普通镜头
void UBossCombatCameraComponent::TickLaunchShot(float DeltaTime)
{
	UBossCameraModifier* M = Modifier.Get();
	if (!M)
		return;
	AHearBladeCharacter* Player = Cast<AHearBladeCharacter>(GetOwner());
	AActor* Subject = ShotSubject.Get();
	const float Age = GetWorld() ? float(GetWorld()->GetTimeSeconds() - ShotStartWorld) : 1.e9f;
	if (!Subject || !Player || bPhasePresentation || Age > ShotSeconds)
	{
		M->ShotWeight = 0.f;
		ShotSubject.Reset();
		bShotInit = false;
		return;
	}
	constexpr float In = .2f, Out = .45f;
	float W = Age < In                  ? FMath::SmoothStep(0.f, 1.f, Age / In)
	          : Age > ShotSeconds - Out ? 1.f - FMath::SmoothStep(0.f, 1.f, (Age - (ShotSeconds - Out)) / Out)
	                                    : 1.f;
	W *= .94f;
	// 先根据脚底高度计算取景点，避免高大 Boss 的胶囊中心把镜头引向上方
	auto Ground = [](const AActor* A)
	{
		const UCapsuleComponent* Cap = A->FindComponentByClass<UCapsuleComponent>();
		const float Half = Cap ? Cap->GetScaledCapsuleHalfHeight() : 90.f;
		return TPair<FVector, float>(A->GetActorLocation() - FVector(0, 0, Half), Half);
	};
	const TPair<FVector, float> PG = Ground(Player), BG = Ground(Subject);
	FVector Axis = BG.Key - PG.Key;
	Axis.Z = 0.f;
	const float Sep = Axis.Size();
	Axis = Sep > 1.f ? Axis / Sep : Player->GetActorForwardVector();
	const FVector Side(-Axis.Y * ShotSide, Axis.X * ShotSide, 0.f);
	const FVector MidGround = (PG.Key + BG.Key) * .5f;
	const float T = FMath::Clamp(Age / ShotSeconds, 0.f, 1.f);
	// 以双方胸部之间为目标，限制 Boss 升空对取景高度的影响
	const float ChestZ = (PG.Key.Z + PG.Value * 1.3f + BG.Key.Z + BG.Value * 1.2f) * .5f;
	const float Lift = 25.f * FMath::Sin(FMath::Clamp(Age / .9f, 0.f, 1.f) * PI);
	const float Dist =
	    FMath::Clamp(Sep * 1.2f + 480.f, 560.f, 1000.f) * FMath::Lerp(1.f, .93f, T); // 同时容纳玩家和 Boss
	const FVector Look(MidGround.X, MidGround.Y, ChestZ + Lift);
	// 镜头保持在胸部附近，略微俯视双方
	FVector Desired = FVector(MidGround.X, MidGround.Y, ChestZ + 45.f) + Side * Dist - Axis * 30.f;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(LaunchShotTick), false, Player);
	Q.AddIgnoredActor(Subject);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Look, Desired, ECC_Camera, Q))
		Desired = Hit.Location + Hit.ImpactNormal * 25.f;
	FRotator Rot = (Look - Desired).Rotation();
	Rot.Roll = -3.f * ShotSide; // 轻微侧倾
	if (!bShotInit)
	{
		ShotSmoothedLoc = Desired;
		ShotSmoothedRot = Rot;
		bShotInit = true;
	}
	ShotSmoothedLoc = FMath::VInterpTo(ShotSmoothedLoc, Desired, DeltaTime, 12.f);
	ShotSmoothedRot = FMath::RInterpTo(ShotSmoothedRot, Rot, DeltaTime, 12.f);
	M->ShotLocation = ShotSmoothedLoc;
	M->ShotRotation = ShotSmoothedRot;
	M->ShotFOV = 60.f;
	M->ShotWeight = W;
}

// 镜头每帧更新

// 更新锁定方向与距离，再推进镜头反馈和特写
void UBossCombatCameraComponent::TickComponent(float DeltaTime, ELevelTick Type,
                                               FActorComponentTickFunction* Tick)
{
	Super::TickComponent(DeltaTime, Type, Tick);
	TickLaunchShot(DeltaTime);
	// 阶段演出接管镜头时，普通锁定逻辑不再覆盖镜头参数
	if (bPhasePresentation)
		return;
	AHearBladeCharacter* Player = Cast<AHearBladeCharacter>(GetOwner());
	APlayerController* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
	if (!PC || !PC->IsLocalController())
		return;
	const UBossCameraConfig* C = Settings();
	if (!Modifier.IsValid() && PC->PlayerCameraManager)
		Modifier = Cast<UBossCameraModifier>(
		    PC->PlayerCameraManager->AddNewCameraModifier(UBossCameraModifier::StaticClass()));
	if (!Director.IsValid())
		for (TActorIterator<ABossBeatDirector> It(GetWorld()); It; ++It)
		{
			Director = *It;
			break;
		}
	ABossBeatDirector* D = Director.Get();
	const bool bEncounter = IsValid(D) && IsValid(D->BossActor) && IsValid(D->BehaviorTreeComponent) &&
	                        D->BehaviorTreeComponent->IsRunning();
	AActor* Boss = bEncounter ? D->BossActor.Get() : nullptr;
	const float Distance =
	    Boss ? FVector::Dist(Player->GetActorLocation(), Boss->GetActorLocation()) : BIG_NUMBER;
	if (bManualSuppression && Distance > C->ReleaseDistance)
		bManualSuppression = false;
	if (!Target.IsValid() && !bManualSuppression && Boss && Distance < C->AcquireDistance)
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BossCameraAcquire), false, Player);
		Q.AddIgnoredActor(Boss);
		if (!GetWorld()->LineTraceTestByChannel(Player->GetFollowCamera()->GetComponentLocation(),
		                                        Boss->GetActorLocation() + FVector(0, 0, 120), ECC_Visibility,
		                                        Q))
			SetCombatTarget(Boss);
	}
	if (Target.IsValid())
	{
		if (!bEncounter || Target.Get() != Boss)
		{
			ClearCombatTarget();
			return;
		}
		// 距离和遮挡都需要持续一段时间才解除锁定，避免边界附近反复切换
		OutOfRangeTime = Distance > C->ReleaseDistance ? OutOfRangeTime + DeltaTime : 0.f;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BossCameraOcclusion), false, Player);
		Q.AddIgnoredActor(Boss);
		OccludedTime = GetWorld()->LineTraceTestByChannel(Player->GetFollowCamera()->GetComponentLocation(),
		                                                  Boss->GetActorLocation() + FVector(0, 0, 120),
		                                                  ECC_Visibility, Q)
		                   ? OccludedTime + DeltaTime
		                   : 0.f;
		if (OutOfRangeTime > C->ReleaseDelay || OccludedTime > C->OcclusionGrace)
		{
			ClearCombatTarget();
			bManualSuppression = true;
		}
	}
	USpringArmComponent* Boom = Player->GetCameraBoom();
	UCameraComponent* Camera = Player->GetFollowCamera();
	if (!Boom || !Camera)
		return;
	if (AActor* Locked = Target.Get())
	{
		FVector AreaCenter;
		float AreaRadius = 0.f, AreaProgress = 0.f;
		const auto* AreaCombo = Locked->FindComponentByClass<UBeatSyncedComboComponent>();
		const bool bAreaWarning =
		    AreaCombo && AreaCombo->GetGroundBurstPreview(AreaCenter, AreaRadius, AreaProgress);
		FAttackRecognitionInfo ReadInfo;
		FVector ReadPosition;
		float ReadProgress = 0.f;
		double ReadKey = 0.;
		const bool bReadAttack =
		    AreaCombo && AreaCombo->GetRecognitionCue(ReadInfo, ReadPosition, ReadProgress, ReadKey);
		const float ReadWeight = bReadAttack ? FMath::SmoothStep(.08f, .7f, ReadProgress) : 0.f;
		const float Near = 1.f - FMath::Clamp((Distance - 250.f) / 650.f, 0.f, 1.f);
		Boom->TargetOffset = FMath::VInterpTo(Boom->TargetOffset, FVector(0.f, 0.f, C->CombatPivotHeight),
		                                      DeltaTime, C->DistanceSpeed);
		Boom->SocketOffset = FMath::VInterpTo(Boom->SocketOffset, FVector(0.f, C->CombatShoulderOffset, 0.f),
		                                      DeltaTime, C->DistanceSpeed);
		const FVector Eye = Player->GetActorLocation() + FVector(0, 0, 105);
		const FVector Chest = Locked->GetActorLocation() + FVector(0, 0, 125);
		FVector Focus =
		    FMath::Lerp(Eye, Chest, FMath::Lerp(C->BossFocusWeight * .75f, C->BossFocusWeight, 1.f - Near));
		if (bWarning)
			Focus = FMath::Lerp(Focus, WarningLanding + FVector(0, 0, 70), C->WarningLandingWeight);
		if (bAreaWarning)
			Focus = FMath::Lerp(Focus, AreaCenter + FVector(0, 0, 60), .45f);
		if (bReadAttack && ReadInfo.bCounterCue)
			Focus = FMath::Lerp(Focus, ReadPosition, .2f * ReadWeight);
		else if (bReadAttack && ReadInfo.bDodgeOnlyCue)
		{
			Focus = FMath::Lerp(Focus, ReadPosition, .28f * ReadWeight);
			// 必须闪避的攻击将视点抬向上身，让玩家看到完整起手动作
			Focus = FMath::Lerp(Focus, Chest + FVector(0, 0, 35), .25f * ReadWeight);
		}
		const bool bClashFocus = GetWorld()->GetTimeSeconds() < CinematicFocusEndWorldSeconds;
		if (bClashFocus)
			Focus = FMath::Lerp(Focus, CinematicFocusLocation, .72f);
		// 用未受遮挡压缩的镜头位置计算朝向，避免靠墙时镜头伸缩与转向相互影响
		const FRotator CurrentRotation = PC->GetControlRotation();
		const FVector VirtualCamera = Boom->GetComponentLocation() + Boom->TargetOffset -
		                              CurrentRotation.Vector() * Boom->TargetArmLength +
		                              CurrentRotation.RotateVector(Boom->SocketOffset);
		FRotator Aim = (Focus - VirtualCamera).Rotation();
		Aim.Pitch = FMath::ClampAngle(Aim.Pitch, -40.f, FMath::Lerp(20.f, C->MaxCloseRangeUpwardPitch, Near));
		// 必须闪避的攻击略微抬高视角，突出 Boss 的起手姿态
		if (bReadAttack && ReadInfo.bDodgeOnlyCue)
			Aim.Pitch += 5.f * ReadWeight;
		if (GetWorld()->GetTimeSeconds() - LastLookInputTime > .6f)
		{
			ManualYaw = FMath::FInterpTo(ManualYaw, 0.f, DeltaTime, 3.f);
			ManualPitch = FMath::FInterpTo(ManualPitch, 0.f, DeltaTime, 3.f);
		}
		// 忽略小角度偏差并限制转向速度，减少近身闪避时镜头突然甩动
		const bool bLooking = GetWorld()->GetTimeSeconds() - LastLookInputTime <= .6f;
		if (!bLooking)
		{
			const float DeadZone = FMath::Lerp(6.f, 14.f, Near); // 近距离允许更大的朝向偏差
			const float Err = FRotator::NormalizeAxis(Aim.Yaw - CurrentRotation.Yaw);
			Aim.Yaw = CurrentRotation.Yaw + FMath::Sign(Err) * FMath::Max(0.f, FMath::Abs(Err) - DeadZone);
		}
		Aim.Yaw += ManualYaw;
		Aim.Pitch += ManualPitch;
		FRotator Next = FMath::RInterpTo(CurrentRotation, Aim, DeltaTime, FMath::Min(C->RotationSpeed, 3.5f));
		const float MaxTurn = 150.f * DeltaTime;
		Next.Yaw = CurrentRotation.Yaw +
		           FMath::Clamp(FRotator::NormalizeAxis(Next.Yaw - CurrentRotation.Yaw), -MaxTurn, MaxTurn);
		PC->SetControlRotation(Next);
		// 先按双方距离求镜头臂长，再根据对刀和范围预警调整取景
		float DesiredArm = FMath::Clamp(FMath::Max(C->CombatArmLengthAtCloseRange, 400.f + Distance * .12f),
		                                C->FreeArmLength, C->MaxCombatArmLength) *
		                   .92f;
		if (bClashFocus)
			DesiredArm *= .72f;
		if (bAreaWarning)
			DesiredArm = FMath::Max(DesiredArm, FMath::Min(850.f, AreaRadius * 1.65f));
		if (bReadAttack && ReadInfo.bDodgeOnlyCue)
			DesiredArm *= 1.f - .16f * ReadWeight;
		else if (bReadAttack && ReadInfo.bCounterCue)
			DesiredArm *= 1.f - .06f * ReadWeight;
		Boom->TargetArmLength =
		    FMath::FInterpTo(Boom->TargetArmLength, DesiredArm, DeltaTime, C->DistanceSpeed);
		float DesiredFOV =
		    FMath::Max(FMath::Lerp(C->FreeFOV, C->CloseRangeFOV, Near),
			           FMath::GetMappedRangeValueClamped(FVector2D(500.f, 1800.f),
			                                             FVector2D(C->FreeFOV, C->MaxCombatFOV), Distance));
		if (bAreaWarning)
			DesiredFOV = FMath::Max(DesiredFOV, 95.f);
		if (bReadAttack && ReadInfo.bDodgeOnlyCue)
			DesiredFOV -= 9.f * ReadWeight; // 缩小视野角度，让 Boss 在画面中更突出
		else if (bReadAttack && ReadInfo.bCounterCue)
			DesiredFOV -= 3.f * ReadWeight;
		Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, DesiredFOV, DeltaTime, 5.f));
	}
	else
	{
		Boom->TargetOffset = FMath::VInterpTo(Boom->TargetOffset, FVector::ZeroVector, DeltaTime, 4.f);
		Boom->SocketOffset = FMath::VInterpTo(Boom->SocketOffset, FVector::ZeroVector, DeltaTime, 4.f);
		Boom->TargetArmLength =
		    FMath::FInterpTo(Boom->TargetArmLength, C->FreeArmLength * .92f, DeltaTime, 4.f);
		Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, C->FreeFOV, DeltaTime, 5.f));
	}
}
