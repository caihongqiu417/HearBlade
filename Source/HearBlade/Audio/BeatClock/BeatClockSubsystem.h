// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BeatClockSubsystem.generated.h"

class UAudioComponent;

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

	// Unreal在关卡创建时自动调用
	// 系统被创建时的准备工作
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	// Unreal 每帧调用一次，用来检查是否进入新的一拍。
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	
	virtual TStatId GetStatId() const override;

	// ========================================================
	// 启动和停止
	// ========================================================

	// 开始计算拍子
	UFUNCTION(BlueprintCallable, Category = "拍子时钟")
	void StartClock(FBeatClockSettings InSettings);

	// 同时发出音乐播放命令并设定拍子原点。音乐未指定时只启动拍子时钟。
	UFUNCTION(BlueprintCallable, Category = "拍子时钟")
	void StartClockWithMusic(FBeatClockSettings InSettings, UAudioComponent* MusicComponent, float MusicStartTimeSeconds = 0.0f);

	// 停止计算拍子
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

	// 与 GetBeatTimeSeconds 使用相同的时间轴，供攻击预定器比较。
	UFUNCTION(BlueprintPure, Category = "拍子时钟")
	double GetAudioNowSeconds() const;

	// 返回一拍需要多少秒。
	UFUNCTION(BlueprintPure, Category = "拍子时钟")
	double GetSecondsPerBeat() const;

	UFUNCTION(BlueprintPure, Category = "拍子时钟")
	int32 GetBeatsPerBar() const { return Settings.BeatsPerBar; }

	// 返回当前小节编号，从 0 开始。
	UFUNCTION(BlueprintPure, Category = "拍子时钟")
	int32 GetCurrentBarIndex() const;

	// 返回当前拍子在小节中的位置，从 0 开始。
	UFUNCTION(BlueprintPure, Category = "拍子时钟")
	int32 GetCurrentBeatInBar() const;

	// Briefly clear space in the music mix for a blade strike, then recover.
	void TriggerImpactDuck(float MusicGain, float HoldSeconds = 0.07f, float ReleaseSpeed = 9.f);

	// Register every music stem that must remain phase locked with the main song.
	// Cinematic rate changes are applied to all registered components together.
	void RegisterSynchronizedMusicComponent(UAudioComponent* Component);
	void UnregisterSynchronizedMusicComponent(UAudioComponent* Component);

	// Changes musical time and audio pitch together. Gameplay input and camera
	// stay at normal world speed while beat-authored animation remains aligned.
	void SetMusicalPlaybackRate(float TargetRate, float BlendSeconds = 0.06f);
	void SetTemporaryMusicalPlaybackRate(float TargetRate, float BlendSeconds, float MaximumDurationSeconds);
	void RestoreMusicalPlaybackRate(float BlendSeconds = 0.24f);
	UFUNCTION(BlueprintPure, Category = "拍子时钟")
	float GetMusicalPlaybackRate() const { return CurrentPlaybackRate; }

	// ========================================================
	// 拍子事件
	// ========================================================

	// 当进入新的一拍时发出拍子事件
	UPROPERTY(BlueprintAssignable, Category = "拍子时钟")
	FOnBeatClockBeat OnBeat;

private:

	UPROPERTY()
	FBeatClockSettings Settings;

	bool bRunning = false;

	// 开始计算拍子时的音频时间。
	double ClockStartAudioSeconds = 0.0;

	// 上一次已经发出的拍子编号；-1 表示还没有发出过拍子事件。
	int32 LastEmittedBeat = -1;
	TWeakObjectPtr<UAudioComponent> MusicSource;
	float BaseMusicVolume = 1.f;
	float CurrentMusicGain = 1.f;
	float DuckHoldRemaining = 0.f;
	float DuckReleaseSpeed = 9.f;
	struct FSynchronizedMusicSource
	{
		TWeakObjectPtr<UAudioComponent> Component;
		float BasePitch = 1.f;
	};
	TArray<FSynchronizedMusicSource> SynchronizedMusicSources;
	double MusicalAudioSeconds = 0.0;
	double LastRawAudioSeconds = 0.0;
	float CurrentPlaybackRate = 1.f;
	float TargetPlaybackRate = 1.f;
	float PlaybackRateInterpSpeed = 100.f;
	double PlaybackRateRestoreRawAudioSeconds = -1.0;
};
