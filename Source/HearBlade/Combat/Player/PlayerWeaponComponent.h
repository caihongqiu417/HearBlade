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
    void Initialize(UStaticMeshComponent* Sword);
    void Calibrate(float VerticalOffsetCm);
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
