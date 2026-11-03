#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/View/RhythmCombatWidget.h"

// 更新原版贴图控件的位置和颜色，控件布局与资源由 UMG 蓝图保存
void URhythmCombatWidget::RefreshOriginalStyle()
{
    if (!WidgetTree)
        return;
    const auto &M = ViewModel;
    auto Find = [&](FName Name) { return WidgetTree->FindWidget(Name); };
    auto Color = [](ERhythmDodgeJudgment Grade) {
        switch (Grade)
        {
        case ERhythmDodgeJudgment::Perfect:
            return FLinearColor(1, .76f, .12f);
        case ERhythmDodgeJudgment::Good:
            return FLinearColor(.22f, .78f, .90f);
        case ERhythmDodgeJudgment::GoodHold:
            return FLinearColor(.12f, .82f, 1);
        case ERhythmDodgeJudgment::PerfectHold:
            return FLinearColor(.20f, .95f, .82f);
        case ERhythmDodgeJudgment::Bad:
            return FLinearColor(.42f, .52f, .58f);
        case ERhythmDodgeJudgment::HoldBreak:
            return FLinearColor(.86f, .16f, .24f);
        default:
            return FLinearColor(.62f, .22f, .20f);
        }
    };
    // 新判定从侧栏底部进入，旧卡片上移，超过五张时让最旧一张淡出
    for (int32 I = 0; I < 6; ++I)
    {
        auto *Panel = Find(*FString::Printf(TEXT("JudgmentCard%d"), I));
        if (!Panel)
            continue;
        const FRhythmJudgmentView *Card =
            I == 5 ? (M.bHasOutgoingCard ? &M.OutgoingCard : nullptr)
                   : (M.JudgmentCards.IsValidIndex(I) ? &M.JudgmentCards[I] : nullptr);
        Panel->SetVisibility(Card ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (!Card)
            continue;
        const float Age = float(M.WorldTime - Card->Time);
        const float Row = FMath::Lerp(Card->StartSlot, Card->TargetSlot, M.CardMove);
        Panel->SetRenderTranslation({0, (Row - I) * 85.68f});
        Panel->SetRenderOpacity(I == 5 ? 1.f - M.CardMove : 1.f);
        const bool Perfect =
            Card->Grade == ERhythmDodgeJudgment::Perfect || Card->Grade == ERhythmDodgeJudgment::PerfectHold;
        Panel->SetRenderScale(FVector2D((Perfect ? 1.07f : 1.f) * (Age < .18f ? M.ComboPunch : 1.f)));
        if (auto *Plate = Cast<UImage>(Find(*FString::Printf(TEXT("JudgmentPlate%d"), I))))
            Plate->SetColorAndOpacity(Color(Card->Grade));
        if (auto *Word = Cast<UImage>(Find(*FString::Printf(TEXT("JudgmentWord%d"), I))))
        {
            if (const auto *B = JudgmentWords.Find(Card->Grade))
                Word->SetBrush(*B);
            const float WordScale = Perfect                                     ? 1.18f
                                    : Card->Grade == ERhythmDodgeJudgment::Bad  ? .88f
                                    : Card->Grade == ERhythmDodgeJudgment::Miss ? .82f
                                                                                : 1.f;
            Word->SetRenderScale(FVector2D(WordScale));
        }
        if (auto *Burst = Find(*FString::Printf(TEXT("JudgmentBurst%d"), I)))
        {
            Burst->SetRenderOpacity(Perfect ? FMath::Clamp(1.f - Age / .65f, 0.f, 1.f) * .7f : 0.f);
            Burst->SetRenderScale(FVector2D(1.f + FMath::Clamp(Age, 0.f, .65f) * .4f));
        }
    }
    if (auto *W = Cast<UTextBlock>(Find(TEXT("ComboCount"))))
        W->SetText(FText::FromString(FString::Printf(TEXT("×%d"), M.ComboCount)));
    if (auto *W = Find(TEXT("ComboPanel")))
    {
        W->SetRenderScale(FVector2D(M.ComboPunch));
        W->SetRenderOpacity(M.ComboCount > 0 ? 1.f : .45f);
    }
    if (auto *Rail = Find(TEXT("JudgmentPanel")))
        Rail->SetRenderScale(FVector2D(1.f + M.BeatPulse * .033f));
    if (auto *Wave = Find(TEXT("AttackWave")))
    {
        Wave->SetRenderScale(FVector2D(1.f + (1.f - M.BeatPulse) * .8f));
        Wave->SetRenderOpacity(M.BeatPulse * .65f);
    }
    if (auto *Failure = Cast<UTextBlock>(Find(TEXT("FailureText"))))
    {
        Failure->SetText(M.FailureText);
        Failure->SetRenderOpacity(M.FailureOpacity);
    }
    for (const TCHAR *Name : {TEXT("CinematicTop"), TEXT("CinematicBottom")})
        if (auto *W = Find(Name))
            W->SetRenderOpacity(M.bHasPlayer && !M.bDead ? 1.f - M.Opacity : 0.f);
    // 攻击数据可以指定专用图标，没有覆盖时使用蓝图配置的防御类型图标
    for (int32 I = 0; I < M.Notes.Num() && I < 8; ++I)
        if (auto *W = Cast<UImage>(Find(*FString::Printf(TEXT("Note%d"), I))))
        {
            const auto &Note = M.Notes[I];
            if (Note.IconOverride)
                W->SetBrushFromTexture(Note.IconOverride);
            else if (const auto *B = ResponseIcons.Find(Note.Response))
                W->SetBrush(*B);
            W->SetColorAndOpacity(Note.Color);
            W->SetRenderScale(FVector2D(FMath::Clamp(Note.Scale, .25f, 3.f)));
        }
    // 世界坐标已由控制器投影，这里只处理界面缩放与短暂的命中动画
    const float DPI = FMath::Max(.01f, UWidgetLayoutLibrary::GetViewportScale(this));
    auto Effect = [&](FName Name, FVector2D Position, float Opacity, float Scale) {
        if (auto *W = Find(Name))
        {
            if (auto *Slot = Cast<UCanvasPanelSlot>(W->Slot))
                Slot->SetPosition(Position / DPI);
            W->SetRenderOpacity(FMath::Clamp(Opacity, 0.f, 1.f));
            W->SetRenderScale(FVector2D(Scale));
        }
    };
    Effect(TEXT("ImpactRing"), M.ImpactPosition, M.ImpactOpacity * .55f, 1.f + M.ImpactAge * 1.8f);
    Effect(TEXT("ImpactSlash"), M.ImpactPosition, M.ImpactOpacity, M.ImpactScale);
    Effect(TEXT("TipBurst"), M.TipPosition, M.bTipVisible ? (1.f - M.TipAge / .18f) : 0.f,
           .5f + M.TipAge * 3.f);
}
