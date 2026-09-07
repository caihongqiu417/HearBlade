// Fill out your copyright notice in the Description page of Project Settings.

#include "BeatClockTestActor.h"

// ============================================================
// 创建测试对象
// ============================================================

ABeatClockTestActor::ABeatClockTestActor()
{
	// 测试对象自己不需要每帧运行，拍子时钟会负责检查拍子。
	PrimaryActorTick.bCanEverTick = false;

	// 配置第一步使用的拍子设置。
	ClockSettings.BPM = 120.0;
	ClockSettings.BeatsPerBar = 4;
	ClockSettings.BeatSubdivision = 1;
	ClockSettings.StartOffsetSeconds = 0.0;
}

// ============================================================
// 开始和结束
// ============================================================

void ABeatClockTestActor::BeginPlay()
{
	Super::BeginPlay();

	// 拍子时钟由 Unreal 自动创建，测试对象只需要取得它。
	UBeatClockSubsystem* BeatClock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();

	// 监听进入新拍子时发出的拍子事件。
	BeatClock->OnBeat.AddDynamic(this, &ABeatClockTestActor::HandleBeat);

	// 启动拍子系统，开始计算拍子。
	// TODO：真正播放音乐时，应在音乐开始播放的时刻调用这个函数。
	BeatClock->StartClock(ClockSettings);
}

void ABeatClockTestActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UBeatClockSubsystem* BeatClock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();

	// 测试对象结束前取消监听拍子事件。
	BeatClock->OnBeat.RemoveDynamic(this, &ABeatClockTestActor::HandleBeat);

	BeatClock->StopClock();

	Super::EndPlay(EndPlayReason);
}

// ============================================================
// 拍子事件回调
// ============================================================

void ABeatClockTestActor::HandleBeat(int32 BeatIndex /* 全部拍子编号 */, int32 BeatInBar /* 当前小节内的拍子编号 */)
{
	// BeatInBar 从 0 开始，输出时加 1，让日志从第 1 拍开始显示。
	UE_LOG(LogTemp, Log, TEXT("[HearBlade 拍子时钟] 总拍子=%d，小节内拍子=%d"), BeatIndex, BeatInBar + 1);
}
