// 保存教程阶段与完成条件，并安排敌人陪练和界面提示
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/Data/PlayerAttackSet.h"
#include "Combat/Data/AttackData.h"
#include "Combat/Player/PlayerDefenseComponent.h"
#include "CombatTutorialComponent.generated.h"

class UBeatSyncedComboComponent;
class UPlayerDefenseComponent;
class UPlayerAttackComponent;
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
	// 推进教程步骤、跳过操作、练习攻击与提示显示
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	// 解除教程事件并恢复临时修改的输入状态
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
	float SkipHold = 0.f; // 手柄长按跳过的进度，范围为零到一
	// 界面传入跳过按钮的屏幕范围，按住 Alt 后可点击
	FBox2D SkipButton = FBox2D(ForceInit);
	bool bCursorMode = false, bSkipHover = false;
	double StepStartWorld = 0.;
	double SuccessFlashWorld = -1000.; // 当前步骤的完成次数刚增加
	double MissWorld = -1000.;         // 上一条练习未成功完成
	// 正式战斗中的一次性短提示
	FString Callout;
	TArray<FName> CalloutGlyphs;
	double CalloutWorld = -1000.;

  private:
	// 只把符合当前练习要求的防御结果计入进度
	// 收到玩家命中后更新攻击练习进度
	UFUNCTION() void HandleRhythmFeedback(const FRhythmSuccessFeedbackData& Feedback);
	// 收到阶段变化后更新对应教程提示
	UFUNCTION() void HandleAttackHit(AActor* Target, float Damage, EPlayerAttackGrade Grade);
	// 查找玩家和敌人，绑定练习需要的结果事件
	UFUNCTION() void HandleBossPhase(int32 Phase);
	void Bind();
	// 进入指定教程步骤，重置计数与提示
	void Begin(ECombatTutorialStep NewStep);
	// 需要重新开始正式战斗时重载关卡
	void Finish(bool bRestartFight);
	// 根据攻击输入字母推进连招练习
	void CountCombo(TCHAR Letter);
	// 暂停或恢复敌人自主行动，让教程控制练习节奏
	void SetBossHold(bool bHold);
	// 累加当前步骤的完成次数
	void Count(int32 Amount = 1);
	// 按拍点安排下一次练习攻击，避免连续指令挤在一起
	void TickPractice(double WorldNow);
	// 检测输入设备变化，更新教程按键提示
	void TickDevice();
	// 更新正式战斗中的短时教学提示
	void TickCallouts(double WorldNow);
	// 陪练期间行为树暂停，由教程控制 Boss 靠近玩家
	void TickBossApproach(float DeltaTime, bool bActive);
	// 按当前输入设备更新按键图标
	void RefreshGlyphs();
	// 返回当前练习要求的防御方式
	ERhythmResponseType PracticeResponse() const;
	// 完成状态保存在 GameInstance，退出游戏后重置
	bool IsDone() const;
	// 保存教程已完成状态
	void MarkDone() const;
	// 切换教程需要的鼠标光标和输入模式
	void SetCursorMode(bool bOn);

	TWeakObjectPtr<UPlayerDefenseComponent> Combat;
	TWeakObjectPtr<UPlayerAttackComponent> Attacks;
	TWeakObjectPtr<UBeatSyncedComboComponent> Boss;
	bool bBound = false, bBossHeld = false, bPracticePending = false;
	double NextPracticeWorld = 0., AdvanceAtWorld = -1.;
	TWeakObjectPtr<class ABossBeatDirector> Director;
	FVector MoveStart = FVector::ZeroVector;
	int32 OpeningMarksAtStart = 0;
	double LastOpeningCounted = -1000.;
	bool bCalloutResonance = false, bCalloutHold = false, bCalloutArea = false;
};
