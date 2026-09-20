// Fill out your copyright notice in the Description page of Project Settings.

#include "BeatClockSubsystem.h"

#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Stats/Stats.h"

/*
 * 文件作用：提供当前关卡共用的拍子时间。
 * 计算使用音频时间；每帧只检查是否进入新的一拍并发出拍子事件。
 * 后续 Boss 查询指定拍子的时间，不直接使用每帧运行的时间。
 */

// ============================================================
// 开始和结束
// ============================================================

void UBeatClockSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// 调用父类初始化函数，让 Unreal 正确管理这个系统。
	Super::Initialize(Collection);
	bRunning = false;
	LastEmittedBeat = -1;
}

void UBeatClockSubsystem::Deinitialize()
{
	StopClock();
	bRunning = false;
	OnBeat.Clear();
	Super::Deinitialize();
}

// ============================================================
// 启动和停止
// ============================================================

void UBeatClockSubsystem::StartClock(FBeatClockSettings InSettings)
{
	Settings = InSettings;

	// 检查设置的数值。
	Settings.BPM = FMath::Max(Settings.BPM, 1.0);
	Settings.BeatsPerBar = FMath::Max(Settings.BeatsPerBar, 1);
	Settings.BeatSubdivision = FMath::Max(Settings.BeatSubdivision, 1);

	// 记录启动时间
	LastRawAudioSeconds = UGameplayStatics::GetAudioTimeSeconds(GetWorld());
	MusicalAudioSeconds = LastRawAudioSeconds;
	CurrentPlaybackRate = TargetPlaybackRate = 1.f;
	PlaybackRateInterpSpeed = 100.f;
	PlaybackRateRestoreRawAudioSeconds = -1.0;
	ClockStartAudioSeconds = MusicalAudioSeconds;
	LastEmittedBeat = -1;
	bRunning = true;
}

void UBeatClockSubsystem::StartClockWithMusic(FBeatClockSettings InSettings, UAudioComponent* MusicComponent, float MusicStartTimeSeconds)
{
	if (IsValid(MusicComponent))
	{
		MusicComponent->Play(FMath::Max(0.0f, MusicStartTimeSeconds));
	}
	StartClock(InSettings);
	MusicSource = MusicComponent;
	RegisterSynchronizedMusicComponent(MusicComponent);
	BaseMusicVolume = IsValid(MusicComponent) ? MusicComponent->VolumeMultiplier : 1.f;
	CurrentMusicGain = 1.f;
	DuckHoldRemaining = 0.f;
	UE_LOG(LogTemp, Log, TEXT("BeatClock music start: BPM=%.3f offset=%.3f sourceStart=%.3f sound=%s playing=%d"),
		Settings.BPM, Settings.StartOffsetSeconds, MusicStartTimeSeconds,
		MusicComponent && MusicComponent->GetSound() ? *MusicComponent->GetSound()->GetName() : TEXT("None"),
		IsValid(MusicComponent) && MusicComponent->IsPlaying() ? 1 : 0);
}

void UBeatClockSubsystem::StopClock()
{
	if (UAudioComponent* Source = MusicSource.Get()) Source->SetVolumeMultiplier(BaseMusicVolume);
	for (const FSynchronizedMusicSource& Entry : SynchronizedMusicSources)
		if (UAudioComponent* Source = Entry.Component.Get()) Source->SetPitchMultiplier(Entry.BasePitch);
	SynchronizedMusicSources.Reset();
	MusicSource.Reset();
	CurrentMusicGain = 1.f;
	DuckHoldRemaining = 0.f;
	bRunning = false;
	CurrentPlaybackRate = TargetPlaybackRate = 1.f;
	PlaybackRateRestoreRawAudioSeconds = -1.0;
}

void UBeatClockSubsystem::RegisterSynchronizedMusicComponent(UAudioComponent* Component)
{
	if (!IsValid(Component)) return;
	for (const FSynchronizedMusicSource& Entry : SynchronizedMusicSources)
		if (Entry.Component.Get() == Component) return;
	SynchronizedMusicSources.Add({Component, Component->PitchMultiplier});
	Component->SetPitchMultiplier(Component->PitchMultiplier * CurrentPlaybackRate);
}

