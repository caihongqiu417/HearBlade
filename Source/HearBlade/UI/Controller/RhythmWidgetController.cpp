// 每帧汇总玩家、判定、敌人、节拍、教程和锁定信息，生成界面显示数据
// 读取玩法状态并生成显示快照，不修改生命、伤害或节奏判定
#include "UI/Controller/RhythmWidgetController.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Combat/Boss/BeatSyncedComboComponent.h"
#include "Combat/Boss/BossBeatDirector.h"
#include "Combat/Camera/BossCombatCameraComponent.h"
#include "Combat/Tutorial/CombatTutorialComponent.h"
#include "Combat/Player/PlayerAttackComponent.h"
#include "Combat/Player/PlayerDefenseComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

// 仅允许本地玩家死亡后重新加载当前关卡
void URhythmWidgetController::RestartLevel()
{
    Rebind();
    if (!Player.IsValid() || !Player->IsLocalController() || !Combat.IsValid() || Combat->Health > 0.f) return;
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}

// 通过绑定的玩家控制器取得当前世界
UWorld* URhythmWidgetController::GetWorld() const
{
	return Player.IsValid() ? Player->GetWorld() : nullptr;
}

// 界面处理器生命周期

// 重置旧状态并绑定当前玩家
void URhythmWidgetController::Initialize(APlayerController* InPlayer)
{
	Shutdown();
	Player = InPlayer;
	Rebind();
}

// 停止频谱分析，清空玩法引用与显示缓存
void URhythmWidgetController::Shutdown()
{
	if (bSpectrumStarted && GetWorld())
		UAudioMixerBlueprintLibrary::StopAnalyzingOutput(GetWorld(), nullptr);
	bSpectrumStarted = false;
	Combat.Reset();
	Boss.Reset();
	Player.Reset();
	History.Reset();
	SmoothedSpectrum.Reset();
	LastJudgment = -1000.;
	NextBossSearch = 0.;
	ViewModel = FRhythmHUDViewModel();
}

// 玩家与敌人引用更新

// 更新角色与敌人引用，角色变化时清空旧显示状态
void URhythmWidgetController::Rebind()
{
	APawn* Pawn = Player.IsValid() ? Player->GetPawn() : nullptr;
	auto* NewCombat = Pawn ? Pawn->FindComponentByClass<UPlayerDefenseComponent>() : nullptr;
	if (Combat.Get() != NewCombat)
	{
		Combat = NewCombat;
		// 角色切换后丢弃旧判定记录，避免重生时显示上一角色的反馈
		History.Reset();
		LastJudgment = -1000.;
		ViewModel = FRhythmHUDViewModel();
		Boss.Reset();
		NextBossSearch = 0.;
	}
	// 角色缺少教程组件时创建并注册，避免重复实例化
	if (Pawn && NewCombat && !Pawn->FindComponentByClass<UCombatTutorialComponent>())
	{
		auto* Tutorial = NewObject<UCombatTutorialComponent>(Pawn, TEXT("CombatTutorial"));
		Tutorial->RegisterComponent();
	}
	if (GetWorld() && !Boss.IsValid() && GetWorld()->GetTimeSeconds() >= NextBossSearch)
	{
		// Boss 尚未出现时限频查找，避免每帧遍历场景
		NextBossSearch = GetWorld()->GetTimeSeconds() + .5;
		for (TActorIterator<ABossBeatDirector> It(GetWorld()); It; ++It)
			if (IsValid(It->BossActor))
				if (auto* Found = It->BossActor->FindComponentByClass<UBeatSyncedComboComponent>())
				{
					Boss = Found;
					ViewModel.BossHealthTrail = Found->GetBossHealthFraction();
					break;
				}
	}
}

// 以当前玩法状态刷新界面初始值
void URhythmWidgetController::BroadcastInitialValues()
{
	Update(0.f);
}

