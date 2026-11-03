// 把原版战斗界面的贴图与布局恢复到现有 UMG 蓝图，保留蓝图事件图
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/View/RhythmCombatWidget.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"

int32 RestoreHearBladeUI()
{
    const FString PackageName = TEXT("/Game/UI/MVC/WBP_RhythmCombatHUD");
    auto *BP = LoadObject<UWidgetBlueprint>(nullptr, *(PackageName + TEXT(".WBP_RhythmCombatHUD")));
    if (!BP || !BP->WidgetTree)
        return 1;
    auto *Tree = BP->WidgetTree.Get();
    auto Find = [&](FName Name) { return Tree->FindWidget(Name); };
    auto *Game = Cast<UCanvasPanel>(Find(TEXT("GameplayRoot")));
    auto *Judge = Cast<UCanvasPanel>(Find(TEXT("JudgmentPanel")));
    if (!Game || !Judge)
        return 2;
    BP->Modify();
    Tree->Modify();
    auto Layout = [](UWidget *W, FVector2D Pos, FVector2D Size, FVector2D Anchor = FVector2D::ZeroVector,
                     FVector2D Align = FVector2D::ZeroVector) {
        if (!W)
            return;
        if (auto *S = Cast<UCanvasPanelSlot>(W->Slot))
        {
            S->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
            S->SetAlignment(Align);
            S->SetPosition(Pos);
            S->SetSize(Size);
        }
    };
    auto Brush = [](const FString &Name, bool Crop = false) {
        FSlateBrush B;
        if (Name.IsEmpty())
            return B;
        const FString Path = TEXT("/Game/UI/RhythmCombat/") + Name;
        B.SetResourceObject(
            LoadObject<UTexture2D>(nullptr, *(Path + TEXT(".") + FPaths::GetCleanFilename(Name))));
        B.DrawAs = ESlateBrushDrawType::Image;
        if (Crop)
            B.SetUVRegion(FBox2f(FVector2f(.20f, .08f), FVector2f(.80f, .90f)));
        return B;
    };
    auto Image = [&](FName Name, UCanvasPanel *Parent, FVector2D Pos, FVector2D Size, const FString &Texture,
                     FLinearColor Color = FLinearColor::White, bool Crop = false) {
        auto *W = Cast<UImage>(Find(Name));
        if (!W)
        {
            W = Tree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
            Parent->AddChildToCanvas(W);
        }
        W->bIsVariable = true;
        W->SetBrush(Brush(Texture, Crop));
        W->SetColorAndOpacity(Color);
        W->SetVisibility(ESlateVisibility::HitTestInvisible);
        Layout(W, Pos, Size);
        return W;
    };
    auto Panel = [&](FName Name, UCanvasPanel *Parent, FVector2D Pos, FVector2D Size) {
        auto *W = Cast<UCanvasPanel>(Find(Name));
        if (!W)
        {
            W = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), Name);
            Parent->AddChildToCanvas(W);
        }
        W->bIsVariable = true;
        Layout(W, Pos, Size);
        return W;
    };
    const FLinearColor Gold(1, .76f, .12f), Cyan(.12f, .85f, 1);
    // 保留原来的框体，缩小生命文字并恢复分段刻度
    auto *Player = Cast<UCanvasPanel>(Find(TEXT("PlayerPanel")));
    Layout(Player, {-28.56f, -20.16f}, {546, 218.4f});
    Layout(Find(TEXT("PlayerFrame")), {0, 0}, {546, 218.4f});
    Layout(Find(TEXT("PlayerHealth")), {143.64f, 84.84f}, {357.84f, 23.52f});
    Layout(Find(TEXT("Resonance")), {143.64f, 126}, {357.84f, 6.72f});
    if (auto *W = Find(TEXT("HealthTextBackground")))
        W->SetVisibility(ESlateVisibility::Collapsed);
    Layout(Find(TEXT("HealthText")), {422, 56}, {155, 22});
    if (auto *W = Cast<UTextBlock>(Find(TEXT("HealthText"))))
    {
        auto Font = W->GetFont();
        Font.Size = 11;
        W->SetFont(Font);
    }
    Layout(Find(TEXT("ResonanceLabel")), {144, 140}, {300, 20});
    Layout(Find(TEXT("ModeText")), {144, 178}, {380, 24});
    Layout(Find(TEXT("AttackText")), {144, 202}, {380, 24});
    for (int32 I = 1; I < 5; ++I)
        Image(*FString::Printf(TEXT("HealthDivider%d"), I), Player, {143.64f + 357.84f * I / 5.f, 84.84f},
              {2.5f, 23.52f}, TEXT(""), FLinearColor(.015f, .025f, .045f));

    Layout(Find(TEXT("BossPanel")), {0, 0}, {714, 147}, {.5f, .045f}, {.5f, 0});
    // 评级侧栏由背景、五张卡片和连击面板组成，全部可在 Designer 中编辑
    Layout(Judge, {-15, 0}, {239.4f, 621.6f}, {1, .13f}, {1, 0});
    for (const TCHAR *Name : {TEXT("ComboText"), TEXT("JudgmentText"), TEXT("History0"), TEXT("History1"),
                              TEXT("History2"), TEXT("History3")})
        if (auto *W = Find(Name))
            W->SetVisibility(ESlateVisibility::Collapsed);
    Image(TEXT("JudgmentBackbone"), Judge, {0, 0}, {239.4f, 621.6f}, TEXT("T_HUD_JudgmentRail_Backbone"));
    for (int32 I = 0; I < 6; ++I)
    {
        auto *Card = Panel(*FString::Printf(TEXT("JudgmentCard%d"), I), Judge, {94.08f, 87.36f + I * 85.68f},
                           {199.92f, 73.5f});
        Cast<UCanvasPanelSlot>(Card->Slot)->SetAlignment({.5f, .5f});
        Image(*FString::Printf(TEXT("JudgmentBurst%d"), I), Card, {-11, -74}, {222.6f, 222.6f},
              TEXT("T_HUD_JudgmentBurst"), Gold);
        Image(*FString::Printf(TEXT("JudgmentPlate%d"), I), Card, {0, 0}, {199.92f, 73.5f},
              TEXT("T_HUD_JudgmentCard"), Gold);
        Image(*FString::Printf(TEXT("JudgmentWord%d"), I), Card, {22.68f, 14.91f}, {154.56f, 43.68f},
              TEXT("T_HUD_Word_PERFECT"), FLinearColor::White, true);
        // 设计器显示五张样例卡片，运行时立即按真实判定记录更新
        Card->SetVisibility(I < 5 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    auto *Combo = Panel(TEXT("ComboPanel"), Judge, {-10.92f, 505.26f}, {210, 89.88f});
    Image(TEXT("ComboFrame"), Combo, {0, 0}, {210, 89.88f}, TEXT("T_HUD_ComboPanel"), Gold);
    Image(TEXT("ComboWord"), Combo, {28.56f, 30.66f}, {99.96f, 28.56f}, TEXT("T_HUD_Word_COMBO"),
          FLinearColor::White, true);
    auto *Count = Cast<UTextBlock>(Find(TEXT("ComboCount")));
    if (!Count)
    {
        Count = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ComboCount"));
        Combo->AddChildToCanvas(Count);
    }
    Count->bIsVariable = true;
    auto Font = Cast<UTextBlock>(Find(TEXT("ComboText")))->GetFont();
    Font.Size = 29;
    Count->SetFont(Font);
    Count->SetColorAndOpacity(Gold);
    Count->SetText(FText::FromString(TEXT("×5")));
    Layout(Count, {132, 27}, {85, 48});

    // 每类防御动作使用原来的图标，长按条仍由已有控件显示
    auto *Rhythm = Cast<UCanvasPanel>(Find(TEXT("RhythmPanel")));
    Layout(Find(TEXT("RailEnd")), {780, 95}, {1, 1});
    auto *Gate = Image(TEXT("JudgeGate"), Rhythm, {212, 95}, {52, 52}, TEXT("T_HUD_PlayerBeatRing"), Gold);
    Cast<UCanvasPanelSlot>(Gate->Slot)->SetAlignment({.5f, .5f});
    Gate->SetRenderTransformAngle(0);
    for (int32 I = 0; I < 8; ++I)
    {
        auto *Note = Image(*FString::Printf(TEXT("Note%d"), I), Rhythm, {300.f + I * 60, 95}, {52, 52},
                           TEXT("Responses/T_Response_Dodge_Minimal"));
        Cast<UCanvasPanelSlot>(Note->Slot)->SetAlignment({.5f, .5f});
        Note->SetRenderTransformAngle(0);
    }
    auto *Pulse = Cast<UCanvasPanel>(Find(TEXT("AttackPulsePanel")));
    auto *Core = Image(TEXT("AttackCore"), Pulse, {230, 30}, {64, 64}, TEXT("T_HUD_PlayerBeatRing"), Gold);
    Cast<UCanvasPanelSlot>(Core->Slot)->SetAlignment({.5f, .5f});
    Core->SetRenderTransformAngle(0);
    auto *Wave = Image(TEXT("AttackWave"), Pulse, {230, 30}, {64, 64}, TEXT("T_HUD_PlayerBeatRing"), Cyan);
    Cast<UCanvasPanelSlot>(Wave->Slot)->SetAlignment({.5f, .5f});
    Layout(Find(TEXT("SpectrumPanel")), {28, 0}, {270, 52}, {0, .48f});
    for (int32 I = 0; I < 18; ++I)
        if (auto *Bar = Cast<UProgressBar>(Find(*FString::Printf(TEXT("Spectrum%d"), I))))
            Bar->SetFillColorAndOpacity(
                FLinearColor::LerpUsingHSV(FLinearColor(1, .10f, .025f), Gold, I / 17.f));

    // 命中装饰跟随世界位置投影，图案保存在蓝图而不是运行时临时绘制
    for (const TCHAR *Name : {TEXT("ImpactRing"), TEXT("TipBurst")})
    {
        auto *W = Image(Name, Game, {0, 0}, {180, 180}, TEXT("T_HUD_JudgmentBurst"), Gold);
        Cast<UCanvasPanelSlot>(W->Slot)->SetAlignment({.5f, .5f});
        W->SetRenderOpacity(0);
    }
    auto *Slash = Image(TEXT("ImpactSlash"), Game, {0, 0}, {220, 100}, TEXT("T_HUD_AttackSlash"), Gold);
    Cast<UCanvasPanelSlot>(Slash->Slot)->SetAlignment({.5f, .5f});
    Slash->SetRenderOpacity(0);

    Tree->ForEachWidget([&](UWidget *W) {
        if (!BP->WidgetVariableNameToGuidMap.Contains(W->GetFName()))
            BP->WidgetVariableNameToGuidMap.Add(W->GetFName(), FGuid::NewGuid());
    });
    // 失误提示和过场遮幅分别更新，不随普通战斗面板一起隐藏
    auto *Failure = Cast<UTextBlock>(Find(TEXT("FailureText")));
    if (!Failure)
    {
        Failure = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("FailureText"));
        Game->AddChildToCanvas(Failure);
    }
    Failure->bIsVariable = true;
    Font.Size = 24;
    Failure->SetFont(Font);
    Failure->SetColorAndOpacity(FLinearColor(1, .45f, .08f));
    Failure->SetJustification(ETextJustify::Center);
    Layout(Failure, {0, 0}, {320, 45}, {.5f, .405f}, {.5f, .5f});
    auto *Root = Cast<UCanvasPanel>(Tree->RootWidget);
    for (int32 I = 0; I < 2; ++I)
    {
        auto *Bar = Image(I ? TEXT("CinematicBottom") : TEXT("CinematicTop"), Root, {0, 0}, {0, 0}, TEXT(""),
                          FLinearColor::Black);
        auto *Slot = Cast<UCanvasPanelSlot>(Bar->Slot);
        Slot->SetAnchors(I ? FAnchors(0, .9f, 1, 1) : FAnchors(0, 0, 1, .1f));
        Slot->SetOffsets(FMargin(0));
        Bar->SetRenderOpacity(0);
    }
    Tree->ForEachWidget([&](UWidget *W) {
        if (!BP->WidgetVariableNameToGuidMap.Contains(W->GetFName()))
            BP->WidgetVariableNameToGuidMap.Add(W->GetFName(), FGuid::NewGuid());
    });
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
    if (BP->Status == BS_Error)
        return 3;
    // 贴图表写入蓝图默认值，运行时不拼接路径加载资源
    auto *Defaults = Cast<URhythmCombatWidget>(BP->GeneratedClass->GetDefaultObject());
    if (!Defaults)
        return 4;
    Defaults->JudgmentWords.Reset();
    Defaults->JudgmentWords.Add(ERhythmDodgeJudgment::Perfect, Brush(TEXT("T_HUD_Word_PERFECT"), true));
    Defaults->JudgmentWords.Add(ERhythmDodgeJudgment::Good, Brush(TEXT("T_HUD_Word_GOOD"), true));
    Defaults->JudgmentWords.Add(ERhythmDodgeJudgment::Bad, Brush(TEXT("T_HUD_Word_BAD"), true));
    Defaults->JudgmentWords.Add(ERhythmDodgeJudgment::Miss, Brush(TEXT("T_HUD_Word_MISS"), true));
    Defaults->JudgmentWords.Add(ERhythmDodgeJudgment::PerfectHold,
                                Defaults->JudgmentWords[ERhythmDodgeJudgment::Perfect]);
    Defaults->JudgmentWords.Add(ERhythmDodgeJudgment::GoodHold,
                                Defaults->JudgmentWords[ERhythmDodgeJudgment::Good]);
    Defaults->JudgmentWords.Add(ERhythmDodgeJudgment::HoldBreak,
                                Defaults->JudgmentWords[ERhythmDodgeJudgment::Miss]);
    Defaults->ResponseIcons.Reset();
    Defaults->ResponseIcons.Add(ERhythmResponseType::Dodge,
                                Brush(TEXT("Responses/T_Response_Dodge_Minimal")));
    Defaults->ResponseIcons.Add(ERhythmResponseType::Block,
                                Brush(TEXT("Responses/T_Response_Block_Minimal")));
    Defaults->ResponseIcons.Add(ERhythmResponseType::Counter,
                                Brush(TEXT("Responses/T_Response_Counter_Minimal")));
    Defaults->ResponseIcons.Add(ERhythmResponseType::HoldBlock,
                                Brush(TEXT("Responses/T_Response_HoldBlock_Minimal")));
    Defaults->ResponseIcons.Add(ERhythmResponseType::DodgeOrBlock,
                                Defaults->ResponseIcons[ERhythmResponseType::Dodge]);
    for (const auto &Pair : Defaults->JudgmentWords)
        if (!Pair.Value.GetResourceObject())
            return 5;
    for (const auto &Pair : Defaults->ResponseIcons)
        if (!Pair.Value.GetResourceObject())
            return 6;
    BP->MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    const bool Saved = UPackage::SavePackage(
        BP->GetOutermost(), BP,
        *FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension()),
        Args);
    UE_LOG(LogTemp, Display, TEXT("RhythmRestore: saved=%d judgment brushes=%d response brushes=%d"), Saved,
           Defaults->JudgmentWords.Num(), Defaults->ResponseIcons.Num());
    return Saved ? 0 : 7;
}
