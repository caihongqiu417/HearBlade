// 执行敌人的节拍连击，连接攻击数据、动画推进、空间修正与伤害窗口
#include "Combat/Boss/BeatSyncedComboComponent.h"
#include "Combat/Feedback/TransientCombatFX.h"
#include "Combat/Feedback/AttackReadabilityComponent.h"
#include "Combat/Data/MusicEmphasisData.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Combat/Boss/BossBeatDirector.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "EngineUtils.h"

#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "Combat/Data/AttackData.h"
#include "Combat/Boss/BossMotionWarpingComponent.h"
#include "Combat/Boss/BossLocomotionAnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Combat/Player/PlayerRhythmCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "RootMotionModifier_SkewWarp.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "Engine/World.h"
#include "Components/PoseableMeshComponent.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

namespace
{
UNiagaraComponent* SpawnBriefFire(UWorld* World, UNiagaraSystem* Effect, const FVector& Location,
                                  float Lifetime, float Scale = 1.f)
{
	if (!World || !Effect)
		return nullptr;
	// 依赖角色网格采样的火星特效不能直接放在未附着的瞬移位置
	if (IsMeshSampledCourtEmber(Effect))
	{
		Effect = LoadObject<UNiagaraSystem>(
		    nullptr, TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/VFX/NS_Fire_local_System.NS_Fire_local_System"));
#if !UE_BUILD_SHIPPING
		if (FParse::Param(FCommandLine::Get(), TEXT("CourtFXProbe")))
		{
			static int32 CaptureIndex = 0;
			const int32 Index = ++CaptureIndex;
			UE_LOG(LogTemp, Display, TEXT("CourtFXProbe: blink uses %s at %s"), *GetPathNameSafe(Effect),
			       *Location.ToString());
			if (Index <= 2)
				for (const float Delay : {.1f, .5f, 1.2f})
				{
					FTimerHandle CaptureTimer;
					World->GetTimerManager().SetTimer(
					    CaptureTimer,
					    [Index, Delay]()
					    {
						    FScreenshotRequest::RequestScreenshot(
						        FPaths::ProjectSavedDir() /
						            FString::Printf(TEXT("Screenshots/BlinkFX_%d_%03d.png"), Index,
									                FMath::RoundToInt(Delay * 1000)),
						        false, false);
					    },
					    Delay, false);
				}
		}
#endif
	}
	if (!Effect)
		return nullptr;
	if (UNiagaraComponent* Component =
	        UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Effect, Location))
	{
		Component->SetWorldScale3D(FVector(Scale));
		RetireCombatFX(Component, FMath::Max(0.1f, Lifetime));
		return Component;
	}
	return nullptr;
}

void AlignBossToGround(AActor* Boss, AActor* Target)
{
	const UCapsuleComponent* Capsule = Boss ? Cast<UCapsuleComponent>(Boss->GetRootComponent()) : nullptr;
	if (!Capsule)
		return;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BossGroundAlign), false, Boss);
	if (Target)
		Query.AddIgnoredActor(Target);
	const FVector Location = Boss->GetActorLocation();
	FHitResult Ground;
	if (!Boss->GetWorld()->LineTraceSingleByChannel(
	        Ground, Location + FVector(0, 0, 60),
	        Location - FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight() + 180), ECC_Visibility, Query) ||
	    Ground.ImpactNormal.Z < 0.6f)
		return;
	const float GroundedZ = Ground.ImpactPoint.Z + Capsule->GetScaledCapsuleHalfHeight() + 2.f;
	if (FMath::Abs(GroundedZ - Location.Z) > 90.f)
		return;
	if (FMath::Abs(GroundedZ - Location.Z) > 0.5f)
		Boss->SetActorLocation(FVector(Location.X, Location.Y, GroundedZ), true);
}
}

UBeatSyncedComboComponent::UBeatSyncedComboComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	FinalImpactCinematic.bEnabled = true;
	FinalImpactCinematic.MusicAndBossRate = .88f;
	FinalImpactCinematic.LeadBeats = 0.f;
	FinalImpactCinematic.HoldAfterImpactBeats = .16f;
	FinalImpactCinematic.RecoverySeconds = .14f;
	FinalImpactCinematic.TriggerChance = 1.f;
	FinalImpactCinematic.CameraStrength = 1.f;
	FinalImpactCinematic.ClashActorRate = .48f;
	FinalImpactCinematic.ClashActorSlowSeconds = .11f;
	LocomotionAnimClass = UBossLocomotionAnimInstance::StaticClass();
	BloodHitMetalEffect = LoadObject<UNiagaraSystem>(
	    nullptr,
	    TEXT("/Game/Blood_VFX_Pack/Particles/Systems/P_Splash_HitWithMetal_01.P_Splash_HitWithMetal_01"));
	BloodHeavyMetalEffect = LoadObject<UNiagaraSystem>(
	    nullptr,
	    TEXT("/Game/Blood_VFX_Pack/Particles/Systems/P_BigSplash_HitWithMetal.P_BigSplash_HitWithMetal"));
}

// 接收引擎伤害事件，转入 Boss 的伤害处理
void UBeatSyncedComboComponent::ReceiveBossDamage(AActor* DamagedActor, float Damage,
                                                  const UDamageType* DamageType, AController* InstigatedBy,
                                                  AActor* DamageCauser)
{
	ApplyBossDamageFrom(Damage, DamageCauser);
}

// 处理没有明确来源的伤害
void UBeatSyncedComboComponent::ApplyBossDamage(float Damage)
{
	ApplyBossDamageFrom(Damage, AttackTarget.Get());
}

// 扣除生命并更新受击表现，死亡时停止当前连招
void UBeatSyncedComboComponent::ApplyBossDamageFrom(float Damage, const AActor* DamageSource,
                                                    bool bForceHeavyBlood, float ExplicitPoise,
                                                    FVector ImpactLocation)
{
	if (!FMath::IsFinite(Damage) || Damage <= 0.f || BossHealth <= 0.f || bPhaseTransitionActive)
		return;
	const float PoiseDamage = ExplicitPoise >= 0.f ? ExplicitPoise : Damage * PoiseDamagePerHealthDamage;
	const bool bHeavyBlood = bForceHeavyBlood || PoiseDamage >= BossPoise || Damage >= BossHealth;
	SpawnBossBlood(DamageSource, bHeavyBlood, ImpactLocation);
	// 教程中仍播放受击和硬直，但不扣 Boss 生命，避免练习时触发阶段转换

	if (bTutorialNoDamage)
	{
		ApplyBossPoiseDamage(PoiseDamage, DamageSource);
		return;
	}
	BossHealth = FMath::Clamp(BossHealth - Damage, 0.f, MaxBossHealth);
	if (bEnableSecondPhase && BossPhase == 1 && BossHealth > 0.f &&
	    GetBossHealthFraction() <= SecondPhaseHealthThreshold)
	{
		if (!bPhaseChangePending)
			PhaseRequestWorldSeconds = GetWorld()->GetTimeSeconds();
		bPhaseChangePending = true;
	}
	UE_LOG(LogTemp, Display, TEXT("BossHealth: %.0f / %.0f"), BossHealth, MaxBossHealth);
	if (BossHealth <= 0.f)
	{
		CancelCombo();
		for (TActorIterator<ABossBeatDirector> It(GetWorld()); It; ++It)
			if (It->BossActor == GetOwner() && It->BehaviorTreeComponent)
				It->BehaviorTreeComponent->StopTree(EBTStopMode::Forced);
		return;
	}
	if (!bPhaseChangePending)
		ApplyBossPoiseDamage(PoiseDamage, DamageSource);
}

// 根据攻击来源确定方向，在命中位置生成血液效果
void UBeatSyncedComboComponent::SpawnBossBlood(const AActor* DamageSource, bool bHeavy,
                                               FVector ImpactLocation) const
{
	if (!GetWorld() || !GetOwner())
		return;
	UNiagaraSystem* Effect = bHeavy ? BloodHeavyMetalEffect.Get() : BloodHitMetalEffect.Get();
	if (!Effect)
		return;
	FVector Direction = DamageSource ? GetOwner()->GetActorLocation() - DamageSource->GetActorLocation()
	                                 : GetOwner()->GetActorForwardVector();
	Direction.Z = FMath::Clamp(Direction.Z, -0.25f, 0.25f);
	if (!Direction.Normalize())
		Direction = GetOwner()->GetActorForwardVector();
	const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(GetOwner()->GetRootComponent());
	const float Height = Capsule ? Capsule->GetScaledCapsuleHalfHeight() * 0.25f : 25.0f;
	const FVector Location = ImpactLocation.IsNearlyZero()
	                             ? GetOwner()->GetActorLocation() + FVector(0, 0, Height)
	                             : ImpactLocation;
	const float Scale = (bHeavy ? BloodHeavyScale : BloodHitScale) * .65f;
	RetireCombatFX(UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Effect, Location,
	                                                              Direction.Rotation(), FVector(Scale)),
	               .12f, .65f);
	UE_LOG(LogTemp, Display, TEXT("BossBloodVFX: heavy=%d effect=%s location=%s"), bHeavy ? 1 : 0,
	       *GetNameSafe(Effect), *Location.ToString());
}

// 减少韧性，达到破防条件时打断攻击并进入硬直
void UBeatSyncedComboComponent::ApplyBossPoiseDamage(float Amount, const AActor* DamageSource)
{
	if (!GetWorld() || Amount <= 0.0f || BossHealth <= 0.0f)
		return;
	const bool bWasAttacking = bQueued || bPlaying;
	BossPoise = FMath::Max(0.0f, BossPoise - Amount);
	LastBossPoiseDamageWorldSeconds = GetWorld()->GetTimeSeconds();
	UE_LOG(LogTemp, Display, TEXT("BossPoise: damage=%.1f remaining=%.1f/%.1f attacking=%d"), Amount,
	       BossPoise, MaxBossPoise, bWasAttacking ? 1 : 0);
	if (BossPoise <= KINDA_SMALL_NUMBER)
	{
		StartBossHitReaction(DamageSource, HeavyHitReactionSeconds, true);
		BossPoise = MaxBossPoise;
	}
	else if (!bWasAttacking)
	{
		StartBossHitReaction(DamageSource, LightHitReactionSeconds, false);
	}
	else
	{
		TArray<USkeletalMeshComponent*> Meshes;
		GetOwner()->GetComponents(Meshes);
		const FVector Local =
		    GetOwner()
		        ->GetActorTransform()
		        .InverseTransformVectorNoScale(DamageSource ? DamageSource->GetActorLocation() -
				                                                  GetOwner()->GetActorLocation()
				                                            : GetOwner()->GetActorForwardVector())
		        .GetSafeNormal2D();
		for (auto* Mesh : Meshes)
			if (auto* Anim = Cast<UBossLocomotionAnimInstance>(Mesh->GetAnimInstance()))
				Anim->PlayArmorFlinch(FVector2D(Local.X, Local.Y), Amount >= 25.f ? 1.f : .65f);
		UE_LOG(LogTemp, Display, TEXT("BossHitFeedback: armored flinch poise=%.1f"), BossPoise);
	}
}

// 记录受击方向和持续时间，启动方向受击动画
void UBeatSyncedComboComponent::StartBossHitReaction(const AActor* DamageSource, float Duration,
                                                     bool bInterruptAttack)
{
	if (!GetWorld() || !GetOwner() || BossHealth <= 0.0f)
		return;
	// 后跳期间不插入轻微受击动作，避免两种动作同时控制身体
	if (IsBackstepping() && !bInterruptAttack)
		return;
	FVector ReactionDirection = DamageSource
	                                ? DamageSource->GetActorLocation() - GetOwner()->GetActorLocation()
	                                : GetOwner()->GetActorForwardVector();
	ReactionDirection.Z = 0.0f;
	if (!ReactionDirection.Normalize())
		ReactionDirection = -GetOwner()->GetActorForwardVector();
	const FVector Local =
	    GetOwner()->GetActorTransform().InverseTransformVectorNoScale(ReactionDirection).GetSafeNormal2D();
	if (bInterruptAttack)
		CancelCombo();
	bBossHitReacting = true;
	BossHitReactionEndWorldSeconds = GetWorld()->GetTimeSeconds() + FMath::Max(0.12f, Duration);
	LocomotionResumeWorldSeconds = BossHitReactionEndWorldSeconds + 0.08;
	TArray<USkeletalMeshComponent*> Meshes;
	GetOwner()->GetComponents<USkeletalMeshComponent>(Meshes);
	int32 ReactingMeshCount = 0;
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (!IsValid(Mesh))
			continue;
		if (UBossLocomotionAnimInstance* Animation =
		        Cast<UBossLocomotionAnimInstance>(Mesh->GetAnimInstance()))
		{
			Animation->SetLocomotion(0.0f, FVector::ZeroVector, 0.0f);
			Animation->StartDirectionalHit(FVector2D(Local.X, Local.Y), Duration);
			++ReactingMeshCount;
		}
	}
	UE_LOG(LogTemp, Display,
	       TEXT("BossHitReaction: direction=(%.2f,%.2f) duration=%.2f interrupt=%d meshes=%d"), Local.X,
	       Local.Y, Duration, bInterruptAttack ? 1 : 0, ReactingMeshCount);
}

// 结束方向受击并恢复行走动画状态
void UBeatSyncedComboComponent::EndBossHitReaction()
{
	if (!bBossHitReacting)
		return;
	bBossHitReacting = false;
	BossHitReactionEndWorldSeconds = -1.0;
	TArray<USkeletalMeshComponent*> Meshes;
	GetOwner()->GetComponents<USkeletalMeshComponent>(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (!IsValid(Mesh))
			continue;
		if (UBossLocomotionAnimInstance* Animation =
		        Cast<UBossLocomotionAnimInstance>(Mesh->GetAnimInstance()))
			Animation->EndDirectionalHit();
	}
}

// 绑定伤害事件，检查身体和动画配置，准备战斗使用的组件
void UBeatSyncedComboComponent::BeginPlay()
{
	Super::BeginPlay();
	LoadedMusicEmphasis = MusicEmphasisData.LoadSynchronous();
	if (!GetOwner()->FindComponentByClass<UAttackReadabilityComponent>())
	{
		auto* Readability = NewObject<UAttackReadabilityComponent>(GetOwner());
		Readability->RegisterComponent();
	}
	// 修正已有蓝图和关卡实例中保留的旧默认值
	if (FMath::IsNearlyEqual(MaxBossHealth, 1000.f) || FMath::IsNearlyEqual(MaxBossHealth, 1200.f) ||
	    FMath::IsNearlyEqual(MaxBossHealth, 1500.f) || FMath::IsNearlyEqual(MaxBossHealth, 2000.f) ||
	    FMath::IsNearlyEqual(MaxBossHealth, 5000.f) || FMath::IsNearlyEqual(MaxBossHealth, 1800.f))
		MaxBossHealth = 2700.f; // 旧生命值为 1800
	BossHealth = FMath::Max(1.f, MaxBossHealth);
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("PhaseTwoVisualTest")))
	{
		BossPhase = 2;
		CompletedPhaseTwoMelee = 2;
		NextAreaAfterMelee = 2;
		UE_LOG(LogTemp, Display, TEXT("PhaseTwoVisualTest: test-only immediate phase 2 and area selection"));
	}