void UBeatClockSubsystem::UnregisterSynchronizedMusicComponent(UAudioComponent* Component)
{
	for (int32 Index = SynchronizedMusicSources.Num() - 1; Index >= 0; --Index)
	{
		if (SynchronizedMusicSources[Index].Component.Get() == Component)
		{
			if (IsValid(Component)) Component->SetPitchMultiplier(SynchronizedMusicSources[Index].BasePitch);
			SynchronizedMusicSources.RemoveAt(Index);
		}
	}
}

void UBeatClockSubsystem::SetMusicalPlaybackRate(float TargetRate, float BlendSeconds)
{
	TargetPlaybackRate = FMath::Clamp(TargetRate, .35f, 1.f);
	PlaybackRateInterpSpeed = FMath::Abs(TargetPlaybackRate - CurrentPlaybackRate) /
		FMath::Max(BlendSeconds, .001f);
	UE_LOG(LogTemp, Display, TEXT("BeatCinematic: musical rate %.3f -> %.3f blend=%.3fs"),
		CurrentPlaybackRate, TargetPlaybackRate, BlendSeconds);
}

void UBeatClockSubsystem::SetTemporaryMusicalPlaybackRate(float TargetRate, float BlendSeconds,
	float MaximumDurationSeconds)
{
	SetMusicalPlaybackRate(TargetRate, BlendSeconds);
	PlaybackRateRestoreRawAudioSeconds = UGameplayStatics::GetAudioTimeSeconds(GetWorld()) +
		FMath::Max(.05f, MaximumDurationSeconds);
}

void UBeatClockSubsystem::RestoreMusicalPlaybackRate(float BlendSeconds)
{
	PlaybackRateRestoreRawAudioSeconds = -1.0;
	SetMusicalPlaybackRate(1.f, BlendSeconds);
}

void UBeatClockSubsystem::TriggerImpactDuck(float MusicGain, float HoldSeconds, float ReleaseSpeed)
{
	if (UAudioComponent* Source = MusicSource.Get(); bRunning && Source && Source->IsPlaying())
	{
		CurrentMusicGain = FMath::Min(CurrentMusicGain, FMath::Clamp(MusicGain, .2f, 1.f));
		DuckHoldRemaining = FMath::Max(DuckHoldRemaining, HoldSeconds);
		DuckReleaseSpeed = FMath::Max(ReleaseSpeed, 1.f);
		Source->SetVolumeMultiplier(BaseMusicVolume * CurrentMusicGain);
		UE_LOG(LogTemp, Display, TEXT("BeatMusicDuck: applied gain=%.2f source=%s"),
			CurrentMusicGain, *GetNameSafe(Source));
	}
}

// ============================================================
// 读取拍子时间
// ============================================================

bool UBeatClockSubsystem::IsRunning() const
{
	return bRunning;
}

double UBeatClockSubsystem::GetAudioNowSeconds() const
{
	// 读取 Unreal 从关卡开始运行后经过的音频时间，单位是秒。
	// 游戏暂停时，这个时间也会暂停。
	return bRunning ? MusicalAudioSeconds : UGameplayStatics::GetAudioTimeSeconds(GetWorld());
}

double UBeatClockSubsystem::GetSecondsPerBeat() const
{
	// 每拍秒数 = 60 秒 ÷ 每分钟拍数
	// 例如在120BPM下，一拍就对应0.5s
	return 60.0 / Settings.BPM;
}

double UBeatClockSubsystem::GetCurrentBeat() const
{
	if (!bRunning)
	{
		return 0.0;
	}

	// 第0拍发生的时间
	const double BeatZeroTime = ClockStartAudioSeconds + Settings.StartOffsetSeconds;

	// 当前拍子 =（当前音频时间 − 第 0 拍发生的时间）÷ 每拍持续时间
	return (GetAudioNowSeconds() - BeatZeroTime) / GetSecondsPerBeat();
}

int32 UBeatClockSubsystem::GetCurrentBeatIndex() const
{
	if (!bRunning)
	{
		return -1;
	}

	return FMath::FloorToInt(GetCurrentBeat());
}

int32 UBeatClockSubsystem::GetNextBeatIndex() const
{
	// 找到当前时间之后最近的拍子。
	return FMath::Max(0, FMath::CeilToInt(GetCurrentBeat() - KINDA_SMALL_NUMBER));
}

