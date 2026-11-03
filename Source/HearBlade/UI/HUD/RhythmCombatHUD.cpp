// 创建蓝图界面，并更新界面控制器
#include "UI/HUD/RhythmCombatHUD.h"
#include "UI/View/RhythmCombatWidget.h"
#include "UI/Controller/RhythmWidgetController.h"
#include "HighResScreenshot.h"
#include "GameFramework/PlayerController.h"
#include "HearBladePlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#if !UE_BUILD_SHIPPING
bool VerifyRhythmUIBindings(URhythmCombatWidget* View, URhythmWidgetController* Controller);
#endif

// 返回是否使用蓝图界面；返回 false 时，由调用方绘制旧版界面
bool ARhythmCombatHUD::UpdateBlueprintHUD()
{
    UpdateTouchControls();
	if (FParse::Param(FCommandLine::Get(), TEXT("LegacyHUD")))
		return false;
	if (!PlayerOwner || !PlayerOwner->IsLocalController())
		return false;
	// 只尝试加载一次，资源缺失时也不逐帧重试
	if (!bMVCLoadAttempted)
	{
		bMVCLoadAttempted = true;
		UClass* WidgetClass = LoadClass<URhythmCombatWidget>(
		    nullptr, TEXT("/Game/UI/MVC/WBP_RhythmCombatHUD.WBP_RhythmCombatHUD_C"));
		if (WidgetClass)
		{
			GetWidgetController();
			MVCWidget = CreateWidget<URhythmCombatWidget>(PlayerOwner, WidgetClass);
			if (MVCWidget)
			{
				MVCWidget->AddToViewport(10);
				MVCWidget->SetWidgetController(MVCController);
				// 控件接好通知后，立即填入当前血量、节拍和提示
				MVCController->BroadcastInitialValues();
				UE_LOG(LogTemp, Display,
				       TEXT("RhythmMVC: Widget Blueprint attached; controller initialized."));
			}
		}
		if (!MVCWidget)
			UE_LOG(LogTemp, Error,
			       TEXT("RhythmMVC: Missing /Game/UI/MVC/WBP_RhythmCombatHUD; falling back to legacy HUD."));
	}
	if (!MVCWidget || !MVCController)
		return false;
	MVCController->Update(GetWorld()->GetDeltaSeconds());
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("MVCUIVerify")) && GetWorld()->GetTimeSeconds() > 12.f &&
	    !bMVCVerifyCaptured)
	{
		bMVCVerifyCaptured = true;
		ensureAlwaysMsgf(VerifyRhythmUIBindings(MVCWidget, MVCController),
		                 TEXT("RhythmMVC binding verification failed"));
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/MVC_Runtime.png"),
		                                      true, false);
		UE_LOG(LogTemp, Display, TEXT("RhythmMVC: Runtime verify player=%d health=%.3f boss=%d notes=%d"),
		       MVCController->ViewModel.bHasPlayer, MVCController->ViewModel.HealthFraction,
		       MVCController->ViewModel.bBossVisible, MVCController->ViewModel.Notes.Num());
	}
	if (bMVCVerifyCaptured && GetWorld()->GetTimeSeconds() > 15.f)
		PlayerOwner->ConsoleCommand(TEXT("quit"));
#endif
	return true;
}

// 处理器可供蓝图界面和旧界面共用，不依赖控件资产加载成功
URhythmWidgetController* ARhythmCombatHUD::GetWidgetController()
{
    if (!MVCController && PlayerOwner && PlayerOwner->IsLocalController())
    {
        MVCController = NewObject<URhythmWidgetController>(this);
        MVCController->Initialize(PlayerOwner);
    }
    return MVCController;
}

// 触控界面由 HUD 创建和销毁，输入配置仍沿用现有控制器蓝图
void ARhythmCombatHUD::UpdateTouchControls()
{
    if (bTouchControlsChecked) return;
    auto* Player = Cast<AHearBladePlayerController>(PlayerOwner);
    if (!Player || !Player->IsLocalController()) return;
    bTouchControlsChecked = true;
    if (!Player->ShouldUseTouchControls() || !Player->GetTouchControlsClass()) return;
    TouchControls = CreateWidget<UUserWidget>(Player, Player->GetTouchControlsClass());
    if (TouchControls) TouchControls->AddToPlayerScreen(0);
}
