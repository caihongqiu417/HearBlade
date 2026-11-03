// 注册节拍编辑器的菜单、窗口与模块生命周期
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "BeatComboEditorWidget.h"
#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ImageUtils.h"

class FHearBladeBeatEditorModule : public IModuleInterface
{
  public:
	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()
		    ->RegisterNomadTabSpawner(TEXT("HearBladeBeatComboEditor"),
			                          FOnSpawnTab::CreateRaw(this, &FHearBladeBeatEditorModule::SpawnTab))
		    .SetDisplayName(FText::FromString(TEXT("节拍连招编辑器")))
		    .SetMenuType(ETabSpawnerMenuType::Hidden);
		UToolMenus::RegisterStartupCallback(
		    FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FHearBladeBeatEditorModule::RegisterMenu));
		if (FParse::Param(FCommandLine::Get(), TEXT("BeatEditorAudit")))
			AuditStart = FTSTicker::GetCoreTicker().AddTicker(
			    FTickerDelegate::CreateLambda(
			        [this](float)
			        {
				        const auto Tab =
				            FGlobalTabmanager::Get()->TryInvokeTab(FTabId(TEXT("HearBladeBeatComboEditor")));
				        AuditCapture = FTSTicker::GetCoreTicker().AddTicker(
				            FTickerDelegate::CreateLambda(
				                [Tab](float)
				                {
					                TArray<FColor> Pixels;
					                FIntVector Size;
					                if (Tab.IsValid() && FSlateApplication::Get().TakeScreenshot(
					                                         Tab->GetContent(), Pixels, Size))
					                {
						                TArray64<uint8> Png;
						                FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
						                FFileHelper::SaveArrayToFile(
						                    Png, *(FPaths::ProjectSavedDir() /
											       TEXT("Screenshots/BeatEditorPhaseTwo.png")));
						                UE_LOG(LogTemp, Display, TEXT("BeatEditorAudit: screenshot %dx%d"),
										       Size.X, Size.Y);
					                }
					                FPlatformMisc::RequestExit(false);
					                return false;
				                }),
				            8.f);
				        return false;
			        }),
			    12.f);
	}

	virtual void ShutdownModule() override
	{
		FTSTicker::GetCoreTicker().RemoveTicker(AuditStart);
		FTSTicker::GetCoreTicker().RemoveTicker(AuditCapture);
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TEXT("HearBladeBeatComboEditor"));
	}

  private:
	FTSTicker::FDelegateHandle AuditStart, AuditCapture;

	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs&)
	{
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SBeatComboEditorWidget)];
	}

	void RegisterMenu()
	{
		FToolMenuOwnerScoped Owner(this);
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
		FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("HearBlade"));
		Section.AddMenuEntry(
		    TEXT("OpenHearBladeBeatComboEditor"), FText::FromString(TEXT("节拍连招编辑器")),
		    FText::FromString(TEXT("预览行为树触发攻击后的目标拍点，并编辑独立攻击动作")), FSlateIcon(),
		    FUIAction(FExecuteAction::CreateLambda(
		        []()
		        {
			        FGlobalTabmanager::Get()->TryInvokeTab(FTabId(TEXT("HearBladeBeatComboEditor")));
		        })));
	}
};

IMPLEMENT_MODULE(FHearBladeBeatEditorModule, HearBladeBeatEditor)