#endif
	// 提高韧性，避免轻击频繁打断连招
	if (FMath::IsNearlyEqual(MaxBossPoise, 160.f))
		MaxBossPoise = 320.f;
	if (FMath::IsNearlyEqual(BossPoiseRecoveryPerSecond, 48.f))
		BossPoiseRecoveryPerSecond = 64.f;
	BossPoise = FMath::Max(1.f, MaxBossPoise);
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("PhaseTransitionTest")))
	{
		FTimerHandle TestHandle;
		GetWorld()->GetTimerManager().SetTimer(
		    TestHandle,
		    FTimerDelegate::CreateWeakLambda(
		        this,
		        [this]()
		        {
			        BossPoise = MaxBossPoise + MaxBossHealth * PoiseDamagePerHealthDamage;
			        ApplyBossDamage(MaxBossHealth * .41f);
			        UE_LOG(LogTemp, Display,
					       TEXT("PhaseTransitionTest: crossed threshold while playing=%d queued=%d"),
					       bPlaying, bQueued);
		        }),
		    6.f, false);
	}
#endif
	GetOwner()->OnTakeAnyDamage.AddUniqueDynamic(this, &UBeatSyncedComboComponent::ReceiveBossDamage);
	// Boss 使用普通 Actor，移动碰撞由根胶囊体负责，这里统一修正旧实例的胶囊尺寸与碰撞设置


	if (UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(GetOwner()->GetRootComponent()))
	{
		Capsule->SetCapsuleSize(FMath::Max(Capsule->GetUnscaledCapsuleRadius(), 60.f),
		                        FMath::Max(Capsule->GetUnscaledCapsuleHalfHeight(), 110.f), true);
		Capsule->SetCollisionProfileName(TEXT("Pawn"));
		// 保留与角色和场景的阻挡，但忽略镜头检测，避免绕行时反复拉近镜头

		Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		// 修正旧实例的网格偏移，让脚底对齐胶囊底部


		TArray<USkeletalMeshComponent*> BossMeshes;
		GetOwner()->GetComponents<USkeletalMeshComponent>(BossMeshes);
		for (USkeletalMeshComponent* Mesh : BossMeshes)
		{
			if (!Mesh)
				continue;
			Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
			if (Mesh->GetAttachParent() != Capsule)
				continue;
			FVector Relative = Mesh->GetRelativeLocation();
			if (!FMath::IsNearlyEqual(Relative.Z, -90.f, 2.f) &&
			    !FMath::IsNearlyEqual(Relative.Z, -110.f, 2.f))
				continue;
			Relative.Z = -Capsule->GetUnscaledCapsuleHalfHeight();
			Mesh->SetRelativeLocation(Relative);
		}
		UE_LOG(LogTemp, Display,
		       TEXT("BossCollision: Root capsule radius=%.0f halfHeight=%.0f profile=%s pawnResponse=%d"),
		       Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight(),
		       *Capsule->GetCollisionProfileName().ToString(),
		       static_cast<int32>(Capsule->GetCollisionResponseToChannel(ECC_Pawn)));
	}
	else
	{
		UE_LOG(LogTemp, Error,
		       TEXT("BossCollision: %s needs a root capsule; swept movement cannot block the player"),
		       *GetNameSafe(GetOwner()));
	}
	// 武器和附件也忽略镜头检测，避免单独的碰撞体触发镜头回缩

	TArray<UPrimitiveComponent*> BossPrimitives;
	GetOwner()->GetComponents<UPrimitiveComponent>(BossPrimitives);
	for (UPrimitiveComponent* Primitive : BossPrimitives)
		if (IsValid(Primitive))
			Primitive->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	// 原生动画图统一混合行走、攻击槽和方向受击，受击仍可叠加在蒙太奇上

	UClass* AnimClass = UBossLocomotionAnimInstance::StaticClass();
	if (!AnimClass)
		return;
	const USkeleton* BossSkeleton = LoadObject<USkeleton>(
	    nullptr, TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/MESHES/UE5/SK_Mannequin.SK_Mannequin"));
	if (!BossSkeleton)
		return;
	TArray<USkeletalMeshComponent*> Meshes;
	GetOwner()->GetComponents<USkeletalMeshComponent>(Meshes);
	int32 Assigned = 0;
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (!IsValid(Mesh) || !IsValid(Mesh->GetSkeletalMeshAsset()) ||
		    Mesh->GetSkeletalMeshAsset()->GetSkeleton() != BossSkeleton ||
		    (!MeshComponentTag.IsNone() && !Mesh->ComponentHasTag(MeshComponentTag)))
			continue;
		Mesh->SetAnimInstanceClass(AnimClass);
		if (UAnimInstance* Instance = Mesh->GetAnimInstance())
			Instance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
		if (!LocomotionMesh.IsValid() || Mesh->GetFName() == TEXT("UE5_body"))
			LocomotionMesh = Mesh;
		++Assigned;
	}
	// 先更新网格姿态再执行连招检测，刀刃位置才能对应当前动画帧
	if (LocomotionMesh.IsValid())
		AddTickPrerequisiteComponent(LocomotionMesh.Get());
	static const TCHAR* WalkNames[] = {TEXT("move_walk_front"), TEXT("move_walk_frontR45"),
	                                   TEXT("move_walk_right"), TEXT("move_walk_backR45"),
	                                   TEXT("move_walk_back"),  TEXT("move_walk_backL45"),
	                                   TEXT("move_walk_left"),  TEXT("move_walk_frontL45")};
	DirectionalWalkAnimations.Reset();
	for (const TCHAR* Name : WalkNames)
	{
		const FString Path =
		    FString::Printf(TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/DEMO/UE5_ANIMATIONS/%s.%s"), Name, Name);
		UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, *Path);
		DirectionalWalkAnimations.Add(Sequence);
		if (Sequence)
		{
			const float Length = Sequence->GetPlayLength();
			const float RootDistance =
			    Sequence->ExtractRootMotionFromRange(0.0f, Length, FAnimExtractContext(Length, true))
			        .GetTranslation()
			        .Size2D();
			UE_LOG(LogTemp, Display, TEXT("BossLocomotion: Clip=%s RootDistance=%.1f Length=%.2f"), Name,
			       RootDistance, Length);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("BeatSyncedCombo: Assigned locomotion AnimBP to %d meshes on %s"), Assigned,
	       *GetNameSafe(GetOwner()));
	// 缓存已有火焰组件的原始缩放，后续按拍点调整，不每帧重新查找
	TArray<UNiagaraComponent*> NiagaraComponents;
	GetOwner()->GetComponents<UNiagaraComponent>(NiagaraComponents);
	for (UNiagaraComponent* Component : NiagaraComponents)
	{
		if (Component && Component->GetName().Contains(TEXT("Fire"), ESearchCase::IgnoreCase))
			FirePulseTargets.Add({Component, Component->GetRelativeScale3D()});
	}
	UE_LOG(LogTemp, Display, TEXT("BeatFire: %d persistent fire components follow the music beat"),
	       FirePulseTargets.Num());
	// 分别收集武器发光和盔甲表现，动态材质只影响当前 Boss
	TArray<UMeshComponent*> VisualMeshes;
	GetOwner()->GetComponents<UMeshComponent>(VisualMeshes);
	USceneComponent* WeaponAnchor = nullptr;
	for (UMeshComponent* VisualMesh : VisualMeshes)
	{
		if (!IsValid(VisualMesh))
			continue;
		bool bArmorMesh = false;
		for (int32 Index = 0; Index < VisualMesh->GetNumMaterials(); ++Index)
		{
			UMaterialInterface* Material = VisualMesh->GetMaterial(Index);
			if (!IsValid(Material))
				continue;
			const bool bSword = Material->GetName().Contains(TEXT("Sword_Fire"), ESearchCase::IgnoreCase);
			const bool bBodyFire =
			    !bSword && Material->GetName().Contains(TEXT("_Fire"), ESearchCase::IgnoreCase);
			if (!bSword && !bBodyFire)
				continue;
			bArmorMesh |= bBodyFire;
			float BaseIntensity = 20.f;
			if (!bSword || !Material->GetScalarParameterValue(
			                   FMaterialParameterInfo(TEXT("Emissive Intensity")), BaseIntensity))
				continue;
			if (UMaterialInstanceDynamic* DynamicMaterial = VisualMesh->CreateDynamicMaterialInstance(Index))
			{
				SwordGlowTargets.Add({DynamicMaterial, BaseIntensity});
				if (!WeaponAnchor)
					WeaponAnchor = VisualMesh;
			}
		}
		if (bArmorMesh)
			BeatOverlayMeshes.Add(VisualMesh);
	}
	if (!BeatOverlayMaterial)
		BeatOverlayMaterial = LoadObject<UMaterialInterface>(
		    nullptr, TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/MATERIALS/M_BossBeatOverlay.M_BossBeatOverlay"));
	if (BeatOverlayMaterial)
	{
		BeatOverlayInstance = UMaterialInstanceDynamic::Create(BeatOverlayMaterial, GetOwner());
		BeatOverlayInstance->SetScalarParameterValue(TEXT("StrongPulse"), 0.f);
		BeatOverlayInstance->SetScalarParameterValue(TEXT("WeakPulse"), 0.f);
		for (const TWeakObjectPtr<UMeshComponent>& Mesh : BeatOverlayMeshes)
			if (UMeshComponent* Component = Mesh.Get())
				Component->SetOverlayMaterial(BeatOverlayInstance);
	}
	// 优先把灯光挂到武器，找不到时依次使用右手挂点和根组件
	FName WeaponSocket = NAME_None;
	if (!WeaponAnchor)
	{
		WeaponAnchor = LocomotionMesh.Get();
		if (USkeletalMeshComponent* BodyMesh = LocomotionMesh.Get();
		    BodyMesh && BodyMesh->DoesSocketExist(TEXT("hand_r")))
			WeaponSocket = TEXT("hand_r");
	}
	if (!WeaponAnchor)
		WeaponAnchor = GetOwner()->GetRootComponent();
	if (WeaponAnchor)
	{
		UPointLightComponent* Light =
		    NewObject<UPointLightComponent>(GetOwner(), TEXT("BossStrongBeatSwordLight"));
		GetOwner()->AddInstanceComponent(Light);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetupAttachment(WeaponAnchor, WeaponSocket);
		Light->SetRelativeLocation(WeaponAnchor == GetOwner()->GetRootComponent() ? FVector(0.f, 0.f, 150.f)
		                                                                          : FVector(25.f, 0.f, 0.f));
		Light->SetLightColor(FLinearColor(1.f, .18f, .025f));
		Light->SetAttenuationRadius(270.f);
		Light->SetCastShadows(false);
		Light->SetIntensity(0.f);
		Light->RegisterComponent();
		SwordChargeLight = Light;
	}
	USoundBase* UserSwordSlice = LoadObject<USoundBase>(
	    nullptr,
	    TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/SOUND/SW_DragonKnight_SwordSlice.SW_DragonKnight_SwordSlice"));
	const bool bLegacyStrong = StrongBeatSound && StrongBeatSound->GetName().StartsWith(TEXT("SW_BossBlade"));
	const bool bLegacyWeak = WeakBladeSound && WeakBladeSound->GetName().StartsWith(TEXT("SW_BossBlade"));
	if (UserSwordSlice && (!StrongBeatSound || bLegacyStrong))
		StrongBeatSound = UserSwordSlice;
	if (UserSwordSlice && (!WeakBladeSound || bLegacyWeak))
		WeakBladeSound = UserSwordSlice;
	UE_LOG(
	    LogTemp, Display,
	    TEXT(
	        "BossStrongBeat: swordMaterials=%d overlayMeshes=%d overlay=%s anchor=%s socket=%s strongSound=%s weakSound=%s"),
	    SwordGlowTargets.Num(), BeatOverlayMeshes.Num(), *GetNameSafe(BeatOverlayMaterial),
	    *GetNameSafe(WeaponAnchor), *WeaponSocket.ToString(), *GetNameSafe(StrongBeatSound),
	    *GetNameSafe(WeakBladeSound));
}

// 停止连招与演出，清理临时效果和事件绑定
void UBeatSyncedComboComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAudioComponent* Voice = ActiveBladeVoice.Get())
		Voice->Stop();
	ActiveBladeVoice.Reset();
	EndUppercutFeedback();
	EndPlayerResonance();
	ClearGroundBurst();
	EndCinematicAccent(true);
	Super::EndPlay(EndPlayReason);
}

// 读取行走动画时长，供步伐与音乐拍点对齐
float UBeatSyncedComboComponent::GetWalkClipLengthSeconds() const
{
	return DirectionalWalkAnimations.IsValidIndex(0) && DirectionalWalkAnimations[0]
	           ? DirectionalWalkAnimations[0]->GetPlayLength()
			   : 0.f;
}

