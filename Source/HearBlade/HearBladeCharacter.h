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
	UPROPERTY(Transient) TObjectPtr<UInputAction> DodgeAction;
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> DodgeMapping;

private:
	void UpdateKnightLocomotion(float DeltaSeconds);
	void UpdateCombatFacing(float DeltaSeconds);
	void UpdateSwordDirection(float DeltaSeconds);
	void CalibrateSwordGrip();
	int32 SwordCalibrationFrames = 0;
	bool bSwordGripCalibrated = false;
	UPROPERTY(EditAnywhere, Category="装备|武器", meta=(ClampMin="-20.0", ClampMax="20.0"))
	float SwordGripVerticalOffsetCm = -6.0f;
	UPROPERTY(EditAnywhere, Category="装备|武器", meta=(ClampMin="1.0", ClampMax="60.0"))
	float SwordDirectionInterpSpeed = 24.0f;
	UPROPERTY(Transient) TObjectPtr<UBlendSpace> KnightLocomotionBlendSpace;
	TWeakObjectPtr<UAnimationAsset> ActiveKnightAnimation;
	FVector2D SmoothedLocomotionInput = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, Category="移动|战斗锁定", meta=(ClampMin="100.0", ClampMax="350.0"))
	float LockedCombatWalkSpeed = 200.0f;
	UPROPERTY(EditAnywhere, Category="移动|自由状态", meta=(ClampMin="300.0", ClampMax="800.0"))
	float FreeRunSpeed = 500.0f;
	UPROPERTY(EditAnywhere, Category="移动|战斗锁定", meta=(ClampMin="0.25", ClampMax="0.55"))
	float LockedWalkBlendRadius = 0.38f;
	UPROPERTY(EditAnywhere, Category="移动|战斗锁定", meta=(ClampMin="1.0", ClampMax="30.0"))
	float LockedFacingInterpSpeed = 14.0f;
};

