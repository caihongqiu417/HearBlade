// 根据玩家战斗进度推进教程，并保存当前卡片与跳过状态
#include "Combat/Tutorial/CombatTutorialComponent.h"
#include "Combat/Boss/BeatSyncedComboComponent.h"
#include "Combat/Boss/BossBeatDirector.h"
#include "Combat/Boss/BossLocomotionAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Combat/Player/PlayerBeatAttackComponent.h"
#include "Combat/Player/PlayerRhythmCombatComponent.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
TWeakObjectPtr<UGameInstance> GTutorialDoneFor;

struct FTutorialCard
{
	ECombatTutorialStep Step;
	const TCHAR* Title;
	const TCHAR* Body;
	const TCHAR* Keyboard; // 键盘图标资源后缀，用逗号分隔
	const TCHAR* Pad;      // 手柄图标资源后缀，用逗号分隔
	int32 Goal;            // 完成步骤所需次数，零表示按时间推进
	float Seconds;         // 最短展示时间，计时步骤到时后推进
	ECombatTutorialStep Next;
};

const FTutorialCard Cards[] = {
    {ECombatTutorialStep::Dodge, TEXT("DODGE"),
	 TEXT(
         "A broad crimson blade arc means blocking will fail. Read the blade; the rail only shows WHEN to dodge."),
	 TEXT("Shift"), TEXT("B"), 2, 2.f, ECombatTutorialStep::Block},
    {ECombatTutorialStep::Block, TEXT("BLOCK"),
	 TEXT(
         "A short pale blade glint can be blocked. Time a short BLOCK with the beat to deflect and weaken the boss's stance."),
	 TEXT("RMB"), TEXT("LB"), 2, 2.f, ECombatTutorialStep::Counter},
    {ECombatTutorialStep::Counter, TEXT("COUNTER"),
	 TEXT("A gold crossed glint signals a counter opening. Watch the blade, then strike back ON the beat."),
	 TEXT("LMB,Q"), TEXT("RB"), 2, 2.f, ECombatTutorialStep::LightCombo},
    {ECombatTutorialStep::LightCombo, TEXT("LIGHT COMBO"),
	 TEXT("Walk up to the boss and press LIGHT 4 times on the beat: a 4-hit combo."), TEXT("LMB,LMB,LMB,LMB"),
	 TEXT("X,X,X,X"), 4, 2.f, ECombatTutorialStep::HeavyCombo},
    {ECombatTutorialStep::HeavyCombo, TEXT("HEAVY COMBO"),
	 TEXT("Now press HEAVY 4 times on the beat. Slower, but every hit lands harder."),
	 TEXT("RMB,RMB,RMB,RMB"), TEXT("Y,Y,Y,Y"), 4, 2.f, ECombatTutorialStep::Openings},
    {ECombatTutorialStep::Openings, TEXT("STRIKE IN THE GAPS"),
	 TEXT(
         "After a swing, stay close and attack its exposed side. A sidestep can avoid the blade and create a brief opening."),
	 TEXT("Shift,LMB"), TEXT("B,X"), 2, 2.f, ECombatTutorialStep::Resonance},
    {ECombatTutorialStep::Resonance, TEXT("RESONANCE"),
	 TEXT(
         "On-beat play fills RESONANCE. Yours is FULL now: unleash it, then attack - every hit is stronger for 16 beats."),
	 TEXT("R"), TEXT("LT"), 1, 2.f, ECombatTutorialStep::Done},
    {ECombatTutorialStep::Done, TEXT("TUTORIAL COMPLETE"),
	 TEXT("Keep the rhythm. The real fight starts now!"), TEXT(""), TEXT(""), 0, 2.5f,
	 ECombatTutorialStep::None},
};

const FTutorialCard* CardFor(ECombatTutorialStep Step)
{
	for (const FTutorialCard& C : Cards)
		if (C.Step == Step)
			return &C;
	return nullptr;
}

