// 绑定界面控制器，将显示数据交给蓝图事件，并更新对应控件
// 布局保存在 WBP_RhythmCombatHUD 的 Designer 中，本文件不创建控件，也不查询战斗组件
#include "UI/View/RhythmCombatWidget.h"
#include "UI/Controller/RhythmWidgetController.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

// 更换界面使用的控制器
void URhythmCombatWidget::SetWidgetController(URhythmWidgetController* InController)
{
	// 先解除旧控制器的通知绑定
	if (WidgetController)
		WidgetController->OnViewModelChanged.RemoveDynamic(this, &ThisClass::ReceiveModel);
	// 保存新控制器，再接收它提供的数据
	WidgetController = InController;
	if (WidgetController)
	{
		WidgetController->OnViewModelChanged.AddUniqueDynamic(this, &ThisClass::ReceiveModel);
		ReceiveModel(WidgetController->ViewModel); // 新订阅的界面立即得到当前值，不必等下一次变化
	}
}

// 缓存控制器提供的数据，再通知蓝图更新显示
void URhythmCombatWidget::ReceiveModel(const FRhythmHUDViewModel& Model)
{
	// 先保存整份显示数据，再调用蓝图事件，让一次刷新读取同一份数据
	ViewModel = Model;
	OnViewModelUpdated();
}

// 关闭界面时停止接收控制器通知
void URhythmCombatWidget::NativeDestruct()
{
	if (WidgetController)
		WidgetController->OnViewModelChanged.RemoveDynamic(this, &ThisClass::ReceiveModel);
	WidgetController = nullptr;
	Super::NativeDestruct();
}

// 更新“跳过教程”区域在屏幕上的位置
void URhythmCombatWidget::NativeTick(const FGeometry& Geometry, float Delta)
{
	Super::NativeTick(Geometry, Delta);
	if (!WidgetController || !WidgetTree)
		return;
	auto* Skip = WidgetTree->FindWidget(TEXT("SkipRegion"));
	if (Skip && ViewModel.bTutorialVisible && !ViewModel.bDead)
	{
		FVector2D MinPixel, MinViewport, MaxPixel, MaxViewport;
		// 把蓝图中跳过按钮的实际布局换算为屏幕像素，输入检测与界面位置保持一致
		const FGeometry& G = Skip->GetCachedGeometry();
		USlateBlueprintLibrary::LocalToViewport(this, G, FVector2D::ZeroVector, MinPixel, MinViewport);
		USlateBlueprintLibrary::LocalToViewport(this, G, G.GetLocalSize(), MaxPixel, MaxViewport);
		WidgetController->SetTutorialSkipScreenRect(MinPixel, MaxPixel);
	}
	else
		WidgetController->SetTutorialSkipScreenRect(FVector2D::ZeroVector, FVector2D::ZeroVector);
}

