#pragma once
#include "CoreMinimal.h"
#include "RhythmHUDViewModel.generated.h"

// 一个节拍标记的显示数据Position 为相对判定线的比例，0 表示到达判定线
USTRUCT(BlueprintType)

struct FRhythmNoteView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) float Position = 0.f;
	UPROPERTY(BlueprintReadOnly) float Opacity = 0.f;
	UPROPERTY(BlueprintReadOnly) bool bHold = false;
	UPROPERTY(BlueprintReadOnly) bool bRelease = false;
	UPROPERTY(BlueprintReadOnly) float HoldStart = 0.f;
	UPROPERTY(BlueprintReadOnly) float HoldEnd = 0.f;
};
// 一帧界面所需的数据快照蓝图读取这些值，不需要查找角色或敌人组件
USTRUCT(BlueprintType)

struct FRhythmHUDViewModel
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bHasPlayer = false;
	UPROPERTY(BlueprintReadOnly) bool bDead = false;
	UPROPERTY(BlueprintReadOnly) float Opacity = 1.f;
	UPROPERTY(BlueprintReadOnly) float HealthFraction = 0.f;
	UPROPERTY(BlueprintReadOnly) float ResonanceFraction = 0.f;
	UPROPERTY(BlueprintReadOnly) FText HealthText;
	UPROPERTY(BlueprintReadOnly) FText ModeText;
	UPROPERTY(BlueprintReadOnly) FText AttackText;
	UPROPERTY(BlueprintReadOnly) FText ComboText;
	UPROPERTY(BlueprintReadOnly) FText JudgmentText;
	UPROPERTY(BlueprintReadOnly) FLinearColor JudgmentColor = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly) float JudgmentOpacity = 0.f;
	UPROPERTY(BlueprintReadOnly) TArray<FText> JudgmentHistory;
	// 敌人：实际血量与缓慢追赶的装饰血条分别保存
	UPROPERTY(BlueprintReadOnly) bool bBossVisible = false;
	UPROPERTY(BlueprintReadOnly) float BossHealthFraction = 0.f;
	UPROPERTY(BlueprintReadOnly) float BossHealthTrail = 0.f;
	UPROPERTY(BlueprintReadOnly) FText BossName;
	UPROPERTY(BlueprintReadOnly) FText BossHint;
	// 节拍：所有位置共用音乐时钟；频谱仅用于视觉表现
	UPROPERTY(BlueprintReadOnly) bool bClockRunning = false;
	UPROPERTY(BlueprintReadOnly) float BeatPulse = 0.f;
	UPROPERTY(BlueprintReadOnly) TArray<FRhythmNoteView> Notes;
	UPROPERTY(BlueprintReadOnly) TArray<FRhythmNoteView> AttackBeats;
	UPROPERTY(BlueprintReadOnly) FVector2D CounterPosition = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly) float CounterOpacity = 0.f;
	UPROPERTY(BlueprintReadOnly) TArray<float> Spectrum;
	// 教程：文字、图标和跳过进度来自现有教程组件
	UPROPERTY(BlueprintReadOnly) bool bTutorialVisible = false;
	UPROPERTY(BlueprintReadOnly) FText TutorialTitle;
	UPROPERTY(BlueprintReadOnly) FText TutorialBody;
	UPROPERTY(BlueprintReadOnly) FText TutorialProgress;
	UPROPERTY(BlueprintReadOnly) FText TutorialSkip;
	UPROPERTY(BlueprintReadOnly) TArray<FName> TutorialGlyphs;
	UPROPERTY(BlueprintReadOnly) float TutorialSkipFraction = 0.f;
	UPROPERTY(BlueprintReadOnly) FText Callout;
	UPROPERTY(BlueprintReadOnly) float CalloutOpacity = 0.f;
	// 世界提示：这里保存屏幕像素坐标，界面层负责换算缩放
	UPROPERTY(BlueprintReadOnly) FVector2D LockPosition = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly) bool bLockVisible = false;
	UPROPERTY(BlueprintReadOnly) FVector2D ImpactPosition = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly) FText ImpactText;
	UPROPERTY(BlueprintReadOnly) float ImpactOpacity = 0.f;
	UPROPERTY(BlueprintReadOnly) float ImpactScale = 1.f;
	UPROPERTY(BlueprintReadOnly) float FlashOpacity = 0.f;
};