void SplitGlyphs(const TCHAR* List, const TCHAR* Prefix, TArray<FName>& Out)
{
	TArray<FString> Parts;
	FString(List).ParseIntoArray(Parts, TEXT(","), true);
	for (const FString& P : Parts)
		Out.Add(FName(*(FString(Prefix) + P)));
}
}

UCombatTutorialComponent::UCombatTutorialComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

// 读取教程完成记录
bool UCombatTutorialComponent::IsDone() const
{
	// 完成状态保存在 GameInstance，重开关卡保留，退出游戏后重置
	const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	return GI && GTutorialDoneFor.Get() == GI;
}

// 保存教程已完成状态
void UCombatTutorialComponent::MarkDone() const
{
	GTutorialDoneFor = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
}

// 切换教程需要的鼠标光标和输入模式
void UCombatTutorialComponent::SetCursorMode(bool bOn)
{
	bSkipHover = false;
	if (bOn == bCursorMode)
		return;
	bCursorMode = bOn;
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC)
		return;
	PC->SetShowMouseCursor(bOn);
	if (bOn)
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
		PC->SetInputMode(Mode);
	}
	else
		PC->SetInputMode(FInputModeGameOnly());
}

// 查找玩家和敌人，绑定练习需要的结果事件
void UCombatTutorialComponent::Bind()
{
	if (!Combat.IsValid())
		Combat = GetOwner()->FindComponentByClass<UPlayerRhythmCombatComponent>();
	if (!Attacks.IsValid())
		Attacks = GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>();
	if (!Boss.IsValid())
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			if (auto* Combo = It->FindComponentByClass<UBeatSyncedComboComponent>())
			{
				Boss = Combo;
				break;
			}
	if (!Director.IsValid())
		for (TActorIterator<ABossBeatDirector> It(GetWorld()); It; ++It)
		{
			Director = *It;
			break;
		}
	if (!Combat.IsValid() || !Attacks.IsValid() || !Boss.IsValid())
		return;
	Combat->OnRhythmSuccessFeedback.AddUniqueDynamic(this, &UCombatTutorialComponent::HandleRhythmFeedback);
	Attacks->OnAttackHit.AddUniqueDynamic(this, &UCombatTutorialComponent::HandleAttackHit);
	Boss->OnPhaseChanged.AddUniqueDynamic(this, &UCombatTutorialComponent::HandleBossPhase);
	bBound = true;
}

// 暂停或恢复敌人自主行动，让教程控制练习节奏
void UCombatTutorialComponent::SetBossHold(bool bHold)
{
	if (bHold == bBossHeld)
		return;
	UBehaviorTreeComponent* BT = Director.IsValid() ? Director->BehaviorTreeComponent.Get() : nullptr;
	if (bHold)
	{
		if (BT)
			BT->PauseLogic(TEXT("Tutorial"));
		if (Boss.IsValid())
			Boss->CancelCombo();
	}
	else
	{
		TickBossApproach(0.f, false); // 停止教程移动，让行为树重新接管
		if (BT)
			BT->ResumeLogic(TEXT("Tutorial"));
	}
	bBossHeld = bHold;
	if (Boss.IsValid())
		Boss->bTutorialNoDamage = bHold;
	UE_LOG(LogTemp, Display, TEXT("Tutorial: boss %s"), bHold ? TEXT("sparring") : TEXT("released"));
}

// 进入指定教程步骤，重置计数与提示
void UCombatTutorialComponent::Begin(ECombatTutorialStep NewStep)
{
	const FTutorialCard* Card = CardFor(NewStep);
	if (!Card)
	{
		Finish(true);
		return;
	}
	Step = NewStep;
	Progress = 0;
	Goal = Card->Goal;
	AdvanceAtWorld = -1.;
	Title = Card->Title;
	Body = Card->Body;
	StepStartWorld = GetWorld()->GetTimeSeconds();
	NextPracticeWorld = StepStartWorld + 2.0;
	bPracticePending = false;
	MoveStart = GetOwner()->GetActorLocation();
	OpeningMarksAtStart = 0;
	SetBossHold(true);
	RefreshGlyphs();
	UE_LOG(LogTemp, Display, TEXT("Tutorial: step %s"), *Title);
}

