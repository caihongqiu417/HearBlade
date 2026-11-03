#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HearBladeInputComponent.generated.h"

class AHearBladeCharacter;
class UInputAction;
class UInputComponent;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;
class UPlayerRhythmCombatComponent;
class UPlayerBeatAttackComponent;
class UBossCombatCameraComponent;
struct FInputActionValue;

/** 绑定角色输入，按攻防窗口选择鼠标操作，失去控制时清理按键和临时映射 */
UCLASS(ClassGroup=(HearBlade), meta=(BlueprintSpawnableComponent))
class HEARBLADE_API UHearBladeInputComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHearBladeInputComponent();
    // 基础动作沿用角色蓝图已有配置，不改变原资源引用
    void Bind(UInputComponent* Input, UInputAction* Jump, UInputAction* Move,
              UInputAction* Look, UInputAction* MouseLook);
    // 移除绑定和输入映射，并清除持续按键状态
    void Unbind();
    // 由角色在原更新位置调用，保持格挡判断和身体更新的先后顺序
    void UpdateHeldActions();
protected:
    // 组件结束时解除输入绑定
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    friend class FHearBladeInputLifecycleTest;
    // 输入绑定与基础操作
    void CreateCombatActions();
    // 读取移动轴并交给角色
    void Move(const FInputActionValue& Value);
    // 读取视角轴并交给角色镜头
    void Look(const FInputActionValue& Value);
    void JumpStarted();
    void JumpEnded();
    // 战斗操作
    void ToggleCombatLock();
    // 将闪避输入转交角色
    void Dodge();
    // 记录格挡按下，并请求战斗组件举盾
    void BlockStarted();
    // 清除按住状态并请求结束格挡
    void BlockEnded();
    // 将反击输入交给防御组件检查
    void Counter();
    // 将轻击输入交给攻击组件检查
    void LightAttack();
    // 将重击输入交给攻击组件检查
    void HeavyAttack();
    // 请求攻击组件启动共鸣
    void ActivateResonance();
    // 根据当前攻击预警，把左键分发为反击或轻击
    void MousePrimaryCombatPressed();
    // 根据防御窗口，把右键分发为格挡或重击
    void MouseSecondaryCombatPressed();
    // 松开右键时结束由鼠标发起的格挡
    void MouseSecondaryCombatReleased();
    // 把重新开始操作交给界面控制器
    void RestartPressed();

    // 角色组件与已登记的输入
    UPROPERTY(Transient) TObjectPtr<AHearBladeCharacter> Character;
    UPROPERTY(Transient) TObjectPtr<UPlayerRhythmCombatComponent> RhythmCombat;
    UPROPERTY(Transient) TObjectPtr<UPlayerBeatAttackComponent> BeatAttack;
    UPROPERTY(Transient) TObjectPtr<UBossCombatCameraComponent> CombatCamera;
    TWeakObjectPtr<UInputComponent> BoundInput;
    TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
    // 只移除本组件登记的回调，不影响蓝图和其他组件的绑定
    TArray<uint32> BindingHandles;

    // 运行时创建的动作与映射，重新绑定时继续使用
    UPROPERTY(Transient) TObjectPtr<UInputAction> LockOnAction;
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> LockOnMapping;
    UPROPERTY(Transient) TObjectPtr<UInputAction> DodgeAction;
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> DodgeMapping;
    UPROPERTY(Transient) TObjectPtr<UInputAction> BlockAction;
    UPROPERTY(Transient) TObjectPtr<UInputAction> CounterAction;
    UPROPERTY(Transient) TObjectPtr<UInputAction> LightAttackAction;
    UPROPERTY(Transient) TObjectPtr<UInputAction> HeavyAttackAction;
    UPROPERTY(Transient) TObjectPtr<UInputAction> MousePrimaryCombatAction;
    UPROPERTY(Transient) TObjectPtr<UInputAction> MouseSecondaryCombatAction;
    UPROPERTY(Transient) TObjectPtr<UInputAction> ResonanceAction;
    UPROPERTY(Transient) TObjectPtr<UInputAction> RestartAction;
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> DefenseMapping;

    // 右键在窗口外用于重击，窗口内用于格挡，松开时只结束自己启动的格挡
    bool bMouseSecondaryOwnsBlock = false;
    bool bMouseSecondaryHeld = false;
    bool bBlockHeld = false;
};
