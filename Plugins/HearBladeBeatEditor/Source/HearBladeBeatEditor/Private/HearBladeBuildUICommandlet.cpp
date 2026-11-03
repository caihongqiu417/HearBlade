// 创建界面蓝图或通过 RestoreStyle 恢复原版样式，运行游戏时不生成布局
// 创建初始控件布局并连接蓝图事件，编译后保存为可在设计器中编辑的资源
#include "HearBladeBuildUICommandlet.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Styling/CoreStyle.h"
#include "UI/View/RhythmCombatWidget.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"

int32 RestoreHearBladeUI();

UHearBladeBuildUICommandlet::UHearBladeBuildUICommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

// 默认仅创建新蓝图，RestoreStyle 分支更新现有蓝图的原版布局
int32 UHearBladeBuildUICommandlet::Main(const FString& Params)
{
	if (Params.Contains(TEXT("RestoreStyle"))) return RestoreHearBladeUI();
	const FString PackageName = TEXT("/Game/UI/MVC/WBP_RhythmCombatHUD");
	// 已有蓝图可能包含人工布局，检测到后立即退出，不重建控件树
	if (LoadObject<UWidgetBlueprint>(nullptr, *(PackageName + TEXT(".WBP_RhythmCombatHUD"))))
	{
		UE_LOG(LogTemp, Error,
		       TEXT("RhythmMVC: Asset already exists; refusing to overwrite Designer edits."));
		return 2;
	}
	UPackage* Package = CreatePackage(*PackageName);
	auto* Factory = NewObject<UWidgetBlueprintFactory>();
	Factory->ParentClass = URhythmCombatWidget::StaticClass();
	auto* BP = Cast<UWidgetBlueprint>(Factory->FactoryCreateNew(UWidgetBlueprint::StaticClass(), Package,
	                                                            TEXT("WBP_RhythmCombatHUD"),
	                                                            RF_Public | RF_Standalone, nullptr, GWarn));
	if (!BP || !BP->WidgetTree)
		return 1;
	auto* Tree = BP->WidgetTree.Get();
	auto* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	Tree->RootWidget = Root;
	// 统一设置控件锚点、位置和尺寸，下面各类控件复用同一套布局方式
	auto Slot = [](UCanvasPanel* Parent, UWidget* W, FVector2D Pos, FVector2D Size,
	               FVector2D Anchor = FVector2D::ZeroVector, FVector2D Align = FVector2D::ZeroVector)
	{
		W->bIsVariable = true;
		auto* S = Parent->AddChildToCanvas(W);
		S->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
		S->SetAlignment(Align);
		S->SetPosition(Pos);
		S->SetSize(Size);
		return S;
	};
	auto Panel = [&](FName Name, UCanvasPanel* Parent, FVector2D Pos, FVector2D Size,
	                 FVector2D Anchor = FVector2D::ZeroVector, FVector2D Align = FVector2D::ZeroVector)
	{
		auto* W = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), Name);
		Slot(Parent, W, Pos, Size, Anchor, Align);
		return W;
	};
	auto Label = [&](FName Name, UCanvasPanel* Parent, const TCHAR* Text, FVector2D Pos, FVector2D Size,
	                 int32 FontSize, FLinearColor Color)
	{
		auto* W = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		W->SetText(FText::FromString(Text));
		W->SetFont(FSlateFontInfo(LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto")),
		                          FontSize, TEXT("Regular")));
		W->SetColorAndOpacity(Color);
		W->SetShadowOffset({1, 1});
		W->SetShadowColorAndOpacity(FLinearColor::Black);
		W->SetAutoWrapText(true);
		Slot(Parent, W, Pos, Size);
		return W;
	};
	auto Image = [&](FName Name, UCanvasPanel* Parent, FVector2D Pos, FVector2D Size, const TCHAR* Texture,
	                 FLinearColor Color)
	{
		auto* W = Tree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		if (Texture && *Texture)
			W->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr, Texture), false);
		W->SetColorAndOpacity(Color);
		Slot(Parent, W, Pos, Size);
		return W;
	};
	auto Bar = [&](FName Name, UCanvasPanel* Parent, FVector2D Pos, FVector2D Size, FLinearColor Color)
	{
		auto* W = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), Name);
		FProgressBarStyle Style = W->GetWidgetStyle();
		Style.BackgroundImage.TintColor = FSlateColor(FLinearColor(.015f, .025f, .045f, .65f));
		W->SetWidgetStyle(Style);
		W->SetPercent(.75f);
		W->SetFillColorAndOpacity(Color);
		Slot(Parent, W, Pos, Size);
		return W;
	};
	const FLinearColor Gold(1, .76f, .24f), Cyan(.12f, .85f, 1), White(.95f, .96f, 1),
	    Ink(.015f, .025f, .045f, .9f);
	auto* Game = Panel(TEXT("GameplayRoot"), Root, {0, 0}, {0, 0});
	auto* GameSlot = Cast<UCanvasPanelSlot>(Game->Slot);
	GameSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	GameSlot->SetOffsets(FMargin(0));
	auto* PlayerPanel = Panel(TEXT("PlayerPanel"), Game, {-20, -12}, {520, 250});
	Image(TEXT("PlayerFrame"), PlayerPanel, {0, 0}, {520, 208},
	      TEXT("/Game/UI/RhythmCombat/T_HUD_PlayerStatusFrame_v2.T_HUD_PlayerStatusFrame_v2"), White);
	Bar(TEXT("PlayerHealth"), PlayerPanel, {137, 81}, {341, 22}, FLinearColor(.94f, .025f, .018f));
	Bar(TEXT("Resonance"), PlayerPanel, {137, 120}, {341, 7}, Cyan);
	// 原边框贴图带有示例数字，先遮住示例，再显示真实生命值
	Image(TEXT("HealthTextBackground"), PlayerPanel, {130, 42}, {260, 36}, nullptr,
	      FLinearColor(.01f, .015f, .025f, 1.f));
	Label(TEXT("HealthText"), PlayerPanel, TEXT("HP 500 / 500"), {137, 48}, {341, 30}, 18, White);
	Label(TEXT("ResonanceLabel"), PlayerPanel, TEXT("RESONANCE"), {137, 132}, {341, 20}, 11, Cyan);
	Label(TEXT("ModeText"), PlayerPanel, TEXT("ATTACK  LMB LIGHT / RMB HEAVY"), {137, 164}, {370, 32}, 12,
	      Gold);
	Label(TEXT("AttackText"), PlayerPanel, TEXT(""), {137, 196}, {370, 28}, 13, Cyan);
	auto* BossPanel = Panel(TEXT("BossPanel"), Game, {0, 20}, {680, 140}, {.5f, 0}, {.5f, 0});
	Bar(TEXT("BossTrail"), BossPanel, {88, 51}, {503, 11}, FLinearColor(.58f, .16f, .018f));
	Bar(TEXT("BossHealth"), BossPanel, {88, 51}, {503, 11}, FLinearColor(.55f, .018f, .025f));
	Image(TEXT("BossFrame"), BossPanel, {0, 0}, {680, 113},
	      TEXT("/Game/UI/RhythmCombat/T_HUD_BossHealthFrame_Gold.T_HUD_BossHealthFrame_Gold"), White);
	Label(TEXT("BossName"), BossPanel, TEXT("DRAGON KNIGHT"), {120, 20}, {460, 30}, 16, White);
	Label(TEXT("BossHint"), BossPanel, TEXT(""), {180, 91}, {400, 35}, 20, Gold);
	auto* Judge = Panel(TEXT("JudgmentPanel"), Game, {-36, 210}, {240, 210}, {1, 0}, {1, 0});
	Label(TEXT("ComboText"), Judge, TEXT("0 COMBO"), {0, 0}, {240, 40}, 26, Gold);
	Label(TEXT("JudgmentText"), Judge, TEXT("PERFECT"), {0, 43}, {240, 35}, 20, Cyan);
	for (int32 I = 0; I < 4; ++I)
		Label(*FString::Printf(TEXT("History%d"), I), Judge, TEXT(""), {0, 84.f + I * 25}, {240, 24}, 12,
		      FLinearColor(.62f, .72f, .8f));
	// 预先创建节奏轨道控件，运行时只更新位置、尺寸和透明度
	auto* Rhythm = Panel(TEXT("RhythmPanel"), Game, {0, -8}, {850, 170}, {.5f, 1}, {.5f, 1});
	Image(TEXT("RailArt"), Rhythm, {0, -45}, {850, 283},
	      TEXT("/Game/UI/RhythmCombat/T_HUD_RhythmRail_Clean.T_HUD_RhythmRail_Clean"), White);
	for (int32 I = 0; I < 8; ++I)
		Image(*FString::Printf(TEXT("Hold%d"), I), Rhythm, {160, 75}, {0, 10}, TEXT(""),
		      FLinearColor(.08f, .72f, .9f, .7f));
	for (int32 I = 0; I < 8; ++I)
	{
		auto* N = Image(*FString::Printf(TEXT("Note%d"), I), Rhythm, {200.f + I * 70, 80}, {22, 22}, TEXT(""),
		                Cyan);
		Cast<UCanvasPanelSlot>(N->Slot)->SetAlignment({.5f, .5f});
		N->SetRenderTransformAngle(45.f);
	}
	auto* Gate = Image(TEXT("JudgeGate"), Rhythm, {212, 95}, {29, 29}, TEXT(""), Gold);
	Cast<UCanvasPanelSlot>(Gate->Slot)->SetAlignment({.5f, .5f});
	Gate->SetRenderTransformAngle(45.f);
	auto* End = Image(TEXT("RailEnd"), Rhythm, {780, 80}, {1, 1}, TEXT(""), FLinearColor::Transparent);
	End->SetRenderOpacity(0.f);
	Label(TEXT("RailLabel"), Rhythm, TEXT("TIMING"), {190, 135}, {200, 24}, 11, Gold);
	auto* Pulse = Panel(TEXT("AttackPulsePanel"), Game, {0, 0}, {460, 60}, {.5f, .72f}, {.5f, .5f});
	Image(TEXT("AttackPulseLine"), Pulse, {0, 29}, {460, 2}, TEXT(""), FLinearColor(.12f, .6f, .8f, .4f));
	auto* Core = Image(TEXT("AttackCore"), Pulse, {230, 30}, {26, 26}, TEXT(""), Gold);
	Cast<UCanvasPanelSlot>(Core->Slot)->SetAlignment({.5f, .5f});
	Core->SetRenderTransformAngle(45.f);
	for (int32 I = 0; I < 6; ++I)
		for (int32 Side = 0; Side < 2; ++Side)
		{
			auto* W = Image(*FString::Printf(TEXT("AttackBeat%d_%d"), I, Side), Pulse, {0, 30}, {3, 19},
			                TEXT(""), Cyan);
			Cast<UCanvasPanelSlot>(W->Slot)->SetAlignment({.5f, .5f});
		}
	auto* Spectrum = Panel(TEXT("SpectrumPanel"), Game, {-40, -42}, {270, 52}, {1, 1}, {1, 1});
	for (int32 I = 0; I < 18; ++I)
	{
		auto* W = Bar(*FString::Printf(TEXT("Spectrum%d"), I), Spectrum, {I * 15.f, 0}, {8, 52}, Cyan);
		W->SetBarFillType(EProgressBarFillType::BottomToTop);
	}
	// 教程文字、图标和跳过区域集中在同一面板，便于整体显示与隐藏
	auto* Tutorial = Panel(TEXT("TutorialPanel"), Game, {0, 148}, {720, 190}, {.5f, 0}, {.5f, 0});
	Image(TEXT("TutorialBackdrop"), Tutorial, {0, 0}, {720, 190},
	      TEXT("/Game/UI/Tutorial/T_TUT_Card.T_TUT_Card"), White);
	Label(TEXT("TutorialTitle"), Tutorial, TEXT("TUTORIAL"), {25, 16}, {420, 32}, 22, Gold);
	Label(TEXT("TutorialBody"), Tutorial, TEXT("Follow the rhythm."), {25, 55}, {470, 75}, 15, White);
	Label(TEXT("TutorialProgress"), Tutorial, TEXT("0 / 3"), {25, 145}, {110, 25}, 16, Gold);
	Label(TEXT("TutorialSkip"), Tutorial, TEXT("HOLD ALT + CLICK SKIP"), {145, 148}, {380, 25}, 12, White);
	Image(TEXT("SkipRegion"), Tutorial, {560, 140}, {135, 34}, TEXT(""), Ink);
	Label(TEXT("SkipLabel"), Tutorial, TEXT("SKIP"), {596, 143}, {95, 30}, 16, Gold);
	Bar(TEXT("SkipProgress"), Tutorial, {560, 177}, {135, 3}, Gold);
	for (int32 I = 0; I < 4; ++I)
		Image(*FString::Printf(TEXT("Glyph%d"), I), Tutorial, {520.f + (I % 2) * 85, 30.f + (I / 2) * 53},
		      {67, 43}, TEXT(""), White);
	auto* Callout = Label(TEXT("CalloutText"), Game, TEXT(""), {0, 0}, {720, 70}, 20, Gold);
	auto* CS = Cast<UCanvasPanelSlot>(Callout->Slot);
	CS->SetAnchors(FAnchors(.5f, .72f));
	CS->SetAlignment({.5f, .5f});
	auto* Lock = Label(TEXT("LockMarker"), Game, TEXT("▼"), {0, 0}, {32, 36}, 24, Gold);
	Cast<UCanvasPanelSlot>(Lock->Slot)->SetAlignment({.5f, 1.f});
	auto* Counter = Image(
	    TEXT("CounterMarker"), Game, {0, 0}, {64, 64},
	    TEXT("/Game/UI/RhythmCombat/Responses/T_Response_Counter_Minimal.T_Response_Counter_Minimal"), Gold);
	Cast<UCanvasPanelSlot>(Counter->Slot)->SetAlignment({.5f, .5f});
	Counter->SetRenderOpacity(0.f);
	auto* Impact = Label(TEXT("ImpactText"), Game, TEXT(""), {0, 0}, {320, 80}, 36, Gold);
	Cast<UCanvasPanelSlot>(Impact->Slot)->SetAlignment({.5f, .5f});
	auto* Flash = Image(TEXT("ImpactFlash"), Game, {0, 0}, {0, 0}, TEXT(""), White);
	auto* FS = Cast<UCanvasPanelSlot>(Flash->Slot);
	FS->SetAnchors(FAnchors(0, 0, 1, 1));
	FS->SetOffsets(FMargin(0));
	Flash->SetRenderOpacity(0);
	// 失败界面放在根画布上，显示时覆盖普通战斗界面
	auto* Death = Panel(TEXT("DeathPanel"), Root, {0, 0}, {0, 0});
	auto* DS = Cast<UCanvasPanelSlot>(Death->Slot);
	DS->SetAnchors(FAnchors(0, 0, 1, 1));
	DS->SetOffsets(FMargin(0));
	auto* Back = Image(TEXT("DeathBackdrop"), Death, {0, 0}, {0, 0}, TEXT(""), FLinearColor(0, 0, 0, .75f));
	auto* BS = Cast<UCanvasPanelSlot>(Back->Slot);
	BS->SetAnchors(FAnchors(0, 0, 1, 1));
	BS->SetOffsets(FMargin(0));
	auto* Message =
	    Label(TEXT("DeathMessage"), Death, TEXT("DEFEATED\nENTER - RETRY"), {0, 0}, {500, 140}, 36, White);
	auto* MS = Cast<UCanvasPanelSlot>(Message->Slot);
	MS->SetAnchors(FAnchors(.5f, .45f));
	MS->SetAlignment({.5f, .5f});
	Message->SetJustification(ETextJustify::Center);
	Death->SetVisibility(ESlateVisibility::Collapsed);
	// 刷新事件写入蓝图事件图，控件布局保存在资源中，可继续在设计器里编辑
	UEdGraph* Graph = BP->UbergraphPages.Num() ? BP->UbergraphPages[0] : nullptr;
	if (!Graph)
	{
		Graph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("EventGraph"), UEdGraph::StaticClass(),
		                                              UEdGraphSchema_K2::StaticClass());
		FBlueprintEditorUtils::AddUbergraphPage(BP, Graph);
	}
	auto* Event = NewObject<UK2Node_Event>(Graph);
	Event->EventReference.SetExternalMember(
	    GET_FUNCTION_NAME_CHECKED(URhythmCombatWidget, OnViewModelUpdated),
	    URhythmCombatWidget::StaticClass());
	Event->bOverrideFunction = true;
	Graph->AddNode(Event, false, false);
	Event->CreateNewGuid();
	Event->PostPlacedNewNode();
	Event->AllocateDefaultPins();
	auto* Call = NewObject<UK2Node_CallFunction>(Graph);
	Call->SetFromFunction(URhythmCombatWidget::StaticClass()->FindFunctionByName(
	    GET_FUNCTION_NAME_CHECKED(URhythmCombatWidget, RefreshViewFromModel)));
	Graph->AddNode(Call, false, false);
	Call->CreateNewGuid();
	Call->PostPlacedNewNode();
	Call->AllocateDefaultPins();
	Call->NodePosX = 340;
	const auto* Schema = GetDefault<UEdGraphSchema_K2>();
	if (!Schema->TryCreateConnection(Event->FindPin(UEdGraphSchema_K2::PN_Then), Call->GetExecPin()))
		return 3;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	// 先编译检查蓝图连接，失败时不保存半成品
	FKismetEditorUtilities::CompileBlueprint(BP);
	if (BP->Status == BS_Error || !BP->GeneratedClass)
	{
		UE_LOG(LogTemp, Error, TEXT("RhythmMVC: Blueprint compile failed"));
		return 4;
	}
	FAssetRegistryModule::AssetCreated(BP);
	Package->MarkPackageDirty();
	FSavePackageArgs Save;
	Save.TopLevelFlags = RF_Public | RF_Standalone;
	Save.SaveFlags = SAVE_NoError;
	const FString Filename =
	    FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	const bool Saved = UPackage::SavePackage(Package, BP, *Filename, Save);
	TArray<UWidget*> Widgets;
	Tree->GetAllWidgets(Widgets);
	UE_LOG(LogTemp, Display, TEXT("RhythmMVC: saved=%d widgets=%d graph=%s asset=%s"), Saved, Widgets.Num(),
	       *Graph->GetName(), *Filename);
	return Saved ? 0 : 5;
}
