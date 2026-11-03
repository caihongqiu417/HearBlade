#pragma once
#include "CoreMinimal.h"
#include "Combat/Player/PlayerRhythmCombatComponent.h"
#include "RhythmHUDViewModel.generated.h"

// 评级卡片只保存显示数据，排列与动画由蓝图界面处理
USTRUCT(BlueprintType)
struct FRhythmJudgmentView
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) ERhythmDodgeJudgment Grade = ERhythmDodgeJudgment::None;
    UPROPERTY(BlueprintReadOnly) float ErrorMs = 0.f;
    UPROPERTY(BlueprintReadOnly) double Time = 0.;
    UPROPERTY(BlueprintReadOnly) float StartSlot = 5.f;
    UPROPERTY(BlueprintReadOnly) float TargetSlot = 4.f;
};

// 一个节拍标记的显示数据，Position 为相对判定线的比例，零表示到达判定线
USTRUCT(BlueprintType)

struct FRhythmNoteView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) ERhythmResponseType Response = ERhythmResponseType::Dodge;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UTexture2D> IconOverride;
    UPROPERTY(BlueprintReadOnly) FLinearColor Color = FLinearColor::White;
    UPROPERTY(BlueprintReadOnly) float Scale = 1.f;
    UPROPERTY(BlueprintReadOnly) float Position = 0.f;
	UPROPERTY(BlueprintReadOnly) float Opacity = 0.f;
	UPROPERTY(BlueprintReadOnly) bool bHold = false;
	UPROPERTY(BlueprintReadOnly) bool bRelease = false;
	UPROPERTY(BlueprintReadOnly) float HoldStart = 0.f;
	UPROPERTY(BlueprintReadOnly) float HoldEnd = 0.f;
};
// 一帧界面所需的显示数据，蓝图读取这些值，不需要查找角色或敌人组件
USTRUCT(BlueprintType)

struct FRhythmHUDViewModel
{
	GENERATED_BODY()
	// 玩家状态与生命、共鸣显示
	UPROPERTY(BlueprintReadOnly) bool bHasPlayer = false;
	UPROPERTY(BlueprintReadOnly) bool bDead = false;
	UPROPERTY(BlueprintReadOnly) float Opacity = 1.f;
	UPROPERTY(BlueprintReadOnly) float HealthFraction = 0.f;
	UPROPERTY(BlueprintReadOnly) float ResonanceFraction = 0.f;
	UPROPERTY(BlueprintReadOnly) FText HealthText;
	UPROPERTY(BlueprintReadOnly) FText ModeText;
	UPROPERTY(BlueprintReadOnly) FText AttackText;
	UPROPERTY(BlueprintReadOnly) FText ComboText;
	// 最近一次判定与历史记录
	UPROPERTY(BlueprintReadOnly) FText JudgmentText;
	UPROPERTY(BlueprintReadOnly) FLinearColor JudgmentColor = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly) float JudgmentOpacity = 0.f;
	UPROPERTY(BlueprintReadOnly) TArray<FText> JudgmentHistory;
    UPROPERTY(BlueprintReadOnly) TArray<FRhythmJudgmentView> JudgmentCards;
    UPROPERTY(BlueprintReadOnly) FRhythmJudgmentView OutgoingCard;
    UPROPERTY(BlueprintReadOnly) bool bHasOutgoingCard = false;
    UPROPERTY(BlueprintReadOnly) float CardMove = 1.f;
    UPROPERTY(BlueprintReadOnly) double WorldTime = 0.;
    UPROPERTY(BlueprintReadOnly) int32 ComboCount = 0;
    UPROPERTY(BlueprintReadOnly) float ComboPunch = 1.f;
    UPROPERTY(BlueprintReadOnly) FText FailureText;
    UPROPERTY(BlueprintReadOnly) float FailureOpacity = 0.f;
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
    UPROPERTY(BlueprintReadOnly) float ImpactAge = 1.f;
    UPROPERTY(BlueprintReadOnly) bool bImpactHeavy = false;
    UPROPERTY(BlueprintReadOnly) FVector2D TipPosition = FVector2D::ZeroVector;
    UPROPERTY(BlueprintReadOnly) float TipAge = 1.f;
    UPROPERTY(BlueprintReadOnly) bool bTipVisible = false;
	UPROPERTY(BlueprintReadOnly) float FlashOpacity = 0.f;
};
