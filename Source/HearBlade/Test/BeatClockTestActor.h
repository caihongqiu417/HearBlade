// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "BeatClockTestActor.generated.h"

// ============================================================
// 拍子时钟测试对象
// ============================================================
// 这个对象继承 Unreal 的 AActor。
// 编译后用它创建蓝图，再把蓝图放入当前关卡。
// 它只负责启动拍子时钟、接收拍子事件和输出测试结果，不属于正式 Boss 系统。
UCLASS(Blueprintable)
class HEARBLADE_API ABeatClockTestActor : public AActor
{
	GENERATED_BODY()

public:
	ABeatClockTestActor();

	// ========================================================
	// 开始和结束
	// ========================================================

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ========================================================
	// 测试设置
	// ========================================================

	// 可以在测试蓝图中修改拍子设置。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拍子时钟测试")
	FBeatClockSettings ClockSettings;

private:
	// ========================================================
	// 拍子事件
	// ========================================================

	// 收到拍子事件后，把结果写入输出日志。
	UFUNCTION()
	void HandleBeat(int32 BeatIndex /* 全部拍子编号 */, int32 BeatInBar /* 当前小节内的拍子编号 */);
};