// 结束教程，恢复操作与敌人行动
void UCombatTutorialComponent::Finish(bool bRestartFight)
{
	MarkDone();
	SetCursorMode(false);
	Step = ECombatTutorialStep::None;
	Glyphs.Reset();
	Goal = Progress = 0;
	SkipHold = 0.f;
	SetBossHold(false);
	if (bRestartFight && GetWorld())
	{
		// 重新开始战斗，GameInstance 中的完成状态会阻止教程重复启动
		UE_LOG(LogTemp, Display, TEXT("Tutorial: done, restarting the boss fight"));
		UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
	}
}

// 根据攻击输入字母推进连招练习
void UCombatTutorialComponent::CountCombo(TCHAR Letter)
{
	// 只统计连续相同按键的连招，中断或混入其他按键后重新计数
	if (!Attacks.IsValid() || Goal <= 0 || AdvanceAtWorld > 0.)
		return;
	int32 Length = 0;
	for (TCHAR C : Attacks->InputSequence)
	{
		if (C == TEXT(' '))
			continue;
		if (C != Letter)
		{
			Length = 0;
			break;
		}
		++Length;
	}
	const int32 NewProgress = FMath::Min(Goal, Length);
	if (NewProgress > Progress)
		SuccessFlashWorld = GetWorld()->GetTimeSeconds();
	Progress = NewProgress;
	if (Progress >= Goal)
		AdvanceAtWorld = GetWorld()->GetTimeSeconds() + 1.2;
}

// 按当前输入设备更新按键图标
void UCombatTutorialComponent::RefreshGlyphs()
{
	Glyphs.Reset();
	if (const FTutorialCard* Card = CardFor(Step))
		SplitGlyphs(bGamepad ? Card->Pad : Card->Keyboard, bGamepad ? TEXT("T_TUT_P_") : TEXT("T_TUT_K_"),
		            Glyphs);
}

// 累加当前步骤的完成次数
void UCombatTutorialComponent::Count(int32 Amount)
{
	if (Goal <= 0 || AdvanceAtWorld > 0.)
		return;
	Progress = FMath::Min(Goal, Progress + Amount);
	SuccessFlashWorld = GetWorld()->GetTimeSeconds();
	if (Progress >= Goal)
		AdvanceAtWorld = SuccessFlashWorld + .9; // 保留完成提示的展示时间
}

// 返回当前练习要求的防御方式
ERhythmResponseType UCombatTutorialComponent::PracticeResponse() const
{
	switch (Step)
	{
	case ECombatTutorialStep::Block:
		return ERhythmResponseType::Block;
	case ECombatTutorialStep::Counter:
		return ERhythmResponseType::Counter;
	default:
		return ERhythmResponseType::Dodge;
	}
}

// 只把符合当前练习要求的防御结果计入进度
void UCombatTutorialComponent::HandleRhythmFeedback(const FRhythmSuccessFeedbackData& Feedback)
{
	if (Feedback.bPlayerAttack || Step == ECombatTutorialStep::None)
		return;
	if (!Feedback.bSuccess)
	{
		MissWorld = GetWorld()->GetTimeSeconds();
		return;
	}
	const bool bMatch =
	    (Step == ECombatTutorialStep::Dodge && Feedback.Response == ERhythmResponseType::Dodge) ||
	    (Step == ECombatTutorialStep::Block && Feedback.Response == ERhythmResponseType::Block) ||
	    (Step == ECombatTutorialStep::Counter && Feedback.Response == ERhythmResponseType::Counter);
	if (bMatch)
		Count();
}

// 收到玩家命中后更新攻击练习进度
void UCombatTutorialComponent::HandleAttackHit(AActor*, float, EPlayerAttackGrade Grade)
{
	if (Step == ECombatTutorialStep::LightCombo)
		CountCombo(TEXT('L'));
	else if (Step == ECombatTutorialStep::HeavyCombo)
		CountCombo(TEXT('H'));
}