// 将有效屏幕区域传给教程组件，无效区域按空范围处理
void URhythmWidgetController::SetTutorialSkipScreenRect(FVector2D Minimum, FVector2D Maximum)
{
	APawn* Pawn = Player.IsValid() ? Player->GetPawn() : nullptr;
	if (auto* T = Pawn ? Pawn->FindComponentByClass<UCombatTutorialComponent>() : nullptr)
		T->SkipButton =
		    Maximum.X > Minimum.X && Maximum.Y > Minimum.Y ? FBox2D(Minimum, Maximum) : FBox2D(ForceInit);
}

// 汇总本帧显示数据

// 更新界面数据，再通知控件刷新
void URhythmWidgetController::Update(float Delta)
{
	Rebind();
	auto* PC = Player.Get();
	auto* State = Combat.Get();
	if (!PC || !State || !GetWorld())
	{
		ViewModel = FRhythmHUDViewModel();
		OnViewModelChanged.Broadcast(ViewModel);
		return;
	}
	// 分别更新玩家、敌人、节拍和教程的显示数据
	UpdatePlayerPanel(Delta);
	UpdateJudgments();
	UpdateBossPanel(Delta);
	UpdateRhythmTrack(Delta);
	UpdateTutorial();
	UpdateLockMarker();
	OnViewModelChanged.Broadcast(ViewModel);
}

// 玩家状态与命中反馈

// 更新玩家生命、共鸣状态和攻击反馈的显示数据
void URhythmWidgetController::UpdatePlayerPanel(float Delta)
{
	auto& M = ViewModel;
	auto* PC = Player.Get();
	auto* State = Combat.Get();
	APawn* Pawn = PC->GetPawn();
	const double Now = GetWorld()->GetTimeSeconds();
	M.bHasPlayer = true;
	M.bDead = State->Health <= 0.f;
	M.Opacity = FMath::FInterpConstantTo(M.Opacity, State->IsPhaseCinematic() ? 0.f : 1.f, Delta,
	                                     State->IsPhaseCinematic() ? 3.5f : 2.f);
	// 界面使用归一化比例，角色组件仍保存和结算真实生命值
	M.HealthFraction = FMath::Clamp(State->GetHealthFraction(), 0.f, 1.f);
	M.ResonanceFraction = FMath::Clamp(State->GetResonanceFraction(), 0.f, 1.f);
	M.HealthText = FText::FromString(FString::Printf(TEXT("HP %d / %d"), FMath::RoundToInt(State->Health),
	                                                 FMath::RoundToInt(State->MaxHealth)));
	M.ComboText = FText::FromString(FString::Printf(TEXT("%d COMBO"), State->ComboCount));
	M.ModeText = FText::FromString(State->IsDefenseInputWindowOpen() ? TEXT("DEFEND  LMB COUNTER / RMB BLOCK")
	                                                             : TEXT("ATTACK  LMB LIGHT / RMB HEAVY"));
	// 先清空短时反馈，只有仍在显示期限内的攻击结果才重新写入
	M.AttackText = FText::GetEmpty();
	M.ImpactOpacity = 0.f;
	M.bTipVisible = false;
	M.WorldTime = Now;
	M.ComboCount = State->ComboCount;
	M.FlashOpacity = 0.f;
	if (auto* Attack = Pawn->FindComponentByClass<UPlayerAttackComponent>())
	{
		if (Attack->ResonanceState == EPlayerResonanceState::Ready)
			M.ModeText = FText::FromString(TEXT("RESONANCE READY  [R / LT]"));
		if (Attack->ResonanceState == EPlayerResonanceState::Armed)
			M.ModeText = FText::FromString(TEXT("GET READY ... NEXT DOWNBEAT"));
		if (Attack->IsResonanceActive())
			M.ModeText = FText::FromString(FString::Printf(TEXT("RESONANCE  %02d BEATS"),
			                                               FMath::CeilToInt(Attack->GetResonanceRemainingBeats())));
		if (Now - Attack->LastJudgedWorldSeconds < 1.2)
			M.AttackText =
			    FText::FromString(Attack->InputSequence +
				                  (Attack->LastGrade == EPlayerAttackGrade::Perfect ? TEXT(" PERFECT")
				                   : Attack->LastGrade == EPlayerAttackGrade::Good  ? TEXT(" GOOD")
				                                                                    : TEXT(" OFF BEAT")));
		M.ImpactAge = float(Now - Attack->ImpactFxWorldTime);
        M.bImpactHeavy = Attack->bImpactFxHeavy || Attack->bImpactFxLaunch;
        M.TipAge = float(Now - Attack->TipBurstWorldTime);
        M.bTipVisible = M.TipAge >= 0.f && M.TipAge < .18f &&
            PC->ProjectWorldLocationToScreen(Attack->TipBurstWorld, M.TipPosition, true);
        const float Age = M.ImpactAge;
		if (Age >= 0.f && Age < .5f &&
		    PC->ProjectWorldLocationToScreen(Attack->ImpactFxWorld, M.ImpactPosition, true))
		{
			M.ImpactText = FText::FromString(Attack->ImpactFxWord);
			M.ImpactOpacity = 1.f - FMath::SmoothStep(.16f, .5f, Age);
			M.ImpactScale = FMath::Lerp(1.5f, 1.f, FMath::Clamp(Age / .16f, 0.f, 1.f)) *
			                FMath::Clamp(Attack->ImpactFxPower, .8f, 1.5f);
			if (Attack->bImpactFxFrame && Age < .034f)
				M.FlashOpacity = FMath::Min(.1f, .07f * Attack->ImpactFxPower);
		}
	}
}

