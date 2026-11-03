// 提供攻击数据运行测试的场景对象

#include "Test/AttackDataTestActor.h"

// 初始化测试对象
AAttackDataTestActor::AAttackDataTestActor()
{
	// 开启测试对象的逐帧更新
	PrimaryActorTick.bCanEverTick = true;
}

// 测试对象进入关卡时调用
void AAttackDataTestActor::BeginPlay()
{
	Super::BeginPlay();
}

// 测试对象每帧更新
void AAttackDataTestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
