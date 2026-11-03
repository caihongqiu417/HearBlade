// 提供攻击数据运行测试的场景对象
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AttackDataTestActor.generated.h"

UCLASS()

class HEARBLADE_API AAttackDataTestActor : public AActor
{
	GENERATED_BODY()

  public:
	// Sets default values for this actor's properties
	AAttackDataTestActor();

  protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

  public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