// 评级卡片与连击

// 将最近一次判定整理成文字，并在判定时间变化时保存历史，避免每帧重复加入
void URhythmWidgetController::UpdateJudgments()
{
	auto& M = ViewModel;
	auto* State = Combat.Get();
	const double Now = GetWorld()->GetTimeSeconds();
	const auto Grade = State->LastJudgment;
	const TCHAR* GradeText = Grade == ERhythmDodgeJudgment::Perfect       ? TEXT("PERFECT")
	                         : Grade == ERhythmDodgeJudgment::Good        ? TEXT("GOOD")
	                         : Grade == ERhythmDodgeJudgment::Bad         ? TEXT("BAD")
	                         : Grade == ERhythmDodgeJudgment::Miss        ? TEXT("MISS")
	                         : Grade == ERhythmDodgeJudgment::HoldBreak   ? TEXT("HOLD BREAK")
	                         : Grade == ERhythmDodgeJudgment::PerfectHold ? TEXT("PERFECT HOLD")
	                         : Grade == ERhythmDodgeJudgment::GoodHold    ? TEXT("GOOD HOLD")
	                                                                      : TEXT("");
	M.JudgmentText =
	    FText::FromString(FString::Printf(TEXT("%s  %+.0f ms"), GradeText, State->LastJudgmentErrorMs));
	M.JudgmentColor = (Grade == ERhythmDodgeJudgment::Perfect || Grade == ERhythmDodgeJudgment::PerfectHold)
	                      ? FLinearColor(1, .76f, .2f)
	                  : (Grade == ERhythmDodgeJudgment::Good || Grade == ERhythmDodgeJudgment::GoodHold)
	                      ? FLinearColor(.2f, .9f, 1)
	                      : FLinearColor(1, .25f, .3f);
	const double JudgmentAt = State->GetLastJudgmentWorldSeconds();
	M.JudgmentOpacity = Grade != ERhythmDodgeJudgment::None
	                        ? 1.f - FMath::SmoothStep(.8f, 1.5f, float(Now - JudgmentAt))
	                        : 0.f;
	if (JudgmentAt != LastJudgment && Grade != ERhythmDodgeJudgment::None)
	{
		LastJudgment = JudgmentAt;
		// 评级仅在结果变化时入队，五张卡片从底部进入，最旧一张从顶部退出
        for (auto& Card : M.JudgmentCards) Card.StartSlot = Card.TargetSlot;
        if (M.JudgmentCards.Num() >= 5)
        {
            M.OutgoingCard = M.JudgmentCards[0];
            M.OutgoingCard.StartSlot = M.OutgoingCard.TargetSlot;
            M.OutgoingCard.TargetSlot = -1.f;
            M.bHasOutgoingCard = true;
            M.JudgmentCards.RemoveAt(0);
        }
        FRhythmJudgmentView Card;
        Card.Grade = Grade;
        Card.ErrorMs = State->LastJudgmentErrorMs;
        Card.Time = Now;
        M.JudgmentCards.Add(Card);
        for (int32 I = 0; I < M.JudgmentCards.Num(); ++I)
            M.JudgmentCards[I].TargetSlot = 5 - M.JudgmentCards.Num() + I;
        if (Grade == ERhythmDodgeJudgment::Bad || Grade == ERhythmDodgeJudgment::Miss || Grade == ERhythmDodgeJudgment::HoldBreak)
        {
            if (auto* Camera = State->GetOwner()->FindComponentByClass<UBossCombatCameraComponent>())
                Camera->TriggerFailureFeedback(Grade != ERhythmDodgeJudgment::Bad);
        }
        History.Insert(M.JudgmentText, 0);
		if (History.Num() > 4)
			History.SetNum(4);
	}
	M.JudgmentHistory = History;
    // 卡片移动与淡出使用游戏时间，只影响显示，不参与战斗评级
    const float Age = float(Now - LastJudgment);
    M.CardMove = 1.f - FMath::Pow(1.f - FMath::Clamp(Age / .14f, 0.f, 1.f), 3.f);
    if (Age >= .14f) M.bHasOutgoingCard = false;
    M.ComboPunch = Age < .18f ? 1.f + .08f * (1.f - FMath::Clamp(Age / .18f, 0.f, 1.f)) : 1.f;
    const bool Failed = Grade == ERhythmDodgeJudgment::Bad || Grade == ERhythmDodgeJudgment::Miss || Grade == ERhythmDodgeJudgment::HoldBreak;
    M.FailureOpacity = Failed ? 1.f - FMath::SmoothStep(.35f, .8f, Age) : 0.f;
    M.FailureText = FText::FromString(Grade == ERhythmDodgeJudgment::HoldBreak ? TEXT("HOLD BREAK") :
        Grade == ERhythmDodgeJudgment::Miss ? TEXT("MISS") : State->LastJudgmentErrorMs < 0.f ? TEXT("TOO EARLY") : TEXT("TOO LATE"));
}