double UBeatClockSubsystem::GetBeatTimeSeconds(double BeatIndex) const
{
	const double BeatZeroTime = ClockStartAudioSeconds + Settings.StartOffsetSeconds;

	// Boss 直接查询目标拍子的时间，不需要等到某一帧再猜测。
	return BeatZeroTime + BeatIndex * GetSecondsPerBeat();
}

int32 UBeatClockSubsystem::GetCurrentBarIndex() const
{
	const int32 BeatIndex = GetCurrentBeatIndex();

	if (BeatIndex < 0)
	{
		return -1;
	}

	// 小节编号 = 当前拍子 / 每小节拍数
	// 例如当前在第 8 拍，则处于第 8/4=2 小节
	return BeatIndex / Settings.BeatsPerBar;
}

int32 UBeatClockSubsystem::GetCurrentBeatInBar() const
{
	const int32 BeatIndex = GetCurrentBeatIndex();

	if (BeatIndex < 0)
	{
		return -1;
	}

	// 例如当前在第 6 拍，6 % 4 = 2 ，所以处于当前小节的第二拍
	return BeatIndex % Settings.BeatsPerBar;
}

// ============================================================
// 每帧检查拍子
// ============================================================

void UBeatClockSubsystem::Tick(float DeltaTime)
{
	if (!bRunning)
	{
		return;
	}
	const double RawNow = UGameplayStatics::GetAudioTimeSeconds(GetWorld());
	if (PlaybackRateRestoreRawAudioSeconds > 0.0 && RawNow >= PlaybackRateRestoreRawAudioSeconds)
	{
		UE_LOG(LogTemp, Warning, TEXT("BeatCinematic: failsafe restored musical playback rate"));
		RestoreMusicalPlaybackRate(.10f);
	}
	const double RawDelta = FMath::Max(0.0, RawNow - LastRawAudioSeconds);
	LastRawAudioSeconds = RawNow;
	const float PreviousRate = CurrentPlaybackRate;
	CurrentPlaybackRate = FMath::FInterpConstantTo(CurrentPlaybackRate, TargetPlaybackRate,
		static_cast<float>(RawDelta), PlaybackRateInterpSpeed);
	MusicalAudioSeconds += RawDelta * (PreviousRate + CurrentPlaybackRate) * .5;
	for (int32 Index = SynchronizedMusicSources.Num() - 1; Index >= 0; --Index)
	{
		FSynchronizedMusicSource& Entry = SynchronizedMusicSources[Index];
		if (UAudioComponent* Source = Entry.Component.Get())
			Source->SetPitchMultiplier(Entry.BasePitch * CurrentPlaybackRate);
		else SynchronizedMusicSources.RemoveAt(Index);
	}
	if (UAudioComponent* Source = MusicSource.Get(); Source && CurrentMusicGain < .999f)
	{
		const float PreviousGain = CurrentMusicGain;
		if (DuckHoldRemaining > 0.f) DuckHoldRemaining = FMath::Max(0.f, DuckHoldRemaining - DeltaTime);
		else CurrentMusicGain = FMath::FInterpTo(CurrentMusicGain, 1.f, DeltaTime, DuckReleaseSpeed);
		if (CurrentMusicGain >= .995f) CurrentMusicGain = 1.f;
		Source->SetVolumeMultiplier(BaseMusicVolume * CurrentMusicGain);
		if (PreviousGain < 1.f && CurrentMusicGain == 1.f)
			UE_LOG(LogTemp, Display, TEXT("BeatMusicDuck: restored music volume"));
	}

	// 获取当前拍子
	const int32 CurrentBeat = GetCurrentBeatIndex();

	if (CurrentBeat < 0)
	{
		return;
	}
	
	while (LastEmittedBeat < CurrentBeat /* 如果当前拍子没发出过 */)
	{
		// 更新
		++LastEmittedBeat;
		
		// 广播
		const int32 BeatInBar = LastEmittedBeat % Settings.BeatsPerBar;
		OnBeat.Broadcast(LastEmittedBeat/* 当前拍子 */, BeatInBar /* 当前位于该小节的第几拍 */);
	}
}

// ============================================================
// 每帧运行设置
// ============================================================

TStatId UBeatClockSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBeatClockSubsystem, STATGROUP_Tickables);
}

bool UBeatClockSubsystem::IsTickable() const
{
	return bRunning;
}
