// Fill out your copyright notice in the Description page of Project Settings.

#include "BeatClockTestActor.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"

// ============================================================
// 创建测试对象
// ============================================================

ABeatClockTestActor::ABeatClockTestActor()
{
	// 测试对象自己不需要每帧运行，拍子时钟会负责检查拍子。
	PrimaryActorTick.bCanEverTick = false;
	MusicComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("MusicComponent"));
	MusicComponent->bAutoActivate = false;
	MusicComponent->bAllowSpatialization = false;
	MusicComponent->bIsUISound = true;

	// 配置第一步使用的拍子设置。
	ClockSettings.BPM = 104.0;
	ClockSettings.BeatsPerBar = 4;
	ClockSettings.BeatSubdivision = 1;
	ClockSettings.StartOffsetSeconds = 0.204;
	Music = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Music/SW_AintItFun.SW_AintItFun"));
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

	// 同一游戏线程调用音乐播放和拍子时钟启动；最终音频设备延迟仍需实测校准。
	if (Music)
	{
		MusicComponent->SetSound(Music);
		FBeatClockSettings MusicSettings = ClockSettings;
		const double SecondsPerBeat = 60.0 / FMath::Max(MusicSettings.BPM, 1.0);
		MusicSettings.StartOffsetSeconds = FMath::Fmod(MusicSettings.StartOffsetSeconds - MusicStartTimeSeconds, SecondsPerBeat);
		if (MusicSettings.StartOffsetSeconds < 0.0)
		{
			MusicSettings.StartOffsetSeconds += SecondsPerBeat;
		}
		BeatClock->StartClockWithMusic(MusicSettings, MusicComponent, MusicStartTimeSeconds);
	}
	else
	{
		BeatClock->StartClock(ClockSettings);
	}
}

void ABeatClockTestActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UBeatClockSubsystem* BeatClock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();

	// 测试对象结束前取消监听拍子事件。
	BeatClock->OnBeat.RemoveDynamic(this, &ABeatClockTestActor::HandleBeat);

	BeatClock->StopClock();
	MusicComponent->Stop();

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