// 敌人状态

// 整理敌人血条和阶段提示，延迟血条只用于表现，不参与敌人生命计算
void URhythmWidgetController::UpdateBossPanel(float Delta)
{
	auto& M = ViewModel;
	APawn* Pawn = Player->GetPawn();
	auto* B = Boss.Get();
	M.bBossVisible = B && IsValid(B->GetOwner()) &&
	                 FVector::DistSquared(Pawn->GetActorLocation(), B->GetOwner()->GetActorLocation()) <
	                     FMath::Square(3500.f);
	M.BossHint = FText::GetEmpty();
	if (M.bBossVisible)
	{
		M.BossHealthFraction = B->GetBossHealthFraction();
		M.BossHealthTrail = FMath::Max(
		    M.BossHealthFraction, FMath::FInterpTo(M.BossHealthTrail, M.BossHealthFraction, Delta, 2.5f));
		M.BossName = B->BossDisplayName;
		const float Area = B->GetGroundBurstRemainingBeats();
		if (Area >= 0.f)
			M.BossHint = FText::FromString(
			    FString::Printf(TEXT("离开危险圈 · %d"), FMath::Max(1, FMath::CeilToInt(Area))));
		else if (B->IsGroundBurstRecovery())
			M.BossHint = FText::FromString(TEXT("破绽 · 追击"));
		else if (B->GetPhaseTransitionProgress() >= 0.f)
			M.BossHint = FText::FromString(TEXT("PHASE II"));
	}
}

// 节拍轨道与音乐频谱

