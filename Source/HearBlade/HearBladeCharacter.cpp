// 组装角色、相机与战斗组件，并把输入交给相应组件
// Copyright Epic Games, Inc. All Rights Reserved.

#include "HearBladeCharacter.h"
#include "Input/HearBladeInputComponent.h"
#include "Combat/Player/PlayerWeaponComponent.h"
#include "Combat/Player/PlayerLocomotionComponent.h"
#include "Combat/Player/PlayerCombatAnimInstance.h"
#include "Combat/Player/PlayerBeatAttackComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/PlayerController.h"
#include "HearBlade.h"
#include "Combat/Camera/BossCombatCameraComponent.h"
#include "Combat/Player/PlayerRhythmCombatComponent.h"
#include "Combat/Feedback/PerfectDodgeFeedbackComponent.h"

AHearBladeCharacter::AHearBladeCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
    PlayerInput = CreateDefaultSubobject<UHearBladeInputComponent>(TEXT("PlayerInput"));
    Weapon = CreateDefaultSubobject<UPlayerWeaponComponent>(TEXT("Weapon"));
    Locomotion = CreateDefaultSubobject<UPlayerLocomotionComponent>(TEXT("Locomotion"));
	// 设置角色碰撞胶囊尺寸
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// 控制器旋转先影响镜头，角色朝向由移动和战斗逻辑决定
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// 设置基础移动参数
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// 这些参数可在角色蓝图中覆盖
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// 创建镜头弹簧臂，遇到遮挡时自动缩短距离
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// 创建跟随镜头
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	CombatCamera = CreateDefaultSubobject<UBossCombatCameraComponent>(TEXT("BossCombatCamera"));
	RhythmCombat = CreateDefaultSubobject<UPlayerRhythmCombatComponent>(TEXT("RhythmCombat"));
	BeatAttack = CreateDefaultSubobject<UPlayerBeatAttackComponent>(TEXT("BeatAttack"));
	PerfectDodgeFeedback =
	    CreateDefaultSubobject<UPerfectDodgeFeedbackComponent>(TEXT("PerfectDodgeFeedback"));
	EquippedSword = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EquippedSword"));
	EquippedSword->SetupAttachment(GetMesh(), TEXT("hand_r"));
	// 剑跟随手的位置，朝向单独计算，避免翻滚结束时突然指向身后
	EquippedSword->SetAbsolute(false, true, false);
	// 剑身沿模型局部 Y 轴，握持角度在动画开始更新后校准
	EquippedSword->SetRelativeRotation(FRotator::ZeroRotator);
	EquippedSword->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EquippedSword->SetGenerateOverlapEvents(false);
	EquippedSword->SetStaticMesh(LoadObject<UStaticMesh>(
	    nullptr, TEXT("/Game/Characters/Dark_Knight/Dark_Knight_Male/Meshes/SM_DKM_Sword.SM_DKM_Sword")));

	// 优先使用角色原生骨架的移动动画，缺失时使用已有混合空间
	KnightLocomotionBlendSpace = LoadObject<UBlendSpace>(
	    nullptr,
	    TEXT(
	        "/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/BS_DKM_CombatLocomotion.BS_DKM_CombatLocomotion"));
	PursuitBlendSpace =
	    LoadObject<UBlendSpace>(nullptr, TEXT("/Game/Combat/Player/BS_PlayerEightWay.BS_PlayerEightWay"));
	if (!KnightLocomotionBlendSpace)
		KnightLocomotionBlendSpace = PursuitBlendSpace;

	// 角色网格继续由角色蓝图配置
}

// 角色取得控制权后，将引擎创建的输入组件交给输入处理组件
void AHearBladeCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    PlayerInput->Bind(PlayerInputComponent, JumpAction, MoveAction, LookAction, MouseLookAction);
}

void AHearBladeCharacter::UnPossessed()
{
    PlayerInput->Unbind();
    Super::UnPossessed();
}

void AHearBladeCharacter::OnRep_Controller()
{
    Super::OnRep_Controller();
    if (!IsLocallyControlled()) PlayerInput->Unbind();
}

