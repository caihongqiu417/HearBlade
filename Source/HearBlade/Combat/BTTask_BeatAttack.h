#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "Combat/BeatSyncedComboComponent.h"
#include "BTTask_BeatAttack.generated.h"

class UAttackDataAsset;
class UBeatClockSubsystem;
class APawn;

// 行为树决定何时、用哪段攻击；组件只负责将该段的命中排到下一可用拍。
UCLASS()
class HEARBLADE_API UBTTask_BeatAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_BeatAttack();

	UPROPERTY(EditAnywhere, Category="节拍攻击")
	TObjectPtr<UAttackDataAsset> AttackData;

	// 从这里随机选择一套完整连招。数组为空时使用上方 AttackData。
	UPROPERTY(EditAnywhere, Category="节拍攻击")
	TArray<TObjectPtr<UAttackDataAsset>> AttackPool;
	UPROPERTY(EditAnywhere, Category="节拍攻击|二阶段")
	TSoftObjectPtr<UAttackDataAsset> PhaseTwoAreaAttack = TSoftObjectPtr<UAttackDataAsset>(FSoftObjectPath(TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_DragonKnight_Combo03_Beat.DA_DragonKnight_Combo03_Beat")));

	UPROPERTY(EditAnywhere, Category="节拍攻击", meta=(ClampMin="0.0"))
	float ActivationRange = 1500.0f;

	// Desired spacing while circling the player. The attack can start after a short orbit.
	UPROPERTY(EditAnywhere, Category="节拍攻击|环绕", meta=(ClampMin="100.0", DisplayName="环绕半径（厘米）"))
	float ApproachStopDistance = 400.0f;

	UPROPERTY(EditAnywhere, Category="节拍攻击|环绕", meta=(ClampMin="0.0", DisplayName="靠近与退让速度"))
	float ApproachSpeed = 220.0f;

	UPROPERTY(EditAnywhere, Category="节拍攻击|环绕", meta=(ClampMin="0.0", DisplayName="环绕速度"))
	float OrbitSpeed = 160.0f;

	UPROPERTY(EditAnywhere, Category="节拍攻击|环绕", meta=(ClampMin="50.0", DisplayName="动画参考步速"))
	float AnimationReferenceSpeed = 160.0f;

	// Complete one walking animation cycle over this many music beats.
	UPROPERTY(EditAnywhere, Category="节拍攻击|漫步节奏", meta=(ClampMin="1.0", DisplayName="走路循环拍数"))
	float WalkCycleBeats = 2.0f;

	// Small continuous speed accent around each beat; 0 disables the pulse.
	UPROPERTY(EditAnywhere, Category="节拍攻击|漫步节奏", meta=(ClampMin="0.0", ClampMax="0.3", DisplayName="步速拍点重音"))
	float WalkBeatAccentStrength = 0.16f;

	// Don't launch a fresh combo immediately after the last one; show the circling first.
	UPROPERTY(EditAnywhere, Category="节拍攻击|环绕", meta=(ClampMin="0.0", DisplayName="攻击前最短环绕时间"))
	float MinimumOrbitSeconds = 0.15f;

	UPROPERTY(EditAnywhere, Category="节拍攻击|环绕", meta=(ClampMin="0.0", DisplayName="最小攻击距离"))
	float MinAttackDistance = 180.0f;

	UPROPERTY(EditAnywhere, Category="节拍攻击", meta=(ClampMin="0.0"))
	float PostAttackCooldownSeconds = 0.35f;

	UPROPERTY(EditAnywhere, Category="节拍攻击|转向", meta=(ClampMin="1.0"))
	float TurnSpeedDegreesPerSecond = 360.0f;

	UPROPERTY(EditAnywhere, Category="节拍攻击|转向", meta=(ClampMin="0.0", ClampMax="45.0"))
	float FacingToleranceDegrees = 5.0f;

	UPROPERTY(EditAnywhere, Category="节拍攻击|闪现")
	FBeatBlinkSettings Blink;

	UPROPERTY(EditAnywhere, Category="节拍攻击")
	FBlackboardKeySelector BossActorKey;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	float UpdateOrbitMovement(UBeatSyncedComboComponent* Combo, AActor* Boss, const APawn* Player,
		const UBeatClockSubsystem* Clock, float DeltaSeconds);
	bool bAttackStarted = false;
	TWeakObjectPtr<UAttackDataAsset> LastSelectedAttack;
	bool bLoggedWaitingForRange = false;
	bool bLoggedTurning = false;
	bool bOrbitClockwise = true;
	double OrbitStartWorldSeconds = 0.0;
	double LastMovementLogWorldSeconds = -1.0e9;
	double LastBlockedWorldSeconds = -1.0e9;
	double LastAttackCompletedWorldSeconds = -1.0e9;
};
