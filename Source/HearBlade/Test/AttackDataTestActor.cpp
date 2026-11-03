// 提供攻击数据运行测试的场景对象
// Fill out your copyright notice in the Description page of Project Settings.

#include "Test/AttackDataTestActor.h"

// Sets default values
AAttackDataTestActor::AAttackDataTestActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AAttackDataTestActor::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AAttackDataTestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
