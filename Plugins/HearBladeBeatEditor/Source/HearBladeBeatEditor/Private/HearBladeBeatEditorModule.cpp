#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "BeatComboEditorWidget.h"

class FHearBladeBeatEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
			TEXT("HearBladeBeatComboEditor"),
			FOnSpawnTab::CreateRaw(this, &FHearBladeBeatEditorModule::SpawnTab))
			.SetDisplayName(FText::FromString(TEXT("节拍连招编辑器")))
			.SetMenuType(ETabSpawnerMenuType::Hidden);
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(
			this, &FHearBladeBeatEditorModule::RegisterMenu));
	}

	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TEXT("HearBladeBeatComboEditor"));
	}

private:
	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs&)
	{
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)
		[
			SNew(SBeatComboEditorWidget)
		];
	}

	void RegisterMenu()
	{
		FToolMenuOwnerScoped Owner(this);
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
		FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("HearBlade"));
		Section.AddMenuEntry(
			TEXT("OpenHearBladeBeatComboEditor"),
			FText::FromString(TEXT("节拍连招编辑器")),
			FText::FromString(TEXT("预览行为树触发攻击后的目标拍点，并编辑独立攻击动作")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				FGlobalTabmanager::Get()->TryInvokeTab(FTabId(TEXT("HearBladeBeatComboEditor")));
			})));
	}
};

IMPLEMENT_MODULE(FHearBladeBeatEditorModule, HearBladeBeatEditor)
