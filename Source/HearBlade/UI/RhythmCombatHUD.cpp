#include "UI/RhythmCombatHUD.h"

#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "Combat/BeatSyncedComboComponent.h"
#include "Combat/BossBeatDirector.h"
#include "Combat/BossCombatCameraComponent.h"
#include "Combat/RhythmResponseVisualData.h"
#include "CanvasItem.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "GlobalRenderResources.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
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
	static ConstructorHelpers::FObjectFinder<UTexture2D> AttackTimingRail(TEXT("/Game/UI/RhythmCombat/T_HUD_RhythmRail_Clean.T_HUD_RhythmRail_Clean"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> BossFrame(TEXT("/Game/UI/RhythmCombat/T_HUD_BossHealthFrame.T_HUD_BossHealthFrame"));
	if (BossFrame.Succeeded()) BossHealthFrameTexture = BossFrame.Object;
	static ConstructorHelpers::FObjectFinder<UTexture2D> AttackSlash(TEXT("/Game/UI/RhythmCombat/T_HUD_AttackSlash.T_HUD_AttackSlash"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> DodgeIcon(TEXT("/Game/UI/RhythmCombat/Responses/T_Response_Dodge_Minimal.T_Response_Dodge_Minimal"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> BlockIcon(TEXT("/Game/UI/RhythmCombat/Responses/T_Response_Block_Minimal.T_Response_Block_Minimal"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> CounterIcon(TEXT("/Game/UI/RhythmCombat/Responses/T_Response_Counter_Minimal.T_Response_Counter_Minimal"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> HoldIcon(TEXT("/Game/UI/RhythmCombat/Responses/T_Response_HoldBlock_Minimal.T_Response_HoldBlock_Minimal"));
	static ConstructorHelpers::FObjectFinder<URhythmResponseVisualData> Visuals(TEXT("/Game/UI/RhythmCombat/Responses/DA_RhythmResponseVisuals.DA_RhythmResponseVisuals"));
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
	if (DodgeIcon.Succeeded()) DodgeResponseTexture = DodgeIcon.Object;
	if (BlockIcon.Succeeded()) BlockResponseTexture = BlockIcon.Object;
	if (CounterIcon.Succeeded()) CounterResponseTexture = CounterIcon.Object;
	if (HoldIcon.Succeeded()) HoldResponseTexture = HoldIcon.Object;
	if (Visuals.Succeeded()) ResponseVisuals = Visuals.Object;
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
	DrawBossHealth(Scale);
	DrawLockMarker();
	DrawJudgment(State, Scale, Now);
}

void ARhythmCombatHUD::DrawBossHealth(float Scale)
{
	UBeatSyncedComboComponent* Boss = FindCueCombo();
	if (!Boss || !IsValid(Boss->GetOwner()) || !PlayerOwner->GetPawn()) return;
	if (FVector::DistSquared(PlayerOwner->GetPawn()->GetActorLocation(), Boss->GetOwner()->GetActorLocation()) > FMath::Square(3500.f)) return;
	const float Health = Boss->GetBossHealthFraction();
	if (HealthBarTarget.Get() != Boss) { HealthBarTarget = Boss; BossHealthTrail = Health; }
	BossHealthTrail = FMath::Max(Health, FMath::FInterpTo(BossHealthTrail, Health, GetWorld()->GetDeltaSeconds(), 2.5f));
	const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	float PulseScale = 1.f;
	if (Clock && Clock->IsRunning())
	{
		const float BeatPhase = static_cast<float>(Clock->GetCurrentBeat() - FMath::FloorToDouble(Clock->GetCurrentBeat()));
		const float BeatImpulse = FMath::Exp(-BeatPhase * 7.5f);
		const bool bDownbeat = Clock->GetCurrentBeatInBar() == 0;
		PulseScale += BeatImpulse * (bDownbeat ? .009f : .005f);
	}
	const float BaseW = FMath::Min(1000.f * Scale, Canvas->SizeX * .72f);
	const float W = BaseW * PulseScale;
	const float X = (Canvas->SizeX - W) * .5f;
	const float Y = Canvas->SizeY * .18f;
	const float H = 12.f * Scale;
	DrawRect(FLinearColor(.015f,.008f,.008f,.94f), X + W*.08f, Y-H*.5f, W*.84f, H);
	DrawRect(FLinearColor(.52f,.28f,.09f,1.f), X + W*.08f, Y-H*.5f, W*.84f*BossHealthTrail, H);
	DrawRect(FLinearColor(.53f,.035f,.025f,1.f), X + W*.08f, Y-H*.5f, W*.84f*Health, H);
	if (BossHealthFrameTexture)
	{
		FCanvasTileItem Frame(FVector2D(X,Y-W/6.f), BossHealthFrameTexture->GetResource(), FVector2D(W,W/3.f), FLinearColor::White);
		Frame.BlendMode = SE_BLEND_Translucent; Canvas->DrawItem(Frame);
	}
	const float BaseX = (Canvas->SizeX - BaseW) * .5f;
	FCanvasTextItem Name(FVector2D(BaseX+BaseW*.08f,Y-34.f*Scale), Boss->BossDisplayName, GEngine->GetMediumFont(), FLinearColor(.88f,.81f,.65f,1.f));
	Name.Scale = FVector2D(Scale); Name.EnableShadow(FLinearColor::Black); Canvas->DrawItem(Name);
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
	Combo->GetUpcomingImpactCues(Cues, 8);
	if (Cues.IsEmpty()) return;

	const double AudioNow = Clock->GetAudioNowSeconds();
	float MaxPromptBeats = 3.0f;
	for (const FUpcomingImpactCue& Cue : Cues) MaxPromptBeats = FMath::Max(MaxPromptBeats, Cue.PromptLeadBeats);
	const float LookAhead = FMath::Max(1.8f, static_cast<float>(Clock->GetSecondsPerBeat() * MaxPromptBeats));
	const float FirstUntilInput = static_cast<float>(Cues[0].ImpactAudioSeconds - Cues[0].IdealDodgeLeadSeconds - AudioNow);
	if (FirstUntilInput > LookAhead + .2f) return;
	const float FadeIn = 1.0f - FMath::SmoothStep(LookAhead - .42f, LookAhead + .12f, FirstUntilInput);
	const float BeatPhase = static_cast<float>(Clock->GetCurrentBeat() - FMath::FloorToDouble(Clock->GetCurrentBeat()));
	const float BeatImpulse = FMath::Exp(-BeatPhase * 6.5f);
	const bool bDownbeatNow = Clock->GetCurrentBeatInBar() == 0;
	const float DecorationPulse = 1.0f + BeatImpulse * (bDownbeatNow ? .009f : .005f);

	// Gameplay geometry is fixed. Only the illustrated decoration breathes, so
	// the gate and every authored command keep their exact screen position.
	const float TrackWidth = 1000.0f * Scale;
	const float TrackHeight = TrackWidth * (724.0f / 2172.0f);
	const float TrackLeft = Canvas->SizeX * .5f - TrackWidth * .5f;
	const float TrackTop = Canvas->SizeY - TrackHeight - 8.0f * Scale;
	const float JudgeX = TrackLeft + TrackWidth * .25f;
	const float TrackRight = TrackLeft + TrackWidth * .91f;
	const float TravelWidth = TrackRight - JudgeX - 30.0f * Scale;
	const float MidY = TrackTop + TrackHeight * .493f;
	// A single fixed gate gives the new empty rail an unambiguous timing reference.

	// Timing windows sit behind the original illustrated rail.
	const float GoodPixels = State->GetGoodWindowSeconds() / LookAhead * TravelWidth;
	const float PerfectPixels = State->GetPerfectWindowSeconds() / LookAhead * TravelWidth;
	DrawRect(FLinearColor(.03f, .70f, .92f, .07f * FadeIn), JudgeX - GoodPixels, MidY - 10 * Scale,
		GoodPixels * 2.0f, 20 * Scale);
	DrawRect(FLinearColor(1.0f, .68f, .08f, .14f * FadeIn), JudgeX - PerfectPixels, MidY - 16 * Scale,
		PerfectPixels * 2.0f, 32 * Scale);
	if (AttackTimingRailTexture)
	{
		const FVector2D VisualSize(TrackWidth * DecorationPulse, TrackHeight * DecorationPulse);
		const FVector2D VisualOrigin(TrackLeft - (VisualSize.X - TrackWidth) * .5f,
			TrackTop - (VisualSize.Y - TrackHeight) * .5f);
		FCanvasTileItem Frame(VisualOrigin, AttackTimingRailTexture->GetResource(), VisualSize,
			FLinearColor(1.f, 1.f, 1.f, FadeIn * (.90f + BeatImpulse * .10f)));
		Frame.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Frame);
	}
	else
	{
		DrawRect(FLinearColor(.04f, .72f, .92f, .78f), TrackLeft, MidY - Scale, TrackWidth, 2 * Scale);
		DrawRect(FLinearColor(1.0f, .95f, .72f, 1.0f), JudgeX - Scale, MidY - 54 * Scale, 2 * Scale, 108 * Scale);
	}
	// The timing anchor stays visible over the decorative texture. Judge the
	// center of each icon against this line, not its expanding outline.
	DrawRect(FLinearColor(.005f, .01f, .015f, .95f * FadeIn), JudgeX - 3 * Scale,
		MidY - 48 * Scale, 6 * Scale, 96 * Scale);
	DrawRect(FLinearColor(1.f, .91f, .62f, FadeIn), JudgeX - Scale,
		MidY - 48 * Scale, 2 * Scale, 96 * Scale);
	const float TierAge = static_cast<float>(GetWorld()->GetTimeSeconds() - ComboTierUpWorldSeconds);
	if (TierAge >= 0.f && TierAge < .42f)
	{
		const float T = TierAge / .42f;
		const float SweepX = FMath::Lerp(TrackLeft + TrackWidth * .05f, TrackRight, T);
		const float SweepAlpha = FMath::Sin(T * UE_PI);
		DrawRect(FLinearColor(.12f, .9f, 1.f, .20f * SweepAlpha), SweepX - 32.f * Scale,
			MidY - 31.f * Scale, 64.f * Scale, 62.f * Scale);
		DrawRect(FLinearColor(1.f, .62f, .06f, .85f * SweepAlpha), SweepX - 3.f * Scale,
			MidY - 43.f * Scale, 6.f * Scale, 86.f * Scale);
	}

	// Beat ticks show musical spacing but stay secondary to the actual attack markers.
	const int32 FirstBeat = Clock->GetNextBeatIndex();
	for (int32 Step = 0; Step < 4; ++Step)
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
	// One short cue at half a beat. It is keyed by the exact impact audio timestamp,
	// so DrawHUD cannot retrigger it on every frame.
	if (!VisibleCues.IsEmpty() && ResponseVisuals)
	{
		const FUpcomingImpactCue& NextCue = *VisibleCues[0].Cue;
		const float HalfBeat = static_cast<float>(Clock->GetSecondsPerBeat() * .5);
		if (VisibleCues[0].UntilIdealInput <= HalfBeat && VisibleCues[0].UntilIdealInput > 0.0f &&
			FMath::Abs(NextCue.ImpactAudioSeconds - LastPromptedImpactAudioSeconds) > .001)
		{
			const FRhythmResponseVisual& Visual = ResponseVisuals->Get(NextCue.RequiredResponse);
			USoundBase* Sound = NextCue.bStrongBeat && ResponseVisuals->StrongApproachSound
				? ResponseVisuals->StrongApproachSound.Get()
				: (Visual.PromptSound ? Visual.PromptSound.Get() : ResponseVisuals->ApproachSound.Get());
			if (Sound) UGameplayStatics::PlaySound2D(GetWorld(), Sound, NextCue.bStrongBeat ? 1.0f : .72f);
			LastPromptedImpactAudioSeconds = NextCue.ImpactAudioSeconds;
		}
	}
	// A hold is a readable lane rather than another one-shot icon. Before the start it is a
	// quiet cyan channel; while the player is blocking it lights up and carries scan packets
	// toward the release gate. If the start has already crossed the judge line, keep drawing
	// from the judge line so the player never loses the remaining hold duration.
	for (int32 EndIndex = 0; EndIndex < VisibleCues.Num(); ++EndIndex)
	{
		const FUpcomingImpactCue& EndCue = *VisibleCues[EndIndex].Cue;
		if (EndCue.RequiredResponse != ERhythmResponseType::HoldBlock ||
			EndCue.HoldMarker != ERhythmHoldMarkerType::End) continue;

		int32 StartIndex = INDEX_NONE;
		for (int32 SearchIndex = EndIndex - 1; SearchIndex >= 0; --SearchIndex)
		{
			const FUpcomingImpactCue& Candidate = *VisibleCues[SearchIndex].Cue;
			if (Candidate.RequiredResponse == ERhythmResponseType::HoldBlock &&
				Candidate.HoldMarker == ERhythmHoldMarkerType::Start && Candidate.HoldGroup == EndCue.HoldGroup)
			{
				StartIndex = SearchIndex;
				break;
			}
		}

		const float RawStartX = StartIndex != INDEX_NONE ? VisibleCues[StartIndex].X : JudgeX;
		const float X0 = FMath::Clamp(RawStartX, JudgeX, TrackRight);
		const float X1 = FMath::Clamp(VisibleCues[EndIndex].X, JudgeX, TrackRight);
		const float LaneWidth = X1 - X0;
		if (LaneWidth <= 3.0f * Scale) continue;

		const bool bHolding = State->IsBlocking();
		const float LaneHalfHeight = 13.0f * Scale;
		const float LaneTop = MidY - LaneHalfHeight;
		const float HoldPulse = .72f + BeatImpulse * .28f;
		const FLinearColor EdgeColor = bHolding
			? FLinearColor(.08f, .76f, .82f, (.76f + BeatImpulse * .14f) * FadeIn)
			: FLinearColor(.05f, .38f, .48f, .62f * FadeIn);

		// Thin black housing and two luminous rails keep the lane legible over the world.
		DrawRect(FLinearColor(.005f, .012f, .022f, .82f * FadeIn), X0, LaneTop,
			LaneWidth, LaneHalfHeight * 2.0f);
		DrawRect(FLinearColor(.02f, .22f, .30f, (bHolding ? .68f : .30f) * FadeIn),
			X0 + 3.0f * Scale, MidY - 6.0f * Scale, FMath::Max(1.0f, LaneWidth - 6.0f * Scale), 12.0f * Scale);
		DrawRect(EdgeColor, X0, LaneTop, LaneWidth, 2.0f * Scale);
		DrawRect(EdgeColor, X0, MidY + LaneHalfHeight - 2.0f * Scale, LaneWidth, 2.0f * Scale);

		// A continuous core confirms that the button is currently held. The moving packets use
		// the audio beat phase, so their motion stays musical without creating another timer.
		if (bHolding)
		{
			DrawRect(FLinearColor(.05f, .64f, .72f, .56f * HoldPulse * FadeIn),
				X0 + 4.0f * Scale, MidY - 4.0f * Scale, FMath::Max(1.0f, LaneWidth - 8.0f * Scale), 8.0f * Scale);
			const float PacketSpacing = 52.0f * Scale;
			const float PacketTravel = static_cast<float>(FMath::Frac(Clock->GetCurrentBeat())) * PacketSpacing;
			for (float PacketX = X1 - PacketTravel; PacketX > X0 + 5.0f * Scale; PacketX -= PacketSpacing)
			{
				const float PacketWidth = FMath::Min(12.0f * Scale, PacketX - X0);
				DrawRect(FLinearColor(.30f, .88f, .92f, .78f * FadeIn), PacketX - PacketWidth,
					MidY - 3.0f * Scale, PacketWidth, 6.0f * Scale);
			}
		}

		// Start bracket and release gate. The release gate warms to gold during the final beat.
		DrawRect(EdgeColor, X0 - 2.0f * Scale, LaneTop - 4.0f * Scale, 3.0f * Scale,
			LaneHalfHeight * 2.0f + 8.0f * Scale);
		const float ReleaseApproach = 1.0f - FMath::Clamp(VisibleCues[EndIndex].UntilIdealInput /
			FMath::Max(.01f, static_cast<float>(Clock->GetSecondsPerBeat())), 0.0f, 1.0f);
		const FLinearColor ReleaseColor = FMath::Lerp(EdgeColor,
			FLinearColor(1.0f, .68f, .08f, FadeIn), ReleaseApproach);
		DrawRect(ReleaseColor, X1 - 2.0f * Scale, LaneTop - (4.0f + 5.0f * ReleaseApproach) * Scale,
			4.0f * Scale, LaneHalfHeight * 2.0f + (8.0f + 10.0f * ReleaseApproach) * Scale);
		DrawSolidTriangle(Canvas,
			FVector2D(X1 - 10.0f * Scale, LaneTop - 5.0f * Scale),
			FVector2D(X1 + 10.0f * Scale, LaneTop - 5.0f * Scale),
			FVector2D(X1, LaneTop + 3.0f * Scale), ReleaseColor);
		break;
	}

	// Present the combo as one connected blade phrase. Individual impacts are broad cuts in the
	// same ribbon, rather than a row of small rhythm-game notes.
	if (!VisibleCues.IsEmpty())
	{
		// The illustrated rail already connects the prompts; avoid a second overlaid ribbon.

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
			const FRhythmResponseVisual* DefaultVisual = ResponseVisuals ? &ResponseVisuals->Get(Cue.RequiredResponse) : nullptr;
			FLinearColor Accent = DefaultVisual ? DefaultVisual->Color : FLinearColor(.70f, .36f, 1.f, 1.f);
			Accent.A = CueAlpha;
			if (bPerfectNow || Cue.bStrongBeat) Accent = FLinearColor(1.f, .69f, .07f, CueAlpha);
			Accent *= Cue.IconColor;

			// A real torn brush slash sprite replaces the old geometric marker.
			UTexture2D* ResponseTexture = Cue.IconOverride;
			if (!ResponseTexture && DefaultVisual) ResponseTexture = DefaultVisual->Icon;
			if (!ResponseTexture)
			{
				switch (Cue.RequiredResponse)
				{
				case ERhythmResponseType::Block: ResponseTexture = BlockResponseTexture; break;
				case ERhythmResponseType::Counter: ResponseTexture = CounterResponseTexture; break;
				case ERhythmResponseType::HoldBlock: ResponseTexture = HoldResponseTexture; break;
				default: ResponseTexture = DodgeResponseTexture; break;
				}
			}
			if (Cue.RequiredResponse == ERhythmResponseType::Dodge && !Cue.IconOverride)
			{
				// A leaning figure and two short motion strokes communicate a sidestep.
				const float Unit = 1.35f * Scale * Pulse * Cue.IconScale;
				const FVector2D Center(Visual.X, MidY);
				auto Stroke = [&](FVector2D A, FVector2D B, FLinearColor Color, float Width)
				{
					A = Center + A * Unit; B = Center + B * Unit;
					DrawLine(A.X, A.Y, B.X, B.Y, FLinearColor(.006f, .012f, .02f, CueAlpha), (Width + 3.f) * Unit);
					Color.A = CueAlpha;
					DrawLine(A.X, A.Y, B.X, B.Y, Color, Width * Unit);
				};
				const FLinearColor Ivory(.85f, .92f, .92f, CueAlpha);
				const FLinearColor Cyan(.04f, .67f, .74f, CueAlpha);
				Stroke(FVector2D(5,-12), FVector2D(-4,3), Ivory, 4);
				Stroke(FVector2D(1,-6), FVector2D(14,0), Cyan, 3);
				Stroke(FVector2D(-4,3), FVector2D(9,19), Ivory, 4);
				Stroke(FVector2D(-4,3), FVector2D(-18,17), Ivory, 4);
				Stroke(FVector2D(-25,-9), FVector2D(-12,-9), Cyan, 2);
				Stroke(FVector2D(-29,-2), FVector2D(-16,-2), Cyan, 2);
				DrawSolidQuad(Canvas, Center + FVector2D(8,-26)*Unit, Center + FVector2D(14,-20)*Unit,
					Center + FVector2D(8,-14)*Unit, Center + FVector2D(2,-20)*Unit, Ivory);
			}
			else if (ResponseTexture)
			{
				const float DefaultScale = DefaultVisual ? DefaultVisual->IconScale : 1.0f;
				// Hold continuation notes are subordinate to the continuous lane. Start and release
				// remain clear anchors; intermediate checks become small pulses instead of shields.
				const float IconBaseSize = Cue.RequiredResponse == ERhythmResponseType::HoldBlock
					? (Cue.HoldMarker == ERhythmHoldMarkerType::None ? 40.0f : 78.0f)
					: 96.0f;
				const FVector2D IconSize(IconBaseSize * Scale * Pulse * Cue.IconScale * DefaultScale);
				// A dark enlarged silhouette separates the symbol from fire, sky and other bright scenery.
				const FVector2D OutlineSize = IconSize * 1.13f;
				FCanvasTileItem Outline(FVector2D(Visual.X, MidY) - OutlineSize * .5f,
					ResponseTexture->GetResource(), OutlineSize, FLinearColor(.005f, .009f, .014f, CueAlpha * .88f));
				Outline.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(Outline);
				// Only the short judgment approach receives an extra highlight; cruising prompts remain calm.
				if (bGoodNow)
				{
					const FVector2D HighlightSize = IconSize * 1.18f;
					FCanvasTileItem Highlight(FVector2D(Visual.X, MidY) - HighlightSize * .5f,
						ResponseTexture->GetResource(), HighlightSize,
						FLinearColor(Accent.R, Accent.G, Accent.B, CueAlpha * (bPerfectNow ? .34f : .16f)));
					Highlight.BlendMode = SE_BLEND_Translucent;
					Canvas->DrawItem(Highlight);
				}
				const float IconTone = bPerfectNow ? 1.0f : .82f;
				FCanvasTileItem Icon(FVector2D(Visual.X, MidY) - IconSize * .5f,
					ResponseTexture->GetResource(), IconSize, FLinearColor(IconTone, IconTone, IconTone, CueAlpha));
				Icon.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(Icon);
			}
			else if (Cue.RequiredResponse == ERhythmResponseType::Dodge && AttackSlashTexture)
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
				const float Size = 35.f * Scale * Pulse * Cue.IconScale;
				DrawSolidQuad(Canvas, FVector2D(Visual.X - Size, MidY), FVector2D(Visual.X, MidY - Size),
					FVector2D(Visual.X + Size, MidY), FVector2D(Visual.X, MidY + Size),
					FLinearColor(.01f, .02f, .03f, CueAlpha * .9f));
				const TCHAR* Label = Cue.RequiredResponse == ERhythmResponseType::Block ? TEXT("BLOCK") :
					Cue.RequiredResponse == ERhythmResponseType::Counter ? TEXT("COUNTER") :
					Cue.HoldMarker == ERhythmHoldMarkerType::Start ? TEXT("HOLD") :
					Cue.HoldMarker == ERhythmHoldMarkerType::End ? TEXT("RELEASE") : TEXT("KEEP");
				DrawText(Label, Accent, Visual.X - Size * .82f, MidY - 8.f * Scale,
					GEngine ? GEngine->GetSmallFont() : nullptr, Scale * .82f, false);
			}

			if (Cue.bStrongBeat)
			{
				DrawRect(FLinearColor(1.f, .91f, .58f, .9f * CueAlpha), Visual.X - 12 * Scale, MidY - Scale,
					24 * Scale, 2 * Scale);
			}
		}

		// As the next cut approaches, energy brackets converge on the gate and culminate in a flash.
		const float Approach = FMath::SmoothStep(0.0f, 1.0f, 1.0f - FMath::Clamp(VisibleCues[0].UntilIdealInput / .72f, 0.0f, 1.0f));
		if (Approach > KINDA_SMALL_NUMBER)
		{
			const float BracketOffset = FMath::Lerp(42.0f, 10.0f, Approach) * Scale;
			const float BracketH = FMath::Lerp(10.0f, 24.0f, Approach) * Scale;
			const FLinearColor GateColor(1.f, .66f, .06f, Approach * .78f);
			DrawSolidQuad(Canvas,
				FVector2D(JudgeX - BracketOffset - 13 * Scale, MidY), FVector2D(JudgeX - BracketOffset, MidY - BracketH),
				FVector2D(JudgeX - BracketOffset + 5 * Scale, MidY - BracketH), FVector2D(JudgeX - BracketOffset - 5 * Scale, MidY), GateColor);
			DrawSolidQuad(Canvas,
				FVector2D(JudgeX + BracketOffset + 13 * Scale, MidY), FVector2D(JudgeX + BracketOffset, MidY + BracketH),
				FVector2D(JudgeX + BracketOffset - 5 * Scale, MidY + BracketH), FVector2D(JudgeX + BracketOffset + 5 * Scale, MidY), GateColor);
			DrawRect(FLinearColor(1.f, .92f, .66f, Approach * Approach * .75f), JudgeX - (1.f + 2.f * Approach) * Scale,
				MidY - (25.f + 10.f * Approach) * Scale, (2.f + 4.f * Approach) * Scale,
				(50.f + 20.f * Approach) * Scale);
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
		const int32 NewTier = UPlayerRhythmCombatComponent::GetFeedbackTierForCombo(State->ComboCount);
		if (LastComboCount != INDEX_NONE && NewTier > LastComboTier)
			ComboTierUpWorldSeconds = Now;
		LastComboTier = NewTier;
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
		else
		{
			const FString Label = GetJudgmentText(Entry.Grade);
			DrawText(Label, FLinearColor(1.f, 1.f, 1.f, Alpha), Center.X - 76.f * Scale,
				Center.Y - 14.f * Scale, GEngine ? GEngine->GetMediumFont() : nullptr, Scale, false);
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
	case ERhythmDodgeJudgment::GoodHold: return GoodWordTexture;
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
	case ERhythmDodgeJudgment::GoodHold: return FLinearColor(0.12f, 0.82f, 1.0f);
	case ERhythmDodgeJudgment::Bad: return FLinearColor(0.42f, 0.52f, 0.58f);
	case ERhythmDodgeJudgment::Miss: return FLinearColor(0.62f, 0.22f, 0.20f);
	case ERhythmDodgeJudgment::HoldBreak: return FLinearColor(0.86f, 0.16f, 0.24f);
	case ERhythmDodgeJudgment::PerfectHold: return FLinearColor(0.20f, 0.95f, 0.82f);
	default: return FLinearColor::Transparent;
	}
}

FString ARhythmCombatHUD::GetJudgmentText(ERhythmDodgeJudgment Grade)
{
	switch (Grade)
	{
	case ERhythmDodgeJudgment::Perfect: return TEXT("PERFECT");
	case ERhythmDodgeJudgment::Good: return TEXT("GOOD");
	case ERhythmDodgeJudgment::GoodHold: return TEXT("GOOD HOLD");
	case ERhythmDodgeJudgment::Bad: return TEXT("BAD");
	case ERhythmDodgeJudgment::Miss: return TEXT("MISS");
	case ERhythmDodgeJudgment::HoldBreak: return TEXT("HOLD BREAK");
	case ERhythmDodgeJudgment::PerfectHold: return TEXT("PERFECT HOLD");
	default: return FString();
	}
}
