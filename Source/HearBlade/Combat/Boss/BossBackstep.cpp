// 处理敌人连击后的后跳，以及玩家攻击造成的击退位移
#include "Combat/Boss/BeatSyncedComboComponent.h"
#include "Combat/Boss/BossBeatDirector.h"
#include "Combat/Boss/BossLocomotionAnimInstance.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

// 连击结束后按拍点后跳，后跳期间暂停行为树，结束时恢复

static void SetBossBrainPaused(UWorld* World, AActor* Boss, bool bPause)
{
	for (TActorIterator<ABossBeatDirector> It(World); It; ++It)
		if (It->BossActor == Boss && It->BehaviorTreeComponent)
		{
			if (bPause)
				It->BehaviorTreeComponent->PauseLogic(TEXT("Backstep"));
			else
				It->BehaviorTreeComponent->ResumeLogic(TEXT("Backstep"));
		}
}

// 根据状态、距离和连续次数安排后跳，起跳与落地分别对齐相邻拍点
void UBeatSyncedComboComponent::TryScheduleBackstep()
{
	UWorld* World = GetWorld();
	const UBeatClockSubsystem* Clock = World ? World->GetSubsystem<UBeatClockSubsystem>() : nullptr;
	AActor* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Clock || !Clock->IsRunning() || !Player || BossHealth <= 0.f)
	{
		return;
	}

	// 阶段转换和玩家共鸣期间，由各自流程控制敌人位置
	if (bPhaseTransitionActive || bPhaseChangePending || bPlayerResonance)
	{
		return;
	}

	// 教程陪练不插入随机后跳，避免打断指定练习
	if (bTutorialNoDamage || bPracticeNote)
	{
		return;
	}

	if (bBossHitReacting || bQueued || bPlaying || IsBackstepping())
	{
		return;
	}
	// 限制连续后跳次数和触发距离，再按概率决定是否后跳
	if (ConsecutiveBacksteps >= 3 || FMath::FRand() > BackstepChance ||
	    FVector::Dist2D(Player->GetActorLocation(), GetOwner()->GetActorLocation()) > BackstepTriggerDistance)
	{
		ConsecutiveBacksteps = 0;
		return;
	}
	// 每拍时长，单位为秒；本次后跳持续一个拍间隔
	const double SPB = Clock->GetSecondsPerBeat();
	// 至少留出短暂准备时间，在下一可用拍点起跳，一拍后落地
	double StartBeat = FMath::CeilToDouble(Clock->GetCurrentBeat());
	if (Clock->GetBeatTimeSeconds(StartBeat) - Clock->GetAudioNowSeconds() < .15)
		StartBeat += 1.;
	BackstepStartAudio = Clock->GetBeatTimeSeconds(StartBeat);
	BackstepEndAudio = BackstepStartAudio + SPB;
	bBackstepPending = true;
	bBackstepActive = false;
	BackstepTravelled = 0.f;
	++ConsecutiveBacksteps;
	SetBossBrainPaused(World, GetOwner(), true);
	UE_LOG(LogTemp, Display, TEXT("BossBackstep: scheduled beat=%.0f"), StartBeat);
}

void UBeatSyncedComboComponent::StartKickKnockback(const AActor* Source, float Distance, float Seconds)
{
	if (!Source || BossHealth <= 0.f || bPhaseTransitionActive || bPlayerResonance || IsBackstepping())
		return;
	FVector Dir = GetOwner()->GetActorLocation() - Source->GetActorLocation();
	Dir.Z = 0.f;
	KnockDir = Dir.GetSafeNormal();
	if (KnockDir.IsNearlyZero())
		KnockDir = -GetOwner()->GetActorForwardVector();
	// Boss 正在出刀时减少击退距离，保留攻击中的抗打断表现
	KnockDistance = bPlaying ? Distance * .3f : Distance;
	KnockSeconds = FMath::Max(.05f, Seconds);
	KnockElapsed = 0.f;
	KnockTravelled = 0.f;
	UE_LOG(LogTemp, Display, TEXT("BossKnockback: kick distance=%.0f"), KnockDistance);
}

void UBeatSyncedComboComponent::TickKnockback(float DeltaTime)
{
	if (KnockDistance <= 0.f || KnockTravelled >= KnockDistance)
		return;
	KnockElapsed += DeltaTime;
	const float U = FMath::Clamp(KnockElapsed / KnockSeconds, 0.f, 1.f);
	// 缓出曲线给出累计目标距离，每帧只应用与已移动距离的差
	const float Want = KnockDistance * (1.f - FMath::Square(1.f - U)); // fast shove, soft stop
	FHitResult Hit;
	GetOwner()->AddActorWorldOffset(KnockDir * (Want - KnockTravelled), true, &Hit);
	KnockTravelled = Hit.IsValidBlockingHit() ? KnockDistance : Want; // wall: stop
	if (U >= 1.f)
		KnockDistance = 0.f;
}

