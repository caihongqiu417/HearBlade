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
	auto CreateMusicLayer = [this](const TCHAR* Name)
	{
		UAudioComponent* Component = CreateDefaultSubobject<UAudioComponent>(Name);
		Component->SetupAttachment(MusicComponent);
		Component->bAutoActivate = false;
		Component->bAllowSpatialization = false;
		Component->bIsUISound = true;
		return Component;
	};
	ComboLowComponent = CreateMusicLayer(TEXT("ComboLowComponent"));
	ComboSnareComponent = CreateMusicLayer(TEXT("ComboSnareComponent"));
	ComboHighComponent = CreateMusicLayer(TEXT("ComboHighComponent"));

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
	ComboLowLoop = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/SW_ComboTier1Low.SW_ComboTier1Low"));
	ComboSnareLoop = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/SW_ComboTier2Snare.SW_ComboTier2Snare"));
	ComboHighLoop = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/SW_ComboTier3High.SW_ComboTier3High"));
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
	if (APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		BoundCombat = Player->FindComponentByClass<UPlayerRhythmCombatComponent>();
		if (BoundCombat.IsValid())
			BoundCombat->OnRhythmSuccessFeedback.AddDynamic(this, &ABeatClockTestActor::HandleRhythmFeedback);
	}

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
		BeatClock->RegisterSynchronizedMusicComponent(CombatDrumComponent);
		BeatClock->RegisterSynchronizedMusicComponent(ComboLowComponent);
		BeatClock->RegisterSynchronizedMusicComponent(ComboSnareComponent);
		BeatClock->RegisterSynchronizedMusicComponent(ComboHighComponent);
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
			StartComboLayers(static_cast<float>(DrumStartTime));
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
	if (BoundCombat.IsValid())
		BoundCombat->OnRhythmSuccessFeedback.RemoveDynamic(this, &ABeatClockTestActor::HandleRhythmFeedback);

	BeatClock->StopClock();
	MusicComponent->Stop();
	if (CombatDrumComponent) CombatDrumComponent->Stop();
	if (ComboLowComponent) ComboLowComponent->Stop();
	if (ComboSnareComponent) ComboSnareComponent->Stop();
	if (ComboHighComponent) ComboHighComponent->Stop();

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
	if (BeatInBar == 0 && Clock && BeatIndex == Clock->GetCurrentBeatIndex()) ApplyComboTierAtBar();
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

void ABeatClockTestActor::StartComboLayers(float SourceStartTime)
{
	const auto StartLayer = [SourceStartTime](UAudioComponent* Component, USoundBase* Sound)
	{
		if (!Component || !Sound) return;
		Component->SetSound(Sound);
		Component->SetVolumeMultiplier(0.f);
		Component->Play(SourceStartTime);
	};
	StartLayer(ComboLowComponent, ComboLowLoop);
	StartLayer(ComboSnareComponent, ComboSnareLoop);
	StartLayer(ComboHighComponent, ComboHighLoop);
	UE_LOG(LogTemp, Display, TEXT("ComboMusic: synchronized layers started at %.6f low=%s snare=%s high=%s"),
		SourceStartTime, *GetNameSafe(ComboLowLoop), *GetNameSafe(ComboSnareLoop), *GetNameSafe(ComboHighLoop));
}

void ABeatClockTestActor::HandleRhythmFeedback(const FRhythmSuccessFeedbackData& Feedback)
{
	PendingComboTier = Feedback.bSuccess ? FMath::Clamp(Feedback.FeedbackTier, 0, 3) : 0;
	if (Feedback.bSuccess && Feedback.FeedbackTier > UPlayerRhythmCombatComponent::GetFeedbackTierForCombo(Feedback.PreviousCombo))
	{
		UE_LOG(LogTemp, Display, TEXT("ComboMusic: tier-up requested combo=%d tier=%d event=%lld"),
			Feedback.ComboCount, PendingComboTier, Feedback.EventId);
	}
}

void ABeatClockTestActor::ApplyComboTierAtBar()
{
	if (CurrentComboTier == PendingComboTier) return;
	CurrentComboTier = PendingComboTier;
	const UBeatClockSubsystem* Clock = GetWorld() ? GetWorld()->GetSubsystem<UBeatClockSubsystem>() : nullptr;
	const float FadeSeconds = Clock ? static_cast<float>(Clock->GetSecondsPerBeat() * 2.0) : 1.15f;
	if (ComboLowComponent) ComboLowComponent->AdjustVolume(FadeSeconds, CurrentComboTier >= 1 ? ComboLowVolume : 0.f);
	if (ComboSnareComponent) ComboSnareComponent->AdjustVolume(FadeSeconds, CurrentComboTier >= 2 ? ComboSnareVolume : 0.f);
	if (ComboHighComponent) ComboHighComponent->AdjustVolume(FadeSeconds, CurrentComboTier >= 3 ? ComboHighVolume : 0.f);
	UE_LOG(LogTemp, Display, TEXT("ComboMusic: applied tier=%d fade=%.3fs volumes=%.2f/%.2f/%.2f"), CurrentComboTier,
		FadeSeconds, CurrentComboTier >= 1 ? ComboLowVolume : 0.f,
		CurrentComboTier >= 2 ? ComboSnareVolume : 0.f, CurrentComboTier >= 3 ? ComboHighVolume : 0.f);
}