// 收到阶段变化后更新对应教程提示
void UCombatTutorialComponent::HandleBossPhase(int32 Phase)
{
	// 第二阶段改用短提示，不再插入格挡教学
}

// 检测输入设备变化，更新教程按键提示
void UCombatTutorialComponent::TickDevice()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC)
		return;
	static const FKey PadKeys[] = {EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Right,
	                               EKeys::Gamepad_FaceButton_Left,   EKeys::Gamepad_FaceButton_Top,
	                               EKeys::Gamepad_LeftShoulder,      EKeys::Gamepad_RightShoulder,
	                               EKeys::Gamepad_LeftTrigger,       EKeys::Gamepad_RightTrigger,
	                               EKeys::Gamepad_Special_Left,      EKeys::Gamepad_Special_Right};
	static const FKey DeskKeys[] = {EKeys::W,
	                                EKeys::A,
	                                EKeys::S,
	                                EKeys::D,
	                                EKeys::LeftShift,
	                                EKeys::LeftMouseButton,
	                                EKeys::RightMouseButton,
	                                EKeys::Q,
	                                EKeys::F,
	                                EKeys::R,
	                                EKeys::SpaceBar,
	                                EKeys::Tab};
	bool bPad = bGamepad;
	for (const FKey& K : PadKeys)
		if (PC->WasInputKeyJustPressed(K))
			bPad = true;
	// 摇杆需要超过阈值才切换设备提示，避免轻微漂移抢走键鼠提示
	if (FMath::Abs(PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX)) > .4f ||
	    FMath::Abs(PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftY)) > .4f)
		bPad = true;
	for (const FKey& K : DeskKeys)
		if (PC->WasInputKeyJustPressed(K))
			bPad = false;
	float MX = 0.f, MY = 0.f;
	PC->GetInputMouseDelta(MX, MY);
	if (FMath::Abs(MX) + FMath::Abs(MY) > 3.f)
		bPad = false;
	// 设备类型实际变化时才刷新按键图标
	if (bPad != bGamepad)
	{
		bGamepad = bPad;
		RefreshGlyphs();
	}
}