// 根据音乐拍点调整角色火焰强度
void UBeatSyncedComboComponent::UpdateBeatFirePulse(const UBeatClockSubsystem* Clock)
{
	float Scale = 1.f;
	if (Clock && Clock->IsRunning() && FireBeatPulseStrength > 0.f)
	{
		const double Beat = Clock->GetCurrentBeat();
		// 用当前拍的小数部分计算火焰起伏，小节首拍额外增强
		const float Phase = static_cast<float>(FMath::Frac(Beat));
		const float Accent = FMath::Pow(FMath::Max(0.f, FMath::Cos(2.f * UE_PI * Phase)), 4.f);
		const int32 NearestBeat = FMath::RoundToInt(Beat);
		const bool bBarStart = Clock->GetBeatsPerBar() > 0 && NearestBeat % Clock->GetBeatsPerBar() == 0;
		Scale += FireBeatPulseStrength * Accent * (bBarStart ? 1.35f : 1.f);
	}
	if (Clock && Clock->IsRunning() && bStrongBeatThisAttack && (bQueued || bPlaying))
	{
		const double UntilImpact = ScheduledFirstImpactAudioSeconds - Clock->GetAudioNowSeconds();
		const double ChargeSeconds = FMath::Max(.01, StrongBeatChargeBeats * Clock->GetSecondsPerBeat());
		if (UntilImpact > 0.0 && UntilImpact <= ChargeSeconds)
		{
			const float Alpha = FMath::Clamp(static_cast<float>(1.0 - UntilImpact / ChargeSeconds), 0.f, 1.f);
			const float Ease = Alpha * Alpha * (3.f - 2.f * Alpha);
			Scale *= FMath::Lerp(1.f, .32f, Ease);
		}
		else if (bBlinkExecuted && UntilImpact <= 0.0 && UntilImpact > -.3)
		{
			Scale *= 1.f + .95f * FMath::Exp(static_cast<float>(UntilImpact / .11));
		}
	}
	FAttackRecognitionInfo ReadInfo;
	FVector ReadPosition;
	float ReadProgress = 0.f;
	double ReadKey = 0.;
	const bool bReadingAttack = GetRecognitionCue(ReadInfo, ReadPosition, ReadProgress, ReadKey);
	for (const FFirePulseTarget& Target : FirePulseTargets)
		if (UNiagaraComponent* Component = Target.Component.Get())
		{
			const bool bWeapon = Component->GetName().Contains(TEXT("Sword"), ESearchCase::IgnoreCase) ||
			                     Component->GetName().Contains(TEXT("Weapon"), ESearchCase::IgnoreCase);
			// 出现攻击预警时降低武器火焰，避免盖住应对提示
			const float ReadabilityScale = bReadingAttack ? (bWeapon          ? .38f
			                                                 : BossPhase >= 2 ? .65f
			                                                                  : 1.f)
			                                              : 1.f;
			Component->SetRelativeScale3D(Target.BaseScale * Scale * ReadabilityScale);
		}
}

// 更新强拍攻击的蓄力火焰与身体表现
void UBeatSyncedComboComponent::UpdateStrongBeatPresentation(const UBeatClockSubsystem* Clock)
{
	float Glow = 1.f;
	float LightIntensity = 0.f;
	if (Clock && Clock->IsRunning() && bStrongBeatThisAttack && (bQueued || bPlaying))
	{
		const double UntilImpact = ScheduledFirstImpactAudioSeconds - Clock->GetAudioNowSeconds();
		const double ChargeSeconds = FMath::Max(.01, StrongBeatChargeBeats * Clock->GetSecondsPerBeat());
		if (UntilImpact > 0.0 && UntilImpact <= ChargeSeconds)
		{
			const float Alpha = FMath::Clamp(static_cast<float>(1.0 - UntilImpact / ChargeSeconds), 0.f, 1.f);
			const float Ease = Alpha * Alpha * (3.f - 2.f * Alpha);
			Glow = FMath::Lerp(1.f, StrongBeatSwordGlowMultiplier, Ease);
			LightIntensity = 3800.f * Ease;
			// 蓄力开始只生成一次预警特效，后续帧只更新大小和亮度
			if (!bStrongBeatChargeStarted)
			{
				bStrongBeatChargeStarted = true;
				if (BlinkSettings.WarningEffect)
				{
					const UCapsuleComponent* Capsule =
					    Cast<UCapsuleComponent>(GetOwner()->GetRootComponent());
					const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 110.f;
					OriginWarningFX =
					    SpawnBriefFire(GetWorld(), BlinkSettings.WarningEffect,
						               GetOwner()->GetActorLocation() - FVector(0, 0, HalfHeight - 2.f),
						               static_cast<float>(ChargeSeconds + .1), 1.4f);
				}
				UE_LOG(LogTemp, Display, TEXT("BossStrongBeat: charge beat=%d audio=%.3f target=%.3f"),
				       FirstImpactBeat, Clock->GetAudioNowSeconds(), ScheduledFirstImpactAudioSeconds);
			}
			if (UNiagaraComponent* Effect = OriginWarningFX.Get())
				Effect->SetWorldScale3D(FVector(FMath::Lerp(1.4f, .35f, Ease)));
		}
		else if (bBlinkExecuted && UntilImpact <= 0.0 && UntilImpact > -.3)
		{
			const float Flash = FMath::Exp(static_cast<float>(UntilImpact / .11));
			Glow = 1.f + 4.f * Flash;
			LightIntensity = 7600.f * Flash;
		}
		if (bBlinkWarningShown && !bBlinkExecuted && BlinkSettings.WarningBeats > 0.f)
		{
			const float WarningProgress =
			    FMath::Clamp(static_cast<float>(1.0 - UntilImpact / (BlinkSettings.WarningBeats *
				                                                     Clock->GetSecondsPerBeat())),
				             0.f, 1.f);
			const float WarningScale = FMath::Lerp(1.3f, .42f, WarningProgress);
			if (UNiagaraComponent* Effect = LandingWarningFX.Get())
				Effect->SetWorldScale3D(FVector(WarningScale));
		}
	}
	FAttackRecognitionInfo ReadInfo;
	FVector ReadPosition;
	float ReadProgress = 0.f;
	double ReadKey = 0.;
	// 刀刃预警显示期间压低装饰发光，保持攻击轮廓清楚
	const bool bReadingBlade = GetRecognitionCue(ReadInfo, ReadPosition, ReadProgress, ReadKey) &&
	                           ReadInfo.Shape != EAttackRecognitionShape::GroundWarning;
	for (const FSwordGlowTarget& Target : SwordGlowTargets)
		if (UMaterialInstanceDynamic* Material = Target.Material.Get())
			Material->SetScalarParameterValue(TEXT("Emissive Intensity"),
			                                  Target.BaseIntensity * Glow * (bReadingBlade ? .32f : 1.f));
	if (UPointLightComponent* Light = SwordChargeLight.Get())
		Light->SetIntensity(LightIntensity);
}

// 记录一次材质闪光的强度和时间
void UBeatSyncedComboComponent::TriggerBeatOverlay(bool bStrong, double AudioNow)
{
	if (!BeatOverlayInstance)
		return;
	if (bStrong)
	{
		LastStrongOverlayAudioSeconds = AudioNow;
		BeatOverlayInstance->SetScalarParameterValue(TEXT("StrongPulse"), 1.f);
	}
	else
	{
		LastWeakOverlayAudioSeconds = AudioNow;
		BeatOverlayInstance->SetScalarParameterValue(TEXT("WeakPulse"), 1.f);
	}
}

// 更新并衰减护甲材质的拍点与受击效果
void UBeatSyncedComboComponent::UpdateBeatOverlay(const UBeatClockSubsystem* Clock)
{
	if (!BeatOverlayInstance || !Clock || !Clock->IsRunning())
		return;
	const double Now = Clock->GetAudioNowSeconds();
	auto Envelope = [Now](double Last, float Duration) -> float
	{
		const double Age = Now - Last;
		if (Age < 0.0 || Age >= Duration)
			return 0.f;
		const float Alpha = static_cast<float>(Age / Duration);
		return FMath::Square(1.f - Alpha);
	};
	BeatOverlayInstance->SetScalarParameterValue(
	    TEXT("StrongPulse"), Envelope(LastStrongOverlayAudioSeconds, FMath::Max(.1f, StrongOverlaySeconds)));
	BeatOverlayInstance->SetScalarParameterValue(
	    TEXT("WeakPulse"),
	    FMath::Max(Envelope(LastWeakOverlayAudioSeconds, FMath::Max(.05f, WeakOverlaySeconds)),
		           HitOverlayStrength * Envelope(LastHitOverlayAudioSeconds, .1f)));
}

// 恢复强拍演出修改的身体与特效状态
void UBeatSyncedComboComponent::ResetStrongBeatPresentation()
{
	const bool bWasCharging = bStrongBeatChargeStarted;
	for (const FSwordGlowTarget& Target : SwordGlowTargets)
		if (UMaterialInstanceDynamic* Material = Target.Material.Get())
			Material->SetScalarParameterValue(TEXT("Emissive Intensity"), Target.BaseIntensity);
	if (UPointLightComponent* Light = SwordChargeLight.Get())
		Light->SetIntensity(0.f);
	if (UNiagaraComponent* Effect = OriginWarningFX.Get())
		Effect->Deactivate();
	if (UNiagaraComponent* Effect = LandingWarningFX.Get())
		Effect->Deactivate();
	OriginWarningFX.Reset();
	LandingWarningFX.Reset();
	bStrongBeatChargeStarted = false;
	bStrongBeatThisAttack = false;
	if (bWasCharging)
		UE_LOG(LogTemp, Display, TEXT("BossStrongBeat: restored sword glow, light and warning fire"));
}

// 接收攻击资源，确定首击拍点并建立本次连招的运行数据
bool UBeatSyncedComboComponent::QueueAttack(UAttackDataAsset* InAttackData)
{
	if (bPlayerResonance && InAttackData != ResonanceCounterAttack)
		return false;
	if (bQueued || bPlaying || bBossHitReacting || !IsValid(InAttackData))
		return false;
	if (IsBackstepping())
		return false;
	AttackData = InAttackData;
	bPracticeNote = false;
	return QueueCombo();
}

// 只在本次攻击开始时选择防御指令组合，后续提示与判定共用结果
void UBeatSyncedComboComponent::SelectRuntimeResponsePattern()
{
	RuntimeImpactMarkers.Reset();
	ActiveResponsePattern = NAME_None;
	if (!IsValid(AttackData))
		return;
	// 先复制原标记，只修改本次攻击的副本，不污染多次攻击共用的资源
	RuntimeImpactMarkers = AttackData->ImpactMarkers;

	// 只有权重为正且替换区间没有越界的指令组合参与抽选
	float TotalWeight = 0.0f;
	for (const FAttackResponsePattern& Pattern : AttackData->ResponsePatterns)
	{
		if (Pattern.Weight > 0.0f && !Pattern.Instructions.IsEmpty() && Pattern.FirstImpactIndex >= 0 &&
		    Pattern.FirstImpactIndex + Pattern.Instructions.Num() <= RuntimeImpactMarkers.Num())
		{
			TotalWeight += Pattern.Weight;
		}
	}
	if (TotalWeight <= 0.0f)
		return;

	float Choice = FMath::FRandRange(0.0f, TotalWeight);
	const FAttackResponsePattern* Selected = nullptr;
	for (const FAttackResponsePattern& Pattern : AttackData->ResponsePatterns)
	{
		if (Pattern.Weight <= 0.0f || Pattern.Instructions.IsEmpty() || Pattern.FirstImpactIndex < 0 ||
		    Pattern.FirstImpactIndex + Pattern.Instructions.Num() > RuntimeImpactMarkers.Num())
			continue;
		Choice -= Pattern.Weight;
		if (Choice <= 0.0f)
		{
			Selected = &Pattern;
			break;
		}
	}
	if (!Selected)
		return;

	// 选中后只替换应对方式和长按分组，命中时间与伤害保持不变
	for (int32 InstructionIndex = 0; InstructionIndex < Selected->Instructions.Num(); ++InstructionIndex)
	{
		FAttackImpactMarker& Marker = RuntimeImpactMarkers[Selected->FirstImpactIndex + InstructionIndex];
		const FAttackResponseInstruction& Instruction = Selected->Instructions[InstructionIndex];
		Marker.RequiredResponse = Instruction.RequiredResponse;
		Marker.HoldMarker = Instruction.RequiredResponse == ERhythmResponseType::HoldBlock
		                        ? Instruction.HoldMarker
		                        : ERhythmHoldMarkerType::None;
		Marker.HoldGroup = Instruction.RequiredResponse == ERhythmResponseType::HoldBlock
		                       ? Instruction.HoldGroup
		                       : NAME_None;
	}
	ActiveResponsePattern = Selected->PatternName;
	UE_LOG(LogTemp, Display, TEXT("BeatSyncedCombo: ResponsePattern Attack=%s Pattern=%s"),
	       *GetNameSafe(AttackData), *ActiveResponsePattern.ToString());
}

// 确定本次连招哪些命中使用慢放表现
void UBeatSyncedComboComponent::SelectRuntimeCinematicAccents()
{
	// 先恢复音乐和角色速度，再清除慢放状态
	EndCinematicAccent(true);
	RuntimeCinematicEnabled.Init(false, RuntimeImpactMarkers.Num());
	if (bPlayerResonance)
		return;
	bool bHasAuthoredAccent = false;
	for (int32 Index = 0; Index < RuntimeImpactMarkers.Num(); ++Index)
	{
		const FImpactCinematicSettings& Settings = RuntimeImpactMarkers[Index].Cinematic;
		if (!Settings.bEnabled)
			continue;
		bHasAuthoredAccent = true;
		RuntimeCinematicEnabled[Index] = FMath::FRand() <= Settings.TriggerChance;
	}
	if (!bHasAuthoredAccent && bEnableOccasionalFinalImpactCinematic && !RuntimeImpactMarkers.IsEmpty() &&
	    FMath::FRand() <= FinalImpactCinematicChance)
	{
		RuntimeImpactMarkers.Last().Cinematic = FinalImpactCinematic;
		RuntimeCinematicEnabled.Last() = true;
	}
	ActiveCinematicImpactIndex = INDEX_NONE;
	bCinematicAccentActive = false;
}

// 到达配置时刻后启动慢放，并在结束条件满足时恢复
void UBeatSyncedComboComponent::UpdateCinematicAccent(UBeatClockSubsystem* Clock, double AudioNow)
{
	if (!Clock || !Clock->IsRunning())
		return;
	if (bCinematicAccentActive && GetWorld() &&
	    GetWorld()->GetTimeSeconds() >= CinematicActorSlowEndWorldSeconds)
	{
		if (AActor* Boss = GetOwner())
			Boss->CustomTimeDilation = SavedBossCustomTimeDilation;
		if (AActor* Player = CinematicPlayerActor.Get())
			Player->CustomTimeDilation = SavedPlayerCustomTimeDilation;
		CinematicPlayerActor.Reset();
		CinematicActorSlowEndWorldSeconds = -1000.0;
	}
	// 用未随音乐变速的时间结束慢放，避免恢复流程被慢放自身拖延

	if (bCinematicAccentActive && GetWorld() &&
	    GetWorld()->GetTimeSeconds() >= CinematicAccentEndWorldSeconds)
		EndCinematicAccent(false);
}

