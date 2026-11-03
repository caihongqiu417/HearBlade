// 验证界面初始数据、状态更新和控制器替换
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "UI/View/RhythmCombatWidget.h"
#include "UI/Controller/RhythmWidgetController.h"

#if !UE_BUILD_SHIPPING
// 仅在带有 MVCUIVerify 参数的测试进程中运行
// 检查蓝图事件与界面控件，不改变玩法状态
bool VerifyRhythmUIBindings(URhythmCombatWidget* View, URhythmWidgetController* Controller)
{
	if (!View || !Controller || !View->WidgetTree)
		return false;
	const FRhythmHUDViewModel Original = Controller->ViewModel;
	auto* Health = Cast<UProgressBar>(View->WidgetTree->FindWidget(TEXT("PlayerHealth")));
	auto* Death = View->WidgetTree->FindWidget(TEXT("DeathPanel"));
	auto* Note = View->WidgetTree->FindWidget(TEXT("Note0"));
	if (!Health || !Death || !Note)
		return false;
	FRhythmHUDViewModel Sample = Original;
	Sample.bHasPlayer = true;
	Sample.bDead = false;
	Sample.HealthFraction = .37f;
	Sample.Notes.Reset();
	FRhythmNoteView N;
	N.Position = .5f;
	N.Opacity = 1.f;
	Sample.Notes.Add(N);
    FRhythmJudgmentView Card;
    Card.Grade = ERhythmDodgeJudgment::Perfect;
    Card.Time = Sample.WorldTime - 1.;
    Sample.JudgmentCards = {Card};
    Sample.CardMove = 1.f;
	Controller->OnViewModelChanged.Broadcast(Sample);
	bool Passed = FMath::IsNearlyEqual(Health->GetPercent(), .37f) &&
	              Death->GetVisibility() == ESlateVisibility::Collapsed &&
	              Note->GetVisibility() != ESlateVisibility::Collapsed;
	// 蓝图中保留原版卡片，图标会随防御类型变化
    auto* CardPanel = View->WidgetTree->FindWidget(TEXT("JudgmentCard0"));
    auto* Word = Cast<UImage>(View->WidgetTree->FindWidget(TEXT("JudgmentWord0")));
    auto* Icon = Cast<UImage>(Note);
    Passed &= CardPanel && CardPanel->GetVisibility() != ESlateVisibility::Collapsed;
    Passed &= Word && Word->GetBrush().GetResourceObject() &&
        Word->GetBrush().GetResourceObject()->GetName() == TEXT("T_HUD_Word_PERFECT");
    for (ERhythmResponseType Type : {ERhythmResponseType::Dodge, ERhythmResponseType::Block,
         ERhythmResponseType::Counter, ERhythmResponseType::HoldBlock, ERhythmResponseType::DodgeOrBlock})
    {
        Sample.Notes[0].Response = Type;
        Controller->OnViewModelChanged.Broadcast(Sample);
        const FSlateBrush* Brush = View->ResponseIcons.Find(Type);
        Passed &= Icon && Brush && Brush->GetResourceObject() &&
            Icon->GetBrush().GetResourceObject() == Brush->GetResourceObject();
    }
    Sample.JudgmentCards.Reset();
    Controller->OnViewModelChanged.Broadcast(Sample);
    Passed &= CardPanel && CardPanel->GetVisibility() == ESlateVisibility::Collapsed;
    Sample.bDead = true;
	Sample.Notes.Reset();
	Controller->OnViewModelChanged.Broadcast(Sample);
	Passed &= Death->GetVisibility() != ESlateVisibility::Collapsed &&
	          Note->GetVisibility() == ESlateVisibility::Collapsed;
	// 更换界面控制器后，旧控制器不能继续更新界面
	auto* Replacement = NewObject<URhythmWidgetController>(View);
	Replacement->ViewModel.HealthFraction = .62f;
	Replacement->ViewModel.bHasPlayer = true;
	View->SetWidgetController(Replacement);
	Controller->OnViewModelChanged.Broadcast(Sample);
	Passed &= FMath::IsNearlyEqual(View->ViewModel.HealthFraction, .62f);
	View->SetWidgetController(Controller);
	Controller->OnViewModelChanged.Broadcast(Original);
	Passed &= FMath::IsNearlyEqual(Health->GetPercent(), Original.HealthFraction);
	UE_LOG(
	    LogTemp, Display,
	    TEXT(
	        "RhythmMVC: Binding verification %s (initial value, Blueprint event, health, death, cue removal, "
	        "source replacement, original cards, response icons)"),
	    Passed ? TEXT("PASS") : TEXT("FAIL"));
	return Passed;
}
// 固定显示样例方便检查五张卡片和四种防御图标，只在明确指定的测试进程使用
void PreviewRestoredRhythmUI(URhythmCombatWidget* View)
{
    auto M = View->ViewModel;
    M.bHasPlayer=true; M.bDead=false; M.bTutorialVisible=false; M.Opacity=1.f;
    M.bClockRunning=true; M.bBossVisible=true; M.ComboCount=5; M.ComboPunch=1;
    M.CardMove=1; M.bHasOutgoingCard=false; M.JudgmentCards.Reset();
    M.BossName=FText::FromString(TEXT("DRAGON KNIGHT"));
    M.BossHealthFraction=.72f; M.BossHealthTrail=.76f;
    for(int32 I=0;I<5;++I)
    {
        FRhythmJudgmentView C; C.Grade=ERhythmDodgeJudgment::Perfect;
        C.Time=M.WorldTime-1.; C.StartSlot=I; C.TargetSlot=I; M.JudgmentCards.Add(C);
    }
    M.Notes.Reset();
    for(int32 I=0;I<4;++I)
    {
        FRhythmNoteView N; N.Response=ERhythmResponseType(I);
        N.Position=.2f+I*.21f; N.Opacity=1.f; M.Notes.Add(N);
    }
    View->ViewModel=M; View->RefreshViewFromModel();
}
#endif
