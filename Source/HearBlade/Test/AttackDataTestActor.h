// 提供攻击数据运行测试的场景对象

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AttackDataTestActor.generated.h"

UCLASS()

class HEARBLADE_API AAttackDataTestActor : public AActor
{
	GENERATED_BODY()

  public:
	// 初始化测试对象的默认设置
	AAttackDataTestActor();

  protected:
	// 测试对象进入关卡时调用
	virtual void BeginPlay() override;

  public:
	// 测试对象每帧更新
	virtual void Tick(float DeltaTime) override;
};
