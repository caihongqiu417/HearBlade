// Fill out your copyright notice in the Description page of Project Settings.

#include "BeatClockTestActor.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Combat/AttackData.h"
#include "Kismet/GameplayStatics.h"

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
	CombatDrumComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("CombatDrumComponent"));
	CombatDrumComponent->SetupAttachment(MusicComponent);
	CombatDrumComponent->bAutoActivate = false;
	CombatDrumComponent->bAllowSpatialization = false;
	CombatDrumComponent->bIsUISound = true;

	// 配置第一步使用的拍子设置。
	ClockSettings.BPM = 104.0;
	ClockSettings.BeatsPerBar = 4;
	ClockSettings.BeatSubdivision = 1;
	ClockSettings.StartOffsetSeconds = 0.204;
	Music = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Music/SW_AintItFun.SW_AintItFun"));
	SyncAttackData = LoadObject<UAttackDataAsset>(nullptr,
		TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_DragonKnight_Combo01_Beat.DA_DragonKnight_Combo01_Beat"));
	DrumKick = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/SW_BossDrumKick.SW_BossDrumKick"));
	DrumTick = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/SW_BossDrumTick.SW_BossDrumTick"));
	CombatDrumLoop = LoadObject<USoundBase>(nullptr,
		TEXT("/Game/Audio/SFX/SW_BossCombatDrumLoop.SW_BossCombatDrumLoop"));
}

// ============================================================
// 开始和结束
// ============================================================

void ABeatClockTestActor::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Display, TEXT("BossDrums: enabled=%d stem=%s stemVolume=%.2f musicVolume=%.2f kick=%s tick=%s volumes=%.2f/%.2f/%.2f"),
		bEnableCombatDrums ? 1 : 0, *GetNameSafe(CombatDrumLoop), CombatDrumStemVolume, MusicMixVolume,
		*GetNameSafe(DrumKick), *GetNameSafe(DrumTick),
		StrongDrumVolume, MiddleDrumVolume, WeakDrumVolume);

	// 拍子时钟由 Unreal 自动创建，测试对象只需要取得它。
	UBeatClockSubsystem* BeatClock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();

	// 监听进入新拍子时发出的拍子事件。
	BeatClock->OnBeat.AddDynamic(this, &ABeatClockTestActor::HandleBeat);

	// 同一游戏线程调用音乐播放和拍子时钟启动；最终音频设备延迟仍需实测校准。
	if (Music)
	{
		MusicComponent->SetSound(Music);
		MusicComponent->SetVolumeMultiplier(MusicMixVolume);
		FBeatClockSettings MusicSettings = ClockSettings;
		float StartTime = MusicStartTimeSeconds;
		double SourceBeatPhase = ClockSettings.StartOffsetSeconds;
		if (IsValid(SyncAttackData))
		{
			MusicSettings.BPM = SyncAttackData->ReferenceBPM;
			MusicSettings.StartOffsetSeconds = SyncAttackData->ReferenceBeatPhaseSeconds;
			SourceBeatPhase = SyncAttackData->ReferenceBeatPhaseSeconds;
			StartTime = static_cast<float>(SyncAttackData->ReferenceMusicStartSeconds);
		}
		const double SecondsPerBeat = 60.0 / FMath::Max(MusicSettings.BPM, 1.0);
		MusicSettings.StartOffsetSeconds = FMath::Fmod(MusicSettings.StartOffsetSeconds - StartTime, SecondsPerBeat);
		if (MusicSettings.StartOffsetSeconds < 0.0)
		{
			MusicSettings.StartOffsetSeconds += SecondsPerBeat;
		}
		BeatClock->StartClockWithMusic(MusicSettings, MusicComponent, StartTime);
		if (bEnableCombatDrums && bUseLoopingDrumStem && CombatDrumLoop && CombatDrumComponent)
		{
			const double BarSeconds = SecondsPerBeat * FMath::Max(1, MusicSettings.BeatsPerBar);
			double DrumStartTime = FMath::Fmod(StartTime - SourceBeatPhase, BarSeconds);
			if (DrumStartTime < 0.0) DrumStartTime += BarSeconds;
			CombatDrumComponent->SetSound(CombatDrumLoop);
			CombatDrumComponent->SetVolumeMultiplier(CombatDrumStemVolume);
			CombatDrumComponent->Play(static_cast<float>(DrumStartTime));
			UE_LOG(LogTemp, Display, TEXT("CombatDrumStem: playing=%d sourceStart=%.6f bar=%.6f BPM=%.3f"),
				CombatDrumComponent->IsPlaying() ? 1 : 0, DrumStartTime, BarSeconds, MusicSettings.BPM);
		}
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
	if (CombatDrumComponent) CombatDrumComponent->Stop();

	Super::EndPlay(EndPlayReason);
}

// ============================================================
// 拍子事件回调
// ============================================================

void ABeatClockTestActor::HandleBeat(int32 BeatIndex /* 全部拍子编号 */, int32 BeatInBar /* 当前小节内的拍子编号 */)
{
	UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	// After a hitch, the clock can emit skipped beats together. Don't play old
	// percussion on top of the current beat.
	const bool bStemIsPlaying = CombatDrumComponent && CombatDrumComponent->IsPlaying();
	if (bEnableCombatDrums && !bStemIsPlaying && Clock && BeatIndex == Clock->GetCurrentBeatIndex() &&
		(!MusicComponent || MusicComponent->IsPlaying()))
	{
		USoundBase* AccentSound = (BeatInBar == 0 || BeatInBar == 2) ? DrumKick : DrumTick;
		const float Volume = BeatInBar == 0 ? StrongDrumVolume
			: BeatInBar == 2 ? MiddleDrumVolume : WeakDrumVolume;
		if (AccentSound && Volume > 0.f)
			UGameplayStatics::PlaySound2D(this, AccentSound, Volume);
		if (BeatIndex < 4)
			UE_LOG(LogTemp, Display, TEXT("BossDrums: Beat=%d InBar=%d Sound=%s Volume=%.2f"),
				BeatIndex, BeatInBar + 1, *GetNameSafe(AccentSound), Volume);
		UE_LOG(LogTemp, Verbose, TEXT("BossDrums: Beat=%d InBar=%d Volume=%.2f"), BeatIndex, BeatInBar + 1, Volume);
	}
	// BeatInBar 从 0 开始，输出时加 1，让日志从第 1 拍开始显示。
	UE_LOG(LogTemp, Log, TEXT("[HearBlade 拍子时钟] 总拍子=%d，小节内拍子=%d"), BeatIndex, BeatInBar + 1);
}