// 记录实际交锋位置并通知镜头表现
void UBeatSyncedComboComponent::TriggerCinematicClash(int32 ImpactIndex, const FVector& ClashLocation,
                                                      bool bStrongBeat)
{
	if (bPhaseChangePending || bPhaseTransitionActive)
		return;
	if (ImpactIndex != GetImpactCount() - 1 || !RuntimeCinematicEnabled.IsValidIndex(ImpactIndex) ||
	    !RuntimeCinematicEnabled[ImpactIndex] || !RuntimeImpactMarkers.IsValidIndex(ImpactIndex))
		return;
	UBeatClockSubsystem* Clock = GetWorld() ? GetWorld()->GetSubsystem<UBeatClockSubsystem>() : nullptr;
	if (!Clock || !Clock->IsRunning())
		return;
	const FImpactCinematicSettings& Settings = RuntimeImpactMarkers[ImpactIndex].Cinematic;
	RuntimeCinematicEnabled[ImpactIndex] = false;
	ActiveCinematicImpactIndex = ImpactIndex;
	bCinematicAccentActive = true;
	const float MusicHoldSeconds =
	    FMath::Max(.04f, Settings.HoldAfterImpactBeats * static_cast<float>(Clock->GetSecondsPerBeat()));
	CinematicAccentEndWorldSeconds = GetWorld()->GetTimeSeconds() + MusicHoldSeconds;
	Clock->SetTemporaryMusicalPlaybackRate(Settings.MusicAndBossRate, .025f,
	                                       MusicHoldSeconds + FMath::Max(.05f, Settings.RecoverySeconds) +
	                                           .15f);
	if (AActor* Boss = GetOwner())
	{
		SavedBossCustomTimeDilation = Boss->CustomTimeDilation;
		Boss->CustomTimeDilation = Settings.ClashActorRate;
	}
	if (AActor* Player = AttackTarget.Get())
	{
		CinematicPlayerActor = Player;
		SavedPlayerCustomTimeDilation = Player->CustomTimeDilation;
		Player->CustomTimeDilation = Settings.ClashActorRate;
	}
	CinematicActorSlowEndWorldSeconds = GetWorld()->GetTimeSeconds() + Settings.ClashActorSlowSeconds;
	OnCinematicAccentImpact.Broadcast(Settings.CameraStrength, bStrongBeat, ClashLocation);
	UE_LOG(LogTemp, Display, TEXT("BeatCinematicClash: impact=%d rate=%.2f holdBeats=%.2f location=%s"),
	       ImpactIndex + 1, Settings.MusicAndBossRate, Settings.HoldAfterImpactBeats,
	       *ClashLocation.ToString());
}

// 恢复音乐和角色速度，再结束本次慢放
void UBeatSyncedComboComponent::EndCinematicAccent(bool bImmediate)
{
	if (!bCinematicAccentActive)
		return;
	float Recovery = .28f;
	if (RuntimeImpactMarkers.IsValidIndex(ActiveCinematicImpactIndex))
		Recovery = RuntimeImpactMarkers[ActiveCinematicImpactIndex].Cinematic.RecoverySeconds;
	if (UBeatClockSubsystem* Clock = GetWorld() ? GetWorld()->GetSubsystem<UBeatClockSubsystem>() : nullptr)
		Clock->RestoreMusicalPlaybackRate(bImmediate ? .015f : Recovery);
	if (AActor* Boss = GetOwner())
		Boss->CustomTimeDilation = SavedBossCustomTimeDilation;
	if (AActor* Player = CinematicPlayerActor.Get())
		Player->CustomTimeDilation = SavedPlayerCustomTimeDilation;
	CinematicPlayerActor.Reset();
	CinematicActorSlowEndWorldSeconds = -1000.0;
	CinematicAccentEndWorldSeconds = -1000.0;
	UE_LOG(LogTemp, Display, TEXT("BeatCinematic: END impact=%d recovery=%.3f"),
	       ActiveCinematicImpactIndex + 1, bImmediate ? .015f : Recovery);
	bCinematicAccentActive = false;
	ActiveCinematicImpactIndex = INDEX_NONE;
	OnCinematicAccentEnded.Broadcast();
}

const FAttackImpactMarker& UBeatSyncedComboComponent::GetImpactMarker(int32 Index) const
{
	if (RuntimeImpactMarkers.IsValidIndex(Index))
		return RuntimeImpactMarkers[Index];
	check(AttackData && AttackData->ImpactMarkers.IsValidIndex(Index));
	return AttackData->ImpactMarkers[Index];
}

// 保存下一次攻击的瞬移配置，等待 QueueAttack 安排时间
void UBeatSyncedComboComponent::ConfigureNextBlink(const FBeatBlinkSettings& Settings)
{
	BlinkSettings = Settings;
	bBlinkRequested = true;
	bBlinkWarningShown = false;
	bBlinkExecuted = false;
}

// 清除本次瞬移的目标、预警和执行状态
void UBeatSyncedComboComponent::ClearBlink()
{
	if (bBlinkWarningShown && !bBlinkExecuted)
		OnBlinkCancelled.Broadcast();
	ResetStrongBeatPresentation();
	bBlinkRequested = false;
	bBlinkWarningShown = false;
	bBlinkExecuted = false;
}

// 确定瞬移落点，后续按已锁定的位置执行
bool UBeatSyncedComboComponent::LockBlinkDestination()
{
	AActor* Boss = GetOwner();
	AActor* Target = AttackTarget.Get();
	if (!IsValid(Boss) || !IsValid(Target))
		return false;
	const FVector TargetForward = Target->GetActorForwardVector().GetSafeNormal2D();
	const float Side = bNextBlinkOnRight ? 1.0f : -1.0f;
	const FVector Direction =
	    TargetForward.RotateAngleAxis(Side * BlinkSettings.SideAngleDegrees, FVector::UpVector);
	const FVector Desired = Target->GetActorLocation() + Direction * BlinkSettings.LandingDistance;
	FHitResult Ground;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BeatBlinkGround), false, Boss);
	Query.AddIgnoredActor(Target);
	if (!GetWorld()->LineTraceSingleByChannel(Ground, Desired + FVector(0, 0, 250),
	                                          Desired - FVector(0, 0, 600), ECC_Visibility, Query))
		return false;
	const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Boss->GetRootComponent());
	if (!Capsule)
		return false;
	FVector Candidate = Ground.ImpactPoint + FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight() + 3.0f);
	const FRotator Facing(0, (Target->GetActorLocation() - Candidate).Rotation().Yaw, 0);
	if (!GetWorld()->FindTeleportSpot(Boss, Candidate, Facing) ||
	    FVector::Dist2D(Candidate, Desired) > 75.0f ||
	    FVector::Dist2D(Candidate, Target->GetActorLocation()) < 150.0f)
		return false;
	LockedBlinkLocation = Candidate;
	LockedBlinkGroundZ = Ground.ImpactPoint.Z;
	LockedBlinkRotation = Facing;
	bNextBlinkOnRight = !bNextBlinkOnRight;
	return true;
}

// 记录攻击目标，供追踪和动作对齐使用
void UBeatSyncedComboComponent::SetAttackTarget(AActor* InTarget)
{
	AttackTarget = InTarget;
}

// 移除攻击使用的动作对齐目标
void UBeatSyncedComboComponent::ClearWarping()
{
	if (IsValid(WarpComponent))
	{
		WarpComponent->RemoveAllWarpTargets();
		WarpComponent->DestroyComponent();
		WarpComponent = nullptr;
	}
	ActiveWarpWindowIndex = INDEX_NONE;
	bStrikeCommitted = false;
}

// 更新本次攻击要移动到的位置和朝向
void UBeatSyncedComboComponent::UpdateWarpTarget(float AnimationPosition)
{
	if (!IsValid(WarpComponent) || !AttackTarget.IsValid() || !IsValid(AttackData))
		return;
	for (int32 Index = 0; Index < AttackData->ImpactMarkers.Num(); ++Index)
	{
		const float Impact = AttackData->ImpactMarkers[Index].AnimationTimeSeconds;
		const float PreviousImpact = Index > 0 ? AttackData->ImpactMarkers[Index - 1].AnimationTimeSeconds
		                                       : AttackData->ClipStartSeconds;
		// 上一击结束后，才允许为下一击调整位置
		const float WindowStart = FMath::Max(PreviousImpact + (Index > 0 ? 0.05f : 0.0f),
		                                     Impact - AttackData->WarpLeadAnimationSeconds);
		if (AnimationPosition < WindowStart || AnimationPosition > Impact)
			continue;
		if (ActiveWarpWindowIndex != Index)
		{
			ActiveWarpWindowIndex = Index;
			// 记住这一击的起点，后续移动距离始终从这里计算
			WarpAnchorLocation = GetOwner()->GetActorLocation();
			bStrikeCommitted = false;
			UE_LOG(LogTemp, Display,
			       TEXT("BeatSyncedCombo: MotionWarping window=%d start=%.3f end=%.3f playerDistance=%.0f"),
			       Index + 1, WindowStart, Impact,
			       FVector::Dist2D(WarpAnchorLocation, AttackTarget->GetActorLocation()));
		}
		// 出手方向锁定后沿用已保存目标，玩家后续移动不再改变本击落点
		if (bStrikeCommitted)
		{
			WarpComponent->AddOrUpdateWarpTargetFromTransform(TEXT("Player"), CommittedWarpTarget);
			return;
		}
		FVector TargetLocation = AttackTarget->GetActorLocation();
		FVector ToPlayer = TargetLocation - WarpAnchorLocation;
		ToPlayer.Z = 0.0f;
		if (ToPlayer.IsNearlyZero())
			return;
		const FVector Direction = ToPlayer.GetSafeNormal();
		// 保留目标前方的停止距离，避免将敌人移动到玩家中心
		TargetLocation -= Direction * AttackData->WarpStopDistance;
		FVector Travel = TargetLocation - WarpAnchorLocation;
		Travel.Z = 0.0f;
		Travel = Travel.GetClampedToMaxSize(AttackData->MaxWarpTravelPerStrike);
		TargetLocation = WarpAnchorLocation + Travel;
		const FRotator Facing(0.0f, (AttackTarget->GetActorLocation() - TargetLocation).Rotation().Yaw, 0.0f);
		const FTransform WarpTarget(Facing, TargetLocation);
		// 将动画位置换算为播放经过时间后求差，得到距离命中的剩余秒数
		const float Remaining = AttackData->GetElapsedAtAnimationTime(Impact) -
		                        AttackData->GetElapsedAtAnimationTime(AnimationPosition);
		if (AttackData->bCommitStrikeDirection && Remaining <= AttackData->StrikeCommitLeadSeconds)
		{
			bStrikeCommitted = true;
			CommittedWarpTarget = WarpTarget;
			CommittedPlayerLocation = AttackTarget->GetActorLocation();
			CommittedStrikeDirection =
			    (CommittedPlayerLocation - GetOwner()->GetActorLocation()).GetSafeNormal2D();
			UE_LOG(LogTemp, Display, TEXT("BeatSyncedCombo: strike committed impact=%d remaining=%.3f"),
			       Index + 1, Remaining);
		}
		WarpComponent->AddOrUpdateWarpTargetFromTransform(TEXT("Player"), WarpTarget);
		return;
	}
}

int32 UBeatSyncedComboComponent::GetImpactCount() const
{
	if (!RuntimeImpactMarkers.IsEmpty())
		return RuntimeImpactMarkers.Num();
	return AttackData && !AttackData->ImpactMarkers.IsEmpty() ? AttackData->ImpactMarkers.Num() : 1;
}

float UBeatSyncedComboComponent::GetImpactTime(int32 Index) const
{
	return AttackData->ImpactMarkers.IsEmpty() ? AttackData->AuthoredImpactTime
	                                           : GetImpactMarker(Index).AnimationTimeSeconds;
}

// 提供下一击的命中时间与理想闪避提前量
bool UBeatSyncedComboComponent::GetNextImpactDodgeTiming(double& OutImpactAudioSeconds,
                                                         float& OutIdealLeadSeconds) const
{
	if ((!bQueued && !bPlaying) || !IsValid(AttackData) || NextImpactIndex >= GetImpactCount())
		return false;
	const double ClipStartElapsed = AttackData->GetElapsedAtAnimationTime(AttackData->ClipStartSeconds);
	const double MarkerElapsed =
	    AttackData->GetElapsedAtAnimationTime(GetImpactTime(NextImpactIndex)) - ClipStartElapsed;
	OutImpactAudioSeconds = ScheduledStartAudioSeconds + MarkerElapsed;
	OutIdealLeadSeconds = AttackData->ImpactMarkers.IsValidIndex(NextImpactIndex)
	                          ? GetImpactMarker(NextImpactIndex).IdealDodgeLeadSeconds
	                          : 0.14f;
	return true;
}

// 提供提示：向外提供后续命中时刻与应对信息，界面只使用这些信息画轨道，不反向决定命中
void UBeatSyncedComboComponent::GetUpcomingImpactCues(TArray<FUpcomingImpactCue>& OutCues, int32 MaxCount,
                                                      bool bIncludeRecent) const
{
	OutCues.Reset();
	if ((!bQueued && !bPlaying) || !IsValid(AttackData) || MaxCount <= 0)
		return;
	const UBeatClockSubsystem* Clock = GetWorld() ? GetWorld()->GetSubsystem<UBeatClockSubsystem>() : nullptr;
	const int32 BeatsPerBar = Clock ? FMath::Max(1, Clock->GetBeatsPerBar()) : 4;
	const double ClipStartElapsed = AttackData->GetElapsedAtAnimationTime(AttackData->ClipStartSeconds);
	for (int32 Index = bIncludeRecent ? FMath::Max(0, NextImpactIndex - 1) : NextImpactIndex;
	     Index < GetImpactCount() && OutCues.Num() < MaxCount; ++Index)
	{
		FUpcomingImpactCue& Cue = OutCues.AddDefaulted_GetRef();
		// 从片段起点累加分段播放时间，再加本次起手时间，得到真实命中时刻
		const double MarkerElapsed =
		    AttackData->GetElapsedAtAnimationTime(GetImpactTime(Index)) - ClipStartElapsed;
		Cue.ImpactAudioSeconds = ScheduledStartAudioSeconds + MarkerElapsed;
		Cue.IdealDodgeLeadSeconds = AttackData->ImpactMarkers.IsValidIndex(Index)
		                                ? GetImpactMarker(Index).IdealDodgeLeadSeconds
		                                : 0.14f;
		Cue.ImpactIndex = Index;
		if (AttackData->ImpactMarkers.IsValidIndex(Index))
		{
			const FAttackImpactMarker& Marker = GetImpactMarker(Index);
			Cue.PromptLeadBeats = Marker.PromptLeadBeats;
			Cue.DamageShape = Marker.DamageShape;
			if (Marker.DamageShape == EBossAttackDamageShape::GroundBurst)
				Cue.IdealDodgeLeadSeconds = 0.f;
			// 读取本次攻击选定的指令副本，让提示与伤害结算保持一致
			Cue.RequiredResponse = Marker.RequiredResponse;
			Cue.HoldMarker = Marker.HoldMarker;
			if (Marker.RequiredResponse == ERhythmResponseType::HoldBlock &&
			    Marker.HoldMarker == ERhythmHoldMarkerType::End)
				Cue.IdealDodgeLeadSeconds = 0.f;
			Cue.HoldGroup = Marker.HoldGroup;
			Cue.IconOverride = Marker.PromptIconOverride;
			Cue.IconScale = Marker.PromptIconScale;
			Cue.IconColor = Marker.PromptIconColor;
		}
		const double BeatOffset =
		    AttackData->ImpactMarkers.IsValidIndex(Index) ? GetImpactMarker(Index).BeatOffset : 0.0;
		const double TargetBeat = FirstImpactBeat + BeatOffset;
		const int32 RoundedBeat = FMath::RoundToInt(TargetBeat);
		Cue.bStrongBeat =
		    (AttackData->ImpactMarkers.IsValidIndex(Index) && GetImpactMarker(Index).bForceStrongAccent) ||
		    (FMath::Abs(TargetBeat - RoundedBeat) < .05 && RoundedBeat % BeatsPerBar == 0);
	}
}

