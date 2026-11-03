// 组装主角移动、镜头与战斗组件，输入交给 PlayerInput
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "HearBladeCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UHearBladeInputComponent;
class UPlayerWeaponComponent;
class UPlayerLocomotionComponent;
class UBossCombatCameraComponent;
class UPlayerDefenseComponent;
class UPerfectDodgeFeedbackComponent;
class UStaticMeshComponent;
class UBlendSpace;
class UAnimationAsset;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 * 组装角色组件，提供移动、视角和闪避入口
 * 按键解析由输入组件处理，战斗判定由各战斗组件处理
 */
// 创建并连接输入、移动、武器和战斗组件
// 角色保留移动与镜头入口，具体玩法交给对应组件
UCLASS(abstract)

class AHearBladeCharacter : public ACharacter
{
	GENERATED_BODY()

	/** 控制镜头距离和遮挡回缩的弹簧臂 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
	          meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** 玩家跟随镜头 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
	          meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBossCombatCameraComponent> CombatCamera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPlayerDefenseComponent> DefenseComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UPlayerAttackComponent> AttackComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPerfectDodgeFeedbackComponent> PerfectDodgeFeedback;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> EquippedSword;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
    TObjectPtr<UHearBladeInputComponent> PlayerInput;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
    TObjectPtr<UPlayerWeaponComponent> Weapon;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
    TObjectPtr<UPlayerLocomotionComponent> Locomotion;

  protected:
	/** 跳跃动作 */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* JumpAction;

	/** 移动动作 */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* MoveAction;

	/** 手柄视角动作 */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* LookAction;

	/** 鼠标视角动作 */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* MouseLookAction;

  public:
	/** 创建角色组件 */
	AHearBladeCharacter();
	// 依次更新持续输入、角色朝向、行走姿态和握剑方向
	virtual void Tick(float DeltaSeconds) override;
	// 初始化武器与移动组件，建立角色表现所需引用
	virtual void BeginPlay() override;

	// 命中检测前同步剑的朝向，避免使用上一帧的姿态
	void SyncAttackWeapon()
	{
		UpdateSwordDirection(0.f);
	}

  protected:
	/** 将角色当前的输入组件交给 PlayerInput */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    // 失去控制时解除输入，避免旧按键状态残留
    virtual void UnPossessed() override;
    // 控制器同步变化后检查并清理输入状态
    virtual void OnRep_Controller() override;

  public:
	/** 接收按键或触控传入的移动方向 */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoMove(float Right, float Forward);

	/** 接收按键或触控传入的视角变化 */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** 接收触控跳跃操作 */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoJumpStart();

	/** 接收触控跳跃操作 */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoJumpEnd();

  public:
	/** 取得镜头弹簧臂 **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const
	{
		return CameraBoom;
	}

	/** 取得跟随镜头 **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const
	{
		return FollowCamera;
	}

	// 将移动意图交给战斗组件作为闪避方向
	UFUNCTION(BlueprintCallable, Category = "Combat") void DoDodge();
	// 取得按镜头朝向换算的移动意图，动作期间也保留输入方向
	FVector GetWorldMoveIntent() const;
  private:
	FVector2D CurrentMoveInput = FVector2D::ZeroVector;
	// 将移动与行走动画更新交给移动组件
	void UpdateKnightLocomotion(float DeltaSeconds);
	// 战斗锁定时按当前动作调整角色朝向
	void UpdateCombatFacing(float DeltaSeconds);
	// 将握剑方向更新交给武器组件
	void UpdateSwordDirection(float DeltaSeconds);
	void CalibrateSwordGrip();
	UPROPERTY(EditAnywhere, Category = "装备|武器", meta = (ClampMin = "-20.0", ClampMax = "20.0"))
	float SwordGripVerticalOffsetCm = -6.0f;
	UPROPERTY(EditAnywhere, Category = "装备|武器", meta = (ClampMin = "1.0", ClampMax = "60.0"))
	float SwordDirectionInterpSpeed = 24.0f;
	UPROPERTY(Transient) TObjectPtr<UBlendSpace> KnightLocomotionBlendSpace;
	// 追击继续使用 BS_PlayerEightWay 外圈的快跑动画
	UPROPERTY(Transient) TObjectPtr<UBlendSpace> PursuitBlendSpace;
	UPROPERTY(EditAnywhere, Category = "移动|战斗锁定", meta = (ClampMin = "100.0", ClampMax = "350.0"))
	float LockedCombatWalkSpeed = 200.0f;
	UPROPERTY(EditAnywhere, Category = "移动|自由状态", meta = (ClampMin = "300.0", ClampMax = "800.0"))
	float FreeRunSpeed = 400.0f;
	UPROPERTY(EditAnywhere, Category = "移动|战斗锁定", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float LockedFacingInterpSpeed = 14.0f;
};
