// Copyright Epic Games, Inc. All Rights Reserved.

#include "HearBladeCharacter.h"
#include "Combat/PlayerCombatAnimInstance.h"
#include "Combat/PlayerBeatAttackComponent.h"
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
#include "EnhancedPlayerInput.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "HearBlade.h"
#include "Combat/BossCombatCameraComponent.h"
#include "Combat/PlayerRhythmCombatComponent.h"
#include "Combat/PerfectDodgeFeedbackComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

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
	BeatAttack = CreateDefaultSubobject<UPlayerBeatAttackComponent>(TEXT("BeatAttack"));
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
		TEXT("/Game/Combat/Player/BS_PlayerEightWay.BS_PlayerEightWay"));

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
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &AHearBladeCharacter::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &AHearBladeCharacter::Move);
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
		LightAttackAction = NewObject<UInputAction>(this, TEXT("LightAttackAction"));
		HeavyAttackAction = NewObject<UInputAction>(this, TEXT("HeavyAttackAction"));
		ResonanceAction = NewObject<UInputAction>(this, TEXT("ResonanceAction"));
		LightAttackAction->ValueType = EInputActionValueType::Boolean;
		HeavyAttackAction->ValueType = EInputActionValueType::Boolean;
		ResonanceAction->ValueType = EInputActionValueType::Boolean;
		MousePrimaryCombatAction=NewObject<UInputAction>(this,TEXT("MousePrimaryCombat"));
		MouseSecondaryCombatAction=NewObject<UInputAction>(this,TEXT("MouseSecondaryCombat"));
		MousePrimaryCombatAction->ValueType=EInputActionValueType::Boolean;
		MouseSecondaryCombatAction->ValueType=EInputActionValueType::Boolean;
		DefenseMapping->MapKey(MousePrimaryCombatAction,EKeys::LeftMouseButton);
		DefenseMapping->MapKey(MouseSecondaryCombatAction,EKeys::RightMouseButton);
		EnhancedInputComponent->BindAction(MousePrimaryCombatAction,ETriggerEvent::Started,this,&AHearBladeCharacter::MousePrimaryCombatPressed);
		EnhancedInputComponent->BindAction(MouseSecondaryCombatAction,ETriggerEvent::Completed,this,&AHearBladeCharacter::MouseSecondaryCombatReleased);
		EnhancedInputComponent->BindAction(MouseSecondaryCombatAction,ETriggerEvent::Canceled,this,&AHearBladeCharacter::MouseSecondaryCombatReleased);
		EnhancedInputComponent->BindAction(MouseSecondaryCombatAction,ETriggerEvent::Started,this,&AHearBladeCharacter::MouseSecondaryCombatPressed);
		DefenseMapping->MapKey(LightAttackAction, RhythmKeys->LightAttackGamepad);
		DefenseMapping->MapKey(HeavyAttackAction, EKeys::F);
		DefenseMapping->MapKey(HeavyAttackAction, RhythmKeys->HeavyAttackGamepad);
		DefenseMapping->MapKey(ResonanceAction, RhythmKeys->ResonanceKey);
		DefenseMapping->MapKey(ResonanceAction, RhythmKeys->ResonanceGamepad);
		EnhancedInputComponent->BindAction(LightAttackAction, ETriggerEvent::Started, this, &AHearBladeCharacter::LightAttack);
		EnhancedInputComponent->BindAction(HeavyAttackAction, ETriggerEvent::Started, this, &AHearBladeCharacter::HeavyAttack);
		EnhancedInputComponent->BindAction(ResonanceAction, ETriggerEvent::Started, this, &AHearBladeCharacter::ActivateResonance);
		DefenseMapping->MapKey(BlockAction, RhythmKeys->BlockGamepad);
		if(!RhythmKeys->CounterPrimary.IsMouseButton()) DefenseMapping->MapKey(CounterAction, RhythmKeys->CounterPrimary);
		if(RhythmKeys->CounterSecondary!=RhythmKeys->CounterPrimary && !RhythmKeys->CounterSecondary.IsMouseButton())
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
	CurrentMoveInput=FVector2D(Right,Forward);
	if (RhythmCombat && RhythmCombat->IsPerformingAction()) return;
	const float AttackMoveScale = BeatAttack ? BeatAttack->GetMovementScale() : 1.f;
	Right *= AttackMoveScale; Forward *= AttackMoveScale;
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
	if(RhythmCombat && RhythmCombat->IsPhaseCinematic()) return;
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
	// Older placed/Blueprint instances serialized the previous (too fast) default.
	if (FMath::IsNearlyEqual(FreeRunSpeed, 500.f)) FreeRunSpeed = 400.f;
	MeasureLocomotionSpeeds();
}