// 保存待执行的连击，等到目标拍点再开始播放
bool UBeatSyncedComboComponent::QueueCombo()
{
	if (BossHealth <= 0.f)
		return false;
	if (bQueued || bPlaying || !IsValid(AttackData) || !IsValid(AttackData->AttackMontage))
	{
		return false;
	}
	if (AttackData->ImpactMarkers.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("BeatSyncedCombo: 动作尚未标定命中时间，请先在攻击数据中添加 ImpactMarkers"));
		return false;
	}
	// 先生成本轮应对配置，运行时调整不直接修改数据资产
	SelectRuntimeResponsePattern();
	// 普通刀击允许闪避或格挡，特殊指令保留各自的防御要求
	for (auto& Marker : RuntimeImpactMarkers)
	{
		if (Marker.DamageShape == EBossAttackDamageShape::Blade &&
		    Marker.RequiredResponse == ERhythmResponseType::Block)
			Marker.RequiredResponse = ERhythmResponseType::DodgeOrBlock;
	}
	SelectRuntimeCinematicAccents();

	UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	if (!Clock || !Clock->IsRunning() || AttackData->PlayRate <= 0.0f)
	{
		return false;
	}

	const float MontageLength = AttackData->AttackMontage->GetPlayLength();
	const float ClipStart = AttackData->ClipStartSeconds;
	const float ClipEnd = AttackData->GetClipEndSeconds();
	if (ClipStart < 0.0f || ClipStart >= ClipEnd || ClipEnd > MontageLength)
	{
		UE_LOG(LogTemp, Warning, TEXT("BeatSyncedCombo: 攻击片段起止时间无效"));
		return false;
	}
	// 逐个检查标记顺序和动画范围，错误配置不能进入运行队列
	float PreviousTime = ClipStart - KINDA_SMALL_NUMBER;
	double PreviousBeatOffset = -1.0;
	for (int32 Index = 0; Index < GetImpactCount(); ++Index)
	{
		const float Time = GetImpactTime(Index);
		const double Offset =
		    AttackData->ImpactMarkers.IsEmpty() ? 0.0 : AttackData->ImpactMarkers[Index].BeatOffset;
		if (Time < ClipStart || Time > ClipEnd || Time <= PreviousTime || Offset < PreviousBeatOffset ||
		    (Index == 0 && Offset != 0))
		{
			UE_LOG(LogTemp, Warning, TEXT("BeatSyncedCombo: AttackData 中的命中时间或拍数无效"));
			return false;
		}
		PreviousTime = Time;
		PreviousBeatOffset = Offset;
	}
	for (const float Rate : AttackData->SegmentPlayRates)
	{
		if (Rate <= 0.0f)
		{
			UE_LOG(LogTemp, Warning, TEXT("BeatSyncedCombo: 逐段播放速度必须大于 0"));
			return false;
		}
	}

	// 先把片段起点和首击时间换算为实际播放时长，计入分段变速和蓄力停顿
	const double ClipStartElapsed = AttackData->GetElapsedAtAnimationTime(ClipStart);
	const double FirstImpactDelay =
	    AttackData->GetElapsedAtAnimationTime(GetImpactTime(0)) - ClipStartElapsed;
	const double LeadSeconds =
	    FMath::Max<double>(SchedulingLeadSeconds,
		                   bBlinkRequested ? FMath::Max(BlinkSettings.WarningBeats, StrongBeatChargeBeats) *
		                                             Clock->GetSecondsPerBeat() +
		                                         0.05
		                                   : 0.0);
	const double EarliestImpactTime = Clock->GetAudioNowSeconds() + FirstImpactDelay + LeadSeconds;
	const double BeatDuration = Clock->GetSecondsPerBeat();
	for (int32 Index = 1; Index < GetImpactCount(); ++Index)
	{
		const double AttackGap =
		    AttackData->GetElapsedAtAnimationTime(GetImpactTime(Index)) - FirstImpactDelay;
		const double BeatGap = AttackData->ImpactMarkers[Index].BeatOffset * BeatDuration;
		if (FMath::Abs(AttackGap - BeatGap) > 0.05)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("BeatSyncedCombo: 第 %d 击与目标拍点相差 %.3f 秒；请调整动画间隔或 PlayRate"),
			       Index + 1, AttackGap - BeatGap);
		}
	}
	// 选择来得及播放前摇和预警的拍点，不能为了对拍跳过动作准备
	int32 CandidateBeat = FMath::Max(
	    0, FMath::CeilToInt(Clock->GetCurrentBeat() + (FirstImpactDelay + LeadSeconds) / BeatDuration));
	while (Clock->GetBeatTimeSeconds(CandidateBeat) < EarliestImpactTime)
	{
		++CandidateBeat;
	}
	if (AttackData->FirstImpactBeatInBar > 0)
	{
		const int32 BeatsPerBar = FMath::Max(1, Clock->GetBeatsPerBar());
		const int32 WantedBeatInBar = FMath::Clamp(AttackData->FirstImpactBeatInBar, 1, BeatsPerBar) - 1;
		CandidateBeat += (WantedBeatInBar - CandidateBeat % BeatsPerBar + BeatsPerBar) % BeatsPerBar;
	}
	float MusicPriority = 0.f;
	const int32 EmphasisBeat = SelectMusicEmphasisBeat(AttackData, MusicPriority);
	if (!bPlayerResonance && EmphasisBeat >= CandidateBeat)
	{
		CandidateBeat = EmphasisBeat;
		const double TailGap = AttackData->GetElapsedAtAnimationTime(GetImpactTime(GetImpactCount() - 1)) -
		                       AttackData->GetElapsedAtAnimationTime(GetImpactTime(0));
		UE_LOG(LogTemp, Display, TEXT("MusicEmphasis: attack=%s priority=%.3f firstBeat=%d finalSource=%.3f"),
		       *GetNameSafe(AttackData), MusicPriority, CandidateBeat,
		       Clock->AudioTimeToMusicSourceSeconds(Clock->GetBeatTimeSeconds(CandidateBeat) + TailGap));
	}
	FirstImpactBeat = CandidateBeat;
	bStrongBeatThisAttack = bBlinkRequested && CandidateBeat % FMath::Max(1, Clock->GetBeatsPerBar()) == 0;
	bStrongBeatChargeStarted = false;
	ScheduledFirstImpactAudioSeconds = Clock->GetBeatTimeSeconds(FirstImpactBeat);
	// 由首击拍点倒推出动画开始时间，让击中而不是起手落在拍点上
	ScheduledStartAudioSeconds = ScheduledFirstImpactAudioSeconds - FirstImpactDelay;
	UE_LOG(
	    LogTemp, Display,
	    TEXT(
	        "BeatSyncedCombo: Boss=%s FirstImpactBeat=%d BeatInBar=%d StartAudio=%.3f FirstImpactDelay=%.3f"),
	    *GetNameSafe(GetOwner()), FirstImpactBeat,
	    FirstImpactBeat % FMath::Max(1, Clock->GetBeatsPerBar()) + 1, ScheduledStartAudioSeconds,
	    FirstImpactDelay);
	// 每轮重新开始声音、表现和伤害窗口索引，不能沿用上一轮的结算标记
	NextImpactIndex = 0;
	NextBladeSoundIndex = 0;
	NextDamageWindowIndex = 0;
	ActiveDamageImpactIndex = INDEX_NONE;
	bActiveDamageResolved = false;
	bActiveDamageThreatened = false;
	bContactAtMarker = false;
	SidestepOpening = FBossOpening();
	BladeTraceMesh = FindBladeMesh();
	bLastAttackCompleted = false;
	bPhaseTwoAttackCounted = false;
	bQueued = true;
	return true;
}

// 取消排队或正在播放的连招，清理窗口、移动对齐和临时表现
void UBeatSyncedComboComponent::CancelCombo()
{
	// 取消攻击时一并淡出挥刀声音，避免旧声音的尾音留到下一动作
	if (UAudioComponent* Voice = ActiveBladeVoice.Get(); Voice && Voice->IsPlaying())
		Voice->FadeOut(.06f, 0.f, EAudioFaderCurve::SCurve);
	ActiveBladeVoice.Reset();
	RecordPhaseTwoAttackOutcome(false);
	ClearGroundBurst();
	EndCinematicAccent(true);
	ClearBlink();
	ClearWarping();
	if (bPlaying && IsValid(AttackData) && IsValid(AttackData->AttackMontage))
	{
		TArray<USkeletalMeshComponent*> Meshes;
		GetOwner()->GetComponents<USkeletalMeshComponent>(Meshes);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (IsValid(Mesh) && IsValid(Mesh->GetSkeletalMeshAsset()) &&
			    Mesh->GetSkeletalMeshAsset()->GetSkeleton() == AttackData->AttackMontage->GetSkeleton() &&
			    (MeshComponentTag.IsNone() || Mesh->ComponentHasTag(MeshComponentTag)))
			{
				if (Mesh->GetAnimationMode() == EAnimationMode::AnimationBlueprint && Mesh->GetAnimInstance())
				{
					Mesh->GetAnimInstance()->Montage_Stop(0.15f, AttackData->AttackMontage);
					Mesh->GetAnimInstance()->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
				}
				else
					Mesh->Stop();
			}
		}
	}
	bQueued = false;
	bPlaying = false;
	bLastAttackCompleted = false;
	NextImpactIndex = 0;
	NextBladeSoundIndex = 0;
	NextDamageWindowIndex = 0;
	ActiveDamageImpactIndex = INDEX_NONE;
	bActiveDamageResolved = false;
	bActiveDamageThreatened = false;
	bContactAtMarker = false;
	SidestepOpening = FBossOpening();
	BladeTraceMesh.Reset();
	RootMotionMesh.Reset();
	AttackTarget.Reset();
	LocomotionResumeWorldSeconds = GetWorld()->GetTimeSeconds() + 0.2;
}

// 查找用于刀刃采样的武器网格
UMeshComponent* UBeatSyncedComboComponent::FindBladeMesh() const
{
	if (!GetOwner())
		return nullptr;
	TArray<UMeshComponent*> Meshes;
	GetOwner()->GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* Mesh : Meshes)
	{
		if (!IsValid(Mesh))
			continue;
		const FString Name = Mesh->GetName().ToLower();
		if (Name.Contains(TEXT("sword")) || Name.Contains(TEXT("weapon")) ||
		    Mesh->ComponentHasTag(TEXT("DamageBlade")))
			return Mesh;
	}
	return nullptr;
}