// 把音乐时间换算为轨道上的比例，同时准备反击提示、主角节拍和频谱
void URhythmWidgetController::UpdateRhythmTrack(float Delta)
{
	auto& M = ViewModel;
	auto* PC = Player.Get();
	auto* B = Boss.Get();
	M.Notes.Reset();
	M.AttackBeats.Reset();
	M.CounterOpacity = 0.f;
	if (B)
	{
		FAttackRecognitionInfo Info;
		FVector Position;
		float Progress = 0.f;
		double Key = 0.;
		if (B->GetRecognitionCue(Info, Position, Progress, Key) && Info.bCounterCue &&
		    PC->ProjectWorldLocationToScreen(Position + FVector(0, 0, 12), M.CounterPosition, true))
			M.CounterOpacity = FMath::SmoothStep(0.f, .16f, Progress);
	}
	const auto* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	M.bClockRunning = Clock && Clock->IsRunning();
	M.BeatPulse = 0.f;
	if (M.bClockRunning)
	{
		M.BeatPulse = FMath::Exp(-float(FMath::Frac(Clock->GetCurrentBeat())) * 6.5f);
		for (int32 I = 0; I < 6; ++I)
		{
			const double Slot = FMath::FloorToDouble(Clock->GetCurrentBeat() * 2.) * .5 + I * .5;
			const float Offset = float((Slot - Clock->GetCurrentBeat()) / 2.);
			if (Offset <= 0.f || Offset > 1.f)
				continue;
			FRhythmNoteView N;
			N.Position = Offset;
			N.Opacity =
			    (1.f - Offset * Offset * Offset) * (FMath::IsNearlyZero(FMath::Frac(Slot), .01) ? 1.f : .45f);
			M.AttackBeats.Add(N);
		}
		if (B)
		{
			TArray<FUpcomingImpactCue> Cues;
			B->GetUpcomingImpactCues(Cues, 8);
			const double Audio = Clock->GetAudioNowSeconds();
			const float LookAhead = FMath::Max(.01f, float(Clock->GetSecondsPerBeat() * 6.));
			for (int32 I = 0; I < Cues.Num(); ++I)
			{
				const auto& C = Cues[I];
				// 提示对准应当按键的时间，因此从命中时间中减去操作提前量
				const float Until = float(C.ImpactAudioSeconds - C.IdealDodgeLeadSeconds - Audio);
				const bool Hold = C.RequiredResponse == ERhythmResponseType::HoldBlock;
				if (Hold && (C.HoldMarker == ERhythmHoldMarkerType::None ||
				             C.HoldMarker == ERhythmHoldMarkerType::Continue))
					continue;
				if (Until < -.16f || Until > LookAhead)
					continue;
				FRhythmNoteView N;
				N.Response = C.RequiredResponse;
                N.IconOverride = C.IconOverride;
                N.Color = C.IconColor;
                N.Scale = C.IconScale;
                N.Position = Until / LookAhead;
				const float Lead = C.DamageShape == EBossAttackDamageShape::GroundBurst
				                       ? LookAhead
				                       : float(Clock->GetSecondsPerBeat() * 3.);
				N.Opacity = (1.f - FMath::SmoothStep(Lead - .35f, Lead, Until)) *
				            FMath::SmoothStep(-.16f, -.02f, Until);
				N.bHold = Hold;
				N.bRelease = Hold && C.HoldMarker == ERhythmHoldMarkerType::End;
				// 持续格挡用同组起止标记画长条，中间的连续命中不再单独画音符
				if (N.bRelease)
				{
					double StartAt = Audio;
					for (int32 J = I - 1; J >= 0; --J)
						if (Cues[J].HoldGroup == C.HoldGroup &&
						    Cues[J].HoldMarker == ERhythmHoldMarkerType::Start)
						{
							StartAt = Cues[J].ImpactAudioSeconds - Cues[J].IdealDodgeLeadSeconds;
							break;
						}
					N.HoldStart = FMath::Clamp(float((StartAt - Audio) / LookAhead), 0.f, 1.f);
					N.HoldEnd = FMath::Clamp(Until / LookAhead, 0.f, 1.f);
				}
				M.Notes.Add(N);
			}
		}
		static const TArray<float> Frequencies = {55,  75,   100,  135,  180,  240,  320,  430,  580,
		                                          780, 1050, 1400, 1900, 2550, 3400, 4600, 6200, 8400};
		if (!bSpectrumStarted)
		{
			UAudioMixerBlueprintLibrary::StartAnalyzingOutput(GetWorld(), nullptr);
			bSpectrumStarted = true;
			SmoothedSpectrum.Init(0.f, 18);
		}
		TArray<float> Values;
		UAudioMixerBlueprintLibrary::GetMagnitudeForFrequencies(GetWorld(), Frequencies, Values, nullptr);
		float Peak = 0.f;
		for (float V : Values)
			Peak = FMath::Max(Peak, V);
		// 峰值上升立即跟随、下降缓慢恢复，让频谱缩放不因每帧音量变化而闪烁
		SpectrumPeak = FMath::Max(Peak, FMath::FInterpTo(SpectrumPeak, .0025f, Delta, .65f));
		for (int32 I = 0; I < 18; ++I)
		{
			const float V =
			    Values.IsValidIndex(I)
			        ? FMath::Sqrt(FMath::Clamp(Values[I] / FMath::Max(.0001f, SpectrumPeak), 0.f, 1.f))
			        : 0.f;
			SmoothedSpectrum[I] =
			    FMath::FInterpTo(SmoothedSpectrum[I], FMath::Max(V, M.BeatPulse * .08f), Delta, 12.f);
		}
		M.Spectrum = SmoothedSpectrum;
	}
	else
	{
		M.Spectrum.Reset();
		if (bSpectrumStarted)
		{
			UAudioMixerBlueprintLibrary::StopAnalyzingOutput(GetWorld(), nullptr);
			bSpectrumStarted = false;
		}
	}
}