void UBeatSyncedComboComponent::TickBackstep(float DeltaTime)
{
	TickKnockback(DeltaTime);
	if (!IsBackstepping())
		return;
	UWorld* World = GetWorld();
	const UBeatClockSubsystem* Clock = World ? World->GetSubsystem<UBeatClockSubsystem>() : nullptr;
	AActor* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	// 成功、中断和音乐停止共用收尾，必须恢复行为树并清掉后跳状态
	auto Finish = [&]()
	{
		for (const TWeakObjectPtr<UAnimInstance>& A : BackstepAnims)
			if (UAnimInstance* Anim = A.Get())
				Anim->StopSlotAnimation(.18f, TEXT("DefaultSlot"));
		BackstepAnims.Reset();
		bBackstepPending = bBackstepActive = false;
		if (World)
			SetBossBrainPaused(World, GetOwner(), false);
		UE_LOG(LogTemp, Display, TEXT("BossBackstep: done travelled=%.0f"), BackstepTravelled);
	};
	if (!Clock || !Clock->IsRunning() || !Player || BossHealth <= 0.f || bPhaseTransitionActive ||
	    bPlayerResonance)
	{
		Finish();
		return;
	}
	const double Now = Clock->GetAudioNowSeconds();
	// 后跳期间暂停步行位移，并继续面向玩家
	LocomotionResumeWorldSeconds = World->GetTimeSeconds() + .1;
	FVector ToPlayer = Player->GetActorLocation() - GetOwner()->GetActorLocation();
	ToPlayer.Z = 0.f;
	if (!ToPlayer.IsNearlyZero())
	{
		FRotator Rot = GetOwner()->GetActorRotation();
		Rot.Yaw = FMath::RInterpTo(FRotator(0.f, Rot.Yaw, 0.f), ToPlayer.Rotation(), DeltaTime, 10.f).Yaw;
		GetOwner()->SetActorRotation(Rot);
	}
	if (bBackstepPending)
	{
		if (Now < BackstepStartAudio)
			return;
		bBackstepPending = false;
		bBackstepActive = true;
		BackstepDir = -ToPlayer.GetSafeNormal();
		if (BackstepDir.IsNearlyZero())
			BackstepDir = -GetOwner()->GetActorForwardVector();
		// 优先使用专用后跳动画，缺失时使用后退跑步动画
		UAnimSequence* Clip = LoadObject<UAnimSequence>(
		    nullptr, TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/Anim/avoid_back.avoid_back"), nullptr,
		    LOAD_NoWarn | LOAD_Quiet);
		const bool bHop = Clip != nullptr;
		// 身体由代码移动，动画只保留原地姿态
		// 提取并丢弃动画根位移，避免网格离开碰撞身体后突然弹回
		if (Clip && !Clip->bEnableRootMotion)
		{
			Clip->bEnableRootMotion = true;
			Clip->RootMotionRootLock = ERootMotionRootLock::AnimFirstFrame;
		}
		if (!Clip)
			Clip = LoadObject<UAnimSequence>(
			    nullptr,
			    TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/DEMO/UE5_ANIMATIONS/move_run_back.move_run_back"));
		if (Clip && !Clip->bEnableRootMotion)
		{
			Clip->bEnableRootMotion = true;
			Clip->RootMotionRootLock = ERootMotionRootLock::AnimFirstFrame;
		}
		const float Beat = float(BackstepEndAudio - BackstepStartAudio);
		const float Rate =
		    Clip ? (bHop ? FMath::Clamp(Clip->GetPlayLength() / FMath::Max(.1f, Beat), .7f, 2.f) : 1.35f)
			     : 1.f;
		TArray<USkeletalMeshComponent*> Meshes;
		GetOwner()->GetComponents(Meshes);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
			if (!Anim)
				continue;
			if (auto* Loco = Cast<UBossLocomotionAnimInstance>(Anim))
				Loco->SetLocomotion(0.f, FVector::ZeroVector, 0.f);
			// 所有动画的根位移都提取后丢弃，步行混合空间也不例外
			Anim->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
			if (!Clip || !Mesh->GetSkeletalMeshAsset() ||
			    Mesh->GetSkeletalMeshAsset()->GetSkeleton() != Clip->GetSkeleton())
				continue;
			if (Anim->PlaySlotAnimationAsDynamicMontage(Clip, TEXT("DefaultSlot"), .06f, .18f, Rate,
			                                            bHop ? 1 : 3))
				BackstepAnims.Add(Anim);
		}
		UE_LOG(LogTemp, Display, TEXT("BossBackstep: go clip=%s rate=%.2f meshes=%d"), *GetNameSafe(Clip),
		       Rate, BackstepAnims.Num());
	}
	// 按音乐时间计算后跳进度，每帧只应用尚未完成的距离
	const double U = FMath::Clamp(
	    (Now - BackstepStartAudio) / FMath::Max(.05, BackstepEndAudio - BackstepStartAudio), 0., 1.);
	// 缓出曲线给出累计目标距离，每帧只应用与已移动距离的差
	const float Want = BackstepDistance * float(1. - FMath::Pow(1. - U, 3.));
	const float Step = Want - BackstepTravelled;
	if (Step > .1f)
	{
		FHitResult Hit;
		const FVector Delta = BackstepDir * Step;
		GetOwner()->AddActorWorldOffset(Delta, true, &Hit);
		if (Hit.IsValidBlockingHit())
		{
			// 碰到墙后沿墙面尝试滑动，不穿过障碍
			const FVector Slide = FVector::VectorPlaneProject(Delta * (1.f - Hit.Time), Hit.Normal);
			GetOwner()->AddActorWorldOffset(FVector(Slide.X, Slide.Y, 0.f), true);
		}
		BackstepTravelled = Want;
	}
	if (U >= 1.)
		Finish();
}