// 有效接触：在攻击允许的窗口内检查刀刃与目标，再交给玩家的应对结算
// 动画推进后检查实际接触；接触成立时再结算玩家的防御结果
void UBeatSyncedComboComponent::UpdateDamageSweep(double AudioNow)
{
	if (!bPlaying || !IsValid(AttackData) || !AttackTarget.IsValid() || GetImpactCount() <= 0)
		return;
	const double ClipStartElapsed = AttackData->GetElapsedAtAnimationTime(AttackData->ClipStartSeconds);

	// 按顺序补结算已经结束的窗口，避免掉帧时漏掉整次攻击
	// 先处理已经越过的窗口，低帧率跨过多击时也逐个推进
	// 没有接触的窗口只走躲避或落空处理，不补发固定时间伤害
	while (NextDamageWindowIndex < GetImpactCount())
	{
		const FAttackImpactMarker Defaults;
		const FAttackImpactMarker& Marker = AttackData->ImpactMarkers.IsValidIndex(NextDamageWindowIndex)
		                                        ? GetImpactMarker(NextDamageWindowIndex)
		                                        : Defaults;
		const double MarkerElapsed =
		    AttackData->GetElapsedAtAnimationTime(GetImpactTime(NextDamageWindowIndex)) - ClipStartElapsed;
		const double MarkerTime = ScheduledStartAudioSeconds + MarkerElapsed;
		if (Marker.DamageShape == EBossAttackDamageShape::GroundBurst)
		{
			// 爆炸后保留短暂宽限，让正好踩拍按下的闪避也能参与判定
			if (AudioNow < MarkerTime + (bGroundBurstDodgeThrough ? GroundBurstLateGraceSeconds : 0.f))
				break;
			ResolveGroundBurst(NextDamageWindowIndex, AudioNow);
			++NextDamageWindowIndex;
			ActiveDamageImpactIndex = INDEX_NONE;
			continue;
		}
		float WindowAfter = Marker.DamageWindowAfterSeconds;
		if (Marker.RequiredResponse == ERhythmResponseType::HoldBlock &&
		    Marker.HoldMarker == ERhythmHoldMarkerType::End)
		{
			if (const UPlayerRhythmCombatComponent* Player =
			        AttackTarget->FindComponentByClass<UPlayerRhythmCombatComponent>())
				WindowAfter = FMath::Max(WindowAfter, Player->GetGoodWindowSeconds());
		}
		if (AudioNow <= MarkerTime + WindowAfter)
			break;
		if (ActiveDamageImpactIndex != NextDamageWindowIndex)
		{
			ActiveDamageImpactIndex = NextDamageWindowIndex;
			bActiveDamageResolved = false;
			bActiveDamageThreatened =
			    bPracticeNote ||
			    FVector::Dist2D(AttackTarget->GetActorLocation(), GetOwner()->GetActorLocation()) <= 450.0f;
			ClosestBladeFeedbackLocation = FVector::ZeroVector;
			ClosestBladeAttackDirection =
			    (AttackTarget->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal();
			ClosestBladeFeedbackDistance = BIG_NUMBER;
		}
		if (!bActiveDamageResolved)
			ResolveUncontactedDamageWindow(NextDamageWindowIndex);
		++NextDamageWindowIndex;
		ActiveDamageImpactIndex = INDEX_NONE;
		bActiveDamageThreatened = false;
		bContactAtMarker = false;
	}
	if (NextDamageWindowIndex >= GetImpactCount())
		return;

	const int32 WindowIndex = NextDamageWindowIndex;
	const FAttackImpactMarker Defaults;
	const FAttackImpactMarker& WindowMarker =
	    AttackData->ImpactMarkers.IsValidIndex(WindowIndex) ? GetImpactMarker(WindowIndex) : Defaults;
	const double MarkerElapsed =
	    AttackData->GetElapsedAtAnimationTime(GetImpactTime(WindowIndex)) - ClipStartElapsed;
	const double MarkerAudioTime = ScheduledStartAudioSeconds + MarkerElapsed;
	if (WindowMarker.DamageShape == EBossAttackDamageShape::GroundBurst)
		return;
	if (AudioNow < MarkerAudioTime - WindowMarker.DamageWindowBeforeSeconds)
		return;

	UMeshComponent* Blade = BladeTraceMesh.Get();
	if (!IsValid(Blade))
	{
		Blade = FindBladeMesh();
		BladeTraceMesh = Blade;
	}
	const FVector CurrentBlade = Blade ? Blade->Bounds.Origin
	                                   : GetOwner()->GetActorLocation() +
	                                         GetOwner()->GetActorForwardVector() * 180.0f +
	                                         FVector(0, 0, 100.0f);
	// 每次进入新窗口才清空本击状态，同一窗口后续帧继续累计接触信息
	const bool bNewWindow = ActiveDamageImpactIndex != WindowIndex;
	if (bNewWindow)
	{
		ActiveDamageImpactIndex = WindowIndex;
		bActiveDamageResolved = false;
		bActiveDamageThreatened = false;
		bContactAtMarker = false;
		PreviousBladeTraceLocation = CurrentBlade;
		ClosestBladeFeedbackLocation = CurrentBlade;
		ClosestBladeAttackDirection = GetOwner()->GetActorForwardVector();
		ClosestBladeFeedbackDistance = BIG_NUMBER;
	}
	if (bActiveDamageResolved)
		return;

	AActor* Target = AttackTarget.Get();
	const UCapsuleComponent* Capsule = Target ? Target->FindComponentByClass<UCapsuleComponent>() : nullptr;
	const FVector TargetCenter = Capsule ? Capsule->GetComponentLocation() : Target->GetActorLocation();
	const float TargetRadius = Capsule ? Capsule->GetScaledCapsuleRadius() : 42.0f;
	const float BladeRadius = Blade ? FMath::Clamp(Blade->Bounds.SphereRadius * 0.35f, 35.0f, 105.0f) : 70.0f;
	// 用上一帧到当前帧的刀刃路径检测，减少高速挥刀从玩家身上穿过却未命中的情况
	const float SweepDistance =
	    FMath::PointDistToSegment(TargetCenter, PreviousBladeTraceLocation, CurrentBlade);
	const FVector ClosestOnBladeSweep =
	    FMath::ClosestPointOnSegment(TargetCenter, PreviousBladeTraceLocation, CurrentBlade);
	if (SweepDistance < ClosestBladeFeedbackDistance)
	{
		ClosestBladeFeedbackDistance = SweepDistance;
		ClosestBladeFeedbackLocation = ClosestOnBladeSweep;
		ClosestBladeAttackDirection = (CurrentBlade - PreviousBladeTraceLocation).GetSafeNormal();
		if (ClosestBladeAttackDirection.IsNearlyZero())
			ClosestBladeAttackDirection = (TargetCenter - GetOwner()->GetActorLocation()).GetSafeNormal();
	}
	const float OwnerDistance = FVector::Dist2D(TargetCenter, GetOwner()->GetActorLocation());
	if (bNewWindow)
		bActiveDamageThreatened = bPracticeNote || OwnerDistance <= 450.0f;
	// 拍点附近的补充检测仍受已锁定攻击方向限制，侧移出拳路后不会被强行判中
	const FVector BossToTarget = TargetCenter - GetOwner()->GetActorLocation();
	const FVector FromBoss = BossToTarget.GetSafeNormal2D();
	const float LateralFromCommittedLine =
	    FMath::Abs(FVector::CrossProduct(CommittedStrikeDirection, BossToTarget).Z);
	const bool bWithinCommittedArc =
	    !bStrikeCommitted || (FVector::DotProduct(FromBoss, CommittedStrikeDirection) >= .65f &&
		                      LateralFromCommittedLine <= TargetRadius + 25.f);
	const bool bAtMarkerFallback =
	    FMath::Abs(AudioNow - MarkerAudioTime) <= 0.035 && OwnerDistance <= 300.0f && bWithinCommittedArc;
	// 提前碰到玩家并不永久锁定命中，玩家已经离开原接触位置就撤销记录
	if (bContactAtMarker && FVector::Dist2D(TargetCenter, EarlyContactTargetLocation) > TargetRadius + 45.f)
		bContactAtMarker = false;
	PreviousBladeTraceLocation = CurrentBlade;
	if (SweepDistance > BladeRadius + TargetRadius && !bAtMarkerFallback && !bContactAtMarker)
		return;
	// 刀刃提前接触时先记录，到命中拍点再结算，避免声音和受击表现抢拍
	if (AudioNow < MarkerAudioTime)
	{
		bContactAtMarker = true;
		EarlyContactTargetLocation = TargetCenter;
		return;
	}
	// 持续格挡的松键判定不能早于配置的结束拍点
	if (WindowMarker.RequiredResponse == ERhythmResponseType::HoldBlock &&
	    WindowMarker.HoldMarker == ERhythmHoldMarkerType::End && AudioNow < MarkerAudioTime)
		return;
	if (WindowMarker.RequiredResponse == ERhythmResponseType::HoldBlock &&
	    WindowMarker.HoldMarker == ERhythmHoldMarkerType::End)
	{
		// 保留拍点后的松键窗口，不能在玩家仍按住时提前结束评级
		if (const UPlayerRhythmCombatComponent* Player =
		        Target->FindComponentByClass<UPlayerRhythmCombatComponent>();
		    Player && Player->IsBlocking() && AudioNow < MarkerAudioTime + Player->GetGoodWindowSeconds())
			return;
	}

	// 调用玩家结算前先标记本窗口已处理，回调触发新的状态也不会重复扣血
	bActiveDamageResolved = true;
	const FAttackImpactMarker& Marker =
	    AttackData->ImpactMarkers.IsValidIndex(WindowIndex) ? GetImpactMarker(WindowIndex) : Defaults;
	const double Offset = AttackData->ImpactMarkers.IsEmpty() ? 0.0 : Marker.BeatOffset;
	const double TargetBeat = FirstImpactBeat + Offset;
	const int32 NearestBeat = FMath::RoundToInt(TargetBeat);
	const bool bWholeBeat = FMath::Abs(TargetBeat - NearestBeat) < 0.05;
	const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	const bool bStrong = Clock && bWholeBeat && NearestBeat % FMath::Max(1, Clock->GetBeatsPerBar()) == 0;
	if (UPlayerRhythmCombatComponent* PlayerState =
	        Target->FindComponentByClass<UPlayerRhythmCombatComponent>())
	{
		const bool bAvoided = PlayerState->ResolveBossContact(
		    MarkerAudioTime, TargetBeat, Marker.Damage, Marker.IdealDodgeLeadSeconds, bStrong,
		    ClosestBladeFeedbackLocation, ClosestBladeAttackDirection, Marker.RequiredResponse,
		    Marker.HoldMarker, Marker.HoldGroup, GetOwner()->GetActorLocation());
		if (!bAvoided)
			NotifyConfirmedHit();
		if (bAvoided && PlayerState->LastResponse == ERhythmResponseType::Block &&
		    PlayerState->LastJudgment == ERhythmDodgeJudgment::Perfect)
		{
			// 完美格挡额外削减 Boss 韧性，为后续攻击创造机会
			ApplyBossPoiseDamage(28.f, Target);
		}
		if (bAvoided && Marker.RequiredResponse == ERhythmResponseType::Counter &&
		    (PlayerState->LastJudgment == ERhythmDodgeJudgment::Perfect ||
		     PlayerState->LastJudgment == ERhythmDodgeJudgment::Good))
		{
			const bool bPerfectCounter = PlayerState->LastJudgment == ERhythmDodgeJudgment::Perfect;
			FVector Away = (GetOwner()->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
			ApplyBossDamageFrom(CounterDamage * (bPerfectCounter ? 1.f : .5f), Target, bPerfectCounter);
			if (TryBeginPhaseAscension(Target, ClosestBladeFeedbackLocation, true))
				return;
			if (Away.IsNearlyZero())
				Away = -GetOwner()->GetActorForwardVector();
			FHitResult StaggerHit;
			GetOwner()->AddActorWorldOffset(Away * (bPerfectCounter ? 95.f : 45.f), true, &StaggerHit);
			UE_LOG(LogTemp, Display, TEXT("RhythmCounter: Boss interrupted grade=%s knockback=%.0f"),
			       *UEnum::GetValueAsString(PlayerState->LastJudgment), bPerfectCounter ? 95.f : 45.f);
			StartBossHitReaction(Target, bPerfectCounter ? HeavyHitReactionSeconds : LightHitReactionSeconds,
			                     true);
			TriggerCinematicClash(WindowIndex, ClosestBladeFeedbackLocation, bStrong);
			return;
		}
		if (bAvoided && PlayerState->LastResponse != ERhythmResponseType::Dodge &&
		    (PlayerState->LastJudgment == ERhythmDodgeJudgment::Good ||
		     PlayerState->LastJudgment == ERhythmDodgeJudgment::Perfect ||
		     PlayerState->LastJudgment == ERhythmDodgeJudgment::GoodHold ||
		     PlayerState->LastJudgment == ERhythmDodgeJudgment::PerfectHold) &&
		    TryBeginPhaseAscension(Target, ClosestBladeFeedbackLocation, true))
			return;
		if (bAvoided && Marker.RequiredResponse == ERhythmResponseType::HoldBlock &&
		    Marker.HoldMarker == ERhythmHoldMarkerType::End &&
		    PlayerState->LastJudgment == ERhythmDodgeJudgment::PerfectHold)
		{
			FVector Away = (GetOwner()->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
			if (Away.IsNearlyZero())
				Away = -GetOwner()->GetActorForwardVector();
			FHitResult ShockHit;
			GetOwner()->AddActorWorldOffset(Away * 70.0f, true, &ShockHit);
			ApplyBossPoiseDamage(45.0f, Target);
			UE_LOG(LogTemp, Display, TEXT("PerfectHoldShock: boss knockback=70 resonance=%.1f"),
			       PlayerState->Resonance);
		}
		UE_LOG(
		    LogTemp, Display,
		    TEXT("BossDamageSweep: impact=%d blade=%s sweepDistance=%.1f radius=%.1f fallback=%d avoided=%d"),
		    WindowIndex + 1, *GetNameSafe(Blade), SweepDistance, BladeRadius + TargetRadius,
		    bAtMarkerFallback ? 1 : 0, bAvoided ? 1 : 0);
	}
}

// 窗口结束仍未接触时进行躲避或教程判定，不按真实接触扣血
void UBeatSyncedComboComponent::ResolveUncontactedDamageWindow(int32 ImpactIndex)
{
	if (!bActiveDamageThreatened || !AttackTarget.IsValid() || !IsValid(AttackData) || ImpactIndex < 0 ||
	    ImpactIndex >= GetImpactCount())
		return;
	const FAttackImpactMarker Defaults;
	const FAttackImpactMarker& Marker =
	    AttackData->ImpactMarkers.IsValidIndex(ImpactIndex) ? GetImpactMarker(ImpactIndex) : Defaults;
	const double ClipStartElapsed = AttackData->GetElapsedAtAnimationTime(AttackData->ClipStartSeconds);
	const double MarkerElapsed =
	    AttackData->GetElapsedAtAnimationTime(GetImpactTime(ImpactIndex)) - ClipStartElapsed;
	const double MarkerAudioTime = ScheduledStartAudioSeconds + MarkerElapsed;
	const double Offset = AttackData->ImpactMarkers.IsEmpty() ? 0.0 : Marker.BeatOffset;
	const double TargetBeat = FirstImpactBeat + Offset;
	const int32 NearestBeat = FMath::RoundToInt(TargetBeat);
	const bool bWholeBeat = FMath::Abs(TargetBeat - NearestBeat) < 0.05;
	const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	const bool bStrong = Clock && bWholeBeat && NearestBeat % FMath::Max(1, Clock->GetBeatsPerBar()) == 0;
	bool bHandledByDodge = false;
	if (UPlayerRhythmCombatComponent* PlayerState =
	        AttackTarget->FindComponentByClass<UPlayerRhythmCombatComponent>())
	{
		// 走出刀刃范围不会受伤，只有最近按过闪避才继续计算节奏评级
		const bool bRecentDodge =
		    FMath::Abs(MarkerAudioTime - PlayerState->GetLastDodgeInputAudioSeconds()) <= .55;
		if (!bRecentDodge && !bPracticeNote)
		{
			const FVector PlayerLocation = AttackTarget->GetActorLocation();
			const FVector Delta = PlayerLocation - CommittedPlayerLocation;
			const float LateralTravel = FMath::Abs(FVector::CrossProduct(CommittedStrikeDirection, Delta).Z);
			const bool bClose = FVector::Dist2D(PlayerLocation, GetOwner()->GetActorLocation()) <= 320.f;
			if (bStrikeCommitted && ActiveWarpWindowIndex == ImpactIndex && bClose && LateralTravel >= 65.f)
			{
				SidestepOpening = {EBossOpeningKind::Sidestep, MarkerAudioTime,
				                   MarkerAudioTime + (Clock ? Clock->GetSecondsPerBeat() * 2. : 1.)};
				UE_LOG(LogTemp, Display, TEXT("BossSpatialAvoidance: sidestep impact=%d lateral=%.0f"),
				       ImpactIndex + 1, LateralTravel);
			}
			bActiveDamageResolved = true;
			return;
		}
		FVector FeedbackLocation = ClosestBladeFeedbackLocation;
		if (FeedbackLocation.IsNearlyZero())
			FeedbackLocation = AttackTarget->GetActorLocation() + FVector(0, 0, 85.0f);
		FVector AttackDirection = ClosestBladeAttackDirection;
		if (AttackDirection.IsNearlyZero())
			AttackDirection =
			    (AttackTarget->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal();
		bHandledByDodge = PlayerState->ResolveBossAvoidance(
		    MarkerAudioTime, TargetBeat, Marker.IdealDodgeLeadSeconds, bStrong, FeedbackLocation,
		    AttackDirection, Marker.RequiredResponse, Marker.HoldMarker, Marker.HoldGroup);
	}
	bActiveDamageResolved = true;
	UE_LOG(LogTemp, Display, TEXT("BossDamageSweep: impact=%d noContact=1 threatened=1 dodgeHandled=%d"),
	       ImpactIndex + 1, bHandledByDodge ? 1 : 0);
}

// 启动攻击蒙太奇并恢复到音乐时间对应的动画位置
bool UBeatSyncedComboComponent::StartPlayback(double AudioNowSeconds)
{
	ClearWarping();
	// 启动帧可能晚于预约时间，按当前音乐时间追到正确动画位置
	const float StartPosition = AttackData->GetAnimationTimeAtElapsed(
	    AttackData->GetElapsedAtAnimationTime(AttackData->ClipStartSeconds) +
	    static_cast<float>(AudioNowSeconds - ScheduledStartAudioSeconds));
	if (StartPosition >= AttackData->GetClipEndSeconds())
	{
		return false;
	}

	TArray<USkeletalMeshComponent*> Meshes;
	GetOwner()->GetComponents<USkeletalMeshComponent>(Meshes);
	int32 PlayedCount = 0;
	RootMotionMesh.Reset();
	LastRootMotionPosition = StartPosition;
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (!IsValid(Mesh) || !IsValid(Mesh->GetSkeletalMeshAsset()) ||
		    Mesh->GetSkeletalMeshAsset()->GetSkeleton() != AttackData->AttackMontage->GetSkeleton() ||
		    (!MeshComponentTag.IsNone() && !Mesh->ComponentHasTag(MeshComponentTag)))
		{
			continue;
		}
		if (Mesh->GetAnimationMode() == EAnimationMode::AnimationBlueprint && Mesh->GetAnimInstance())
		{
			UAnimInstance* Instance = Mesh->GetAnimInstance();
			// 位移由本组件提取和修正，清掉自动根位移以免重复移动
			Mesh->ConsumeRootMotion();
			Instance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
			if (Instance->Montage_Play(AttackData->AttackMontage,
			                           AttackData->GetRateAtAnimationTime(StartPosition)) > 0.0f)
			{
				Instance->Montage_SetPosition(AttackData->AttackMontage, StartPosition);
				if (!RootMotionMesh.IsValid())
					RootMotionMesh = Mesh;
				++PlayedCount;
			}
		}
		else
		{
			Mesh->PlayAnimation(AttackData->AttackMontage, false);
			if (UAnimSingleNodeInstance* Instance = Mesh->GetSingleNodeInstance())
			{
				Instance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
				Instance->SetPlayRate(AttackData->GetRateAtAnimationTime(StartPosition));
				Instance->SetPosition(StartPosition, false);
				if (!RootMotionMesh.IsValid())
					RootMotionMesh = Mesh;
				++PlayedCount;
			}
		}
	}
	if (PlayedCount == 0)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("BeatSyncedCombo: Boss 上没有匹配 Montage 骨架的 SkeletalMeshComponent"));
		return false;
	}
	if (AttackData->bWarpToTarget && AttackTarget.IsValid() && RootMotionMesh.IsValid())
	{
		WarpComponent = NewObject<UBossMotionWarpingComponent>(GetOwner(), TEXT("BeatAttackMotionWarping"));
		GetOwner()->AddOwnedComponent(WarpComponent);
		WarpComponent->RegisterComponent();
		UBossMotionWarpingAdapter* Adapter = WarpComponent->CreateOwnerAdapter<UBossMotionWarpingAdapter>();
		Adapter->SetBoss(GetOwner(), RootMotionMesh.Get());
		for (int32 Index = 0; Index < AttackData->ImpactMarkers.Num(); ++Index)
		{
			const float Impact = AttackData->ImpactMarkers[Index].AnimationTimeSeconds;
			const float PreviousImpact = Index > 0 ? AttackData->ImpactMarkers[Index - 1].AnimationTimeSeconds
			                                       : AttackData->ClipStartSeconds;
			// 每次吸附只在该击前摇内生效，不跨过上一击的收尾
			const float WindowStart = FMath::Max(PreviousImpact + (Index > 0 ? 0.05f : 0.0f),
			                                     Impact - AttackData->WarpLeadAnimationSeconds);
			if (WindowStart >= Impact)
				continue;
			URootMotionModifier_SkewWarp::AddRootMotionModifierSkewWarp(
			    WarpComponent, AttackData->AttackMontage, WindowStart, Impact, TEXT("Player"),
			    EWarpPointAnimProvider::None, FTransform::Identity, NAME_None, true, true, true);
		}
		UE_LOG(LogTemp, Display, TEXT("BeatSyncedCombo: MotionWarping enabled for %d impacts"),
		       AttackData->ImpactMarkers.Num());
	}
	OnComboStarted.Broadcast(FirstImpactBeat);
	UE_LOG(LogTemp, Display, TEXT("BeatSyncedCombo: Playback started Boss=%s Audio=%.3f"),
	       *GetNameSafe(GetOwner()), AudioNowSeconds);
	return true;
}

// 按音乐时间推进瞬移、动画、命中窗口和收尾，并处理阶段演出
void UBeatSyncedComboComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (TickComboPoseAudit())
		return;
	// 转阶段优先接管更新，普通攻击和走路不能同时继续推进
	UpdateBossPhase(GetWorld() ? GetWorld()->GetSubsystem<UBeatClockSubsystem>() : nullptr);
	if (bPhaseTransitionActive)
		return;
	TickPlayerResonance();
	if (GetWorld())
	{
		const double WorldNow = GetWorld()->GetTimeSeconds();
		if (bBossHitReacting && WorldNow >= BossHitReactionEndWorldSeconds)
			EndBossHitReaction();
		else if (!bBossHitReacting && BossPoise < MaxBossPoise &&
		         WorldNow - LastBossPoiseDamageWorldSeconds >= BossPoiseRecoveryDelay)
			BossPoise = FMath::Min(MaxBossPoise, BossPoise + BossPoiseRecoveryPerSecond * DeltaTime);
	}
	if (BossHealth <= 0.f)
		return;
	// 先处理后跳和击退，再判断是否允许普通行走推进身体
	TickBackstep(DeltaTime);
	if (USkeletalMeshComponent* Mesh = LocomotionMesh.Get())
	{
		// 动画图负责姿态，这里单独提取根位移
		// Boss 是 Actor，没有 CharacterMovement 自动推进身体
		Mesh->ConsumeRootMotion();
		const UBossLocomotionAnimInstance* Anim = Cast<UBossLocomotionAnimInstance>(Mesh->GetAnimInstance());
		LastLocomotionSpeed = 0.0f;
		bLastLocomotionBlocked = false;
		// 排队等待拍点时继续走路，真正播放攻击后才交给攻击位移
		if (!bPlaying && !IsPhaseTransitionBlocking() && !bBossHitReacting &&
		    GetWorld()->GetTimeSeconds() >= LocomotionResumeWorldSeconds && Anim && Anim->bIsMoving &&
		    DirectionalWalkAnimations.IsValidIndex(0))
		{
			// 八方向动画共用连续的步行计时，混合空间选姿态，根位移推进碰撞身体
			if (const UAnimSequence* Clip = DirectionalWalkAnimations[0])
			{
				const float Length = Clip->GetPlayLength();
				float Remaining = FMath::Max(0.0f, DeltaTime * Mesh->GlobalAnimRateScale);
				float RootDistance = 0.0f;
				// 一帧跨过行走动画结尾时分段提取位移，循环后继续处理剩余时间
				while (Length > KINDA_SMALL_NUMBER && Remaining > KINDA_SMALL_NUMBER)
				{
					const float End = FMath::Min(Length, LocomotionAnimationTime + Remaining);
					RootDistance += Clip->ExtractRootMotionFromRange(LocomotionAnimationTime, End,
					                                                 FAnimExtractContext(End, true))
					                    .GetTranslation()
					                    .Size2D();
					Remaining -= End - LocomotionAnimationTime;
					LocomotionAnimationTime = End >= Length - KINDA_SMALL_NUMBER ? 0.0f : End;
				}
				if (RootDistance > KINDA_SMALL_NUMBER)
				{
					const FVector LocalDirection(Anim->MoveForward, Anim->MoveRight, 0.0f);
					const FVector Delta =
					    GetOwner()->GetActorTransform().TransformVectorNoScale(LocalDirection) * RootDistance;
					const FVector Before = GetOwner()->GetActorLocation();
					const float HalfHeight =
					    Cast<UCapsuleComponent>(GetOwner()->GetRootComponent())
					        ? CastChecked<UCapsuleComponent>(GetOwner()->GetRootComponent())
					              ->GetScaledCapsuleHalfHeight()
					        : 100.0f;
					FCollisionQueryParams Query(SCENE_QUERY_STAT(BossLocomotionFloor), false, GetOwner());
					FHitResult Floor;
					// 先检查计划落脚位置有可走地面，再移动根胶囊
					const FVector Probe = Before + Delta;
					const bool bFloor = GetWorld()->LineTraceSingleByChannel(
					    Floor, Probe + FVector(0, 0, 50.0f), Probe - FVector(0, 0, HalfHeight + 180.0f),
					    ECC_Visibility, Query);
					if (bFloor && Floor.ImpactNormal.Z > 0.6f &&
					    FMath::Abs(Floor.ImpactPoint.Z + HalfHeight - Before.Z) < 130.0f)
					{
						FHitResult Hit;
						GetOwner()->AddActorWorldOffset(Delta, true, &Hit);
						if (Hit.IsValidBlockingHit())
						{
							const FVector Slide =
							    FVector::VectorPlaneProject(Delta * (1.0f - Hit.Time), Hit.Normal);
							GetOwner()->AddActorWorldOffset(FVector(Slide.X, Slide.Y, 0.0f), true);
						}
						AlignBossToGround(GetOwner(), AttackTarget.Get());
					}
					LastLocomotionSpeed = FVector::Dist2D(Before, GetOwner()->GetActorLocation()) /
					                      FMath::Max(DeltaTime, KINDA_SMALL_NUMBER);
					bLastLocomotionBlocked = LastLocomotionSpeed < 5.0f;
				}
			}
		}
		else
		{
			LocomotionAnimationTime = 0.0f;
		}
		if (Anim && Anim->bIsMoving && !bPlaying &&
		    GetWorld()->GetTimeSeconds() - LastLocomotionLogWorldSeconds > 2.0)
		{
			UE_LOG(LogTemp, Display, TEXT("BossLocomotion: Direction=%d ActualSpeed=%.0f Blocked=%d Boss=%s"),
			       Anim->MoveDirectionIndex, LastLocomotionSpeed, bLastLocomotionBlocked ? 1 : 0,
			       *GetOwner()->GetActorLocation().ToString());
			LastLocomotionLogWorldSeconds = GetWorld()->GetTimeSeconds();
		}
	}
	UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	UpdateBeatFirePulse(Clock);
	UpdateStrongBeatPresentation(Clock);
	UpdateBeatOverlay(Clock);
	if (!Clock || !Clock->IsRunning())
	{
		return;
	}
	const double Now = Clock->GetAudioNowSeconds();
	UpdateCinematicAccent(Clock, Now);
	if (bQueued || bPlaying)
	{
		const double WarningTime =
		    ScheduledFirstImpactAudioSeconds - BlinkSettings.WarningBeats * Clock->GetSecondsPerBeat();
		if (bBlinkRequested && !bBlinkWarningShown && Now >= WarningTime)
		{
			if (!LockBlinkDestination())
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("BeatBlink: No safe landing; continuing combo without blink at beat %d"),
				       FirstImpactBeat);
				ClearBlink();
			}
			if (bBlinkRequested)
				bBlinkWarningShown = true;
			if (!bBlinkRequested)
				return;
			OnBlinkWarning.Broadcast(LockedBlinkLocation, ScheduledFirstImpactAudioSeconds);
			if (BlinkSettings.WarningEffect)
			{
				const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(GetOwner()->GetRootComponent());
				const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 110.f;
				const float WarningDuration = BlinkSettings.WarningBeats * Clock->GetSecondsPerBeat();
				if (!OriginWarningFX.IsValid())
					OriginWarningFX =
					    SpawnBriefFire(GetWorld(), BlinkSettings.WarningEffect,
						               GetOwner()->GetActorLocation() - FVector(0, 0, HalfHeight - 2.f),
						               WarningDuration, bStrongBeatThisAttack ? 1.3f : 1.f);
				LandingWarningFX = SpawnBriefFire(
				    GetWorld(), BlinkSettings.WarningEffect,
				    FVector(LockedBlinkLocation.X, LockedBlinkLocation.Y, LockedBlinkGroundZ + 2.f),
				    WarningDuration, bStrongBeatThisAttack ? 1.3f : 1.f);
			}
			UE_LOG(LogTemp, Display, TEXT("BeatBlink: Warning beat=%d Audio=%.3f Landing=%s GroundFXZ=%.1f"),
			       FirstImpactBeat, Now, *LockedBlinkLocation.ToString(), LockedBlinkGroundZ + 2.f);
		}
		// 预警后到真正传送前再检查落点，避免玩家或场景物体进入落点后仍强行传送
		if (bBlinkRequested && bBlinkWarningShown && !bBlinkExecuted &&
		    Now >= ScheduledFirstImpactAudioSeconds)
		{
			FVector CheckedLocation = LockedBlinkLocation;
			if (!GetWorld()->FindTeleportSpot(GetOwner(), CheckedLocation, LockedBlinkRotation) ||
			    FVector::Dist2D(CheckedLocation, LockedBlinkLocation) > 75.0f)
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("BeatBlink: Landing became blocked; continuing combo in place"));
				ClearBlink();
			}
			if (!bBlinkRequested)
				return;
			const FVector From = GetOwner()->GetActorLocation();
			if (RootMotionMesh.IsValid())
			{
				AActor* Ghost = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
				if (Ghost)
				{
					UPoseableMeshComponent* GhostMesh = NewObject<UPoseableMeshComponent>(Ghost);
					Ghost->AddInstanceComponent(GhostMesh);
					GhostMesh->SetSkinnedAssetAndUpdate(RootMotionMesh->GetSkeletalMeshAsset());
					GhostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					GhostMesh->RegisterComponent();
					GhostMesh->CopyPoseFromSkeletalComponent(RootMotionMesh.Get());
					for (int32 MaterialIndex = 0; MaterialIndex < RootMotionMesh->GetNumMaterials();
					     ++MaterialIndex)
						GhostMesh->SetMaterial(MaterialIndex, RootMotionMesh->GetMaterial(MaterialIndex));
					GhostMesh->SetWorldTransform(RootMotionMesh->GetComponentTransform());
					Ghost->SetLifeSpan(0.28f);
				}
			}
			if (!GetOwner()->TeleportTo(CheckedLocation, LockedBlinkRotation, false, false))
			{
				UE_LOG(LogTemp, Warning, TEXT("BeatBlink: Teleport failed; continuing combo in place"));
				ClearBlink();
				return;
			}
			bBlinkExecuted = true;
			if (bStrongBeatThisAttack)
				TriggerBeatOverlay(true, Now);
			AlignBossToGround(GetOwner(), AttackTarget.Get());
			OnBlinkSucceeded.Broadcast(CheckedLocation, ScheduledFirstImpactAudioSeconds);
			if (UNiagaraComponent* Effect = OriginWarningFX.Get())
				Effect->Deactivate();
			if (UNiagaraComponent* Effect = LandingWarningFX.Get())
				Effect->Deactivate();
			const UCapsuleComponent* BurstCapsule = Cast<UCapsuleComponent>(GetOwner()->GetRootComponent());
			const float BurstHalfHeight = BurstCapsule ? BurstCapsule->GetScaledCapsuleHalfHeight() : 110.f;
			if (BlinkSettings.OriginEffect)
				SpawnBriefFire(GetWorld(), BlinkSettings.OriginEffect,
				               From - FVector(0, 0, BurstHalfHeight - 2.f), 0.35f);
			if (BlinkSettings.LandingEffect)
				SpawnBriefFire(GetWorld(), BlinkSettings.LandingEffect,
				               GetOwner()->GetActorLocation() - FVector(0, 0, BurstHalfHeight - 2.f), 0.35f,
				               bStrongBeatThisAttack ? 1.45f : 1.f);
			// 传送后丢弃首击前的根位移，避免把传送前的距离再走一遍
			LastRootMotionPosition = GetImpactTime(0);
			ActiveWarpWindowIndex = INDEX_NONE;
			const UCapsuleComponent* LandedCapsule = Cast<UCapsuleComponent>(GetOwner()->GetRootComponent());
			const float FootClearance = LandedCapsule ? GetOwner()->GetActorLocation().Z -
			                                                LandedCapsule->GetScaledCapsuleHalfHeight() -
			                                                LockedBlinkGroundZ
			                                          : 0.f;
			UE_LOG(
			    LogTemp, Display,
			    TEXT(
			        "BeatBlink: Teleported at first-impact beat=%d ErrorMs=%.1f From=%s To=%s FootClearance=%.1f"),
			    FirstImpactBeat, (Now - ScheduledFirstImpactAudioSeconds) * 1000.0, *From.ToString(),
			    *GetOwner()->GetActorLocation().ToString(), FootClearance);
		}
	}
	// 排队只代表确定了拍点，到起手时刻才创建并播放动画
	if (bQueued && Now >= ScheduledStartAudioSeconds)
	{
		bQueued = false;
		bPlaying = StartPlayback(Now);
		if (!bPlaying)
			ClearBlink();
	}
	if (!bPlaying)
	{
		return;
	}
	UpdateGroundBurst(Now);
	if (!bPlaying)
		return;
	// 伤害结算可能触发弹反、中断或转阶段，返回后先确认连击还在播放
	UpdateDamageSweep(Now);
	if (!bPlaying)
		return;

	// 闪避专用攻击只调整前摇姿态的采样时间
	// 音乐、输入窗口和命中拍点继续使用原时钟
	float DodgeWindupLag = 0.f;
	{
		TArray<FUpcomingImpactCue> Upcoming;
		GetUpcomingImpactCues(Upcoming, 2);
		for (const FUpcomingImpactCue& Cue : Upcoming)
		{
			if (Cue.RequiredResponse != ERhythmResponseType::Dodge ||
			    Cue.DamageShape != EBossAttackDamageShape::Blade)
				continue;
			const float UntilInput = float(Cue.ImpactAudioSeconds - Cue.IdealDodgeLeadSeconds - Now);
			const float Windup = FMath::Clamp(float(Clock->GetSecondsPerBeat()) * .8f, .35f, .65f);
			if (UntilInput > 0.f && UntilInput < Windup)
			{
				const float T = 1.f - UntilInput / Windup;
				DodgeWindupLag = .045f * FMath::Sin(UE_PI * T);
			}
			break;
		}
	}
	const double AnimationPosition = AttackData->GetAnimationTimeAtElapsed(
	    AttackData->GetElapsedAtAnimationTime(AttackData->ClipStartSeconds) +
	    static_cast<float>(Now - ScheduledStartAudioSeconds) - DodgeWindupLag);
	const double ClipStartElapsed = AttackData->GetElapsedAtAnimationTime(AttackData->ClipStartSeconds);
	// 逐个消费已经到时的挥刀音效标记，避免下一帧重复播放同一击
	while (NextBladeSoundIndex < GetImpactCount())
	{
		const double MarkerElapsed =
		    AttackData->GetElapsedAtAnimationTime(GetImpactTime(NextBladeSoundIndex)) - ClipStartElapsed;
		const double MarkerAudioTime = ScheduledStartAudioSeconds + MarkerElapsed;
		if (Now < MarkerAudioTime - BladeWhooshLeadSeconds)
			break;
		const double Offset = AttackData->ImpactMarkers.IsEmpty()
		                          ? 0.0
		                          : AttackData->ImpactMarkers[NextBladeSoundIndex].BeatOffset;
		const double TargetBeat = FirstImpactBeat + Offset;
		const int32 NearestBeat = FMath::RoundToInt(TargetBeat);
		const bool bWholeBeat = FMath::Abs(TargetBeat - NearestBeat) < .05;
		const int32 BeatInBar = NearestBeat % FMath::Max(1, Clock->GetBeatsPerBar());
		const bool bStrongAccent = bWholeBeat && BeatInBar == 0;
		const float AccentVolume =
		    bStrongAccent ? StrongBladeVolume : WeakBladeVolume * (bWholeBeat && BeatInBar == 2 ? 1.f : .82f);
		USoundBase* BladeSound = bStrongAccent ? StrongBeatSound.Get() : WeakBladeSound.Get();
		if (BladeSound && AccentVolume > 0.f)
		{
			Clock->TriggerImpactDuck(bStrongAccent ? StrongBeatMusicGain : WeakBeatMusicGain, .15f, 6.f);
			const float Pitch = bStrongAccent ? .98f : 1.05f;
			// 多段连击只保留当前挥刀声音，旧声音短暂淡出，避免长尾音叠加
			if (UAudioComponent* PreviousVoice = ActiveBladeVoice.Get();
			    PreviousVoice && PreviousVoice->IsPlaying())
				PreviousVoice->FadeOut(.045f, 0.f, EAudioFaderCurve::SCurve);
			const float SafeVolume = AccentVolume * FMath::Min(BladeSfxGain, .75f);
			UAudioComponent* Voice = UGameplayStatics::CreateSound2D(
			    this, BladeSound, 1.f, Pitch, BladeSoundStartTimeSeconds, nullptr, false, true);
			if (Voice)
			{
				Voice->FadeIn(.018f, SafeVolume, BladeSoundStartTimeSeconds, EAudioFaderCurve::SCurve);
				ActiveBladeVoice = Voice;
			}
			UE_LOG(
			    LogTemp, Display,
			    TEXT(
			        "BossBladeVoice: impact=%d component=%s playing=%d duration=%.3f gain=%.2f pitch=%.2f sourceStart=%.3f leadMs=%.1f"),
			    NextBladeSoundIndex + 1, *GetNameSafe(Voice), Voice && Voice->IsPlaying() ? 1 : 0,
			    BladeSound->GetDuration(), SafeVolume, Pitch, BladeSoundStartTimeSeconds,
			    (MarkerAudioTime - Now) * 1000.0);
		}
		++NextBladeSoundIndex;
	}
	TArray<USkeletalMeshComponent*> Meshes;
	GetOwner()->GetComponents<USkeletalMeshComponent>(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (!IsValid(Mesh))
			continue;
		if (Mesh->GetAnimationMode() == EAnimationMode::AnimationBlueprint)
		{
			UAnimInstance* Instance = Mesh->GetAnimInstance();
			if (!Instance || !Instance->Montage_IsPlaying(AttackData->AttackMontage))
				continue;
			Instance->Montage_SetPlayRate(
			    AttackData->AttackMontage,
			    AttackData->ChargeHoldSeconds > 0.f &&
			            FMath::IsNearlyEqual(static_cast<float>(AnimationPosition),
						                     AttackData->ChargePoseAnimationSeconds, .0001f)
			        ? 0.f
					: AttackData->GetRateAtAnimationTime(static_cast<float>(AnimationPosition)));
			if (FMath::Abs(Instance->Montage_GetPosition(AttackData->AttackMontage) - AnimationPosition) >
			    0.02)
				Instance->Montage_SetPosition(AttackData->AttackMontage,
				                              static_cast<float>(AnimationPosition));
		}
		else if (UAnimSingleNodeInstance* Instance = Mesh->GetSingleNodeInstance())
		{
			if (Instance->GetCurrentAsset() != AttackData->AttackMontage)
				continue;
			Instance->SetPlayRate(AttackData->GetRateAtAnimationTime(static_cast<float>(AnimationPosition)));
			if (FMath::Abs(Instance->GetCurrentTime() - AnimationPosition) > 0.02)
				Instance->SetPosition(static_cast<float>(AnimationPosition), false);
		}
	}
	const float CurrentRootPosition =
	    FMath::Clamp(static_cast<float>(AnimationPosition), 0.0f, AttackData->GetClipEndSeconds());
	if (GroundBurstImpact == INDEX_NONE && RootMotionMesh.IsValid() && (!bBlinkRequested || bBlinkExecuted) &&
	    CurrentRootPosition > LastRootMotionPosition)
	{
		UpdateWarpTarget(LastRootMotionPosition);
		const FTransform LocalDelta = AttackData->AttackMontage->ExtractRootMotionFromTrackRange(
		    LastRootMotionPosition, CurrentRootPosition, FAnimExtractContext(CurrentRootPosition, true));
		const FTransform FinalLocalDelta =
		    IsValid(WarpComponent)
		        ? WarpComponent->WarpRootMotion(LocalDelta, DeltaTime, AttackData->AttackMontage,
		                                        LastRootMotionPosition, CurrentRootPosition,
		                                        AttackData->GetRateAtAnimationTime(LastRootMotionPosition))
		        : LocalDelta;
		FTransform WorldDelta = RootMotionMesh->ConvertLocalRootMotionToWorld(FinalLocalDelta);
		// 地面攻击只使用水平根位移，垂直位置另行贴地
		// Boss 没有 CharacterMovement 的重力替它抵消动画中的抬升
		FVector Translation = WorldDelta.GetTranslation();
		Translation.Z = 0.f;
		WorldDelta.SetTranslation(Translation);
		GetOwner()->AddActorWorldTransform(WorldDelta, true);
		AlignBossToGround(GetOwner(), AttackTarget.Get());
		LastRootMotionPosition = CurrentRootPosition;
	}
	// 表现标记使用独立索引，负责拍点通知，不在这里直接扣血
	while (NextImpactIndex < GetImpactCount() && AnimationPosition >= GetImpactTime(NextImpactIndex))
	{
		const double Offset =
		    AttackData->ImpactMarkers.IsEmpty() ? 0.0 : AttackData->ImpactMarkers[NextImpactIndex].BeatOffset;
		const double TargetBeat = FirstImpactBeat + Offset;
		const float BeatError = static_cast<float>(Now - Clock->GetBeatTimeSeconds(TargetBeat));
		const int32 NearestBeat = FMath::RoundToInt(TargetBeat);
		const bool bWholeBeat = FMath::Abs(TargetBeat - NearestBeat) < .05;
		const int32 BeatInBar = NearestBeat % FMath::Max(1, Clock->GetBeatsPerBar());
		const bool bStrongAccent = bWholeBeat && BeatInBar == 0;
		TriggerBeatOverlay(bStrongAccent, Now);
		UE_LOG(LogTemp, Display, TEXT("BossBeatOverlay: Impact=%d BeatInBar=%d Strong=%d ErrorMs=%.1f"),
		       NextImpactIndex + 1, BeatInBar + 1, bStrongAccent ? 1 : 0, BeatError * 1000.f);
		UE_LOG(LogTemp, Display, TEXT("BeatSyncedCombo: Impact=%d TargetBeat=%.2f ErrorMs=%.1f"),
		       NextImpactIndex + 1, TargetBeat, BeatError * 1000.0f);
		if (AttackTarget.IsValid())
		{
			UE_LOG(LogTemp, Display, TEXT("BeatSyncedCombo: Impact=%d playerDistance=%.0f"),
			       NextImpactIndex + 1,
			       FVector::Dist2D(GetOwner()->GetActorLocation(), AttackTarget->GetActorLocation()));
		}
		OnImpact.Broadcast(NextImpactIndex, TargetBeat, BeatError);
		++NextImpactIndex;
	}
	if (AnimationPosition >= AttackData->GetClipEndSeconds())
	{
		// 连击结束时淡出挥刀尾音，让声音和动作一起结束
		if (UAudioComponent* Voice = ActiveBladeVoice.Get(); Voice && Voice->IsPlaying())
			Voice->FadeOut(.08f, 0.f, EAudioFaderCurve::SCurve);
		ActiveBladeVoice.Reset();
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (!IsValid(Mesh))
				continue;
			if (Mesh->GetAnimationMode() == EAnimationMode::AnimationBlueprint && Mesh->GetAnimInstance())
			{
				Mesh->GetAnimInstance()->Montage_Stop(0.15f, AttackData->AttackMontage);
				Mesh->GetAnimInstance()->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
			}
			else if (IsValid(Mesh->GetSingleNodeInstance()) &&
			         Mesh->GetSingleNodeInstance()->GetCurrentAsset() == AttackData->AttackMontage)
			{
				Mesh->Stop();
			}
		}
		bPlaying = false;
		EndCinematicAccent(false);
		bLastAttackCompleted = true;
		RecordPhaseTwoAttackOutcome(true);
		ClearGroundBurst();
		RootMotionMesh.Reset();
		LocomotionResumeWorldSeconds = GetWorld()->GetTimeSeconds() + 0.2;
		ClearWarping();
		AttackTarget.Reset();
		ClearBlink();
		TryScheduleBackstep(); // 先安排后跳再广播，防止回调立即插入下一套攻击
		OnComboFinished.Broadcast();
	}
}
