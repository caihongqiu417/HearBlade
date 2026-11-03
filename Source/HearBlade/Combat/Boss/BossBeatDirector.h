#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossBeatDirector.generated.h"

class UBehaviorTree;
class UBehaviorTreeComponent;
class UBlackboardComponent;

// 独立于 Boss Actor 运行行为树，并把关卡中的 Boss 放进黑板 BossActor 键
UCLASS(Blueprintable)

class HEARBLADE_API ABossBeatDirector : public AActor
{
	GENERATED_BODY()

  public:
	ABossBeatDirector();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍Boss")
	TObjectPtr<AActor> BossActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "节拍Boss")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "节拍Boss")
	TObjectPtr<UBlackboardComponent> BlackboardComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "节拍Boss")
	TObjectPtr<UBehaviorTreeComponent> BehaviorTreeComponent;

  protected:
	// 准备黑板和行为树，等场景初始化完成后再启动敌人逻辑
	virtual void BeginPlay() override;
	// 退出关卡时停止行为树
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
