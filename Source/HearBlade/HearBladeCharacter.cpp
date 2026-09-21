// Copyright Epic Games, Inc. All Rights Reserved.

#include "HearBladeCharacter.h"
#include "Combat/PlayerCombatAnimInstance.h"
#include "Combat/RhythmCombatInputSettings.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "HearBlade.h"
#include "Combat/BossCombatCameraComponent.h"
#include "Combat/PlayerRhythmCombatComponent.h"
#include "Combat/PerfectDodgeFeedbackComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"

AHearBladeCharacter::AHearBladeCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	CombatCamera = CreateDefaultSubobject<UBossCombatCameraComponent>(TEXT("BossCombatCamera"));
	RhythmCombat = CreateDefaultSubobject<UPlayerRhythmCombatComponent>(TEXT("RhythmCombat"));
	PerfectDodgeFeedback = CreateDefaultSubobject<UPerfectDodgeFeedbackComponent>(TEXT("PerfectDodgeFeedback"));
	EquippedSword = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EquippedSword"));
	EquippedSword->SetupAttachment(GetMesh(), TEXT("hand_r"));
	// Follow the animated hand position, but do not inherit its rotation. Manny's
	// roll hand pose points backward and otherwise makes the sword pop on recovery.
	EquippedSword->SetAbsolute(false, true, false);
	// SM_DKM_Sword extends along local +Y. The final grip correction is calculated
	// from the evaluated hand pose during the first gameplay ticks.
	EquippedSword->SetRelativeRotation(FRotator::ZeroRotator);
	EquippedSword->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EquippedSword->SetGenerateOverlapEvents(false);
	EquippedSword->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/Characters/Dark_Knight/Dark_Knight_Male/Meshes/SM_DKM_Sword.SM_DKM_Sword")));

	KnightLocomotionBlendSpace = LoadObject<UBlendSpace>(nullptr,
		TEXT("/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/BS_DKM_CombatLocomotion.BS_DKM_CombatLocomotion"));

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AHearBladeCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHearBladeCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AHearBladeCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHearBladeCharacter::Look);
		LockOnAction = NewObject<UInputAction>(this, TEXT("BossLockOnAction"));
		LockOnAction->ValueType = EInputActionValueType::Boolean;
		LockOnMapping = NewObject<UInputMappingContext>(this, TEXT("BossLockOnMapping"));
		LockOnMapping->MapKey(LockOnAction, EKeys::MiddleMouseButton);
		LockOnMapping->MapKey(LockOnAction, EKeys::Gamepad_RightThumbstick);
		EnhancedInputComponent->BindAction(LockOnAction, ETriggerEvent::Started, this, &AHearBladeCharacter::ToggleCombatLock);
		DodgeAction = NewObject<UInputAction>(this, TEXT("RhythmDodgeAction"));
		DodgeAction->ValueType = EInputActionValueType::Boolean;
		DodgeMapping = NewObject<UInputMappingContext>(this, TEXT("RhythmDodgeMapping"));
		const URhythmCombatInputSettings* RhythmKeys = GetDefault<URhythmCombatInputSettings>();
		DodgeMapping->MapKey(DodgeAction, RhythmKeys->DodgePrimary);
		DodgeMapping->MapKey(DodgeAction, RhythmKeys->DodgeSecondary);
		DodgeMapping->MapKey(DodgeAction, RhythmKeys->DodgeGamepad);
		EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Started, this, &AHearBladeCharacter::Dodge);
		BlockAction = NewObject<UInputAction>(this, TEXT("RhythmBlockAction"));
		BlockAction->ValueType = EInputActionValueType::Boolean;
		CounterAction = NewObject<UInputAction>(this, TEXT("RhythmCounterAction"));
		CounterAction->ValueType = EInputActionValueType::Boolean;
		DefenseMapping = NewObject<UInputMappingContext>(this, TEXT("RhythmDefenseMapping"));
		DefenseMapping->MapKey(BlockAction, RhythmKeys->BlockKeyboardMouse);
		DefenseMapping->MapKey(BlockAction, RhythmKeys->BlockGamepad);
		DefenseMapping->MapKey(CounterAction, RhythmKeys->CounterPrimary);
		DefenseMapping->MapKey(CounterAction, RhythmKeys->CounterSecondary);
		DefenseMapping->MapKey(CounterAction, RhythmKeys->CounterGamepad);
		EnhancedInputComponent->BindAction(BlockAction, ETriggerEvent::Started, this, &AHearBladeCharacter::BlockStarted);
		EnhancedInputComponent->BindAction(BlockAction, ETriggerEvent::Completed, this, &AHearBladeCharacter::BlockEnded);
		EnhancedInputComponent->BindAction(BlockAction, ETriggerEvent::Canceled, this, &AHearBladeCharacter::BlockEnded);
		EnhancedInputComponent->BindAction(CounterAction, ETriggerEvent::Started, this, &AHearBladeCharacter::Counter);
		if (const APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (UEnhancedInputLocalPlayerSubsystem* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			{
				Input->AddMappingContext(LockOnMapping, 1);
				Input->AddMappingContext(DodgeMapping, 2);
				Input->AddMappingContext(DefenseMapping, 2);
			}
		}
	}
	else
	{
		UE_LOG(LogHearBlade, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AHearBladeCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AHearBladeCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AHearBladeCharacter::DoMove(float Right, float Forward)
{
	if (RhythmCombat && RhythmCombat->IsPerformingAction()) return;
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AHearBladeCharacter::DoLook(float Yaw, float Pitch)
{
	if (CombatCamera && CombatCamera->IsLockedOn())
	{
		CombatCamera->AddLookInput(Yaw, Pitch);
		return;
	}
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AHearBladeCharacter::BeginPlay()
{
	Super::BeginPlay();
	GetMesh()->SetAnimInstanceClass(UPlayerCombatAnimInstance::StaticClass());
	if (auto* Animation = Cast<UPlayerCombatAnimInstance>(GetMesh()->GetAnimInstance()))
		Animation->Locomotion = KnightLocomotionBlendSpace;
}

void AHearBladeCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	CalibrateSwordGrip();
	UpdateCombatFacing(DeltaSeconds);
	UpdateSwordDirection(DeltaSeconds);
	UpdateKnightLocomotion(DeltaSeconds);
}

void AHearBladeCharacter::UpdateSwordDirection(float DeltaSeconds)
{
	if (!bSwordGripCalibrated || !EquippedSword) return;

	const FQuat CurrentRotation = EquippedSword->GetComponentQuat();
	FQuat TargetRotation = CurrentRotation;
	float InterpSpeed = SwordDirectionInterpSpeed;
	if (RhythmCombat && RhythmCombat->IsInDefensePose() && GetMesh())
	{
		// Defense animations author the wrist and blade pose together. Reapply the calibrated
		// grip offset in hand space so block, hold and counter can actually raise the sword.
		const FQuat HandRotation = GetMesh()->GetSocketTransform(TEXT("hand_r"), RTS_World).GetRotation();
		TargetRotation = (HandRotation * SwordGripRotationOffset).GetNormalized();
		InterpSpeed *= 1.65f;
	}
	else
	{
		// Locomotion and roll clips disagree about wrist orientation. Keep the blade stable and
		// forward there, which prevents the old backward-sword recovery snap.
		const FVector CurrentDirection = EquippedSword->GetComponentTransform()
			.TransformVectorNoScale(FVector::YAxisVector).GetSafeNormal();
		const FVector DesiredDirection = GetActorForwardVector().GetSafeNormal();
		if (CurrentDirection.IsNearlyZero() || DesiredDirection.IsNearlyZero()) return;
		TargetRotation = (FQuat::FindBetweenNormals(CurrentDirection, DesiredDirection) * CurrentRotation).GetNormalized();
	}
	const float Alpha = 1.0f - FMath::Exp(-InterpSpeed * DeltaSeconds);
	EquippedSword->SetWorldRotation(FQuat::Slerp(CurrentRotation, TargetRotation, Alpha).GetNormalized());
}

void AHearBladeCharacter::UpdateCombatFacing(float DeltaSeconds)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	AActor* TargetActor = CombatCamera ? CombatCamera->GetCombatTarget() : nullptr;
	if (!Movement) return;
	// The hit component turns toward the actual blade source before playing its
	// backward reaction; lock-on facing must not rotate against that short turn.
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
	if (ToTarget.IsNearlyZero()) return;

	const FRotator DesiredRotation(0.0f, ToTarget.Rotation().Yaw, 0.0f);
	const FRotator FacingRotation = FMath::RInterpTo(
		GetActorRotation(), DesiredRotation, DeltaSeconds, LockedFacingInterpSpeed);
	SetActorRotation(FacingRotation);
}

void AHearBladeCharacter::CalibrateSwordGrip()
{
	if (bSwordGripCalibrated || !EquippedSword || ++SwordCalibrationFrames < 3) return;

	const FVector CurrentBladeDirection = EquippedSword->GetComponentTransform()
		.TransformVectorNoScale(FVector::YAxisVector).GetSafeNormal();
	const FVector DesiredBladeDirection = GetActorForwardVector().GetSafeNormal();
	if (CurrentBladeDirection.IsNearlyZero() || DesiredBladeDirection.IsNearlyZero()) return;

	const FQuat WorldCorrection = FQuat::FindBetweenNormals(CurrentBladeDirection, DesiredBladeDirection);
	EquippedSword->SetWorldRotation(WorldCorrection * EquippedSword->GetComponentQuat());
	EquippedSword->SetWorldLocation(EquippedSword->GetComponentLocation() +
		FVector::UpVector * SwordGripVerticalOffsetCm);
	const FQuat HandRotation = GetMesh()->GetSocketTransform(TEXT("hand_r"), RTS_World).GetRotation();
	SwordGripRotationOffset = (HandRotation.Inverse() * EquippedSword->GetComponentQuat()).GetNormalized();
	bSwordGripCalibrated = true;

	const FVector CorrectedDirection = EquippedSword->GetComponentTransform()
		.TransformVectorNoScale(FVector::YAxisVector).GetSafeNormal();
	UE_LOG(LogTemp, Display, TEXT("SwordGrip: forward alignment dot=%.4f"),
		FVector::DotProduct(CorrectedDirection, DesiredBladeDirection));
}

void AHearBladeCharacter::UpdateKnightLocomotion(float DeltaSeconds)
{
	if (!GetMesh() || !KnightLocomotionBlendSpace) return;

	const bool bCombatLocked = CombatCamera && CombatCamera->IsLockedOn();
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		Movement->MaxWalkSpeed = bCombatLocked ? LockedCombatWalkSpeed : FreeRunSpeed;

	// The outer Blend Space ring represents the free run speed. Locked combat
	// movement is physically slower and is also clamped to the inner walk ring.
	const float MaxMoveSpeed = FMath::Max(FreeRunSpeed, 1.0f);
	const FVector LocalVelocity = GetActorTransform().InverseTransformVectorNoScale(GetVelocity());
	FVector2D TargetInput(LocalVelocity.X / MaxMoveSpeed, LocalVelocity.Y / MaxMoveSpeed);
	TargetInput = TargetInput.GetClampedToMaxSize(bCombatLocked ? LockedWalkBlendRadius : 1.0f);
	if (GetVelocity().SizeSquared2D() < 9.0f) TargetInput = FVector2D::ZeroVector;
	SmoothedLocomotionInput = FMath::Vector2DInterpTo(
		SmoothedLocomotionInput, TargetInput, DeltaSeconds, 9.0f);

	if (auto* Instance = Cast<UPlayerCombatAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		Instance->Locomotion = KnightLocomotionBlendSpace;
		Instance->LocomotionPosition = FVector(SmoothedLocomotionInput.X, SmoothedLocomotionInput.Y, 0.0f);
	}
}

void AHearBladeCharacter::ToggleCombatLock()
{
	if (CombatCamera) CombatCamera->ToggleLockOn();
}

void AHearBladeCharacter::Dodge()
{
	DoDodge();
}

void AHearBladeCharacter::BlockStarted()
{
	if (RhythmCombat) RhythmCombat->TryBlockStart();
}

void AHearBladeCharacter::BlockEnded()
{
	if (RhythmCombat) RhythmCombat->EndBlock();
}

void AHearBladeCharacter::Counter()
{
	if (RhythmCombat) RhythmCombat->TryCounter();
}

void AHearBladeCharacter::DoDodge()
{
	if (!RhythmCombat) return;
	FVector Direction = GetLastMovementInputVector();
	if (Direction.IsNearlyZero()) Direction = GetActorForwardVector();
	RhythmCombat->TryDodge(Direction);
}

void AHearBladeCharacter::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

void AHearBladeCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}
