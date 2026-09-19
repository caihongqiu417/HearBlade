#include "UI/RhythmCombatHUD.h"

#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "Combat/BeatSyncedComboComponent.h"
#include "Combat/BossBeatDirector.h"
#include "Combat/BossCombatCameraComponent.h"
#include "CanvasItem.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "GlobalRenderResources.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
void DrawSolidQuad(UCanvas* Canvas, const FVector2D& A, const FVector2D& B, const FVector2D& C,
	const FVector2D& D, const FLinearColor& Color)
{
	if (!Canvas || !Canvas->Canvas || !GWhiteTexture) return;
	FCanvasTriangleItem First(A, B, C, GWhiteTexture);
	First.SetColor(Color);
	Canvas->DrawItem(First);
	FCanvasTriangleItem Second(A, C, D, GWhiteTexture);
	Second.SetColor(Color);
	Canvas->DrawItem(Second);
}

void DrawSolidTriangle(UCanvas* Canvas, const FVector2D& A, const FVector2D& B, const FVector2D& C,
	const FLinearColor& Color)
{
	if (!Canvas || !Canvas->Canvas || !GWhiteTexture) return;
	FCanvasTriangleItem Triangle(A, B, C, GWhiteTexture);
	Triangle.SetColor(Color);
	Canvas->DrawItem(Triangle);
}
}

