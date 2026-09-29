// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "Combat/PlayerRhythmCombatComponent.h"
#include "Combat/PlayerAttackSet.h"
#include "BeatClockTestActor.generated.h"

class UAudioComponent;
class USoundBase;
class UAttackDataAsset;

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

	// 指定音乐后，测试 Actor 会在启动拍子时钟时播放这首音乐。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拍子时钟测试")
	TObjectPtr<USoundBase> Music;

	// 读取节拍编辑器保存的 BPM、相位和音乐起点。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拍子时钟测试")
	TObjectPtr<UAttackDataAsset> SyncAttackData;

	// 原音乐文件开头约四秒无声；从有声段开始测试，并同步换算拍子相位。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拍子时钟测试", meta = (ClampMin = "0.0"))
	float MusicStartTimeSeconds = 4.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "拍子时钟测试")
	TObjectPtr<UAudioComponent> MusicComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "拍子时钟测试|战斗鼓点")
	TObjectPtr<UAudioComponent> CombatDrumComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "拍子时钟测试|连击音乐")
	TObjectPtr<UAudioComponent> ComboLowComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "拍子时钟测试|连击音乐")
	TObjectPtr<UAudioComponent> ComboSnareComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "拍子时钟测试|连击音乐")
	TObjectPtr<UAudioComponent> ComboHighComponent;

	// A quiet extra percussion layer makes the meter audible during the Boss fight.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拍子时钟测试|战斗鼓点")
	bool bEnableCombatDrums = true;

	// A one-bar mastered stem runs on the audio thread and stays aligned even if the game hitches.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拍子时钟测试|战斗鼓点")
	bool bUseLoopingDrumStem = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拍子时钟测试|战斗鼓点")
	TObjectPtr<USoundBase> CombatDrumLoop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拍子时钟测试|战斗鼓点", meta=(ClampMin="0.0", ClampMax="2.0"))
	float CombatDrumStemVolume = 1.35f;

	// Slightly lowering the song gives the guide drums a clear place in the mix.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拍子时钟测试|战斗鼓点", meta=(ClampMin="0.5", ClampMax="1.0"))
	float MusicMixVolume = .78f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拍子时钟测试|战斗鼓点", meta=(ClampMin="0.0", ClampMax="1.0"))
	float StrongDrumVolume = .55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拍子时钟测试|战斗鼓点", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MiddleDrumVolume = .28f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拍子时钟测试|战斗鼓点", meta=(ClampMin="0.0", ClampMax="1.0"))
	float WeakDrumVolume = .16f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拍子时钟测试|战斗鼓点")
	TObjectPtr<USoundBase> DrumKick;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拍子时钟测试|战斗鼓点")
	TObjectPtr<USoundBase> DrumTick;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拍子时钟测试|连击音乐") TObjectPtr<USoundBase> ComboLowLoop;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拍子时钟测试|连击音乐") TObjectPtr<USoundBase> ComboSnareLoop;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拍子时钟测试|连击音乐") TObjectPtr<USoundBase> ComboHighLoop;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拍子时钟测试|连击音乐", meta=(ClampMin="0", ClampMax="1")) float ComboLowVolume = .30f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拍子时钟测试|连击音乐", meta=(ClampMin="0", ClampMax="1")) float ComboSnareVolume = .24f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "拍子时钟测试|连击音乐", meta=(ClampMin="0", ClampMax="1")) float ComboHighVolume = .16f;

private:
	// ========================================================
	// 拍子事件
	// ========================================================

	// 收到拍子事件后，把结果写入输出日志。
	UFUNCTION()
	void HandleBeat(int32 BeatIndex /* 全部拍子编号 */, int32 BeatInBar /* 当前小节内的拍子编号 */);
	UFUNCTION() void HandleRhythmFeedback(const FRhythmSuccessFeedbackData& Feedback);
	UFUNCTION() void HandlePlayerResonance(EPlayerResonanceState State);
	void StartComboLayers(float SourceStartTime);
	void ApplyComboTierAtBar();
	TWeakObjectPtr<UPlayerRhythmCombatComponent> BoundCombat;
	int32 PendingComboTier = 0;
	int32 CurrentComboTier = 0;
	bool bResonanceMusic = false;
	// Music watchdog: keeps the song playing and beat-aligned. If the voice is
	// ever dropped (voice stealing / end of file) it is restarted at the position
	// the beat clock expects, and the song loops on a whole number of bars.
	void MaintainMusic();
	double MusicClockStartAudio = 0., MusicSourceStart = 0., MusicLoopSeconds = 0., MusicBarSeconds = 2., DrumSourceStart = 0.;
	int64 MusicLoopIndex = 0;
};
