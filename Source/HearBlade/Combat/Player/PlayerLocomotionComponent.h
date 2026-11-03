#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerLocomotionComponent.generated.h"

class ACharacter;
class UBlendSpace;
class UBossCombatCameraComponent;
class UPlayerRhythmCombatComponent;
class UPlayerBeatAttackComponent;

/** 根据实际速度选择移动动画，处理锁定步行与追击切换，不修改攻击动画和判定时间 */
UCLASS(ClassGroup=(HearBlade), meta=(BlueprintSpawnableComponent))
class HEARBLADE_API UPlayerLocomotionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPlayerLocomotionComponent();
    // 缓存角色、移动组件和行走资源，并读取动画参考速度
    void Initialize(UBlendSpace* Locomotion, UBlendSpace* Pursuit);
    // 结合移动方向和锁定状态，协调角色速度与八方向行走动画
    void Update(float DeltaSeconds, float LockedCombatWalkSpeed, float FreeRunSpeed);
private:
    friend class FHearBladeLocomotionTest;
    // 从动画测量步速，再换算混合空间的采样位置
    void MeasureSpeeds();
    float RadiusForSpeed(float Speed) const;
    // 把混合空间半径转换为对应的参考移动速度
    float SpeedForRadius(float Radius) const;

    // 使用的角色、动画资源和战斗状态
    UPROPERTY(Transient) TObjectPtr<ACharacter> Character;
    UPROPERTY(Transient) TObjectPtr<UBlendSpace> KnightLocomotionBlendSpace;
    UPROPERTY(Transient) TObjectPtr<UBlendSpace> PursuitBlendSpace;
    UPROPERTY(Transient) TObjectPtr<UBossCombatCameraComponent> CombatCamera;
    UPROPERTY(Transient) TObjectPtr<UPlayerRhythmCombatComponent> RhythmCombat;
    UPROPERTY(Transient) TObjectPtr<UPlayerBeatAttackComponent> BeatAttack;
    // 平滑后的移动方向与追击状态
    FVector2D SmoothedLocomotionInput = FVector2D::ZeroVector;
    bool bPursuitGait = false;
    FVector2D PursuitBlendPosition = FVector2D(1.3f, 0.f);
    float PursuitAnimSpeed = 560.f;
    // 原地动画无法测量根节点速度时，继续使用原来的默认值
    float WalkAnimSpeed = 170.f;
    float RunAnimSpeed = 380.f;
    float FastAnimSpeed = 560.f;
    // 混合空间中步行、跑步和快跑采样点到原点的距离
    float WalkRadius = .4f;
    float RunRadius = 1.f;
    float FastRadius = 1.3f;
};