ARhythmCombatHUD::ARhythmCombatHUD()
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> Frame(TEXT("/Game/UI/RhythmCombat/T_HUD_PlayerStatusFrame_v2.T_HUD_PlayerStatusFrame_v2"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Burst(TEXT("/Game/UI/RhythmCombat/T_HUD_JudgmentBurst.T_HUD_JudgmentBurst"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Slash(TEXT("/Game/UI/RhythmCombat/T_HUD_ComboSlash.T_HUD_ComboSlash"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Rail(TEXT("/Game/UI/RhythmCombat/T_HUD_JudgmentRail_v2.T_HUD_JudgmentRail_v2"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Backbone(TEXT("/Game/UI/RhythmCombat/T_HUD_JudgmentRail_Backbone.T_HUD_JudgmentRail_Backbone"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Card(TEXT("/Game/UI/RhythmCombat/T_HUD_JudgmentCard.T_HUD_JudgmentCard"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> ComboPanel(TEXT("/Game/UI/RhythmCombat/T_HUD_ComboPanel.T_HUD_ComboPanel"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> PerfectWord(TEXT("/Game/UI/RhythmCombat/T_HUD_Word_PERFECT.T_HUD_Word_PERFECT"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> GoodWord(TEXT("/Game/UI/RhythmCombat/T_HUD_Word_GOOD.T_HUD_Word_GOOD"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> BadWord(TEXT("/Game/UI/RhythmCombat/T_HUD_Word_BAD.T_HUD_Word_BAD"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> MissWord(TEXT("/Game/UI/RhythmCombat/T_HUD_Word_MISS.T_HUD_Word_MISS"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> ComboWord(TEXT("/Game/UI/RhythmCombat/T_HUD_Word_COMBO.T_HUD_Word_COMBO"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> AttackTimingRail(TEXT("/Game/UI/RhythmCombat/T_HUD_AttackTimingRail.T_HUD_AttackTimingRail"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> AttackSlash(TEXT("/Game/UI/RhythmCombat/T_HUD_AttackSlash.T_HUD_AttackSlash"));
	if (Frame.Succeeded()) PlayerFrameTexture = Frame.Object;
	if (Burst.Succeeded()) JudgmentBurstTexture = Burst.Object;
	if (Slash.Succeeded()) ComboSlashTexture = Slash.Object;
	if (Rail.Succeeded()) JudgmentRailTexture = Rail.Object;
	if (Backbone.Succeeded()) JudgmentRailBackboneTexture = Backbone.Object;
	if (Card.Succeeded()) JudgmentCardTexture = Card.Object;
	if (ComboPanel.Succeeded()) ComboPanelTexture = ComboPanel.Object;
	if (PerfectWord.Succeeded()) PerfectWordTexture = PerfectWord.Object;
	if (GoodWord.Succeeded()) GoodWordTexture = GoodWord.Object;
	if (BadWord.Succeeded()) BadWordTexture = BadWord.Object;
	if (MissWord.Succeeded()) MissWordTexture = MissWord.Object;
	if (ComboWord.Succeeded()) ComboWordTexture = ComboWord.Object;
	if (AttackTimingRail.Succeeded()) AttackTimingRailTexture = AttackTimingRail.Object;
	if (AttackSlash.Succeeded()) AttackSlashTexture = AttackSlash.Object;
}

void ARhythmCombatHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !PlayerOwner) return;
	const APawn* Pawn = PlayerOwner->GetPawn();
	const UPlayerRhythmCombatComponent* State = Pawn ? Pawn->FindComponentByClass<UPlayerRhythmCombatComponent>() : nullptr;
	if (!State) return;
	// Keep the comic HUD readable without letting it dominate the combat view.
	const float Scale = FMath::Clamp(Canvas->SizeX / 1920.0f, 0.65f, 1.35f) * 0.84f;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	DrawPlayerPanel(State, Scale, Now);
	DrawAttackTimingCue(State, Scale);
	DrawLockMarker();
	DrawJudgment(State, Scale, Now);
}

void ARhythmCombatHUD::DrawLockMarker()
{
	if (!Canvas || !PlayerOwner) return;
	const APawn* Pawn = PlayerOwner->GetPawn();
	const UBossCombatCameraComponent* CombatCamera = Pawn
		? Pawn->FindComponentByClass<UBossCombatCameraComponent>() : nullptr;
	AActor* Target = CombatCamera ? CombatCamera->GetCombatTarget() : nullptr;
	if (!IsValid(Target)) return;

	FVector Anchor = Target->GetActorLocation() + FVector(0.0f, 0.0f, 155.0f);
	bool bFoundHeadSocket = false;
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(Target);
	for (const USkeletalMeshComponent* Mesh : Meshes)
	{
		if (!IsValid(Mesh) || !Mesh->DoesSocketExist(TEXT("head"))) continue;
		const FVector SocketLocation = Mesh->GetSocketLocation(TEXT("head"));
		if (!bFoundHeadSocket || SocketLocation.Z > Anchor.Z)
		{
			Anchor = SocketLocation;
			bFoundHeadSocket = true;
		}
	}
	if (bFoundHeadSocket) Anchor.Z += 30.0f;

	FVector2D ScreenPosition;
	if (!PlayerOwner->ProjectWorldLocationToScreen(Anchor, ScreenPosition, false)) return;
	const float PixelScale = FMath::Clamp(Canvas->SizeY / 1080.0f, 0.75f, 1.25f);
	const float Margin = 30.0f * PixelScale;
	if (ScreenPosition.X < Margin || ScreenPosition.X > Canvas->SizeX - Margin ||
		ScreenPosition.Y < Margin || ScreenPosition.Y > Canvas->SizeY - Margin)
	{
		return;
	}

	// The outer point touches the projected head anchor. Three nested triangles form
	// a crisp comic-style black outline, white keyline and gold lock marker.
	const FVector2D OuterLeft = ScreenPosition + FVector2D(-16.0f, -28.0f) * PixelScale;
	const FVector2D OuterRight = ScreenPosition + FVector2D(16.0f, -28.0f) * PixelScale;
	DrawSolidTriangle(Canvas, OuterLeft, OuterRight, ScreenPosition, FLinearColor(0.005f, 0.005f, 0.008f, 0.98f));

	const FVector2D WhiteTip = ScreenPosition + FVector2D(0.0f, -3.0f) * PixelScale;
	DrawSolidTriangle(Canvas,
		ScreenPosition + FVector2D(-12.5f, -24.5f) * PixelScale,
		ScreenPosition + FVector2D(12.5f, -24.5f) * PixelScale,
		WhiteTip, FLinearColor(0.98f, 0.98f, 0.95f, 1.0f));

	const FVector2D GoldTip = ScreenPosition + FVector2D(0.0f, -6.0f) * PixelScale;
	DrawSolidTriangle(Canvas,
		ScreenPosition + FVector2D(-9.5f, -21.5f) * PixelScale,
		ScreenPosition + FVector2D(9.5f, -21.5f) * PixelScale,
		GoldTip, FLinearColor(1.0f, 0.57f, 0.035f, 1.0f));
}

UBeatSyncedComboComponent* ARhythmCombatHUD::FindCueCombo()
{
	if (CachedCueCombo.IsValid()) return CachedCueCombo.Get();
	if (!GetWorld()) return nullptr;
	for (TActorIterator<ABossBeatDirector> It(GetWorld()); It; ++It)
	{
		if (IsValid(It->BossActor))
		{
			if (UBeatSyncedComboComponent* Combo = It->BossActor->FindComponentByClass<UBeatSyncedComboComponent>())
			{
				CachedCueCombo = Combo;
				return Combo;
			}
		}
	}
	return nullptr;
}

void ARhythmCombatHUD::DrawAttackTimingCue(const UPlayerRhythmCombatComponent* State, float Scale)
{
	if (!State || !Canvas || !GetWorld()) return;
	UBeatSyncedComboComponent* Combo = FindCueCombo();
	const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	if (!Combo || !Clock || !Clock->IsRunning()) return;
	TArray<FUpcomingImpactCue> Cues;
	Combo->GetUpcomingImpactCues(Cues, 4);
	if (Cues.IsEmpty()) return;

	const double AudioNow = Clock->GetAudioNowSeconds();
	const float LookAhead = FMath::Max(1.8f, static_cast<float>(Clock->GetSecondsPerBeat() * 4.0));
	const float FirstUntilInput = static_cast<float>(Cues[0].ImpactAudioSeconds - Cues[0].IdealDodgeLeadSeconds - AudioNow);
	if (FirstUntilInput > LookAhead + .2f) return;
	const float FadeIn = 1.0f - FMath::SmoothStep(LookAhead - .42f, LookAhead + .12f, FirstUntilInput);
	const float BeatPhase = static_cast<float>(Clock->GetCurrentBeat() - FMath::FloorToDouble(Clock->GetCurrentBeat()));
	const float BeatImpulse = FMath::Exp(-BeatPhase * 6.5f);
	const float IdlePulse = 1.0f + BeatImpulse * .007f;

	const float TrackWidth = 900.0f * Scale * IdlePulse;
	const float TrackHeight = TrackWidth * (724.0f / 2172.0f);
	const float TrackLeft = Canvas->SizeX * .5f - TrackWidth * .5f;
	const float TrackTop = Canvas->SizeY - TrackHeight - 8.0f * Scale;
	const float JudgeX = TrackLeft + TrackWidth * .255f;
	const float TrackRight = TrackLeft + TrackWidth * .965f;
	const float TravelWidth = TrackRight - JudgeX - 30.0f * Scale;
	const float MidY = TrackTop + TrackHeight * .493f;

	// Timing windows sit behind the transparent illustrated frame, so the world remains visible.
	const float GoodPixels = State->GetGoodWindowSeconds() / LookAhead * TravelWidth;
	const float PerfectPixels = State->GetPerfectWindowSeconds() / LookAhead * TravelWidth;
	DrawRect(FLinearColor(.03f, .70f, .92f, .11f * FadeIn), JudgeX - GoodPixels, MidY - 19 * Scale,
		GoodPixels * 2.0f, 38 * Scale);
	DrawRect(FLinearColor(1.0f, .68f, .08f, .19f * FadeIn), JudgeX - PerfectPixels, MidY - 28 * Scale,
		PerfectPixels * 2.0f, 56 * Scale);
	if (AttackTimingRailTexture)
	{
		FCanvasTileItem Frame(FVector2D(TrackLeft, TrackTop), AttackTimingRailTexture->GetResource(),
			FVector2D(TrackWidth, TrackHeight), FLinearColor(1.f, 1.f, 1.f, FadeIn * (.88f + BeatImpulse * .12f)));
		Frame.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Frame);
	}
	else
	{
		DrawRect(FLinearColor(.04f, .72f, .92f, .78f), TrackLeft, MidY - Scale, TrackWidth, 2 * Scale);
		DrawRect(FLinearColor(1.0f, .95f, .72f, 1.0f), JudgeX - Scale, MidY - 54 * Scale, 2 * Scale, 108 * Scale);
	}

	// Beat ticks show musical spacing but stay secondary to the actual attack markers.
	const int32 FirstBeat = Clock->GetNextBeatIndex();
	for (int32 Step = 0; Step < 5; ++Step)
	{
		const int32 BeatIndex = FirstBeat + Step;
		const float UntilBeat = static_cast<float>(Clock->GetBeatTimeSeconds(BeatIndex) - AudioNow);
		const float X = JudgeX + UntilBeat / LookAhead * TravelWidth;
		if (X <= JudgeX || X >= TrackRight - 10 * Scale) continue;
		const bool bDownbeat = BeatIndex % FMath::Max(1, Clock->GetBeatsPerBar()) == 0;
		DrawRect(bDownbeat ? FLinearColor(1.f, .71f, .14f, .55f) : FLinearColor(.55f, .88f, 1.f, .27f),
			X - Scale, MidY - (bDownbeat ? 19.f : 10.f) * Scale, 2 * Scale,
			(bDownbeat ? 38.f : 20.f) * Scale);
	}

	struct FBladeCueVisual
	{
		const FUpcomingImpactCue* Cue = nullptr;
		float X = 0.0f;
		float UntilIdealInput = 0.0f;
	};
	TArray<FBladeCueVisual> VisibleCues;
	for (const FUpcomingImpactCue& Cue : Cues)
	{
		// The slash reaches the gate at the same ideal input timestamp used by dodge resolution.
		const float UntilIdealInput = static_cast<float>(Cue.ImpactAudioSeconds - Cue.IdealDodgeLeadSeconds - AudioNow);
		if (UntilIdealInput < -.18f || UntilIdealInput > LookAhead + .1f) continue;
		VisibleCues.Add({ &Cue, JudgeX + UntilIdealInput / LookAhead * TravelWidth, UntilIdealInput });
	}

	// Present the combo as one connected blade phrase. Individual impacts are broad cuts in the
	// same ribbon, rather than a row of small rhythm-game notes.
	if (!VisibleCues.IsEmpty())
	{
		const float PhraseLeft = FMath::Max(JudgeX + 7.0f * Scale, VisibleCues[0].X - 17.0f * Scale);
		const float PhraseRight = FMath::Min(TrackRight, VisibleCues.Last().X + 30.0f * Scale);
		const float HalfRibbon = 9.0f * Scale;
		DrawSolidQuad(Canvas,
			FVector2D(PhraseLeft - 15 * Scale, MidY), FVector2D(PhraseLeft, MidY - HalfRibbon),
			FVector2D(PhraseRight, MidY - HalfRibbon), FVector2D(PhraseRight + 17 * Scale, MidY),
			FLinearColor(.015f, .045f, .070f, .78f * FadeIn));
		DrawSolidQuad(Canvas,
			FVector2D(PhraseLeft - 15 * Scale, MidY), FVector2D(PhraseRight + 17 * Scale, MidY),
			FVector2D(PhraseRight, MidY + HalfRibbon), FVector2D(PhraseLeft, MidY + HalfRibbon),
			FLinearColor(.010f, .025f, .045f, .84f * FadeIn));
		DrawRect(FLinearColor(.08f, .82f, 1.f, (.46f + BeatImpulse * .18f) * FadeIn), PhraseLeft, MidY - 1.2f * Scale,
			FMath::Max(2.0f, PhraseRight - PhraseLeft), 2.4f * Scale);

		for (int32 Index = 0; Index < VisibleCues.Num(); ++Index)
		{
			const FBladeCueVisual& Visual = VisibleCues[Index];
			const FUpcomingImpactCue& Cue = *Visual.Cue;
			const float AbsError = FMath::Abs(Visual.UntilIdealInput);
			const bool bPerfectNow = AbsError <= State->GetPerfectWindowSeconds();
			const bool bGoodNow = AbsError <= State->GetGoodWindowSeconds();
			const float Pulse = 1.0f + (bPerfectNow ? .18f : bGoodNow ? .08f : 0.f);
			const float EnterAlpha = 1.0f - FMath::SmoothStep(LookAhead - .45f, LookAhead + .08f, Visual.UntilIdealInput);
			const float ExitAlpha = FMath::SmoothStep(-.18f, -.015f, Visual.UntilIdealInput);
			const float CueAlpha = FMath::SmoothStep(0.0f, 1.0f, FMath::Min(EnterAlpha, ExitAlpha));
			const FLinearColor Accent = bPerfectNow || Cue.bStrongBeat
				? FLinearColor(1.f, .69f, .07f, CueAlpha) : FLinearColor(.06f, .78f, 1.f, CueAlpha);

			// A real torn brush slash sprite replaces the old geometric marker.
			if (AttackSlashTexture)
			{
				const FVector2D SlashSize((Cue.bStrongBeat ? 96.f : 76.f) * Scale * Pulse,
					(Cue.bStrongBeat ? 150.f : 122.f) * Scale * Pulse);
				FCanvasTileItem Glow(FVector2D(Visual.X, MidY) - SlashSize * .58f,
					AttackSlashTexture->GetResource(), SlashSize * 1.16f,
					FLinearColor(Accent.R, Accent.G, Accent.B, CueAlpha * .24f));
				Glow.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(Glow);
				FCanvasTileItem Slash(FVector2D(Visual.X, MidY) - SlashSize * .5f,
					AttackSlashTexture->GetResource(), SlashSize, Accent);
				Slash.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(Slash);
			}
			else
			{
				const float H = (Cue.bStrongBeat ? 43.f : 34.f) * Scale * Pulse;
				DrawSolidQuad(Canvas, FVector2D(Visual.X - 12 * Scale, MidY + H), FVector2D(Visual.X - 3 * Scale, MidY + H),
					FVector2D(Visual.X + 12 * Scale, MidY - H), FVector2D(Visual.X + 3 * Scale, MidY - H), Accent);
			}

			if (Cue.bStrongBeat)
			{
				DrawRect(FLinearColor(1.f, .91f, .58f, .9f * CueAlpha), Visual.X - 20 * Scale, MidY - 1.5f * Scale,
					40 * Scale, 3 * Scale);
			}
		}

		// As the next cut approaches, energy brackets converge on the gate and culminate in a flash.
		const float Approach = FMath::SmoothStep(0.0f, 1.0f, 1.0f - FMath::Clamp(VisibleCues[0].UntilIdealInput / .72f, 0.0f, 1.0f));
		if (Approach > KINDA_SMALL_NUMBER)
		{
			const float BracketOffset = FMath::Lerp(76.0f, 18.0f, Approach) * Scale;
			const float BracketH = FMath::Lerp(19.0f, 43.0f, Approach) * Scale;
			const FLinearColor GateColor(1.f, .66f, .06f, Approach * .78f);
			DrawSolidQuad(Canvas,
				FVector2D(JudgeX - BracketOffset - 13 * Scale, MidY), FVector2D(JudgeX - BracketOffset, MidY - BracketH),
				FVector2D(JudgeX - BracketOffset + 5 * Scale, MidY - BracketH), FVector2D(JudgeX - BracketOffset - 5 * Scale, MidY), GateColor);
			DrawSolidQuad(Canvas,
				FVector2D(JudgeX + BracketOffset + 13 * Scale, MidY), FVector2D(JudgeX + BracketOffset, MidY + BracketH),
				FVector2D(JudgeX + BracketOffset - 5 * Scale, MidY + BracketH), FVector2D(JudgeX + BracketOffset + 5 * Scale, MidY), GateColor);
			DrawRect(FLinearColor(1.f, .92f, .66f, Approach * Approach), JudgeX - (1.5f + 4.f * Approach) * Scale,
				MidY - (48.f + 22.f * Approach) * Scale, (3.f + 8.f * Approach) * Scale,
				(96.f + 44.f * Approach) * Scale);
			if (AttackSlashTexture)
			{
				const FVector2D EchoSize((62.f + 28.f * Approach) * Scale, (102.f + 42.f * Approach) * Scale);
				const float EchoOffset = FMath::Lerp(58.f, 8.f, Approach) * Scale;
				const FLinearColor EchoTint(1.f, .70f, .10f, Approach * Approach * .25f);
				FCanvasTileItem LeftEcho(FVector2D(JudgeX - EchoOffset, MidY) - EchoSize * .5f,
					AttackSlashTexture->GetResource(), EchoSize, EchoTint);
				LeftEcho.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(LeftEcho);
				FCanvasTileItem RightEcho(FVector2D(JudgeX + EchoOffset, MidY) - EchoSize * .5f,
					AttackSlashTexture->GetResource(), EchoSize, EchoTint);
				RightEcho.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(RightEcho);
				if (Approach > .72f)
				{
					const float Flash = FMath::Square((Approach - .72f) / .28f);
					const FVector2D ImpactSize(112.f * Scale, 172.f * Scale);
					FCanvasTileItem ImpactSlash(FVector2D(JudgeX, MidY) - ImpactSize * .5f,
						AttackSlashTexture->GetResource(), ImpactSize,
						FLinearColor(1.f, .88f, .48f, Flash * .70f));
					ImpactSlash.BlendMode = SE_BLEND_Translucent;
					Canvas->DrawItem(ImpactSlash);
				}
			}
		}
	}

}

void ARhythmCombatHUD::DrawPlayerPanel(const UPlayerRhythmCombatComponent* State, float Scale, double Now)
{
	const FVector2D Origin(18.0f * Scale, 10.0f * Scale);
	float FrameBeatScale = 1.0f;
	float BeatHighlight = 0.0f;
	bool bDownbeat = false;
	if (const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>(); Clock && Clock->IsRunning())
	{
		const double Beat = Clock->GetCurrentBeat();
		const float Phase = static_cast<float>(Beat - FMath::FloorToDouble(Beat));
		// Only the frame breathes. Content remains stable and readable.
		const float Wave = FMath::Cos(Phase * 2.0f * UE_PI);
		FrameBeatScale = 1.0f + Wave * 0.010f;
		BeatHighlight = Wave * 0.5f + 0.5f;
		bDownbeat = Clock->GetCurrentBeatInBar() == 0;
	}
	const FVector2D BaseSize(650.0f * Scale, 260.0f * Scale);
	const FVector2D Size = BaseSize * FrameBeatScale;
	const FVector2D FrameOrigin = Origin - (Size - BaseSize) * 0.5f;

	if (PlayerFrameTexture)
	{
		const FLinearColor Accent = bDownbeat ? FLinearColor(1.0f, 0.28f, 0.03f) : FLinearColor(0.05f, 0.78f, 1.0f);
		const FLinearColor FrameTint = FMath::Lerp(FLinearColor::White, Accent, 0.10f + BeatHighlight * 0.28f);
		FCanvasTileItem Item(FrameOrigin, PlayerFrameTexture->GetResource(), Size, FrameTint);
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
	}
	const float HealthX = Origin.X + 171.0f * Scale;
	const float HealthY = Origin.Y + 101.0f * Scale;
	const float HealthW = 426.0f * Scale;
	const float HealthH = 28.0f * Scale;
	const float HealthSlant = 9.0f * Scale;
	DrawSolidQuad(Canvas, FVector2D(HealthX, HealthY), FVector2D(HealthX + HealthW, HealthY),
		FVector2D(HealthX + HealthW - HealthSlant, HealthY + HealthH), FVector2D(HealthX, HealthY + HealthH),
		FLinearColor(0.075f, 0.025f, 0.025f, 1.0f));
	const float HealthFillW = HealthW * State->GetHealthFraction();
	if (HealthFillW > 1.0f)
		DrawSolidQuad(Canvas, FVector2D(HealthX, HealthY), FVector2D(HealthX + HealthFillW, HealthY),
			FVector2D(HealthX + FMath::Max(0.0f, HealthFillW - HealthSlant), HealthY + HealthH), FVector2D(HealthX, HealthY + HealthH),
			FLinearColor(0.94f, 0.025f, 0.018f, 1.0f));
	for (int32 Segment = 1; Segment < 5; ++Segment)
		DrawRect(FLinearColor(0.015f, 0.015f, 0.02f, .9f), HealthX + HealthW * Segment / 5.0f - Scale,
			HealthY + 2 * Scale, 3 * Scale, 23 * Scale);
	const float ResonanceY = HealthY + 49.0f * Scale;
	const float ResonanceH = 8.0f * Scale;
	DrawSolidQuad(Canvas, FVector2D(HealthX, ResonanceY), FVector2D(HealthX + HealthW, ResonanceY),
		FVector2D(HealthX + HealthW - HealthSlant, ResonanceY + ResonanceH), FVector2D(HealthX, ResonanceY + ResonanceH),
		FLinearColor(0.025f, 0.14f, 0.20f, 1.0f));
	const float ResonanceFillW = HealthW * State->GetResonanceFraction();
	if (ResonanceFillW > 1.0f)
		DrawSolidQuad(Canvas, FVector2D(HealthX, ResonanceY), FVector2D(HealthX + ResonanceFillW, ResonanceY),
			FVector2D(HealthX + FMath::Max(0.0f, ResonanceFillW - HealthSlant), ResonanceY + ResonanceH), FVector2D(HealthX, ResonanceY + ResonanceH),
			FLinearColor(0.05f, 0.88f, 1.0f, 1.0f));
	FCanvasTextItem Hp(FVector2D(HealthX + HealthW - 68 * Scale, HealthY - 34 * Scale),
		FText::FromString(FString::Printf(TEXT("HP %d"), FMath::RoundToInt(State->Health))), GEngine->GetLargeFont(), FLinearColor::White);
	Hp.Scale = FVector2D(1.05f * Scale); Hp.EnableShadow(FLinearColor::Black); Canvas->DrawItem(Hp);
	FCanvasTextItem Resonance(FVector2D(HealthX, HealthY + 65 * Scale), FText::FromString(TEXT("RESONANCE")),
		GEngine->GetSmallFont(), FLinearColor(0.12f, 0.9f, 1.0f));
	Resonance.Scale = FVector2D(.78f * Scale); Resonance.EnableShadow(FLinearColor::Black); Canvas->DrawItem(Resonance);
}

void ARhythmCombatHUD::DrawJudgment(const UPlayerRhythmCombatComponent* State, float Scale, double Now)
{
	constexpr float QueueMoveSeconds = 0.14f;
	const double JudgmentTime = State->GetLastJudgmentWorldSeconds();
	if (State->LastJudgment != ERhythmDodgeJudgment::None && JudgmentTime > LastCapturedJudgmentSeconds)
	{
		for (FJudgmentFeedEntry& Entry : JudgmentFeed) Entry.StartSlot = Entry.TargetSlot;
		if (JudgmentFeed.Num() >= 5)
		{
			OutgoingJudgment = JudgmentFeed[0];
			OutgoingJudgment.StartSlot = OutgoingJudgment.TargetSlot;
			OutgoingJudgment.TargetSlot = -1.0f;
			bHasOutgoingJudgment = true;
			JudgmentFeed.RemoveAt(0);
		}
		FJudgmentFeedEntry NewEntry;
		NewEntry.Grade = State->LastJudgment;
		NewEntry.ErrorMs = State->LastJudgmentErrorMs;
		NewEntry.TimeSeconds = JudgmentTime;
		NewEntry.Combo = State->ComboCount;
		NewEntry.StartSlot = 5.0f;
		JudgmentFeed.Add(NewEntry);
		const int32 FirstSlot = 5 - JudgmentFeed.Num();
		for (int32 Index = 0; Index < JudgmentFeed.Num(); ++Index)
			JudgmentFeed[Index].TargetSlot = static_cast<float>(FirstSlot + Index);
		QueueAnimationStartSeconds = Now;
		LastCapturedJudgmentSeconds = JudgmentTime;
		UE_LOG(LogTemp, Display, TEXT("RhythmHUDQueue: enqueue=%s cards=%d oldest=%s newest=%s combo=%d"),
			*UEnum::GetValueAsString(State->LastJudgment), JudgmentFeed.Num(),
			JudgmentFeed.Num() ? *UEnum::GetValueAsString(JudgmentFeed[0].Grade) : TEXT("None"),
			JudgmentFeed.Num() ? *UEnum::GetValueAsString(JudgmentFeed.Last().Grade) : TEXT("None"), State->ComboCount);
	}
	if (State->ComboCount != LastComboCount)
	{
		LastComboCount = State->ComboCount;
		ComboPunchStartSeconds = Now;
	}

	const FVector2D RailSize(285.0f, 740.0f);
	const FVector2D RailOrigin(Canvas->SizeX - (RailSize.X + 18.0f) * Scale, Canvas->SizeY * .13f);
	UTexture2D* RailTexture = JudgmentRailBackboneTexture ? JudgmentRailBackboneTexture : JudgmentRailTexture;
	if (RailTexture)
	{
		FCanvasTileItem Rail(RailOrigin, RailTexture->GetResource(), RailSize * Scale, FLinearColor::White);
		Rail.BlendMode = SE_BLEND_Translucent; Canvas->DrawItem(Rail);
	}
	const float RailX = Canvas->SizeX - 48.0f * Scale;
	const float RailTop = Canvas->SizeY * .16f;
	const float RailBottom = Canvas->SizeY * .88f;
	if (!RailTexture)
	{
		DrawRect(FLinearColor(0.02f, 0.65f, 0.9f, .26f), RailX, RailTop, 3 * Scale, RailBottom - RailTop);
		for (int32 Tick = 0; Tick < 8; ++Tick)
		{
			const float TickY = FMath::Lerp(RailTop, RailBottom, Tick / 7.0f);
			DrawRect(FLinearColor(0.2f, .9f, 1.0f, .7f), RailX - 7 * Scale, TickY - Scale, 17 * Scale, 2 * Scale);
		}
	}

	const float RawMoveAlpha = FMath::Clamp(static_cast<float>((Now - QueueAnimationStartSeconds) / QueueMoveSeconds), 0.0f, 1.0f);
	const float MoveAlpha = 1.0f - FMath::Pow(1.0f - RawMoveAlpha, 3.0f);
	const auto DrawCard = [&](const FJudgmentFeedEntry& Entry, float Alpha)
	{
		const float Slot = FMath::Lerp(Entry.StartSlot, Entry.TargetSlot, MoveAlpha);
		const float Age = static_cast<float>(Now - Entry.TimeSeconds);
		const bool bNewest = JudgmentFeed.Num() > 0 && &Entry == &JudgmentFeed.Last();
		const float Punch = bNewest && Age < QueueMoveSeconds ? FMath::Lerp(1.08f, 1.0f, MoveAlpha) : 1.0f;
		const FVector2D Center = RailOrigin + FVector2D(112.0f, 104.0f + Slot * 102.0f) * Scale;
		if (JudgmentBurstTexture && Entry.Grade == ERhythmDodgeJudgment::Perfect && Age < .65f)
		{
			const FVector2D BurstSize(265.0f * Scale * Punch);
			FCanvasTileItem Burst(Center - BurstSize * .5f, JudgmentBurstTexture->GetResource(), BurstSize,
				FLinearColor(1, .35f, .04f, Alpha));
			Burst.BlendMode = SE_BLEND_Additive; Canvas->DrawItem(Burst);
		}
		FLinearColor Color = GetJudgmentColor(Entry.Grade); Color.A = Alpha;
		const float GradeCardScale = Entry.Grade == ERhythmDodgeJudgment::Perfect ? 1.07f : 1.0f;
		const FVector2D CardSize(238.0f * Scale * Punch * GradeCardScale, 87.5f * Scale * Punch * GradeCardScale);
		if (JudgmentCardTexture)
		{
			FCanvasTileItem Card(Center - CardSize * .5f, JudgmentCardTexture->GetResource(), CardSize, Color);
			Card.BlendMode = SE_BLEND_Translucent; Canvas->DrawItem(Card);
		}
		else
		{
			DrawRect(FLinearColor(Color.R, Color.G, Color.B, .75f * Alpha), Center.X - CardSize.X * .5f,
				Center.Y - CardSize.Y * .5f, CardSize.X, CardSize.Y);
		}
		if (UTexture2D* WordTexture = GetJudgmentWordTexture(Entry.Grade))
		{
			float GradeWordScale = 1.0f;
			switch (Entry.Grade)
			{
			case ERhythmDodgeJudgment::Perfect: GradeWordScale = 1.18f; break;
			case ERhythmDodgeJudgment::Good: GradeWordScale = 1.00f; break;
			case ERhythmDodgeJudgment::Bad: GradeWordScale = 0.88f; break;
			case ERhythmDodgeJudgment::Miss: GradeWordScale = 0.82f; break;
			default: break;
			}
			const FVector2D WordSize = FVector2D(184.0f, 52.0f) * Scale * Punch * GradeWordScale;
			// The source textures retain generous transparent margins for authoring. Crop those margins here so
			// the actual lettering fills the judgment card and remains readable at gameplay resolution.
			FCanvasTileItem Word(Center - WordSize * .5f, WordTexture->GetResource(), WordSize,
				FVector2D(.20f, .08f), FVector2D(.80f, .90f), FLinearColor(1, 1, 1, Alpha));
			Word.BlendMode = SE_BLEND_Translucent; Canvas->DrawItem(Word);
		}
	};
	if (bHasOutgoingJudgment)
	{
		DrawCard(OutgoingJudgment, 1.0f - RawMoveAlpha);
		if (RawMoveAlpha >= 1.0f) bHasOutgoingJudgment = false;
	}
	for (const FJudgmentFeedEntry& Entry : JudgmentFeed) DrawCard(Entry, 1.0f);

	const FVector2D ComboCenter = RailOrigin + FVector2D(112.0f, 655.0f) * Scale;
	const float ComboAge = static_cast<float>(Now - ComboPunchStartSeconds);
	const float ComboPunch = ComboAge < .18f ? 1.0f + .08f * (1.0f - FMath::Clamp(ComboAge / .18f, 0.0f, 1.0f)) : 1.0f;
	const float ComboAlpha = State->ComboCount == 0 ? .45f : 1.0f;
	const FVector2D PanelSize(250.0f, 107.0f);
	if (ComboPanelTexture)
	{
		const FVector2D DrawSize = PanelSize * Scale * ComboPunch;
		FCanvasTileItem Panel(ComboCenter - DrawSize * .5f, ComboPanelTexture->GetResource(), DrawSize,
			FLinearColor(1, 1, 1, ComboAlpha));
		Panel.BlendMode = SE_BLEND_Translucent; Canvas->DrawItem(Panel);
	}
	{
		if (ComboWordTexture)
		{
			const FVector2D WordSize(119.0f * Scale * ComboPunch, 34.0f * Scale * ComboPunch);
			FCanvasTileItem Word(ComboCenter + FVector2D(-91, -17) * Scale * ComboPunch, ComboWordTexture->GetResource(), WordSize,
				FVector2D(.20f, .08f), FVector2D(.80f, .90f), FLinearColor(1, 1, 1, ComboAlpha));
			Word.BlendMode = SE_BLEND_Translucent; Canvas->DrawItem(Word);
		}
		FCanvasTextItem Count(ComboCenter + FVector2D(55, 0) * Scale,
			FText::FromString(FString::Printf(TEXT("x%d"), State->ComboCount)), GEngine->GetLargeFont(), FLinearColor(1, .72f, .12f));
		Count.bCentreX = true; Count.bCentreY = true; Count.Scale = FVector2D(1.20f * Scale * ComboPunch);
		Count.EnableShadow(FLinearColor::Black); Canvas->DrawItem(Count);
	}
}

UTexture2D* ARhythmCombatHUD::GetJudgmentWordTexture(ERhythmDodgeJudgment Grade) const
{
	switch (Grade)
	{
	case ERhythmDodgeJudgment::Perfect: return PerfectWordTexture;
	case ERhythmDodgeJudgment::Good: return GoodWordTexture;
	case ERhythmDodgeJudgment::Bad: return BadWordTexture;
	case ERhythmDodgeJudgment::Miss: return MissWordTexture;
	default: return nullptr;
	}
}

FLinearColor ARhythmCombatHUD::GetJudgmentColor(ERhythmDodgeJudgment Grade)
{
	switch (Grade)
	{
	case ERhythmDodgeJudgment::Perfect: return FLinearColor(1.0f, 0.76f, 0.12f);
	case ERhythmDodgeJudgment::Good: return FLinearColor(0.22f, 0.78f, 0.90f);
	case ERhythmDodgeJudgment::Bad: return FLinearColor(0.42f, 0.52f, 0.58f);
	case ERhythmDodgeJudgment::Miss: return FLinearColor(0.62f, 0.22f, 0.20f);
	default: return FLinearColor::Transparent;
	}
}

FString ARhythmCombatHUD::GetJudgmentText(ERhythmDodgeJudgment Grade)
{
	switch (Grade)
	{
	case ERhythmDodgeJudgment::Perfect: return TEXT("PERFECT");
	case ERhythmDodgeJudgment::Good: return TEXT("GOOD");
	case ERhythmDodgeJudgment::Bad: return TEXT("BAD");
	case ERhythmDodgeJudgment::Miss: return TEXT("MISS");
	default: return FString();
	}
}
