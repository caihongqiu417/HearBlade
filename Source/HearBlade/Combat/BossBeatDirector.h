#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossBeatDirector.generated.h"

class UBehaviorTree;
class UBehaviorTreeComponent;
class UBlackboardComponent;

// 独立于 Boss Actor 运行行为树，并把关卡中的 Boss 放进黑板 BossActor 键。
UCLASS(Blueprintable)
class HEARBLADE_API ABossBeatDirector : public AActor
{
	GENERATED_BODY()

public:
	ABossBeatDirector();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="节拍Boss")
	TObjectPtr<AActor> BossActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="节拍Boss")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="节拍Boss")
	TObjectPtr<UBlackboardComponent> BlackboardComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="节拍Boss")
	TObjectPtr<UBehaviorTreeComponent> BehaviorTreeComponent;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