void AHearBladeCharacter::MeasureLocomotionSpeeds()
{
	if (!KnightLocomotionBlendSpace) return;
	float Sum[3] = {0.f, 0.f, 0.f}; int32 Count[3] = {0, 0, 0};
	for (const FBlendSample& Sample : KnightLocomotionBlendSpace->GetBlendSamples())
	{
		const float Radius = FVector2D(Sample.SampleValue.X, Sample.SampleValue.Y).Size();
		const UAnimSequence* Clip = Cast<UAnimSequence>(Sample.Animation);
		if (Radius < .1f || !Clip || Clip->GetPlayLength() <= .05f) continue;
		const FTransform A = Clip->ExtractRootTrackTransform(FAnimExtractContext(0.0), nullptr);
		const FTransform B = Clip->ExtractRootTrackTransform(FAnimExtractContext(double(Clip->GetPlayLength())), nullptr);
		const float Speed = FVector::Dist2D(A.GetLocation(), B.GetLocation()) / Clip->GetPlayLength();
		if (Speed < 20.f) continue; // in-place clip: nothing to measure
		const int32 Ring = Radius < .7f ? 0 : Radius < 1.15f ? 1 : 2;
		Sum[Ring] += Speed; ++Count[Ring];
	}
	if (Count[0]) WalkAnimSpeed = Sum[0] / Count[0];
	if (Count[1]) RunAnimSpeed = Sum[1] / Count[1];
	if (Count[2]) FastAnimSpeed = Sum[2] / Count[2];
	RunAnimSpeed = FMath::Max(RunAnimSpeed, WalkAnimSpeed + 50.f);
	FastAnimSpeed = FMath::Max(FastAnimSpeed, RunAnimSpeed + 50.f);
	UE_LOG(LogHearBlade, Display, TEXT("LocomotionSpeeds: walk=%.0f run=%.0f fast=%.0f (measured %d/%d/%d)"),
		WalkAnimSpeed, RunAnimSpeed, FastAnimSpeed, Count[0], Count[1], Count[2]);
}

// Blend space rings: 0.4 = walk, 1.0 = run, 1.3 = fast run.
float AHearBladeCharacter::LocomotionRadiusForSpeed(float Speed) const
{
	if (Speed <= WalkAnimSpeed) return .4f * Speed / FMath::Max(WalkAnimSpeed, 1.f);
	if (Speed <= RunAnimSpeed) return .4f + .6f * (Speed - WalkAnimSpeed) / (RunAnimSpeed - WalkAnimSpeed);
	return FMath::Min(1.3f, 1.f + .3f * (Speed - RunAnimSpeed) / (FastAnimSpeed - RunAnimSpeed));
}

float AHearBladeCharacter::LocomotionSpeedForRadius(float Radius) const
{
	if (Radius <= .4f) return WalkAnimSpeed * Radius / .4f;
	if (Radius <= 1.f) return FMath::Lerp(WalkAnimSpeed, RunAnimSpeed, (Radius - .4f) / .6f);
	return FMath::Lerp(RunAnimSpeed, FastAnimSpeed, (FMath::Min(Radius, 1.3f) - 1.f) / .3f);
}

void AHearBladeCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (RhythmCombat && RhythmCombat->Health <= 0.f)
	{
		if (auto* PC=Cast<APlayerController>(GetController()); PC && PC->WasInputKeyJustPressed(EKeys::Enter))
			UGameplayStatics::OpenLevel(this,FName(*UGameplayStatics::GetCurrentLevelName(this,true)));
		return;
	}
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
	if (((RhythmCombat && RhythmCombat->IsInDefensePose()) || (BeatAttack && BeatAttack->IsPlayingAttackMotion())) && GetMesh())
	{
		// Defense animations author the wrist and blade pose together. Reapply the calibrated
		// grip offset in hand space so block, hold and counter can actually raise the sword.
		const FQuat HandRotation = GetMesh()->GetSocketTransform(TEXT("hand_r"), RTS_World).GetRotation();
		TargetRotation = (HandRotation * SwordGripRotationOffset).GetNormalized();
		InterpSpeed *= 1.65f;
		if (BeatAttack && BeatAttack->IsPlayingAttackMotion()) { EquippedSword->SetWorldRotation(TargetRotation); return; }
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
	if(BeatAttack && BeatAttack->IsPlayingAttackMotion()) return; // Ordinary attack lead-in owns its facing.
	if(RhythmCombat && RhythmCombat->IsPhaseCinematic()) return;
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
	const AActor* Target=CombatCamera?CombatCamera->GetCombatTarget():nullptr;
	const bool bBusy=(RhythmCombat && RhythmCombat->IsPerformingAction()) || (BeatAttack && BeatAttack->IsPlayingAttackMotion());
	const FVector ToTarget=Target?Target->GetActorLocation()-GetActorLocation():FVector::ZeroVector;
	const bool bForwardIntent=GetLastMovementInputVector().SizeSquared()>.04f &&
		FVector::DotProduct(GetLastMovementInputVector().GetSafeNormal2D(),ToTarget.GetSafeNormal2D())>.85f;
	// Separate enter/exit distances prevent gait flicker around the boundary.
	bPursuitGait=bCombatLocked && Target && !bBusy && bForwardIntent && ToTarget.Size2D()>(bPursuitGait?300.f:450.f);
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		Movement->MaxWalkSpeed = bPursuitGait ? FreeRunSpeed*1.3f : bCombatLocked ? FMath::Max(280.f,LockedCombatWalkSpeed) : FreeRunSpeed;

	// Actual local velocity selects eight walk/run directions; the 1.3 outer
	// ring supplies forward pursuit. Never clamp lock-on to walking poses.
	// Map speed onto the ring whose authored stride matches it, instead of
	// speed/FreeRunSpeed (which played a run cycle at walking speed and made
	// the feet skate / float).
	const FVector LocalVelocity = GetActorTransform().InverseTransformVectorNoScale(GetVelocity());
	const float Speed2D = FVector2D(LocalVelocity.X, LocalVelocity.Y).Size();
	FVector2D TargetInput = FVector2D(LocalVelocity.X, LocalVelocity.Y).GetSafeNormal() * LocomotionRadiusForSpeed(Speed2D);
	if (GetVelocity().SizeSquared2D() < 9.0f) TargetInput = FVector2D::ZeroVector;
	SmoothedLocomotionInput = FMath::Vector2DInterpTo(
		SmoothedLocomotionInput, TargetInput, DeltaSeconds, 9.0f);

	if (auto* Instance = Cast<UPlayerCombatAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		Instance->Locomotion = KnightLocomotionBlendSpace;
		Instance->LocomotionPosition = FVector(SmoothedLocomotionInput.X, SmoothedLocomotionInput.Y, 0.0f);
		// While the blend position catches up, scale the cycle so the stride
		// covers exactly the ground the capsule covers.
		const float PoseSpeed = LocomotionSpeedForRadius(SmoothedLocomotionInput.Size());
		Instance->SpeedMatchPlayRate = (Speed2D > 30.f && PoseSpeed > 30.f) ? FMath::Clamp(Speed2D / PoseSpeed, .75f, 1.3f) : 1.f;
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

void AHearBladeCharacter::LightAttack() { if (BeatAttack) BeatAttack->TryAttack(EPlayerAttackInput::Light); }
void AHearBladeCharacter::MousePrimaryCombatPressed()
{
	const bool Defense=RhythmCombat && RhythmCombat->IsMouseDefenseWindow();
	UE_LOG(LogTemp,Display,TEXT("MouseCombat: LMB -> %s"),Defense?TEXT("COUNTER"):TEXT("LIGHT"));
	if(Defense) Counter(); else LightAttack();
}
void AHearBladeCharacter::MouseSecondaryCombatReleased()
{
	// Release follows the role chosen at press; a changing threat window must
	// never turn a light-attack release into a block-end montage.
	if(bMouseSecondaryOwnsBlock && RhythmCombat) RhythmCombat->EndBlock();
	bMouseSecondaryOwnsBlock=false;
}
void AHearBladeCharacter::MouseSecondaryCombatPressed()
{
	const bool Defense=RhythmCombat && RhythmCombat->IsMouseDefenseWindow();
	UE_LOG(LogTemp,Display,TEXT("MouseCombat: RMB -> %s"),Defense?TEXT("BLOCK"):TEXT("HEAVY"));
	bMouseSecondaryOwnsBlock=false;
	if(Defense) bMouseSecondaryOwnsBlock=RhythmCombat->TryBlockStart(); else HeavyAttack();
}
void AHearBladeCharacter::HeavyAttack() { if (BeatAttack) BeatAttack->TryAttack(EPlayerAttackInput::Heavy); }
void AHearBladeCharacter::ActivateResonance() { if (BeatAttack) BeatAttack->TryActivateResonance(); }

FVector AHearBladeCharacter::GetWorldMoveIntent() const
{
	FVector2D Input=CurrentMoveInput;
	if(auto* PC=Cast<APlayerController>(GetController()))
		if(auto* Enhanced=Cast<UEnhancedPlayerInput>(PC->PlayerInput);Enhanced&&MoveAction)
			Input=Enhanced->GetActionValue(MoveAction).Get<FVector2D>();
	const FRotator Yaw(0.f,GetController()?GetController()->GetControlRotation().Yaw:GetActorRotation().Yaw,0.f);
	return (FRotationMatrix(Yaw).GetUnitAxis(EAxis::X)*Input.Y+FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y)*Input.X).GetClampedToMaxSize(1.f);
}

void AHearBladeCharacter::DoDodge()
{
	if (!RhythmCombat) return;
	FVector2D Input=CurrentMoveInput;
	if(auto* PC=Cast<APlayerController>(GetController()))
		if(auto* Enhanced=Cast<UEnhancedPlayerInput>(PC->PlayerInput);Enhanced&&MoveAction)
			Input=Enhanced->GetActionValue(MoveAction).Get<FVector2D>();
	const FRotator Yaw(0.f,GetController()?GetController()->GetControlRotation().Yaw:GetActorRotation().Yaw,0.f);
	FVector Direction=(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X)*Input.Y+FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y)*Input.X).GetSafeNormal2D();
	if(Direction.IsNearlyZero()) Direction=GetActorForwardVector();
	RhythmCombat->TryDodge(Direction);
}

void AHearBladeCharacter::DoJumpStart()
{
	if(RhythmCombat && RhythmCombat->IsPhaseCinematic()) return;
	// signal the character to jump
	Jump();
}

void AHearBladeCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}
