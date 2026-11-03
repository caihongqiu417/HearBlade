#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerWeaponComponent.generated.h"

class ACharacter;
class UStaticMeshComponent;
class UPlayerRhythmCombatComponent;
class UPlayerBeatAttackComponent;

/** 校准握剑角度，按攻击、防御和移动状态更新剑的朝向，不处理伤害判定 */
UCLASS(ClassGroup=(HearBlade), meta=(BlueprintSpawnableComponent))
class HEARBLADE_API UPlayerWeaponComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPlayerWeaponComponent();
    // 保存武器和战斗组件引用，重置握剑校准状态
    void Initialize(UStaticMeshComponent* Sword);
    // 等待骨骼更新后对齐刀刃，并保存剑相对右手的旋转
    void Calibrate(float VerticalOffsetCm);
    // 攻击防御时跟随手部姿态，移动时平滑朝向角色前方
    void UpdateDirection(float DeltaSeconds, float InterpSpeed);
private:
    UPROPERTY(Transient) TObjectPtr<ACharacter> Character;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> EquippedSword;
    UPROPERTY(Transient) TObjectPtr<UPlayerRhythmCombatComponent> RhythmCombat;
    UPROPERTY(Transient) TObjectPtr<UPlayerBeatAttackComponent> BeatAttack;
    // 等骨骼动画更新后再校准，记录剑相对右手的旋转
    int32 SwordCalibrationFrames = 0;
    bool bSwordGripCalibrated = false;
    FQuat SwordGripRotationOffset = FQuat::Identity;
};