// 练习期间控制 Boss 接近玩家，其他时候停止教程移动
void UCombatTutorialComponent::TickBossApproach(float DeltaTime, bool bActive)
{
	AActor* BossActor = Boss.IsValid() ? Boss->GetOwner() : nullptr;
	if (!BossActor)
		return;
	UBossLocomotionAnimInstance* Anim = nullptr;
	TArray<USkeletalMeshComponent*> Meshes;
	BossActor->GetComponents(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
		if ((Anim = Cast<UBossLocomotionAnimInstance>(Mesh->GetAnimInstance())) != nullptr)
			break;
	if (!Anim)
		return;
	// 挥剑期间由战斗组件控制移动，陪练逻辑暂不靠近玩家
	const bool bSwinging = Boss->IsComboQueuedOrPlaying() && !Boss->IsComboQueued();
	// 共鸣期间由反击流程控制 Boss，避免重复修改位置
	if (Attacks.IsValid() &&
	    (Attacks->ResonanceState == EPlayerResonanceState::Armed || Attacks->IsResonanceActive()))
		return;
	if (!bActive || bSwinging || Boss->BossHealth <= 0.f)
	{
		if (!bSwinging)
			Anim->SetLocomotion(0.f, FVector::ZeroVector, DeltaTime);
		return;
	}
	const FVector ToPlayer =
	    (GetOwner()->GetActorLocation() - BossActor->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	const float Distance = ToPlayer.Size();
	if (Distance < 1.f)
		return;
	// 只转向水平朝向，靠近到剑的攻击范围
	FRotator Rot = BossActor->GetActorRotation();
	Rot.Yaw = FMath::RInterpTo(FRotator(0.f, Rot.Yaw, 0.f), ToPlayer.Rotation(), DeltaTime, 6.f).Yaw;
	BossActor->SetActorRotation(Rot);
	const bool bWalk = Distance > 320.f;
	Anim->SetLocomotion(bWalk ? 220.f : 0.f, bWalk ? FVector::ForwardVector : FVector::ZeroVector, DeltaTime);
}

// 按拍点安排下一次练习攻击，避免连续指令挤在一起
void UCombatTutorialComponent::TickPractice(double WorldNow)
{
	const bool bSparring = Step == ECombatTutorialStep::Dodge || Step == ECombatTutorialStep::Block ||
	                       Step == ECombatTutorialStep::Counter || Step == ECombatTutorialStep::Openings;
	if (!bSparring || !Boss.IsValid() || AdvanceAtWorld > 0.)
		return;
	if (Boss->IsComboQueuedOrPlaying())
		return;
	const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	if (!Clock || !Clock->IsRunning())
		return;
	if (bPracticePending) // 上一条练习刚结束，等待几拍再开始
	{
		bPracticePending = false;
		NextPracticeWorld =
		    WorldNow + Clock->GetSecondsPerBeat() * (Step == ECombatTutorialStep::Openings ? 4.0 : 2.0);
	}
	if (WorldNow < NextPracticeWorld)
		return;
	// 先靠近再出招，玩家持续远离时也限制最长等待时间
	if (Boss->GetOwner() &&
	    FVector::Dist2D(Boss->GetOwner()->GetActorLocation(), GetOwner()->GetActorLocation()) > 480.f &&
	    WorldNow < NextPracticeWorld + 6.0)
		return;
	const double Target =
	    FMath::CeilToDouble(Clock->GetCurrentBeat()) + 4.0; // 提前四拍显示练习指令
	if (Boss->QueuePracticeAttack(PracticeResponse(), Target, GetOwner()))
		bPracticePending = true;
	else
		NextPracticeWorld = WorldNow + .5;
}

// 更新正式战斗中的短时教学提示
void UCombatTutorialComponent::TickCallouts(double WorldNow)
{
	if (Step != ECombatTutorialStep::None || !Boss.IsValid() || !IsDone())
		return;
	auto Show = [&](const TCHAR* Text, const TCHAR* Kb, const TCHAR* Pad)
	{
		Callout = Text;
		CalloutWorld = WorldNow;
		CalloutGlyphs.Reset();
		SplitGlyphs(bGamepad ? Pad : Kb, bGamepad ? TEXT("T_TUT_P_") : TEXT("T_TUT_K_"), CalloutGlyphs);
	};
	TArray<FUpcomingImpactCue> Cues;
	Boss->GetUpcomingImpactCues(Cues, 4);
	for (const FUpcomingImpactCue& Cue : Cues)
	{
		if (!bCalloutHold && Cue.RequiredResponse == ERhythmResponseType::HoldBlock)
		{
			bCalloutHold = true;
			Show(TEXT("LONG BAR: hold BLOCK until it ends"), TEXT("RMB"), TEXT("LB"));
			return;
		}
		if (!bCalloutArea && Cue.DamageShape == EBossAttackDamageShape::GroundBurst)
		{
			bCalloutArea = true;
			Show(TEXT("BURNING CIRCLE: DODGE on the burst beat"), TEXT("Shift"), TEXT("B"));
			return;
		}
	}
}

// 推进教程步骤、跳过操作、练习攻击与提示显示
void UCombatTutorialComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                             FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetWorld())
		return;
	if (!bBound)
	{
		Bind();
		if (!bBound)
			return;
	}
	TickDevice();
	const double WorldNow = GetWorld()->GetTimeSeconds();
	const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();

	if (Step != ECombatTutorialStep::None)
	{
		// Boss 死亡或进入第二阶段时结束当前教学
		if (Boss.IsValid() && (Boss->BossHealth <= 0.f || Boss->BossPhase >= 2))
		{
			Finish(false);
			return;
		}
		if (!Combat.IsValid() || Combat->Health <= 0.f || !Boss.IsValid())
		{
			// 玩家死亡时退出教学，但不记录完成，下次挑战仍可学习
			Step = ECombatTutorialStep::None;
			Glyphs.Reset();
			Goal = Progress = 0;
			SkipHold = 0.f;
			SetCursorMode(false);
			SetBossHold(false);
			return;
		}
		// 读取当前玩家控制器，后面处理跳过操作
		const APawn* Pawn = Cast<APawn>(GetOwner());
		const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
		// 手柄长按 View 跳过，键鼠按住 Alt 后点击跳过按钮
		const bool bHeld = PC && PC->IsInputKeyDown(EKeys::Gamepad_Special_Left);
		SkipHold = bHeld ? FMath::Min(1.f, SkipHold + DeltaTime) : 0.f;
		if (SkipHold >= 1.f)
		{
			Finish(true);
			return;
		}
		SetCursorMode(PC && (PC->IsInputKeyDown(EKeys::LeftAlt) || PC->IsInputKeyDown(EKeys::RightAlt)));
		if (bCursorMode && PC)
		{
			float MX = 0.f, MY = 0.f;
			bSkipHover =
			    SkipButton.bIsValid && PC->GetMousePosition(MX, MY) && SkipButton.IsInside(FVector2D(MX, MY));
			if (bSkipHover && PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
			{
				Finish(true);
				return;
			}
		}
		// 先补满共鸣供玩家触发，再等待本轮共鸣结束
		if (Step == ECombatTutorialStep::Resonance && Attacks.IsValid() && Combat.IsValid() &&
		    AdvanceAtWorld < 0.)
		{
			const EPlayerResonanceState R = Attacks->ResonanceState;
			if (Progress == 0)
			{
				if (R == EPlayerResonanceState::Armed || R == EPlayerResonanceState::Active)
				{
					Progress = 1;
					SuccessFlashWorld = WorldNow;
					Body = TEXT("RESONANCE! Keep hitting the boss on the beat until it runs out.");
				}
				else
					Combat->Resonance = Combat->MaxResonance;
			}
			else if (R == EPlayerResonanceState::Filling || R == EPlayerResonanceState::Ready)
				AdvanceAtWorld = WorldNow + .6;
		}

		const FTutorialCard* Card = CardFor(Step);
		const float Age = float(WorldNow - StepStartWorld);
		if (Step == ECombatTutorialStep::Openings && Attacks.IsValid() &&
		    Attacks->LastOpeningHitWorldTime > LastOpeningCounted)
		{
			LastOpeningCounted = Attacks->LastOpeningHitWorldTime;
			if (LastOpeningCounted > StepStartWorld)
				Count();
		}
		if (Card && Card->Goal == 0 && Age >= Card->Seconds && AdvanceAtWorld < 0.)
			AdvanceAtWorld = WorldNow;
		if (AdvanceAtWorld > 0. && WorldNow >= AdvanceAtWorld && (!Card || Age >= Card->Seconds))
		{
			const ECombatTutorialStep Next = Card ? Card->Next : ECombatTutorialStep::None;
			if (Next == ECombatTutorialStep::None)
				Finish(true);
			else
				Begin(Next);
			return;
		}
		TickBossApproach(DeltaTime, true);
		TickPractice(WorldNow);
		return;
	}

	// 战斗和音乐启动后才进入第一阶段教学
	const bool bEncounter =
	    Director.IsValid() && Director->BehaviorTreeComponent && Director->BehaviorTreeComponent->IsRunning();
	if (!IsDone() && bEncounter && Clock && Clock->IsRunning())
	{
		// 仅在第一阶段且玩家存活时启动防御教学
		const bool bFirstBossPhase = Boss.IsValid() && Boss->BossPhase < 2;
		const bool bPlayerAlive = Combat.IsValid() && Combat->Health > 0.f;
		if (bFirstBossPhase && bPlayerAlive)
		{
			Begin(ECombatTutorialStep::Dodge);
			return;
		}
	}
	TickCallouts(WorldNow);
}

// 解除教程事件并恢复临时修改的输入状态
void UCombatTutorialComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetBossHold(false);
	Super::EndPlay(EndPlayReason);
}