// 教程与锁定提示

// 整理教程卡片与短时提示，教程是否完成仍由教程组件决定
void URhythmWidgetController::UpdateTutorial()
{
	auto& M = ViewModel;
	auto* State = Combat.Get();
	APawn* Pawn = Player->GetPawn();
	const double Now = GetWorld()->GetTimeSeconds();
	M.bTutorialVisible = false;
	M.CalloutOpacity = 0.f;
	if (auto* T = Pawn->FindComponentByClass<UCombatTutorialComponent>())
	{
		M.bTutorialVisible = T->IsCardVisible();
		M.TutorialTitle = FText::FromString(T->Title);
		M.TutorialBody = FText::FromString(T->Body);
		M.TutorialProgress = FText::FromString(FString::Printf(TEXT("%d / %d"), T->Progress, T->Goal));
		M.TutorialSkip =
		    FText::FromString(T->bGamepad ? TEXT("HOLD VIEW TO SKIP") : TEXT("HOLD ALT + CLICK SKIP"));
		M.TutorialGlyphs = T->Glyphs;
		M.TutorialSkipFraction = T->SkipHold;
		M.Callout = FText::FromString(T->Callout);
		M.CalloutOpacity =
		    T->Callout.IsEmpty() ? 0.f : 1.f - FMath::SmoothStep(3.f, 4.f, float(Now - T->CalloutWorld));
	}
	const double EscapeAge = Now - State->LastRhythmicEscapeWorldSeconds;
	if (EscapeAge >= 0. && EscapeAge < 1.25)
	{
		M.Callout = FText::FromString(
		    FString::Printf(TEXT("ON BEAT  +%.0f RESONANCE"), State->LastRhythmicEscapeGain));
		M.CalloutOpacity = 1.f - FMath::SmoothStep(.8f, 1.25f, float(EscapeAge));
	}
}

// 把锁定目标头部的位置投影到屏幕，界面层再处理界面缩放
void URhythmWidgetController::UpdateLockMarker()
{
	auto& M = ViewModel;
	auto* PC = Player.Get();
	APawn* Pawn = PC->GetPawn();
	M.bLockVisible = false;
	if (auto* Camera = Pawn->FindComponentByClass<UBossCombatCameraComponent>())
		if (auto* Target = Camera->GetCombatTarget())
		{
			FVector Anchor = Target->GetActorLocation() + FVector(0, 0, 155);
			if (auto* Mesh = Target->FindComponentByClass<USkeletalMeshComponent>();
			    Mesh && Mesh->DoesSocketExist(TEXT("head")))
				Anchor = Mesh->GetSocketLocation(TEXT("head")) + FVector(0, 0, 30);
			M.bLockVisible = PC->ProjectWorldLocationToScreen(Anchor, M.LockPosition, true);
		}
}
