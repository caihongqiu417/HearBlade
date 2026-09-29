// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "HearBladeCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UBossCombatCameraComponent;
class UPlayerRhythmCombatComponent;
class UPerfectDodgeFeedbackComponent;
class UStaticMeshComponent;
class UBlendSpace;
class UAnimationAsset;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AHearBladeCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBossCombatCameraComponent> CombatCamera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UPlayerRhythmCombatComponent> RhythmCombat;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<class UPlayerBeatAttackComponent> BeatAttack;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UPerfectDodgeFeedbackComponent> PerfectDodgeFeedback;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> EquippedSword;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LockOnAction;
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> LockOnMapping;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

public:

	/** Constructor */
	AHearBladeCharacter();	
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	void SyncAttackWeapon() { UpdateSwordDirection(0.f); }

protected:

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);
	void ToggleCombatLock();
	void Dodge();
	void BlockStarted();
	void BlockEnded();
	void Counter();
	void LightAttack();
	void HeavyAttack();
	void ActivateResonance();

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	UFUNCTION(BlueprintCallable, Category="Combat") void DoDodge();
	// Camera-relative world direction the stick/WASD currently asks for (read even while an action ignores movement).
	FVector GetWorldMoveIntent() const;
	UPROPERTY(Transient) TObjectPtr<UInputAction> DodgeAction;
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> DodgeMapping;
	UPROPERTY(Transient) TObjectPtr<UInputAction> BlockAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> CounterAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LightAttackAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> HeavyAttackAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MousePrimaryCombatAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MouseSecondaryCombatAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ResonanceAction;
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> DefenseMapping;

private:
	bool bMouseSecondaryOwnsBlock=false;
	void MousePrimaryCombatPressed();
	void MouseSecondaryCombatReleased();
	void MouseSecondaryCombatPressed();
	FVector2D CurrentMoveInput=FVector2D::ZeroVector;
	void UpdateKnightLocomotion(float DeltaSeconds);
	void UpdateCombatFacing(float DeltaSeconds);
	void UpdateSwordDirection(float DeltaSeconds);
	void CalibrateSwordGrip();
	int32 SwordCalibrationFrames = 0;
	bool bSwordGripCalibrated = false;
	FQuat SwordGripRotationOffset = FQuat::Identity;
	UPROPERTY(EditAnywhere, Category="装备|武器", meta=(ClampMin="-20.0", ClampMax="20.0"))
	float SwordGripVerticalOffsetCm = -6.0f;
	UPROPERTY(EditAnywhere, Category="装备|武器", meta=(ClampMin="1.0", ClampMax="60.0"))
	float SwordDirectionInterpSpeed = 24.0f;
	UPROPERTY(Transient) TObjectPtr<UBlendSpace> KnightLocomotionBlendSpace;
	TWeakObjectPtr<UAnimationAsset> ActiveKnightAnimation;
	FVector2D SmoothedLocomotionInput = FVector2D::ZeroVector;
	bool bPursuitGait = false;
	UPROPERTY(EditAnywhere, Category="移动|战斗锁定", meta=(ClampMin="100.0", ClampMax="350.0"))
	float LockedCombatWalkSpeed = 200.0f;
	UPROPERTY(EditAnywhere, Category="移动|自由状态", meta=(ClampMin="300.0", ClampMax="800.0"))
	float FreeRunSpeed = 400.0f;
	// Authored ground speeds of the walk / run / fast-run rings of the locomotion
	// blend space, measured from the clips at BeginPlay (fallbacks if in-place).
	// Velocity is mapped onto these so the pose matches the speed (no foot skate).
	float WalkAnimSpeed = 170.f, RunAnimSpeed = 380.f, FastAnimSpeed = 560.f;
	void MeasureLocomotionSpeeds();
	float LocomotionRadiusForSpeed(float Speed) const;
	float LocomotionSpeedForRadius(float Radius) const;
	UPROPERTY(EditAnywhere, Category="移动|战斗锁定", meta=(ClampMin="0.25", ClampMax="0.55"))
	float LockedWalkBlendRadius = 0.38f;
	UPROPERTY(EditAnywhere, Category="移动|战斗锁定", meta=(ClampMin="1.0", ClampMax="30.0"))
	float LockedFacingInterpSpeed = 14.0f;
};

