// 验证界面初始数据、状态更新和控制器替换
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "UI/View/RhythmCombatWidget.h"
#include "UI/Controller/RhythmWidgetController.h"

#if !UE_BUILD_SHIPPING
// Only runs in the explicitly requested -MVCUIVerify test process.
// Exercise the Blueprint event and UMG controls without changing gameplay state.
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
	Controller->OnViewModelChanged.Broadcast(Sample);
	bool Passed = FMath::IsNearlyEqual(Health->GetPercent(), .37f) &&
	              Death->GetVisibility() == ESlateVisibility::Collapsed &&
	              Note->GetVisibility() != ESlateVisibility::Collapsed;
	Sample.bDead = true;
	Sample.Notes.Reset();
	Controller->OnViewModelChanged.Broadcast(Sample);
	Passed &= Death->GetVisibility() != ESlateVisibility::Collapsed &&
	          Note->GetVisibility() == ESlateVisibility::Collapsed;
	// Replacing the controller must disconnect the old source.
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
	        "source replacement)"),
	    Passed ? TEXT("PASS") : TEXT("FAIL"));
	return Passed;
}
#endif
