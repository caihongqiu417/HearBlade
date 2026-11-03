// 保存教程阶段与完成条件，并安排敌人陪练和界面提示
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/Data/PlayerAttackSet.h"
#include "Combat/Data/AttackData.h"
#include "Combat/Player/PlayerRhythmCombatComponent.h"
#include "CombatTutorialComponent.generated.h"

class UBeatSyncedComboComponent;
class UPlayerRhythmCombatComponent;
class UPlayerBeatAttackComponent;
struct FRhythmSuccessFeedbackData;

UENUM()
enum class ECombatTutorialStep : uint8
{
	None,
	// 暂停正常行为树，依次练习防御和基础连招，完成后重开正式战斗
	Dodge,
	Block,
	Counter,
	LightCombo,
	HeavyCombo,
	Openings,
	Resonance,
	Done
};

// 根据玩家实际操作推进教学，安排 Boss 陪练并向界面提供提示
// 完成状态仅保留到本次游戏结束，重开关卡不会重复教学
UCLASS(ClassGroup = (Combat))

class HEARBLADE_API UCombatTutorialComponent : public UActorComponent
{
	GENERATED_BODY()
  public:
	UCombatTutorialComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 界面读取的教学状态
	bool IsCardVisible() const
	{
		return Step != ECombatTutorialStep::None;
	}

	ECombatTutorialStep Step = ECombatTutorialStep::None;
	FString Title, Body;
	// 当前设备对应的按键图标，按显示顺序保存
	TArray<FName> Glyphs;
	int32 Progress = 0, Goal = 0;
	bool bGamepad = false;
	float SkipHold = 0.f; // 0..1 while the gamepad skip button is held
	// 界面传入跳过按钮的屏幕范围，按住 Alt 后可点击
	FBox2D SkipButton = FBox2D(ForceInit);
	bool bCursorMode = false, bSkipHover = false;
	double StepStartWorld = 0.;
	double SuccessFlashWorld = -1000.; // a step's goal counter just went up
	double MissWorld = -1000.;         // the last practice note was missed
	// 正式战斗中的一次性短提示
	FString Callout;
	TArray<FName> CalloutGlyphs;
	double CalloutWorld = -1000.;

  private:
	UFUNCTION() void HandleRhythmFeedback(const FRhythmSuccessFeedbackData& Feedback);
	UFUNCTION() void HandleAttackHit(AActor* Target, float Damage, EPlayerAttackGrade Grade);
	UFUNCTION() void HandleBossPhase(int32 Phase);
	void Bind();
	void Begin(ECombatTutorialStep NewStep);
	// 需要重新开始正式战斗时重载关卡
	void Finish(bool bRestartFight);
	void CountCombo(TCHAR Letter);
	void SetBossHold(bool bHold);
	void Count(int32 Amount = 1);
	void TickPractice(double WorldNow);
	void TickDevice();
	void TickCallouts(double WorldNow);
	// 陪练期间行为树暂停，由教程控制 Boss 靠近玩家
	void TickBossApproach(float DeltaTime, bool bActive);
	void RefreshGlyphs();
	ERhythmResponseType PracticeResponse() const;
	// 完成状态保存在 GameInstance，退出游戏后重置
	bool IsDone() const;
	void MarkDone() const;
	void SetCursorMode(bool bOn);

	TWeakObjectPtr<UPlayerRhythmCombatComponent> Combat;
	TWeakObjectPtr<UPlayerBeatAttackComponent> Attacks;
	TWeakObjectPtr<UBeatSyncedComboComponent> Boss;
	bool bBound = false, bBossHeld = false, bPracticePending = false;
	double NextPracticeWorld = 0., AdvanceAtWorld = -1.;
	TWeakObjectPtr<class ABossBeatDirector> Director;
	FVector MoveStart = FVector::ZeroVector;
	int32 OpeningMarksAtStart = 0;
	double LastOpeningCounted = -1000.;
	bool bCalloutResonance = false, bCalloutHold = false, bCalloutArea = false;
};
