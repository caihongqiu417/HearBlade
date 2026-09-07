// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BeatClockSubsystem.generated.h"

// ============================================================
// 拍子事件
// ============================================================
// 当系统进入新的一拍时，会发出这个拍子事件。
// BeatIndex 是从 0 开始计算的全部拍子编号；BeatInBar 是当前拍子在小节中的位置。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnBeatClockBeat,
	int32, BeatIndex, // 全部拍子编号
	int32, BeatInBar // 当前拍子在小节中的位置
);

// ============================================================
// 拍子时钟设置
// ============================================================
// 这些设置描述音乐的速度、小节结构和开始位置。
USTRUCT(BlueprintType)
struct FBeatClockSettings
{
	GENERATED_BODY()

	// 每分钟有多少拍。
	// 例如 BPM=120 表示每拍 0.5 秒。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拍子时钟", meta = (ClampMin = "1.0"))
	double BPM = 120.0;

	// 每小节有多少拍。
	// 例如 4 表示四四拍，3 表示四三拍。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拍子时钟", meta = (ClampMin = "1"))
	int32 BeatsPerBar = 4;

	// 一拍分成多少份。
	// 第一步只保存这个设置，后面制作半拍和四分之一拍时使用。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拍子时钟", meta = (ClampMin = "1"))
	int32 BeatSubdivision = 1;

	// 第 0 拍相对于开始时间的偏移。
	// 如果音乐开头有空白，可以用这个数调整。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拍子时钟")
	double StartOffsetSeconds = 0.0;
};

// ============================================================
// 拍子时钟
// ============================================================
// 这个类继承 Unreal 的 UTickableWorldSubsystem。
// Unreal 会为当前关卡自动创建它，因此不需要把它放进关卡。
// 它负责保存拍子设置、计算拍子时间，以及发出拍子事件。
UCLASS()
class HEARBLADE_API UBeatClockSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// ========================================================
	// 开始和结束
	// ========================================================

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	// Unreal 每帧调用一次，用来检查是否进入新的一拍。
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	
	virtual TStatId GetStatId() const override;

	// ========================================================
	// 启动和停止
	// ========================================================

	// 使用指定设置开始计算拍子。
	UFUNCTION(BlueprintCallable, Category = "拍子时钟")
	void StartClock(FBeatClockSettings InSettings);

	// 停止计算拍子。
	UFUNCTION(BlueprintCallable, Category = "拍子时钟")
	void StopClock();

	// ========================================================
	// 读取拍子时间
	// ========================================================

	// 返回拍子时钟是否正在运行。
	UFUNCTION(BlueprintPure, Category = "拍子时钟")
	bool IsRunning() const;

	// 返回当前拍子，可以带小数，例如 2.5 表示正在两个拍子之间。
	UFUNCTION(BlueprintPure, Category = "拍子时钟")
	double GetCurrentBeat() const;

	// 返回当前的拍子编号（整数）
	UFUNCTION(BlueprintPure, Category = "拍子时钟")
	int32 GetCurrentBeatIndex() const;

	// 返回当前时间之后最近的拍子编号。
	UFUNCTION(BlueprintPure, Category = "拍子时钟")
	int32 GetNextBeatIndex() const;

	// 返回指定拍子到来的时间，后续 Boss 会用这个函数安排攻击。
	UFUNCTION(BlueprintPure, Category = "拍子时钟")
	double GetBeatTimeSeconds(double BeatIndex) const;

	// 返回一拍需要多少秒。
	UFUNCTION(BlueprintPure, Category = "拍子时钟")
	double GetSecondsPerBeat() const;

	// 返回当前小节编号，从 0 开始。
	UFUNCTION(BlueprintPure, Category = "拍子时钟")
	int32 GetCurrentBarIndex() const;

	// 返回当前拍子在小节中的位置，从 0 开始。
	UFUNCTION(BlueprintPure, Category = "拍子时钟")
	int32 GetCurrentBeatInBar() const;

	// ========================================================
	// 拍子事件
	// ========================================================

	// 当进入新的一拍时发出拍子事件
	UPROPERTY(BlueprintAssignable, Category = "拍子时钟")
	FOnBeatClockBeat OnBeat;

private:
	// 读取 Unreal 提供的音频时间，不使用每帧时间累加，避免画面卡顿影响拍子计算。
	double GetAudioNowSeconds() const;

private:

	UPROPERTY()
	FBeatClockSettings Settings;

	bool bRunning = false;

	// 开始计算拍子时的音频时间。
	double ClockStartAudioSeconds = 0.0;

	// 上一次已经发出的拍子编号；-1 表示还没有发出过拍子事件。
	int32 LastEmittedBeat = -1;
};