// 把显示数据写入对应的文本、进度条和图像控件
void URhythmCombatWidget::RefreshViewFromModel()
{
	if (!WidgetTree)
		return;
	const auto& M = ViewModel;
	auto Find = [&](FName Name)
	{
		return WidgetTree->FindWidget(Name);
	};
	// 按蓝图控件名称查找并更新，控件缺失时跳过，不在运行时重建布局
	auto Text = [&](FName Name, const FText& Value)
	{
		if (auto* W = Cast<UTextBlock>(Find(Name)); W && !W->GetText().EqualTo(Value))
			W->SetText(Value);
	};
	auto Percent = [&](FName Name, float Value)
	{
		if (auto* W = Cast<UProgressBar>(Find(Name)))
			W->SetPercent(FMath::Clamp(Value, 0.f, 1.f));
	};
	auto Alpha = [&](FName Name, float Value)
	{
		if (auto* W = Find(Name))
			W->SetRenderOpacity(FMath::Clamp(Value, 0.f, 1.f));
	};
	auto Visible = [&](FName Name, bool Show)
	{
		if (auto* W = Find(Name))
			W->SetVisibility(Show ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	};
	// 玩家与判定：只设置文字、百分比和可见性，死亡不会在界面中结算
	Visible(TEXT("GameplayRoot"), M.bHasPlayer && !M.bDead);
	Alpha(TEXT("GameplayRoot"), M.Opacity);
	Visible(TEXT("DeathPanel"), M.bDead);
	Percent(TEXT("PlayerHealth"), M.HealthFraction);
	Percent(TEXT("Resonance"), M.ResonanceFraction);
	Text(TEXT("HealthText"), M.HealthText);
	Text(TEXT("ModeText"), M.ModeText);
	Text(TEXT("AttackText"), M.AttackText);
	Text(TEXT("ComboText"), M.ComboText);
	Text(TEXT("JudgmentText"), M.JudgmentText);
	Alpha(TEXT("JudgmentText"), M.JudgmentOpacity);
	if (auto* W = Cast<UTextBlock>(Find(TEXT("JudgmentText"))))
		W->SetColorAndOpacity(M.JudgmentColor);
	for (int32 I = 0; I < 4; ++I)
		Text(*FString::Printf(TEXT("History%d"), I),
		     M.JudgmentHistory.IsValidIndex(I) ? M.JudgmentHistory[I] : FText::GetEmpty());
	// 敌人：独立控制敌人面板及延迟血条
	Visible(TEXT("BossPanel"), M.bBossVisible);
	Percent(TEXT("BossHealth"), M.BossHealthFraction);
	Percent(TEXT("BossTrail"), M.BossHealthTrail);
	Text(TEXT("BossName"), M.BossName);
	Text(TEXT("BossHint"), M.BossHint);
	Visible(TEXT("RhythmPanel"), M.bClockRunning);
	if (auto* Gate = Find(TEXT("JudgeGate")))
		Gate->SetRenderScale(FVector2D(1.f + M.BeatPulse * .12f));
	// 轨道两端取自 Designer 中的控件位置，修改蓝图布局后标记仍能沿轨道移动
	FVector2D GatePos(160, 80), EndPos(780, 80);
	if (auto* W = Find(TEXT("JudgeGate")))
		if (auto* S = Cast<UCanvasPanelSlot>(W->Slot))
			GatePos = S->GetPosition();
	if (auto* W = Find(TEXT("RailEnd")))
		if (auto* S = Cast<UCanvasPanelSlot>(W->Slot))
			EndPos = S->GetPosition();
	for (int32 I = 0; I < 8; ++I)
	{
		const FName NoteName(*FString::Printf(TEXT("Note%d"), I)),
		    BandName(*FString::Printf(TEXT("Hold%d"), I));
		const bool Has = M.Notes.IsValidIndex(I);
		Visible(NoteName, Has);
		Visible(BandName, Has && M.Notes[I].bRelease && M.Notes[I].HoldEnd > M.Notes[I].HoldStart);
		if (!Has)
			continue;
		const auto& N = M.Notes[I];
		if (auto* W = Find(NoteName))
		{
			W->SetRenderOpacity(N.Opacity);
			if (auto* S = Cast<UCanvasPanelSlot>(W->Slot))
				S->SetPosition(FVector2D(FMath::Lerp(GatePos.X, EndPos.X, N.Position), GatePos.Y));
			if (auto* Img = Cast<UImage>(W))
				Img->SetColorAndOpacity(N.bHold ? FLinearColor(.2f, .85f, 1) : FLinearColor(.7f, .9f, 1));
		}
		if (auto* W = Find(BandName))
			if (auto* S = Cast<UCanvasPanelSlot>(W->Slot))
			{
				S->SetPosition({FMath::Lerp(GatePos.X, EndPos.X, N.HoldStart), GatePos.Y - 5.f});
				S->SetSize({(EndPos.X - GatePos.X) * (N.HoldEnd - N.HoldStart), 10.f});
			}
	}
	// 主角节拍：两侧标记向中心靠拢，中心按音乐拍点缩放
	Visible(TEXT("AttackPulsePanel"), M.bClockRunning);
	if (auto* W = Find(TEXT("AttackCore")))
		W->SetRenderScale(FVector2D(1.f + M.BeatPulse * .18f));
	for (int32 I = 0; I < 6; ++I)
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const FName Name(*FString::Printf(TEXT("AttackBeat%d_%d"), I, Side));
			Visible(Name, M.AttackBeats.IsValidIndex(I));
			if (!M.AttackBeats.IsValidIndex(I))
				continue;
			if (auto* W = Find(Name))
			{
				W->SetRenderOpacity(M.AttackBeats[I].Opacity);
				if (auto* S = Cast<UCanvasPanelSlot>(W->Slot))
					S->SetPosition(
					    {230.f + (Side ? 1.f : -1.f) * (30.f + M.AttackBeats[I].Position * 190.f), 30.f});
			}
		}
	for (int32 I = 0; I < 18; ++I)
		Percent(*FString::Printf(TEXT("Spectrum%d"), I), M.Spectrum.IsValidIndex(I) ? M.Spectrum[I] : 0.f);
	// 将教程文字和图标填入卡片控件
	Visible(TEXT("TutorialPanel"), M.bTutorialVisible);
	Text(TEXT("TutorialTitle"), M.TutorialTitle);
	Text(TEXT("TutorialBody"), M.TutorialBody);
	Text(TEXT("TutorialProgress"), M.TutorialProgress);
	Text(TEXT("TutorialSkip"), M.TutorialSkip);
	Percent(TEXT("SkipProgress"), M.TutorialSkipFraction);
	Text(TEXT("CalloutText"), M.Callout);
	Alpha(TEXT("CalloutText"), M.CalloutOpacity);
	for (int32 I = 0; I < 4; ++I)
	{
		const FName Name(*FString::Printf(TEXT("Glyph%d"), I));
		Visible(Name, M.TutorialGlyphs.IsValidIndex(I));
		if (auto* Img = Cast<UImage>(Find(Name)); Img && M.TutorialGlyphs.IsValidIndex(I))
		{
			const FString Glyph = M.TutorialGlyphs[I].ToString();
			// 图标标识变化时才加载贴图，避免每帧重复加载相同资源
			const FString Path = FString::Printf(TEXT("/Game/UI/Tutorial/%s.%s"), *Glyph, *Glyph);
			if (!Img->GetBrush().GetResourceObject() ||
			    Img->GetBrush().GetResourceObject()->GetPathName() != Path)
				Img->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr, *Path), false);
		}
	}
	// 世界位置提示：投影得到的是像素，Canvas 控件位置需要除以界面缩放比例
	auto PlaceScreen = [&](FName Name, FVector2D Pixel)
	{
		if (auto* W = Find(Name))
			if (auto* S = Cast<UCanvasPanelSlot>(W->Slot))
				S->SetPosition(Pixel / FMath::Max(.01f, UWidgetLayoutLibrary::GetViewportScale(this)));
	};
	Visible(TEXT("LockMarker"), M.bLockVisible);
	PlaceScreen(TEXT("LockMarker"), M.LockPosition);
	Alpha(TEXT("CounterMarker"), M.CounterOpacity);
	PlaceScreen(TEXT("CounterMarker"), M.CounterPosition);
	Text(TEXT("ImpactText"), M.ImpactText);
	Alpha(TEXT("ImpactText"), M.ImpactOpacity);
	PlaceScreen(TEXT("ImpactText"), M.ImpactPosition);
	if (auto* W = Find(TEXT("ImpactText")))
		W->SetRenderScale(FVector2D(M.ImpactScale));
	Alpha(TEXT("ImpactFlash"), M.FlashOpacity);
	RefreshOriginalStyle();
}