void AHearBladeCharacter::DoMove(float Right, float Forward)
{
	CurrentMoveInput = FVector2D(Right, Forward);
	if (RhythmCombat && RhythmCombat->IsPerformingAction())
		return;
	const float AttackMoveScale = BeatAttack ? BeatAttack->GetMovementScale() : 1.f;
	Right *= AttackMoveScale;
	Forward *= AttackMoveScale;
	if (GetController() != nullptr)
	{
		// 只取镜头水平朝向，不把俯仰角用于地面移动
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// 取得镜头前方
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// 取得镜头右方
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// 提交移动方向
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AHearBladeCharacter::DoLook(float Yaw, float Pitch)
{
	if (RhythmCombat && RhythmCombat->IsPhaseCinematic())
		return;
	if (CombatCamera && CombatCamera->IsLockedOn())
	{
		CombatCamera->AddLookInput(Yaw, Pitch);
		return;
	}
	if (GetController() != nullptr)
	{
		// 更新控制器的水平和俯仰角
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AHearBladeCharacter::BeginPlay()
{
	Super::BeginPlay();
    Weapon->Initialize(EquippedSword);
	GetMesh()->SetAnimInstanceClass(UPlayerCombatAnimInstance::StaticClass());
	if (auto* Animation = Cast<UPlayerCombatAnimInstance>(GetMesh()->GetAnimInstance()))
		Animation->Locomotion = KnightLocomotionBlendSpace;
	// 兼容旧蓝图保存的 500 速度，将原默认速度调整为 400
	if (FMath::IsNearlyEqual(FreeRunSpeed, 500.f))
		FreeRunSpeed = 400.f;
    Locomotion->Initialize(KnightLocomotionBlendSpace, PursuitBlendSpace);
}

void AHearBladeCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
    PlayerInput->UpdateHeldActions();
    if (RhythmCombat && RhythmCombat->Health <= 0.f) return;
	CalibrateSwordGrip();
	UpdateCombatFacing(DeltaSeconds);
	UpdateSwordDirection(DeltaSeconds);
	UpdateKnightLocomotion(DeltaSeconds);
}

void AHearBladeCharacter::UpdateSwordDirection(float DeltaSeconds)
{
    Weapon->UpdateDirection(DeltaSeconds, SwordDirectionInterpSpeed);
}

void AHearBladeCharacter::UpdateCombatFacing(float DeltaSeconds)
{
	if (BeatAttack && BeatAttack->IsPlayingAttackMotion())
		return; // 攻击前摇由攻击组件控制朝向
	if (RhythmCombat && RhythmCombat->IsPhaseCinematic())
		return;
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	AActor* TargetActor = CombatCamera ? CombatCamera->GetCombatTarget() : nullptr;
	if (!Movement)
		return;
	// 受击转身由战斗组件处理，锁定朝向不能同时覆盖它
	if (RhythmCombat && RhythmCombat->IsHitReacting())
	{
		Movement->bOrientRotationToMovement = false;
		return;
	}

	if (!TargetActor)
	{
		Movement->bOrientRotationToMovement = true;
		return;
	}

	Movement->bOrientRotationToMovement = false;
	FVector ToTarget = TargetActor->GetActorLocation() - GetActorLocation();
	ToTarget.Z = 0.0f;
	if (ToTarget.IsNearlyZero())
		return;

	const FRotator DesiredRotation(0.0f, ToTarget.Rotation().Yaw, 0.0f);
	const FRotator FacingRotation =
	    FMath::RInterpTo(GetActorRotation(), DesiredRotation, DeltaSeconds, LockedFacingInterpSpeed);
	SetActorRotation(FacingRotation);
}

// 保持原来的校准时机，参数继续从角色蓝图读取
void AHearBladeCharacter::CalibrateSwordGrip()
{
    Weapon->Calibrate(SwordGripVerticalOffsetCm);
}

void AHearBladeCharacter::UpdateKnightLocomotion(float DeltaSeconds)
{
    Locomotion->Update(DeltaSeconds, LockedCombatWalkSpeed, FreeRunSpeed);
}

FVector AHearBladeCharacter::GetWorldMoveIntent() const
{
	FVector2D Input = CurrentMoveInput;
	if (auto* PC = Cast<APlayerController>(GetController()))
		if (auto* Enhanced = Cast<UEnhancedPlayerInput>(PC->PlayerInput); Enhanced && MoveAction)
			Input = Enhanced->GetActionValue(MoveAction).Get<FVector2D>();
	const FRotator Yaw(
	    0.f, GetController() ? GetController()->GetControlRotation().Yaw : GetActorRotation().Yaw, 0.f);
	return (FRotationMatrix(Yaw).GetUnitAxis(EAxis::X) * Input.Y +
	        FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y) * Input.X)
	    .GetClampedToMaxSize(1.f);
}

void AHearBladeCharacter::DoDodge()
{
	if (!RhythmCombat)
		return;
	FVector2D Input = CurrentMoveInput;
	if (auto* PC = Cast<APlayerController>(GetController()))
		if (auto* Enhanced = Cast<UEnhancedPlayerInput>(PC->PlayerInput); Enhanced && MoveAction)
			Input = Enhanced->GetActionValue(MoveAction).Get<FVector2D>();
	const FRotator Yaw(
	    0.f, GetController() ? GetController()->GetControlRotation().Yaw : GetActorRotation().Yaw, 0.f);
	FVector Direction = (FRotationMatrix(Yaw).GetUnitAxis(EAxis::X) * Input.Y +
	                     FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y) * Input.X)
	                        .GetSafeNormal2D();
	if (Direction.IsNearlyZero())
		Direction = GetActorForwardVector();
	RhythmCombat->TryDodge(Direction);
}

void AHearBladeCharacter::DoJumpStart()
{
	if (RhythmCombat && RhythmCombat->IsPhaseCinematic())
		return;
	// 开始跳跃
	Jump();
}

void AHearBladeCharacter::DoJumpEnd()
{
	// 结束跳跃按住状态
	StopJumping();
}
