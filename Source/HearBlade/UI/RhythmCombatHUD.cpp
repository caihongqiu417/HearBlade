#include "UI/RhythmCombatHUD.h"

#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Combat/BeatSyncedComboComponent.h"
#include "Combat/PlayerBeatAttackComponent.h"
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
#include "Combat/CombatTutorialComponent.h"

namespace
{
void DrawSolidQuad(UCanvas* Canvas, const FVector2D& A, const FVector2D& B, const FVector2D& C,
	const FVector2D& D, const FLinearColor& Color)
{
	if (!Canvas || !Canvas->Canvas || !GWhiteTexture) return;
	FCanvasTriangleItem First(A, B, C, GWhiteTexture);
	FLinearColor Faded=Color;Faded.A*=Canvas->Canvas->AlphaModulate;
	First.SetColor(Faded);First.BlendMode=SE_BLEND_Translucent;
	Canvas->DrawItem(First);
	FCanvasTriangleItem Second(A, C, D, GWhiteTexture);
	Second.SetColor(Faded);Second.BlendMode=SE_BLEND_Translucent;
	Canvas->DrawItem(Second);
}

void DrawSolidTriangle(UCanvas* Canvas, const FVector2D& A, const FVector2D& B, const FVector2D& C,
	const FLinearColor& Color)
{
	if (!Canvas || !Canvas->Canvas || !GWhiteTexture) return;
	FCanvasTriangleItem Triangle(A, B, C, GWhiteTexture);
	FLinearColor Faded=Color;Faded.A*=Canvas->Canvas->AlphaModulate;
	Triangle.SetColor(Faded);Triangle.BlendMode=SE_BLEND_Translucent;
	Canvas->DrawItem(Triangle);
}

void DrawBladeBar(UCanvas* Canvas, float X, float Y, float Width, float Height, float Fraction,
	const FLinearColor& Color)
{
	if (!Canvas || !Canvas->Canvas || !GWhiteTexture || Width <= 0.0f || Height <= 0.0f) return;
	const float FillX = X + Width * FMath::Clamp(Fraction, 0.0f, 1.0f);
	if (FillX <= X + .5f) return;
	const float Tip = FMath::Min(Height * .72f, Width * .08f);
	TArray<FVector2D, TInlineAllocator<8>> Polygon = {
		FVector2D(X, Y + Height * .5f), FVector2D(X + Tip, Y),
		FVector2D(X + Width - Tip, Y), FVector2D(X + Width, Y + Height * .5f),
		FVector2D(X + Width - Tip, Y + Height), FVector2D(X + Tip, Y + Height)
	};
	TArray<FVector2D, TInlineAllocator<8>> Clipped;
	for (int32 Index = 0; Index < Polygon.Num(); ++Index)
	{
		const FVector2D A = Polygon[Index];
		const FVector2D B = Polygon[(Index + 1) % Polygon.Num()];
		const bool bAInside = A.X <= FillX;
		const bool bBInside = B.X <= FillX;
		if (bAInside) Clipped.Add(A);
		if (bAInside != bBInside)
		{
			const float T = (FillX - A.X) / (B.X - A.X);
			Clipped.Add(FMath::Lerp(A, B, T));
		}
	}
	if (Clipped.Num() < 3) return;
	for (int32 Index = 1; Index + 1 < Clipped.Num(); ++Index)
		DrawSolidTriangle(Canvas, Clipped[0], Clipped[Index], Clipped[Index + 1], Color);
}
}

ARhythmCombatHUD::ARhythmCombatHUD()
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> PhaseTwoAtlas(TEXT("/Game/UI/RhythmCombat/T_HUD_PhaseTwo_NoteAtlas.T_HUD_PhaseTwo_NoteAtlas"));
	if (PhaseTwoAtlas.Succeeded()) PhaseTwoNoteAtlas = PhaseTwoAtlas.Object;
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
	static ConstructorHelpers::FObjectFinder<UTexture2D> BossFrame(TEXT("/Game/UI/RhythmCombat/T_HUD_BossHealthFrame_Gold.T_HUD_BossHealthFrame_Gold"));
	if (BossFrame.Succeeded()) BossHealthFrameTexture = BossFrame.Object;
	static ConstructorHelpers::FObjectFinder<UTexture2D> AttackSlash(TEXT("/Game/UI/RhythmCombat/T_HUD_AttackSlash.T_HUD_AttackSlash"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> DodgeIcon(TEXT("/Game/UI/RhythmCombat/Responses/T_Response_Dodge_Minimal.T_Response_Dodge_Minimal"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> BlockIcon(TEXT("/Game/UI/RhythmCombat/Responses/T_Response_Block_Minimal.T_Response_Block_Minimal"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> CounterIcon(TEXT("/Game/UI/RhythmCombat/Responses/T_Response_Counter_Minimal.T_Response_Counter_Minimal"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> HoldIcon(TEXT("/Game/UI/RhythmCombat/Responses/T_Response_HoldBlock_Minimal.T_Response_HoldBlock_Minimal"));
	static ConstructorHelpers::FObjectFinder<URhythmResponseVisualData> Visuals(TEXT("/Game/UI/RhythmCombat/Responses/DA_RhythmResponseVisuals.DA_RhythmResponseVisuals"));
	if (Frame.Succeeded()) PlayerFrameTexture = Frame.Object;
	PlayerBeatRingTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/RhythmCombat/T_HUD_PlayerBeatRing.T_HUD_PlayerBeatRing"));
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

void ARhythmCombatHUD::DrawLine(float X1,float Y1,float X2,float Y2,FLinearColor Color,float Thickness)
{
	Color.A*=PhaseHUDOpacity;
	Super::DrawLine(X1,Y1,X2,Y2,Color,Thickness);
}

void ARhythmCombatHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !PlayerOwner) return;
	const APawn* Pawn = PlayerOwner->GetPawn();
	const UPlayerRhythmCombatComponent* State = Pawn ? Pawn->FindComponentByClass<UPlayerRhythmCombatComponent>() : nullptr;
	if (!State) return;
	if (State->Health <= 0.f)
	{
		DrawRect(FLinearColor(0,0,0,.65f),0,0,Canvas->SizeX,Canvas->SizeY);
		FCanvasTextItem Defeat(FVector2D(Canvas->SizeX*.5f,Canvas->SizeY*.43f),
			FText::FromString(TEXT("DEFEATED\nENTER - RETRY")),GEngine->GetLargeFont(),FLinearColor::White);
		Defeat.bCentreX=true; Defeat.Scale=FVector2D(1.5f); Canvas->DrawItem(Defeat);
		return;
	}
	PhaseHUDOpacity=FMath::FInterpConstantTo(PhaseHUDOpacity,State->IsPhaseCinematic()?0.f:1.f,GetWorld()->GetDeltaSeconds(),State->IsPhaseCinematic()?3.5f:2.f);
	const float Letterbox=(1.f-PhaseHUDOpacity)*Canvas->SizeY*.035f;
	if(Letterbox>.2f)
	{
		DrawRect(FLinearColor(0,0,0,.8f),0,0,Canvas->SizeX,Letterbox);
		DrawRect(FLinearColor(0,0,0,.8f),0,Canvas->SizeY-Letterbox,Canvas->SizeX,Letterbox);
	}
	if(PhaseHUDOpacity<.001f) return;
	const float SavedCanvasAlpha=Canvas->Canvas->AlphaModulate;
	Canvas->Canvas->AlphaModulate*=PhaseHUDOpacity;
	// Keep the comic HUD readable without letting it dominate the combat view.
	const float Scale = FMath::Clamp(Canvas->SizeX / 1920.0f, 0.65f, 1.35f) * 0.84f;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	DrawPlayerPanel(State, Scale, Now);
	DrawAudioVisualizer(Scale);
	DrawAttackTimingCue(State, Scale);
	DrawBossHealth(Scale);
	DrawLockMarker();
	DrawJudgment(State, Scale, Now);
	DrawFailureFeedback(Scale, Now);
	DrawPlayerAttackPulse(State, Scale, Now);
	DrawImpactComic(Scale, Now);
	DrawTutorial(Scale, Now);
	Canvas->Canvas->AlphaModulate=SavedCanvasAlpha;
}

void ARhythmCombatHUD::DrawImpactComic(float Scale,double Now)
{
	// Hi-Fi Rush / Dead as Disco style hit read: impact frame, burst lines from
	// the contact point, and a popping onomatopoeia. Pure Canvas, no assets.
	const APawn* Pawn=PlayerOwner?PlayerOwner->GetPawn():nullptr;
	const UPlayerBeatAttackComponent* A=Pawn?Pawn->FindComponentByClass<UPlayerBeatAttackComponent>():nullptr;
	if(!A || !Canvas) return;
	const float Age=float(Now-A->ImpactFxWorldTime);
	if(Age<0.f || Age>.5f) return;
	const float P=A->ImpactFxPower;
	// 1. Impact frame: one white frame, one dark frame, then gone.
	if(A->bImpactFxFrame)
	{
		if(Age<.034f) DrawRect(FLinearColor(1.f,.97f,.9f,FMath::Min(.38f,.26f*P)),0,0,Canvas->SizeX,Canvas->SizeY);
		else if(Age<.068f) DrawRect(FLinearColor(0.f,0.f,0.f,FMath::Min(.3f,.2f*P)),0,0,Canvas->SizeX,Canvas->SizeY);
	}
	const FVector S=Project(A->ImpactFxWorld);
	if(S.Z<=0.f) return;
	const FVector2D C(S.X,S.Y);
	const FLinearColor Ink=A->bImpactFxHeavy?FLinearColor(1.f,.96f,.93f):FLinearColor(.75f,.95f,1.f);
	// 2. Burst lines, thrown outward and thinning (comic speed burst).
	if(Age<.16f)
	{
		const float T=Age/.16f, E=1.f-FMath::Square(1.f-T);
		const int32 N=A->bImpactFxHeavy?16:10;
		const float Seed=float(FMath::Frac(A->ImpactFxWorldTime*7.31)*6.28);
		for(int32 I=0;I<N;++I)
		{
			const float Ang=Seed+I*2.f*PI/N+((I*37)%7-3)*.05f;
			const FVector2D Dir(FMath::Cos(Ang),FMath::Sin(Ang));
			const float Long=(I%2?1.f:.6f)*(A->bImpactFxHeavy?1.4f:1.f)*P;
			const float Inner=(24.f+150.f*E*Long)*Scale, Outer=Inner+(70.f*(1.f-T)+20.f)*Long*Scale;
			FLinearColor Col=Ink; Col.A=1.f-T;
			DrawLine(C.X+Dir.X*Inner,C.Y+Dir.Y*Inner,C.X+Dir.X*Outer,C.Y+Dir.Y*Outer,Col,(A->bImpactFxHeavy?5.f:3.f)*(1.f-.7f*T)*Scale);
		}
	}
	// 2b. Blade-tip impact: a katana "cut flash" instead of a cartoon balloon.
	//     Crimson/white only: faint glow, a long thin slash streak, an 8-arm
	//     sharp star with a crimson offset, a thin shock ring and a few shards.
	//     Small (~40 px) and short (0.18 s).
	const float TipAge=float(Now-A->TipBurstWorldTime);
	if(TipAge>=0.f && TipAge<.18f)
	{
		const FVector TS=Project(A->TipBurstWorld);
		if(TS.Z>0.f)
		{
			const FVector2D TC(TS.X,TS.Y);
			const float T=TipAge, U=T/.18f, Fade=1.f-U*U, Out=1.f-FMath::Square(1.f-U);
			const float Grow=T<.03f?1.2f*(1.f-FMath::Square(1.f-T/.03f)):FMath::Lerp(1.2f,.9f,(T-.03f)/.15f);
			const float R=(36.f+10.f*FMath::Min(P,1.5f))*Scale*Grow;
			const float Hash=float(FMath::Frac(A->TipBurstWorldTime*3.17));
			// Diagonal cut; the launcher cuts straight up (screen up = -Y).
			const float Rot=A->bImpactFxLaunch?-.5f*PI+(Hash-.5f)*.3f:(Hash<.5f?-1.f:1.f)*(.3f+.5f*Hash)+(Hash<.5f?0.f:PI);
			const FLinearColor White(1.f,1.f,1.f,Fade), Crimson(.8f,.05f,.16f,.92f*Fade);
			auto Disc=[&](FVector2D At,float Rad,FLinearColor Col)
			{
				FVector2D Prev=At+FVector2D(Rad,0.f);
				for(int32 I=1;I<=24;++I)
				{
					const float Ang=I*2.f*PI/24.f; const FVector2D Cur=At+FVector2D(FMath::Cos(Ang),FMath::Sin(Ang))*Rad;
					DrawSolidTriangle(Canvas,At,Prev,Cur,Col); Prev=Cur;
				}
			};
			auto Diamond=[&](FVector2D At,float Ang,float Len,float Wid,FLinearColor Col)
			{
				const FVector2D D(FMath::Cos(Ang),FMath::Sin(Ang)), N(-D.Y,D.X);
				DrawSolidTriangle(Canvas,At+D*Len,At+N*Wid,At-D*Len,Col);
				DrawSolidTriangle(Canvas,At+D*Len,At-N*Wid,At-D*Len,Col);
			};
			// arms: angle offset, length (fraction of R); the long pair follows the cut
			static const float ArmAng[8]={0.f,.9f,1.82f,2.64f,PI,4.09f,4.91f,5.73f};
			static const float ArmLen[8]={1.f,.34f,.5f,.3f,.8f,.36f,.46f,.3f};
			auto Star=[&](FVector2D At,float Rad,FLinearColor Col)
			{
				for(int32 I=0;I<8;++I)
				{
					const int32 J=(I+1)%8;
					const float A0=Rot+ArmAng[I], A1=Rot+ArmAng[J]+(J==0?2.f*PI:0.f), Mid=(A0+A1)*.5f;
					const FVector2D Tip0=At+FVector2D(FMath::Cos(A0),FMath::Sin(A0))*Rad*ArmLen[I];
					const FVector2D Tip1=At+FVector2D(FMath::Cos(A1),FMath::Sin(A1))*Rad*ArmLen[J];
					const FVector2D Waist=At+FVector2D(FMath::Cos(Mid),FMath::Sin(Mid))*Rad*.14f;
					DrawSolidTriangle(Canvas,At,Tip0,Waist,Col); DrawSolidTriangle(Canvas,At,Waist,Tip1,Col);
				}
			};
			// glow
			Disc(TC,R*1.1f,FLinearColor(1.f,.27f,.35f,.07f*Fade));
			Disc(TC,R*.8f,FLinearColor(1.f,.27f,.35f,.09f*Fade));
			Disc(TC,R*.58f,FLinearColor(1.f,.27f,.35f,.12f*Fade));
			// slash streak (crimson under-edge, white blade of light)
			const float Streak=(70.f+60.f*FMath::Min(1.f,T/.05f))*(A->bImpactFxLaunch?1.6f:1.f)*Scale, Thin=FMath::Max(.6f,5.f*(1.f-U))*Scale;
			Diamond(TC+FVector2D(2.f,2.f)*Scale,Rot,Streak*1.02f,Thin*1.6f,Crimson);
			Diamond(TC,Rot,Streak,Thin,White);
			// shock ring
			{
				const float RingR=(10.f+62.f*Out)*Scale; const float Th=FMath::Max(1.f,(5.f-4.5f*U)*Scale);
				FVector2D Prev=TC+FVector2D(RingR,0.f);
				for(int32 I=1;I<=32;++I)
				{
					const float Ang=I*2.f*PI/32.f; const FVector2D Cur=TC+FVector2D(FMath::Cos(Ang),FMath::Sin(Ang))*RingR;
					DrawLine(Prev.X,Prev.Y,Cur.X,Cur.Y,FLinearColor(1.f,.92f,.92f,.8f*Fade*(1.f-U)),Th); Prev=Cur;
				}
			}
			// star
			Star(TC+FVector2D(3.f,3.f)*Scale,R*1.1f,Crimson);
			Star(TC,R,White);
			// shards
			for(int32 I=0;I<7;++I)
			{
				const float Ang=Rot+.4f+I*2.f*PI/7.f+(((I*37)%5)-2)*.12f;
				const FVector2D D(FMath::Cos(Ang),FMath::Sin(Ang)), N(-D.Y,D.X);
				const float R0=(22.f+78.f*Out)*Scale*(I%2?1.f:.8f), L=(18.f*(1.f-U)+5.f)*Scale, W=2.4f*Scale*(1.f-.6f*U);
				const FVector2D P0=TC+D*R0;
				DrawSolidTriangle(Canvas,P0+N*W,TC+D*(R0+L),P0-N*W,White);
			}
		}
	}
	// 2c. Launcher: rising speed lines over the target, thrown upward with it.
	if(A->bImpactFxLaunch && Age<.5f)
	{
		const float U=Age/.5f, E=1.f-FMath::Square(1.f-U);
		for(int32 I=0;I<11;++I)
		{
			const float X=C.X+(float((I*73)%11)-5.f)*16.f*Scale;
			const float Len=(50.f+40.f*float((I*29)%3))*(1.f-.6f*U)*Scale;
			const float Y0=C.Y-(30.f+320.f*E*(.7f+.3f*float((I*17)%4)/3.f))*Scale;
			DrawLine(X,Y0+Len,X,Y0,FLinearColor(1.f,1.f,1.f,.85f*(1.f-U)),(I%3?2.f:3.5f)*Scale);
		}
	}
	// 3. Onomatopoeia: pops in oversized, settles, drifts up and fades.
	if(!A->ImpactFxWord.IsEmpty() && Age<.45f && GEngine)
	{
		const float Pop=Age<.07f?FMath::Lerp(2.1f,1.f,1.f-FMath::Square(1.f-Age/.07f)):1.f+.04f*FMath::Sin(Age*40.f);
		const float Alpha=Age<.3f?1.f:1.f-(Age-.3f)/.15f;
		const FVector2D At=C+FVector2D(46.f,-78.f-40.f*Age)*Scale;
		FLinearColor Col=Ink; Col.A=Alpha;
		FCanvasTextItem Word(At,FText::FromString(A->ImpactFxWord),GEngine->GetLargeFont(),Col);
		Word.Scale=FVector2D((A->bImpactFxHeavy?1.7f:1.3f)*Pop*Scale);
		Word.bCentreX=true; Word.bCentreY=true; Word.bOutlined=true; Word.OutlineColor=FLinearColor(0.f,0.f,0.f,Alpha);
		Word.EnableShadow(FLinearColor(.8f,.05f,.16f,Alpha*.9f),FVector2D(2.f,2.f)*Scale);
		Canvas->DrawItem(Word);
		if(A->bImpactFxPerfect)
		{
			FCanvasTextItem Just(At+FVector2D(0.f,34.f)*Scale*Pop,FText::FromString(TEXT("JUST!")),GEngine->GetMediumFont(),FLinearColor(1.f,1.f,1.f,Alpha));
			Just.Scale=FVector2D(1.3f*Scale); Just.bCentreX=true; Just.bCentreY=true; Just.bOutlined=true; Just.OutlineColor=FLinearColor(0.f,0.f,0.f,Alpha);
			Canvas->DrawItem(Just);
		}
	}
}

void ARhythmCombatHUD::DrawPlayerAttackPulse(const UPlayerRhythmCombatComponent* State,float Scale,double Now)
{
	const auto* Clock=GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	const auto* Attacks=State->GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>();
	if(!Clock || !Clock->IsRunning() || !Attacks || State->Health<=0.f) return;
	Scale=FMath::Max(1.f,Scale);
	const double Beat=Clock->GetCurrentBeat();
	const float Phase=FMath::Frac(Beat);
	const bool Strong=(FMath::CeilToInt(Beat)%FMath::Max(1,Clock->GetBeatsPerBar()))==0;
	const FVector2D Center=bBladeScoreDrawn?BladeAttackTextAnchor-FVector2D(0.f,42.f*Scale):FVector2D(Canvas->SizeX*.5f,Canvas->SizeY*.72f);
	if(!bBladeScoreDrawn)
	{
	// Dead-as-Disco style beat core in the command rail's language: beat bars
	// slide in from BOTH sides along a thin rail and meet in a diamond "core" on
	// the beat (same diamond / navy fill / gold+cyan ticks as the boss rail gate).
	const double SPB=Clock->GetSecondsPerBeat();
	const float Span=230.f*Scale, Inner=30.f*Scale;
	const float Impulse=FMath::Exp(-Phase*9.f);
	const FLinearColor Gold(1.f,.71f,.14f), Cyan(.55f,.88f,1.f), Navy(.002f,.006f,.012f);
	// Rail: dark underlay + faint cyan line, fading toward both ends.
	for(int32 Side=-1;Side<=1;Side+=2)
		for(int32 I=0;I<8;++I)
		{
			const float A0=Inner+(Span-Inner)*I/8.f, A1=Inner+(Span-Inner)*(I+1)/8.f, Fade=1.f-float(I)/8.f;
			DrawRect(FLinearColor(Navy.R,Navy.G,Navy.B,.55f*Fade),Center.X+Side*(Side<0?A1:A0),Center.Y-3.f*Scale,A1-A0,6.f*Scale);
			DrawRect(FLinearColor(.04f,.72f,.92f,.35f*Fade),Center.X+Side*(Side<0?A1:A0),Center.Y-.75f*Scale,A1-A0,1.5f*Scale);
		}
	// Converging bars: whole beats (gold on the downbeat) and faint half-beats.
	const double BeatNow=Clock->GetCurrentBeat();
	for(int32 K=0;K<=5;++K)
	{
		const double SlotBeat=FMath::FloorToDouble(BeatNow*2.)*.5+K*.5;
		const float Until=float((SlotBeat-BeatNow)*SPB);
		const float U=Until/float(2.*SPB); // two beats of look-ahead
		if(U<=0.f || U>1.f) continue;
		const bool bWhole=FMath::IsNearlyEqual(FMath::Frac(SlotBeat),0.,.01);
		const bool bDown=bWhole && (FMath::RoundToInt(SlotBeat)%FMath::Max(1,Clock->GetBeatsPerBar()))==0;
		const float Off=Inner+U*(Span-Inner);
		const float Alpha=(1.f-U*U*U)*(bWhole?1.f:.45f);
		const float H=(bDown?30.f:bWhole?22.f:10.f)*Scale, W=(bWhole?4.f:2.f)*Scale;
		const FLinearColor Col=bDown?Gold:Cyan;
		for(int32 Side=-1;Side<=1;Side+=2)
		{
			const float X=Center.X+Side*Off;
			DrawRect(FLinearColor(0.f,0.f,0.f,.7f*Alpha),X-W*.5f-1.5f*Scale,Center.Y-H*.5f-1.5f*Scale,W+3.f*Scale,H+3.f*Scale);
			DrawRect(FLinearColor(Col.R,Col.G,Col.B,Alpha),X-W*.5f,Center.Y-H*.5f,W,H);
		}
	}
	// The core: navy diamond with a gold edge, crossed-blade glyph, pulsing on
	// the beat; the last attack's grade tints it (gold Perfect, cyan Good, red Bad).
	const float JudgeAge=float(Now-Attacks->LastJudgedWorldSeconds);
	const bool bJudged=JudgeAge>=0.f && JudgeAge<.3f;
	const FLinearColor GradeCol=Attacks->LastGrade==EPlayerAttackGrade::Perfect?Gold:Attacks->LastGrade==EPlayerAttackGrade::Good?Cyan:FLinearColor(1.f,.3f,.22f);
	const float GradeAmt=bJudged?1.f-JudgeAge/.3f:0.f;
	const float R=(22.f+5.f*Impulse+6.f*GradeAmt)*Scale;
	auto Diamond=[&](float Rad){ return TArray<FVector2D>{Center+FVector2D(0,-Rad),Center+FVector2D(Rad,0),Center+FVector2D(0,Rad),Center+FVector2D(-Rad,0)}; };
	auto Outline=[&](float Rad,FLinearColor Col,float Th){ const auto P=Diamond(Rad); for(int32 I=0;I<4;++I) DrawLine(P[I].X,P[I].Y,P[(I+1)%4].X,P[(I+1)%4].Y,Col,Th); };
	{
		const auto P=Diamond(R+3.f*Scale);
		DrawSolidQuad(Canvas,P[0],P[1],P[2],P[3],FLinearColor(0.f,0.f,0.f,.75f));
		const auto Q=Diamond(R);
		FLinearColor Fill=FLinearColor::LerpUsingHSV(FLinearColor(.02f,.05f,.09f),GradeCol*.35f,GradeAmt); Fill.A=.95f;
		DrawSolidQuad(Canvas,Q[0],Q[1],Q[2],Q[3],Fill);
	}
	const FLinearColor Edge=bJudged?GradeCol:(Strong?Gold:FLinearColor(1.f,.91f,.62f));
	Outline(R,FLinearColor(Edge.R,Edge.G,Edge.B,.9f+.1f*Impulse),2.5f*Scale);
	Outline(R*.72f,FLinearColor(.04f,.72f,.92f,.35f+.4f*Impulse),1.2f*Scale);
	// Crossed-blade glyph.
	for(int32 Sgn=-1;Sgn<=1;Sgn+=2)
	{
		const FVector2D Dir=FVector2D(.7071f*Sgn,-.7071f), N(-Dir.Y,Dir.X);
		const float L=R*.62f, Wd=1.8f*Scale;
		const FLinearColor Blade(1.f,.95f,.82f,.9f);
		DrawSolidQuad(Canvas,Center+Dir*L,Center+N*Wd,Center-Dir*L,Center-N*Wd,Blade);
	}
	// On-beat flash ring, and a grade burst that throws the diamond outward.
	if(Phase<.18f) Outline(R+(Phase/.18f)*16.f*Scale,FLinearColor(1.f,1.f,1.f,1.f-Phase/.18f),2.f*Scale);
	if(bJudged) Outline(R+(1.f-GradeAmt)*34.f*Scale,FLinearColor(GradeCol.R,GradeCol.G,GradeCol.B,GradeAmt),3.f*Scale);
	} // !bBladeScoreDrawn
	// No text under the beat core: the diamond, its colour and the burst carry the read.
}

void ARhythmCombatHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bSpectrumAnalysisStarted && GetWorld())
		UAudioMixerBlueprintLibrary::StopAnalyzingOutput(GetWorld(), nullptr);
	bSpectrumAnalysisStarted = false;
	Super::EndPlay(EndPlayReason);
}

void ARhythmCombatHUD::DrawAudioVisualizer(float Scale)
{
	if (!Canvas || !GetWorld()) return;
	const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	if (!Clock || !Clock->IsRunning()) return;

	constexpr int32 BandCount = 18;
	static const TArray<float> Frequencies = {
		55.f, 75.f, 100.f, 135.f, 180.f, 240.f, 320.f, 430.f, 580.f,
		780.f, 1050.f, 1400.f, 1900.f, 2550.f, 3400.f, 4600.f, 6200.f, 8400.f
	};
	if (!bSpectrumAnalysisStarted)
	{
		UAudioMixerBlueprintLibrary::StartAnalyzingOutput(GetWorld(), nullptr);
		bSpectrumAnalysisStarted = true;
		SmoothedSpectrum.Init(0.0f, BandCount);
	}

	TArray<float> Magnitudes;
	UAudioMixerBlueprintLibrary::GetMagnitudeForFrequencies(GetWorld(), Frequencies, Magnitudes, nullptr);
	if (SmoothedSpectrum.Num() != BandCount) SmoothedSpectrum.Init(0.0f, BandCount);
	const float DeltaSeconds = FMath::Clamp(GetWorld()->GetDeltaSeconds(), 0.0f, 0.1f);
	float CurrentPeak = 0.0f;
	for (const float Magnitude : Magnitudes) CurrentPeak = FMath::Max(CurrentPeak, Magnitude);
	SpectrumAutoPeak = FMath::Max(CurrentPeak,
		FMath::FInterpTo(SpectrumAutoPeak, 0.0025f, DeltaSeconds, 0.65f));

	const double Beat = Clock->GetCurrentBeat();
	const float BeatPhase = static_cast<float>(Beat - FMath::FloorToDouble(Beat));
	const float BeatGlow = FMath::Exp(-BeatPhase * 7.0f);
	const bool bDownbeat = Clock->GetCurrentBeatInBar() == 0;
	for (int32 Index = 0; Index < BandCount; ++Index)
	{
		const float Raw = Magnitudes.IsValidIndex(Index) ? Magnitudes[Index] : 0.0f;
		const float AudioValue = FMath::Sqrt(FMath::Clamp(Raw / FMath::Max(SpectrumAutoPeak, 0.0001f), 0.0f, 1.0f));
		// A quiet beat floor keeps the display musically alive during sparse
		// passages while the FFT remains the main source of its silhouette.
		const float BeatShape = 0.5f + 0.5f * FMath::Sin(Index * 1.71f + static_cast<float>(Beat) * 0.41f);
		const float Target = FMath::Clamp(0.06f + AudioValue * 0.84f + BeatGlow * BeatShape * 0.16f, 0.0f, 1.0f);
		const float Speed = Target > SmoothedSpectrum[Index] ? 20.0f : 7.0f;
		SmoothedSpectrum[Index] = FMath::FInterpTo(SmoothedSpectrum[Index], Target, DeltaSeconds, Speed);
	}
	float SpectrumMin = 1.0f;
	float SpectrumMax = 0.0f;
	for (const float Value : SmoothedSpectrum)
	{
		SpectrumMin = FMath::Min(SpectrumMin, Value);
		SpectrumMax = FMath::Max(SpectrumMax, Value);
	}

	// Horizontal mirrored waveform inspired by the supplied reference. It uses
	// the live FFT but stays completely transparent over the game scene.
	const FVector2D WaveStart(20.0f * Scale, Canvas->SizeY * 0.53f);
	const float WaveWidth = 230.0f * Scale;
	constexpr int32 BarCount = 96;
	const float FlowTime = static_cast<float>(GetWorld()->GetTimeSeconds());

	FCanvasLineItem Baseline(WaveStart, WaveStart + FVector2D(WaveWidth, 0.0f));
	Baseline.SetColor(FLinearColor(0.34f, 0.025f, 0.018f, (0.34f + BeatGlow * 0.14f)*PhaseHUDOpacity));
	Baseline.LineThickness = 0.8f * Scale;
	Canvas->DrawItem(Baseline);

	for (int32 Bar = 0; Bar < BarCount; ++Bar)
	{
		const float T = static_cast<float>(Bar) / (BarCount - 1);
		const float BandPosition = T * (BandCount - 1) * 2.0f;
		const float MirroredBand = BandPosition <= BandCount - 1
			? BandPosition : 2.0f * (BandCount - 1) - BandPosition;
		const int32 I0 = FMath::Clamp(FMath::FloorToInt(MirroredBand), 0, BandCount - 1);
		const int32 I1 = FMath::Min(I0 + 1, BandCount - 1);
		const float BaseValue = FMath::Lerp(SmoothedSpectrum[I0], SmoothedSpectrum[I1], MirroredBand - I0);
		const float RelativeValue = (BaseValue - SpectrumMin) / FMath::Max(SpectrumMax - SpectrumMin, 0.035f);
		const float FineMotion = FMath::Square(0.5f + 0.5f * FMath::Sin(
			Bar * 0.43f - FlowTime * (2.6f + I0 * 0.06f)));
		const float Value = FMath::Clamp(0.05f + BaseValue * 0.42f + RelativeValue * 0.68f
			+ FineMotion * (0.06f + BeatGlow * 0.10f), 0.0f, 1.0f);
		const float HalfHeight = (1.0f + FMath::Pow(Value, 1.18f) * 40.0f
			+ BeatGlow * (0.6f + Value * 3.2f)) * Scale;
		const float X = WaveStart.X + T * WaveWidth;

		const FLinearColor DarkRed(0.38f, 0.008f, 0.014f, 0.72f);
		const FLinearColor Scarlet(0.92f, 0.035f, 0.018f, 0.92f);
		const FLinearColor WarmGold(0.95f, 0.48f, 0.055f, 0.96f);
		FLinearColor Color = T < 0.58f
			? FMath::Lerp(DarkRed, Scarlet, T / 0.58f)
			: FMath::Lerp(Scarlet, WarmGold, (T - 0.58f) / 0.42f);
		if (bDownbeat)
			Color = FMath::Lerp(Color, WarmGold, BeatGlow * (0.18f + Value * 0.34f));

		const FVector2D Top(X, WaveStart.Y - HalfHeight);
		const FVector2D Base(X, WaveStart.Y);
		FCanvasLineItem Glow(Top, Base);
		Glow.SetColor(FLinearColor(Color.R, Color.G, Color.B, (0.025f + Value * 0.045f)*PhaseHUDOpacity));
		Glow.LineThickness = (1.7f + Value * 0.7f) * Scale;
		Canvas->DrawItem(Glow);
		FCanvasLineItem BarLine(Top, Base);
		Color.A*=PhaseHUDOpacity;BarLine.SetColor(Color);
		BarLine.LineThickness = (0.58f + Value * 0.34f) * Scale;
		Canvas->DrawItem(BarLine);

		if (Value > 0.58f)
		{
			const float Tip = (1.2f + Value * 2.0f) * Scale;
			const FLinearColor TipColor(1.0f, 0.82f, 0.46f, 0.76f + Value * 0.22f);
			FCanvasLineItem TopTip(FVector2D(X, Top.Y), FVector2D(X, Top.Y + Tip));
			FLinearColor FadedTip=TipColor;FadedTip.A*=PhaseHUDOpacity;TopTip.SetColor(FadedTip);
			TopTip.LineThickness = 1.0f * Scale;
			Canvas->DrawItem(TopTip);
		}
	}
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
		// Health stays restrained so its fill remains easy to read.
		PulseScale += BeatImpulse * (bDownbeat ? .007f : .004f);
	}
	const float BaseW = FMath::Min(850.f * Scale, Canvas->SizeX * .60f);
	const float W = BaseW * PulseScale;
	const float X = (Canvas->SizeX - W) * .5f;
	const float FrameH = W / 6.03f;
	const float FrameY = Canvas->SizeY * .045f;
	// Generated frame center is transparent. Fill and trail are drawn underneath
	// so the gold outline remains crisp at every health value.
	const float RailX = X + W * .13f;
	const float RailY = FrameY + FrameH * .452f;
	const float RailW = W * .74f;
	const float RailH = FrameH * .09f;
	DrawBladeBar(Canvas, RailX, RailY, RailW, RailH, 1.0f, FLinearColor(.025f, .008f, .012f, .96f));
	DrawBladeBar(Canvas, RailX, RailY, RailW, RailH, BossHealthTrail, FLinearColor(.58f, .16f, .018f, .82f));
	DrawBladeBar(Canvas, RailX, RailY, RailW, RailH, Health, FLinearColor(.46f, .012f, .016f, 1.f));
	for (int32 Segment = 1; Segment < 6; ++Segment)
		DrawRect(FLinearColor(.02f, .012f, .014f, .72f), RailX + RailW * Segment / 6.f,
			RailY + 2.f * Scale, 2.f * Scale, RailH - 4.f * Scale);
	if (BossHealthFrameTexture)
	{
		const float BeatAlpha = FMath::Clamp((PulseScale - 1.f) * 130.f, 0.f, 1.f);
		const FLinearColor Tint = FMath::Lerp(FLinearColor::White, FLinearColor(1.f,.73f,.28f,1.f), BeatAlpha * .24f);
		FCanvasTileItem Frame(FVector2D(X, FrameY), BossHealthFrameTexture->GetResource(), FVector2D(W, FrameH), Tint);
		Frame.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Frame);
	}
	FCanvasTextItem Name(FVector2D(X + W * .18f, FrameY + FrameH * .20f), Boss->BossDisplayName,
		GEngine->GetMediumFont(), FLinearColor(.94f,.92f,.86f,1.f));
	Name.Scale = FVector2D(.78f * Scale); Name.EnableShadow(FLinearColor::Black); Canvas->DrawItem(Name);
	const float Transition=Boss->GetPhaseTransitionProgress();
	const float AreaBeats=Boss->GetGroundBurstRemainingBeats();
	if(bAreaWarningWasVisible && AreaBeats<0.f) bAreaTutorialSeen=true;
	bAreaWarningWasVisible=AreaBeats>=0.f;
	if(AreaBeats>=0.f || Boss->IsGroundBurstRecovery())
	{
		const FString Hint=AreaBeats>=0.f
			? FString::Printf(TEXT("%s · %d"),bAreaTutorialSeen?TEXT("范围重击"):TEXT("离开危险圈"),FMath::Max(1,FMath::CeilToInt(AreaBeats)))
			: FString(TEXT("破绽 · 追击"));
		FCanvasTextItem Cue(FVector2D(Canvas->SizeX*.5f,FrameY+FrameH*.8f),FText::FromString(Hint),GEngine->GetLargeFont(),AreaBeats>=0.f?FLinearColor(1.f,.55f,.18f):FLinearColor(1.f,.82f,.35f));
		Cue.bCentreX=true;Cue.Scale=FVector2D(1.1f*Scale);Cue.EnableShadow(FLinearColor::Black);Canvas->DrawItem(Cue);
	}
	if(Transition>=0.f)
	{
		FCanvasTextItem Phase(FVector2D(Canvas->SizeX*.5f-54.f*Scale,FrameY+FrameH*.8f),FText::FromString(TEXT("PHASE II")),GEngine->GetLargeFont(),FLinearColor(1.f,.7f,.2f));
		Phase.Scale=FVector2D(Scale);Phase.EnableShadow(FLinearColor::Black);Canvas->DrawItem(Phase);
		for(int32 I=0;I<4;++I)
			DrawRect(FLinearColor(1.f,.65f,.12f,Transition*4.f>=I?.9f:.2f),Canvas->SizeX*.5f+(I-2)*29.f*Scale,FrameY+FrameH*1.1f,23.f*Scale,3.f*Scale);
	}
}

void ARhythmCombatHUD::DrawTimingGate(const UPlayerRhythmCombatComponent* State, float JudgeX, float MidY, float Scale, float FadeIn, float AbsInputError)
{
	const float PerfectWindow = FMath::Max(.001f, State->GetPerfectWindowSeconds());
	const float GoodWindow = FMath::Max(PerfectWindow, State->GetGoodWindowSeconds());
	const float PerfectAmount = FMath::SmoothStep(0.0f, 1.0f,
		1.0f - FMath::Clamp(AbsInputError / PerfectWindow, 0.0f, 1.0f));
	const float GoodAmount = FMath::SmoothStep(0.0f, 1.0f,
		1.0f - FMath::Clamp(AbsInputError / GoodWindow, 0.0f, 1.0f));
	const float GateScale = 1.0f + GoodAmount * .07f + PerfectAmount * .24f;
	const float HalfSize = 24.0f * Scale * GateScale;
	const FVector2D GateCenter(JudgeX, MidY);
	auto DrawDiamondFill = [&](float InHalfSize, const FLinearColor& Color)
	{
		DrawSolidQuad(Canvas,
			GateCenter + FVector2D(0, -InHalfSize), GateCenter + FVector2D(InHalfSize, 0),
			GateCenter + FVector2D(0, InHalfSize), GateCenter + FVector2D(-InHalfSize, 0), Color);
	};
	auto DrawDiamondOutline = [&](float InHalfSize, const FLinearColor& Color, float Thickness)
	{
		const FVector2D Top = GateCenter + FVector2D(0, -InHalfSize);
		const FVector2D Right = GateCenter + FVector2D(InHalfSize, 0);
		const FVector2D Bottom = GateCenter + FVector2D(0, InHalfSize);
		const FVector2D Left = GateCenter + FVector2D(-InHalfSize, 0);
		DrawLine(Top.X, Top.Y, Right.X, Right.Y, Color, Thickness);
		DrawLine(Right.X, Right.Y, Bottom.X, Bottom.Y, Color, Thickness);
		DrawLine(Bottom.X, Bottom.Y, Left.X, Left.Y, Color, Thickness);
		DrawLine(Left.X, Left.Y, Top.X, Top.Y, Color, Thickness);
	};

	const FLinearColor GateColor = FMath::Lerp(
		FLinearColor(.08f, .78f, 1.0f, FadeIn),
		FLinearColor(1.0f, .72f, .08f, FadeIn), PerfectAmount);
	const FLinearColor WhiteHot = FMath::Lerp(GateColor,
		FLinearColor(1.0f, .98f, .82f, FadeIn), PerfectAmount);

	// Broad translucent energy silhouette behind the physical gate.
	if (GoodAmount > KINDA_SMALL_NUMBER)
	{
		DrawDiamondFill(HalfSize * (1.52f + .34f * PerfectAmount),
			FLinearColor(GateColor.R, GateColor.G, GateColor.B,
				(.07f * GoodAmount + .16f * PerfectAmount) * FadeIn));
		DrawDiamondOutline(HalfSize * (1.24f + .24f * PerfectAmount),
			FLinearColor(GateColor.R, GateColor.G, GateColor.B, .24f * GoodAmount * FadeIn),
			(2.0f + 3.0f * PerfectAmount) * Scale);
	}

	// Asymmetric black blade housing gives the gate the same cut-paper silhouette
	// as the rest of the HUD instead of looking like a primitive shape.
	const float Wing = HalfSize * (.72f + .28f * PerfectAmount);
	DrawSolidQuad(Canvas,
		GateCenter + FVector2D(-HalfSize * 1.62f - Wing, -4.f * Scale),
		GateCenter + FVector2D(-HalfSize * .72f, -HalfSize * .54f),
		GateCenter + FVector2D(-HalfSize * .92f, HalfSize * .43f),
		GateCenter + FVector2D(-HalfSize * 1.62f - Wing * .55f, 7.f * Scale),
		FLinearColor(.002f, .006f, .012f, .94f * FadeIn));
	DrawSolidQuad(Canvas,
		GateCenter + FVector2D(HalfSize * .70f, -HalfSize * .45f),
		GateCenter + FVector2D(HalfSize * 1.60f + Wing * .65f, -8.f * Scale),
		GateCenter + FVector2D(HalfSize * 1.60f + Wing, 4.f * Scale),
		GateCenter + FVector2D(HalfSize * .90f, HalfSize * .55f),
		FLinearColor(.002f, .006f, .012f, .94f * FadeIn));
	// Colored blade insets run through those wings and flare out on Perfect.
	DrawSolidTriangle(Canvas,
		GateCenter + FVector2D(-HalfSize * (1.42f + .34f * PerfectAmount), 0),
		GateCenter + FVector2D(-HalfSize * .68f, -HalfSize * .28f),
		GateCenter + FVector2D(-HalfSize * .82f, HalfSize * .18f),
		FLinearColor(GateColor.R, GateColor.G, GateColor.B, (.54f + .34f * PerfectAmount) * FadeIn));
	DrawSolidTriangle(Canvas,
		GateCenter + FVector2D(HalfSize * (1.42f + .34f * PerfectAmount), 0),
		GateCenter + FVector2D(HalfSize * .68f, HalfSize * .28f),
		GateCenter + FVector2D(HalfSize * .82f, -HalfSize * .18f),
		FLinearColor(GateColor.R, GateColor.G, GateColor.B, (.54f + .34f * PerfectAmount) * FadeIn));

	// Heavy outer chassis, recessed face, and an offset cyan/gold registration edge.
	DrawDiamondFill(HalfSize * 1.10f, FLinearColor(.001f, .004f, .009f, .96f * FadeIn));
	DrawDiamondOutline(HalfSize * 1.10f, FLinearColor(.0f, .0f, .0f, FadeIn), 7.0f * Scale);
	DrawDiamondOutline(HalfSize * 1.02f, GateColor, (2.3f + 2.2f * PerfectAmount) * Scale);

	// Broken edge segments read like four short sword cuts and leave deliberate
	// gaps at the corners for a sharper manga silhouette.
	const FVector2D Top = GateCenter + FVector2D(0, -HalfSize * .88f);
	const FVector2D Right = GateCenter + FVector2D(HalfSize * .88f, 0);
	const FVector2D Bottom = GateCenter + FVector2D(0, HalfSize * .88f);
	const FVector2D Left = GateCenter + FVector2D(-HalfSize * .88f, 0);
	auto DrawBrokenEdge = [&](const FVector2D& A, const FVector2D& B)
	{
		const FVector2D A1 = FMath::Lerp(A, B, .12f);
		const FVector2D B1 = FMath::Lerp(A, B, .43f);
		const FVector2D A2 = FMath::Lerp(A, B, .60f);
		const FVector2D B2 = FMath::Lerp(A, B, .92f);
		DrawLine(A1.X, A1.Y, B1.X, B1.Y, WhiteHot, (2.0f + 1.4f * PerfectAmount) * Scale);
		DrawLine(A2.X, A2.Y, B2.X, B2.Y, WhiteHot, (2.0f + 1.4f * PerfectAmount) * Scale);
	};
	DrawBrokenEdge(Top, Right); DrawBrokenEdge(Right, Bottom);
	DrawBrokenEdge(Bottom, Left); DrawBrokenEdge(Left, Top);

	// Hollow center keeps the incoming command icon readable. A tiny core only
	// appears during Perfect, so the gate feels charged rather than permanently bright.
	DrawDiamondFill(HalfSize * .48f, FLinearColor(.002f, .008f, .014f, .92f * FadeIn));
	DrawDiamondOutline(HalfSize * .48f,
		FLinearColor(WhiteHot.R, WhiteHot.G, WhiteHot.B, (.28f + .62f * PerfectAmount) * FadeIn),
		(1.5f + 2.0f * PerfectAmount) * Scale);
	if (PerfectAmount > .01f)
	{
		DrawDiamondFill(HalfSize * (.16f + .12f * PerfectAmount),
			FLinearColor(1.0f, .98f, .84f, .72f * PerfectAmount * FadeIn));
		// Unequal glint lengths keep the burst graphic and avoid a generic plus sign.
		DrawRect(FLinearColor(1.0f, .98f, .84f, .82f * PerfectAmount * FadeIn),
			JudgeX - (1.2f + 2.2f * PerfectAmount) * Scale, MidY - HalfSize * 1.90f,
			(2.4f + 4.4f * PerfectAmount) * Scale, HalfSize * 3.80f);
		DrawRect(FLinearColor(1.0f, .70f, .08f, .58f * PerfectAmount * FadeIn),
			JudgeX - HalfSize * 1.35f, MidY - (1.0f + PerfectAmount) * Scale,
			HalfSize * 2.70f, (2.0f + 2.0f * PerfectAmount) * Scale);
	}
}

void ARhythmCombatHUD::DrawPhaseTwoTimingRail(const UPlayerRhythmCombatComponent* State,UBeatSyncedComboComponent* Boss,float Scale)
{
	const auto* Clock=GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	if(!Clock || !Clock->IsRunning() || !State || !Boss) return;
	const double Beat=Clock->GetCurrentBeat(), Now=Clock->GetAudioNowSeconds();
	if(PhaseTwoRailFirstBeat<0.) PhaseTwoRailFirstBeat=Beat;
	const float Familiarity=FMath::SmoothStep(8.f,32.f,float(Beat-PhaseTwoRailFirstBeat));
	const float BaseAlpha=FMath::Lerp(.92f,.72f,Familiarity);
	// The same frame, dimensions and timing gate as phase one: learn one HUD.
	const float W=1000.f*Scale,H=W*(724.f/2172.f),X=(Canvas->SizeX-W)*.5f;
	const float Top=Canvas->SizeY-H-8.f*Scale+(1.f-PhaseHUDOpacity)*12.f*Scale;
	const float Y=Top+H*.493f,Gate=X+W*.25f,Right=X+W*.91f,Travel=Right-Gate-30.f*Scale;
	const float Pulse=FMath::Exp(-float(FMath::Frac(Beat))*6.5f);
	const float Decoration=1.f+Pulse*(Clock->GetCurrentBeatInBar()==0?.050f:.030f);
	if(AttackTimingRailTexture && AttackTimingRailTexture->GetResource())
	{
		const FVector2D Size(W*Decoration,H*Decoration);
		FCanvasTileItem Frame({X-(Size.X-W)*.5f,Top-(Size.Y-H)*.5f},AttackTimingRailTexture->GetResource(),Size,
			FLinearColor(1.f,1.f,1.f,.90f+Pulse*.10f));
		Frame.BlendMode=SE_BLEND_Translucent;Canvas->DrawItem(Frame);
	}
	DrawRect(FLinearColor(.005f,.01f,.015f,.95f),Gate-3.f*Scale,Y-48.f*Scale,6.f*Scale,96.f*Scale);
	DrawRect(FLinearColor(1.f,.91f,.62f,1.f),Gate-Scale,Y-48.f*Scale,2.f*Scale,96.f*Scale);
	TArray<FUpcomingImpactCue> Cues;Boss->GetUpcomingImpactCues(Cues,8);
	const float LookAhead=Clock->GetSecondsPerBeat()*6.f;
	float NearestError=100.f;
	for(const auto& Cue:Cues)
	{
		const float Until=Cue.ImpactAudioSeconds-Cue.IdealDodgeLeadSeconds-Now;
		if(Until<-.16f || Until>LookAhead) continue;
		const bool bArea=Cue.DamageShape==EBossAttackDamageShape::GroundBurst;
		const float Lead=bArea?LookAhead:Clock->GetSecondsPerBeat()*3.f;
		const float Fade=(1.f-FMath::SmoothStep(Lead-.35f,Lead,Until))*FMath::SmoothStep(-.16f,-.02f,Until);
		if(Fade<=0.f) continue;
		NearestError=FMath::Min(NearestError,FMath::Abs(Until));
	}
	DrawTimingGate(State,Gate,Y,Scale,BaseAlpha,NearestError);
	for(const auto& Cue:Cues)
	{
		const float Until=Cue.ImpactAudioSeconds-Cue.IdealDodgeLeadSeconds-Now;
		if(Until<-.16f || Until>LookAhead) continue;
		const bool bArea=Cue.DamageShape==EBossAttackDamageShape::GroundBurst;
		const float Lead=bArea?LookAhead:Clock->GetSecondsPerBeat()*3.f;
		const float Fade=(1.f-FMath::SmoothStep(Lead-.35f,Lead,Until))*FMath::SmoothStep(-.16f,-.02f,Until);
		if(Fade<=0.f) continue;
		const float Approach=1.f-FMath::SmoothStep(0.f,State->GetGoodWindowSeconds(),FMath::Abs(Until));
		const float CX=Gate+Until/LookAhead*Travel;
		const float Alpha=Fade*FMath::Lerp(BaseAlpha,1.f,Approach);
		if(bArea)
		{
			const float Start=FMath::Max(Gate,CX-Clock->GetSecondsPerBeat()*4.f/LookAhead*Travel);
			if(CX>Start)
			{
				DrawRect(FLinearColor(.78f,.46f,.15f,Alpha*.12f),Start,Y-3.f*Scale,CX-Start,6.f*Scale);
				DrawLine(Start,Y+4.f*Scale,CX,Y+4.f*Scale,FLinearColor(.94f,.68f,.30f,Alpha*.55f),Scale);
			}
		}
		// Shared neutral timing notes deliberately convey no required input button.
		// Larger notes (was 62/46): readable at a glance while moving the camera.
		const float D=(bArea?100.f:76.f)*Scale*(1.f+Approach*.12f);
		if(PhaseTwoNoteAtlas && PhaseTwoNoteAtlas->GetResource())
		{
			FCanvasTileItem Note({CX-D*.5f,Y-D*.5f},PhaseTwoNoteAtlas->GetResource(),{D,D},FLinearColor(1.f,1.f,1.f,Alpha));
			Note.UV0=bArea?FVector2D(856.f/1536.f,164.f/1024.f):FVector2D(96.f/1536.f,176.f/1024.f);
			Note.UV1=bArea?FVector2D(1456.f/1536.f,764.f/1024.f):FVector2D(672.f/1536.f,752.f/1024.f);
			Note.BlendMode=SE_BLEND_Translucent;Canvas->DrawItem(Note);
		}
	}
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
	bBladeScoreDrawn = false;
	if (!State || !Canvas || !GetWorld()) return;
	UBeatSyncedComboComponent* Combo = FindCueCombo();
	const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	if (!Clock || !Clock->IsRunning()) return;
	if (Combo && Combo->BossPhase >= 2) { DrawPhaseTwoTimingRail(State,Combo,Scale); return; }
	TArray<FUpcomingImpactCue> Cues;
	if (Combo) Combo->GetUpcomingImpactCues(Cues, 8);
	// Blade Score replaces the phase-1 rail once its art is imported (always visible).
	// Blade Score HUD is switched off (back to the original rail). Set true to try it again.
	static const bool bUseBladeScore = false;
	if (bUseBladeScore && LoadBladeScoreArt()) { DrawBladeScore(State, Combo, Cues, Scale); bBladeScoreDrawn = true; return; }
	if (!Combo || Cues.IsEmpty()) return;
	const bool bTimingOnly = false;

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
	// The command rail is a rhythm instrument as well as a timer. Its art can
	// breathe clearly while the judge line and authored command coordinates stay fixed.
	const float DecorationPulse = 1.0f + BeatImpulse * (bDownbeatNow ? .050f : .030f);

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

	// The diamond is the exact input target. Its state comes from the same ideal
	// input timestamp and Perfect window used by gameplay judgment, so the visual
	// flash cannot drift away from the real timing window.
	if (!VisibleCues.IsEmpty())
	{
		const float AbsInputError = FMath::Abs(VisibleCues[0].UntilIdealInput);
		DrawTimingGate(State, JudgeX, MidY, Scale, FadeIn, AbsInputError);
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
	for (int32 EndIndex = 0; !bTimingOnly && EndIndex < VisibleCues.Num(); ++EndIndex)
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
			if (bTimingOnly)
			{
				const float Radius = 17.f * Scale * Pulse;
				const FLinearColor CircleColor = bPerfectNow ? FLinearColor(1.f,.85f,.35f,CueAlpha) : FLinearColor(.9f,.94f,1.f,CueAlpha);
				for (int32 S=0; S<32; ++S)
				{
					const float A=S*UE_TWO_PI/32.f, B=(S+1)*UE_TWO_PI/32.f;
					DrawLine(Visual.X+FMath::Cos(A)*Radius,MidY+FMath::Sin(A)*Radius,
						Visual.X+FMath::Cos(B)*Radius,MidY+FMath::Sin(B)*Radius,CircleColor,3.f*Scale);
				}
				continue;
			}
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
				// (no text label: the icon alone)
			}

			// (no key hint under the note)
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
	// The texture contains generous transparent padding. A slightly negative
	// origin places the visible health and resonance closer to the top-left.
	const FVector2D Origin(-34.0f * Scale, -24.0f * Scale);
	float FrameBeatScale = 1.0f;
	float BeatHighlight = 0.0f;
	bool bDownbeat = false;
	if (const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>(); Clock && Clock->IsRunning())
	{
		const double Beat = Clock->GetCurrentBeat();
		const float Phase = static_cast<float>(Beat - FMath::FloorToDouble(Beat));
		// Only the frame breathes. Content remains stable and readable.
		const float Wave = FMath::Cos(Phase * 2.0f * UE_PI);
		bDownbeat = Clock->GetCurrentBeatInBar() == 0;
		// Keep health movement subtle; only its surrounding frame breathes.
		FrameBeatScale = 1.0f + Wave * (bDownbeat ? 0.007f : 0.004f);
		BeatHighlight = Wave * 0.5f + 0.5f;
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
	if(const auto* Attacks=State->GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>())
	{
		FString Status=State->IsMouseDefenseWindow()?TEXT("DEFEND: LMB COUNTER  RMB BLOCK"):TEXT("ATTACK: LMB LIGHT  RMB HEAVY");
		if(Attacks->ResonanceState==EPlayerResonanceState::Ready) Status=TEXT("RESONANCE READY  [R / LT]");
		if(Attacks->ResonanceState==EPlayerResonanceState::Armed) Status=TEXT("GET READY ... NEXT DOWNBEAT");
		if(Attacks->IsResonanceActive()) Status=FString::Printf(TEXT("RESONANCE  %02d BEATS"),FMath::CeilToInt(Attacks->GetRemainingBeats()));
		FCanvasTextItem Mode(FVector2D(HealthX,HealthY+112*Scale),FText::FromString(Status),GEngine->GetMediumFont(),FLinearColor(1.f,.78f,.15f));
		Mode.Scale=FVector2D(.72f*Scale); Mode.EnableShadow(FLinearColor::Black); Canvas->DrawItem(Mode);
		if(Now-Attacks->LastJudgedWorldSeconds<1.2)
		{
			const TCHAR* Grade=Attacks->LastGrade==EPlayerAttackGrade::Perfect?TEXT("PERFECT"):Attacks->LastGrade==EPlayerAttackGrade::Good?TEXT("GOOD"):TEXT("OFF BEAT");
			FCanvasTextItem Attack(FVector2D(HealthX,HealthY+140*Scale),FText::FromString(Attacks->InputSequence+Grade),GEngine->GetMediumFont(),FLinearColor(.35f,1.f,1.f));
			Attack.Scale=FVector2D(.78f*Scale); Attack.EnableShadow(FLinearColor::Black); Canvas->DrawItem(Attack);
		}
	}
	const double EscapeAge=Now-State->LastRhythmicEscapeWorldSeconds;
	if(EscapeAge>=0. && EscapeAge<1.25)
	{
		const float Alpha=FMath::Clamp(static_cast<float>((1.25-EscapeAge)/.45),0.f,1.f);
		const FString Label=State->LastRhythmicEscapeGain>.01f?FString::Printf(TEXT("ON BEAT  +%d RESONANCE"),FMath::RoundToInt(State->LastRhythmicEscapeGain)):TEXT("ON BEAT  RESONANCE FULL");
		FCanvasTextItem Escape(FVector2D(HealthX,HealthY+88*Scale),FText::FromString(Label),GEngine->GetMediumFont(),FLinearColor(1.f,.72f,.18f,Alpha));
		Escape.Scale=FVector2D(.85f*Scale);Escape.EnableShadow(FLinearColor(0,0,0,Alpha));Canvas->DrawItem(Escape);
	}
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
		if (State->LastJudgment == ERhythmDodgeJudgment::Bad ||
			State->LastJudgment == ERhythmDodgeJudgment::Miss ||
			State->LastJudgment == ERhythmDodgeJudgment::HoldBreak)
		{
			FailureFeedbackStartSeconds = Now;
			FailureFeedbackGrade = State->LastJudgment;
			FailureFeedbackErrorMs = State->LastJudgmentErrorMs;
			if (const APawn* Pawn = PlayerOwner ? PlayerOwner->GetPawn() : nullptr)
			{
				if (UBossCombatCameraComponent* CombatCamera = Pawn->FindComponentByClass<UBossCombatCameraComponent>())
					CombatCamera->TriggerFailureFeedback(State->LastJudgment != ERhythmDodgeJudgment::Bad);
			}
		}
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
	float JudgmentBeatPulse = 1.0f;
	if (const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>(); Clock && Clock->IsRunning())
	{
		const double Beat = Clock->GetCurrentBeat();
		const float Phase = static_cast<float>(Beat - FMath::FloorToDouble(Beat));
		const float Impulse = FMath::Exp(-Phase * 6.0f);
		JudgmentBeatPulse += Impulse * (Clock->GetCurrentBeatInBar() == 0 ? .055f : .033f);
	}
	const FVector2D RailPixelSize = RailSize * Scale;
	const FVector2D RailCenter = RailOrigin + RailPixelSize * .5f;
	const auto PulseRailPoint = [&](const FVector2D& Point)
	{
		return RailCenter + (Point - RailCenter) * JudgmentBeatPulse;
	};
	UTexture2D* RailTexture = JudgmentRailBackboneTexture ? JudgmentRailBackboneTexture : JudgmentRailTexture;
	if (RailTexture)
	{
		const FVector2D PulsedSize = RailPixelSize * JudgmentBeatPulse;
		FCanvasTileItem Rail(RailCenter - PulsedSize * .5f, RailTexture->GetResource(), PulsedSize,
			FLinearColor(1.0f, 1.0f, 1.0f, .92f + (JudgmentBeatPulse - 1.0f) * 1.45f));
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
		const FVector2D Center = PulseRailPoint(RailOrigin + FVector2D(112.0f, 104.0f + Slot * 102.0f) * Scale);
		if (JudgmentBurstTexture && Entry.Grade == ERhythmDodgeJudgment::Perfect && Age < .65f)
		{
			const FVector2D BurstSize(265.0f * Scale * Punch * JudgmentBeatPulse);
			FCanvasTileItem Burst(Center - BurstSize * .5f, JudgmentBurstTexture->GetResource(), BurstSize,
				FLinearColor(1, .35f, .04f, Alpha));
			Burst.BlendMode = SE_BLEND_Additive; Canvas->DrawItem(Burst);
		}
		FLinearColor Color = GetJudgmentColor(Entry.Grade); Color.A = Alpha;
		const float GradeCardScale = Entry.Grade == ERhythmDodgeJudgment::Perfect ? 1.07f : 1.0f;
		const FVector2D CardSize(238.0f * Scale * Punch * GradeCardScale * JudgmentBeatPulse,
			87.5f * Scale * Punch * GradeCardScale * JudgmentBeatPulse);
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
			const FVector2D WordSize = FVector2D(184.0f, 52.0f) * Scale * Punch * GradeWordScale * JudgmentBeatPulse;
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

	const FVector2D ComboCenter = PulseRailPoint(RailOrigin + FVector2D(112.0f, 655.0f) * Scale);
	const float ComboAge = static_cast<float>(Now - ComboPunchStartSeconds);
	const float ComboPunch = ComboAge < .18f ? 1.0f + .08f * (1.0f - FMath::Clamp(ComboAge / .18f, 0.0f, 1.0f)) : 1.0f;
	const float ComboAlpha = State->ComboCount == 0 ? .45f : 1.0f;
	const FVector2D PanelSize(250.0f, 107.0f);
	if (ComboPanelTexture)
	{
		const FVector2D DrawSize = PanelSize * Scale * ComboPunch * JudgmentBeatPulse;
		FCanvasTileItem Panel(ComboCenter - DrawSize * .5f, ComboPanelTexture->GetResource(), DrawSize,
			FLinearColor(1, 1, 1, ComboAlpha));
		Panel.BlendMode = SE_BLEND_Translucent; Canvas->DrawItem(Panel);
	}
	{
		if (ComboWordTexture)
		{
			const FVector2D WordSize(119.0f * Scale * ComboPunch * JudgmentBeatPulse,
				34.0f * Scale * ComboPunch * JudgmentBeatPulse);
			FCanvasTileItem Word(ComboCenter + FVector2D(-91, -17) * Scale * ComboPunch * JudgmentBeatPulse,
				ComboWordTexture->GetResource(), WordSize,
				FVector2D(.20f, .08f), FVector2D(.80f, .90f), FLinearColor(1, 1, 1, ComboAlpha));
			Word.BlendMode = SE_BLEND_Translucent; Canvas->DrawItem(Word);
		}
		FCanvasTextItem Count(ComboCenter + FVector2D(55, 0) * Scale,
			FText::FromString(FString::Printf(TEXT("x%d"), State->ComboCount)), GEngine->GetLargeFont(), FLinearColor(1, .72f, .12f));
		Count.bCentreX = true; Count.bCentreY = true;
		Count.Scale = FVector2D(1.20f * Scale * ComboPunch * JudgmentBeatPulse);
		Count.EnableShadow(FLinearColor::Black); Canvas->DrawItem(Count);
	}
}

void ARhythmCombatHUD::DrawFailureFeedback(float Scale, double Now)
{
	if (!Canvas || FailureFeedbackGrade == ERhythmDodgeJudgment::None) return;
	const bool bMiss = FailureFeedbackGrade == ERhythmDodgeJudgment::Miss ||
		FailureFeedbackGrade == ERhythmDodgeJudgment::HoldBreak;
	const float Duration = bMiss ? .26f : .23f;
	const float Age = static_cast<float>(Now - FailureFeedbackStartSeconds);
	if (Age < 0.0f || Age >= Duration)
	{
		if (Age >= Duration) FailureFeedbackGrade = ERhythmDodgeJudgment::None;
		return;
	}

	// A fast arrival and softer tail make the cue feel like a hit instead of a modal warning.
	const float Attack = FMath::Clamp(Age / .045f, 0.0f, 1.0f);
	const float Tail = 1.0f - FMath::Clamp((Age - .045f) / (Duration - .045f), 0.0f, 1.0f);
	const float Alpha = Attack * FMath::Pow(Tail, .72f);
	const float Punch = 1.0f + .04f * FMath::Sin(FMath::Min(Age / .10f, 1.0f) * UE_PI);
	// Timing correction sits just above the command rail instead of covering the clash point.
	// A true miss still appears nearer the action because it represents a broken sequence.
	const FVector2D Center(Canvas->SizeX * .50f, Canvas->SizeY * (bMiss ? .405f : .70f));
	const FLinearColor Accent = bMiss
		? FLinearColor(.88f, .08f, .055f, Alpha * .78f)
		: FLinearColor(1.0f, .45f, .08f, Alpha * .86f);
	const FLinearColor Dark(.035f, .006f, .01f, Alpha * .46f);

	// Two broken brush brackets converge, but the middle remains transparent so the next cue stays readable.
	const float HalfW = (bMiss ? 138.0f : 58.0f) * Scale * Punch;
	const float HalfH = (bMiss ? 35.0f : 13.0f) * Scale * Punch;
	const float Cut = (bMiss ? 28.0f : 43.0f) * Scale;
	const float OuterReach = (bMiss ? 18.0f : 5.0f) * Scale;
	const float InnerReach = (bMiss ? 16.0f : 4.0f) * Scale;
	const float AccentOuterReach = (bMiss ? 10.0f : 3.0f) * Scale;
	const float AccentInnerReach = (bMiss ? 8.0f : 3.0f) * Scale;
	const float AccentLift = (bMiss ? 7.0f : 3.0f) * Scale;
	const float AccentInnerLift = (bMiss ? 12.0f : 4.0f) * Scale;
	const float AccentOuterLift = (bMiss ? 9.0f : 3.0f) * Scale;
	DrawSolidQuad(Canvas,
		Center + FVector2D(-HalfW, -HalfH), Center + FVector2D(-Cut, -HalfH * .72f),
		Center + FVector2D(-Cut - InnerReach, -HalfH * .43f), Center + FVector2D(-HalfW - OuterReach, -HalfH * .64f), Dark);
	DrawSolidQuad(Canvas,
		Center + FVector2D(Cut, HalfH * .43f), Center + FVector2D(HalfW + OuterReach, HalfH * .64f),
		Center + FVector2D(HalfW, HalfH), Center + FVector2D(Cut + InnerReach, HalfH * .72f), Dark);
	DrawSolidQuad(Canvas,
		Center + FVector2D(-HalfW, -HalfH + AccentLift), Center + FVector2D(-Cut, -HalfH * .72f + AccentLift),
		Center + FVector2D(-Cut - AccentInnerReach, -HalfH * .58f + AccentInnerLift), Center + FVector2D(-HalfW - AccentOuterReach, -HalfH * .52f + AccentOuterLift), Accent);
	DrawSolidQuad(Canvas,
		Center + FVector2D(Cut, HalfH * .43f - AccentLift), Center + FVector2D(HalfW + AccentOuterReach, HalfH * .52f - AccentOuterLift),
		Center + FVector2D(HalfW, HalfH - AccentLift), Center + FVector2D(Cut + AccentInnerReach, HalfH * .58f - AccentInnerLift), Accent);

	const FString MainText = bMiss ? TEXT("MISS") : (FailureFeedbackErrorMs < 0.0f ? TEXT("EARLY") : TEXT("LATE"));
	const FString SubText = bMiss ? TEXT("COMBO BREAK") : FString::Printf(TEXT("BAD  %+.0f ms"), FailureFeedbackErrorMs);
	FCanvasTextItem Main(Center + FVector2D(0.0f, -7.0f * Scale), FText::FromString(MainText),
		GEngine ? GEngine->GetLargeFont() : nullptr, FLinearColor(1.0f, .93f, .86f, Alpha));
	Main.bCentreX = true; Main.bCentreY = true;
	Main.Scale = FVector2D((bMiss ? 1.02f : .92f) * Scale * Punch);
	Main.EnableShadow(FLinearColor(.05f, 0.0f, 0.0f, Alpha));
	Canvas->DrawItem(Main);
	FCanvasTextItem Sub(Center + FVector2D(0.0f, 28.0f * Scale), FText::FromString(SubText),
		GEngine ? GEngine->GetSmallFont() : nullptr, Accent);
	Sub.bCentreX = true; Sub.bCentreY = true; Sub.Scale = FVector2D(.66f * Scale);
	Sub.EnableShadow(FLinearColor::Black); Canvas->DrawItem(Sub);
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

bool ARhythmCombatHUD::LoadBladeScoreArt()
{
	if (BladeArt.Contains(TEXT("T_BS2_Lane"))) return true;
	if (!GetWorld() || GetWorld()->GetTimeSeconds() < NextBladeArtLoadWorld) return false;
	NextBladeArtLoadWorld = GetWorld()->GetTimeSeconds() + 3.0; // assets appear after the editor auto-import
	static const TCHAR* Names[] = {
		TEXT("T_BS2_Lane"), TEXT("T_BS2_Judge"), TEXT("T_BS2_BossNote"), TEXT("T_BS2_AttackNote"), TEXT("T_BS2_HoldBar"),
		TEXT("T_BS2_Glyph_Dodge"), TEXT("T_BS2_Glyph_Block"), TEXT("T_BS2_Glyph_Counter"), TEXT("T_BS2_Glyph_Sword"),
		TEXT("T_BS2_Glyph_Area"), TEXT("T_BS_Spark"), TEXT("T_BS_Word_Fill") };
	TMap<FName, TObjectPtr<UTexture2D>> Loaded;
	for (const TCHAR* Name : Names)
	{
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr,
			*FString::Printf(TEXT("/Game/UI/RhythmCombat/BladeScore/%s.%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Texture) return false; // all or nothing: fall back to the old rail
		Loaded.Add(FName(Name), Texture);
	}
	BladeArt = MoveTemp(Loaded);
	return true;
}

// One lane, one judge ring. Every note is a disc with one white glyph that says
// what to press when it reaches the ring:
//   RED   disc = boss attack  -> dodge / block / counter / hold (key shown above)
//   GOLD  disc = your attack  -> any attack key; only shown in the boss's openings
//                               (gap inside a boss combo = x2, after a combo = x1.2)
void ARhythmCombatHUD::DrawBladeScore(const UPlayerRhythmCombatComponent* State, UBeatSyncedComboComponent* Combo,
	const TArray<FUpcomingImpactCue>& Cues, float Scale)
{
	const UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	const UPlayerBeatAttackComponent* Attacks = State->GetOwner()->FindComponentByClass<UPlayerBeatAttackComponent>();
	const double Now = Clock->GetAudioNowSeconds(), SPB = Clock->GetSecondsPerBeat();
	const double WorldNow = GetWorld()->GetTimeSeconds();
	const float Impulse = FMath::Exp(-static_cast<float>(FMath::Frac(Clock->GetCurrentBeat())) * 7.f);
	auto Tex = [&](const TCHAR* Name) -> UTexture2D* { const TObjectPtr<UTexture2D>* T = BladeArt.Find(FName(Name)); return T ? T->Get() : nullptr; };
	auto Tile = [&](const TCHAR* Name, FVector2D Center, FVector2D Size, FLinearColor Color, bool bAdditive = false)
	{
		UTexture2D* T = Tex(Name);
		if (!T || !T->GetResource() || Color.A <= .003f) return;
		Color.A *= PhaseHUDOpacity;
		FCanvasTileItem Item(Center - Size * .5f, T->GetResource(), Size, Color);
		Item.BlendMode = bAdditive ? SE_BLEND_Additive : SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
	};
	auto KeyCap = [&](FVector2D At, const TCHAR* Text, FLinearColor Edge, float Alpha)
	{
		if (!GEngine || Alpha <= .01f) return;
		const float Wd = (18.f + 11.f * FCString::Strlen(Text)) * Scale, Ht = 26.f * Scale;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, .85f * Alpha), At.X - Wd * .5f - 2.f * Scale, At.Y - Ht * .5f - 2.f * Scale, Wd + 4.f * Scale, Ht + 4.f * Scale);
		DrawRect(FLinearColor(Edge.R, Edge.G, Edge.B, Alpha), At.X - Wd * .5f, At.Y - Ht * .5f, Wd, Ht);
		DrawRect(FLinearColor(.03f, .03f, .05f, Alpha), At.X - Wd * .5f + 2.f * Scale, At.Y - Ht * .5f + 2.f * Scale, Wd - 4.f * Scale, Ht - 4.f * Scale);
		FCanvasTextItem Key(At, FText::FromString(Text), GEngine->GetMediumFont(), FLinearColor(1.f, 1.f, 1.f, Alpha * PhaseHUDOpacity));
		Key.bCentreX = true; Key.bCentreY = true; Key.Scale = FVector2D(.95f * Scale);
		Canvas->DrawItem(Key);
	};

	// ---- Geometry (same footprint as the old rail) ------------------------------
	const float W = 1000.f * Scale;
	const float Left = Canvas->SizeX * .5f - W * .5f;
	const float OldRailH = W * (724.f / 2172.f);
	const float MidY = Canvas->SizeY - OldRailH - 8.f * Scale + OldRailH * .493f;
	const float JudgeX = Left + W * .18f;
	const float EndX = Left + W * .97f;
	float MaxPrompt = 3.f;
	for (const FUpcomingImpactCue& Cue : Cues) MaxPrompt = FMath::Max(MaxPrompt, Cue.PromptLeadBeats);
	const double LookAhead = FMath::Max(1.8, SPB * FMath::Max(4.f, MaxPrompt));
	auto XAt = [&](double Audio) { return JudgeX + float((Audio - Now) / LookAhead) * (EndX - JudgeX); };
	auto Fade = [&](double Audio, float X) {
		const float Past = FMath::Clamp(1.f + float((Audio - Now) / .25), 0.f, 1.f);            // gone .25 s after the ring
		const float Far = 1.f - FMath::SmoothStep(EndX - 80.f * Scale, EndX, X);                // fade in at the far end
		return Past * Far; };
	const FLinearColor Gold(1.f, .72f, .16f), Red(.93f, .2f, .3f);

	TArray<FBossOpening> Openings;
	if (Combo) Combo->GetOpenings(Openings);
	auto OpeningAt = [&](double T) -> const FBossOpening* {
		for (const FBossOpening& O : Openings) if (O.Kind == EBossOpeningKind::Gap && O.Contains(T)) return &O;
		for (const FBossOpening& O : Openings) if (O.Contains(T)) return &O;
		return nullptr; };

	// ---- 1. Lane ------------------------------------------------------------------
	Tile(TEXT("T_BS2_Lane"), FVector2D((JudgeX - 40.f * Scale + EndX) * .5f, MidY), FVector2D(EndX - JudgeX + 40.f * Scale, 60.f * Scale), FLinearColor::White);

	// ---- 2. Next boss note (drives the judge ring colour) ----------------------------
	const FUpcomingImpactCue* NextCue = nullptr; float NextUntil = 1.e9f;
	for (const FUpcomingImpactCue& Cue : Cues)
	{
		const float Until = float(Cue.ImpactAudioSeconds - Cue.IdealDodgeLeadSeconds - Now);
		if (Until > -.12f && Until < NextUntil && !(Cue.RequiredResponse == ERhythmResponseType::HoldBlock && Cue.HoldMarker == ERhythmHoldMarkerType::None))
		{ NextUntil = Until; NextCue = &Cue; }
	}

	// ---- 3. Your attack notes: one gold disc per beat inside an opening ----------------
	// Not drawn within ~0.4 beat of a boss note (that beat belongs to the boss).
	bool bLabelGap = false, bLabelTail = false;
	const double FirstBeat = FMath::FloorToDouble(Clock->GetCurrentBeat()) - 1.;
	for (int32 K = 0; K < 16; ++K)
	{
		const double T = Clock->GetBeatTimeSeconds(FirstBeat + K);
		const float X = XAt(T);
		if (X > EndX) break;
		if (X < JudgeX - 60.f * Scale) continue;
		const FBossOpening* O = OpeningAt(T);
		if (!O) continue;
		bool bNearBoss = false;
		for (const FUpcomingImpactCue& Cue : Cues)
			if (FMath::Abs(T - (Cue.ImpactAudioSeconds - Cue.IdealDodgeLeadSeconds)) < .4 * SPB) { bNearBoss = true; break; }
		if (bNearBoss) continue;
		const bool bGap = O->Kind == EBossOpeningKind::Gap;
		bool bHit = false;
		if (Attacks) for (const FOpeningHitMark& M : Attacks->OpeningHitMarks) if (FMath::Abs(M.Audio - T) < .35 * SPB) { bHit = true; break; }
		const float A = Fade(T, X);
		const float Size = (bGap ? 50.f : 42.f) * Scale;
		if (bHit)
		{
			// Struck: a bright burst in place of the note.
			const float Age = FMath::Max(0.f, float(Now - T));
			Tile(TEXT("T_BS_Spark"), FVector2D(X, MidY), FVector2D(Size, Size) * (1.6f + 2.f * Age), FLinearColor(Gold.R, Gold.G, Gold.B, A), true);
			continue;
		}
		Tile(TEXT("T_BS2_AttackNote"), FVector2D(X, MidY), FVector2D(Size, Size), FLinearColor(1.f, 1.f, 1.f, A * (bGap ? 1.f : .8f)));
		Tile(TEXT("T_BS2_Glyph_Sword"), FVector2D(X, MidY), FVector2D(Size, Size) * .6f, FLinearColor(1.f, 1.f, 1.f, A));
		// Label once per kind: the key, and what it is worth.
		if (X > JudgeX + 20.f * Scale && ((bGap && !bLabelGap) || (!bGap && !bLabelTail)))
		{
			(bGap ? bLabelGap : bLabelTail) = true;
			KeyCap(FVector2D(X, MidY - 44.f * Scale), bGap ? TEXT("ATTACK x2") : TEXT("ATTACK"), Gold, A);
		}
	}

	// ---- 4. Boss notes ----------------------------------------------------------------
	for (int32 E = 0; E < Cues.Num(); ++E) // hold lanes: red bar from HOLD to RELEASE
	{
		const FUpcomingImpactCue& End = Cues[E];
		if (End.RequiredResponse != ERhythmResponseType::HoldBlock || End.HoldMarker != ERhythmHoldMarkerType::End) continue;
		double StartAt = Now;
		for (int32 S = E - 1; S >= 0; --S)
			if (Cues[S].RequiredResponse == ERhythmResponseType::HoldBlock && Cues[S].HoldMarker == ERhythmHoldMarkerType::Start &&
				Cues[S].HoldGroup == End.HoldGroup) { StartAt = Cues[S].ImpactAudioSeconds - Cues[S].IdealDodgeLeadSeconds; break; }
		const float X0 = FMath::Max(XAt(StartAt), JudgeX), X1 = FMath::Min(XAt(End.ImpactAudioSeconds - End.IdealDodgeLeadSeconds), EndX);
		if (X1 - X0 < 4.f * Scale) continue;
		const float Lit = State->IsBlocking() ? .95f : .6f;
		Tile(TEXT("T_BS2_HoldBar"), FVector2D((X0 + X1) * .5f, MidY), FVector2D(X1 - X0, 30.f * Scale), FLinearColor(Red.R, Red.G, Red.B, Lit));
	}
	for (const FUpcomingImpactCue& Cue : Cues)
	{
		const double InputAt = Cue.ImpactAudioSeconds - Cue.IdealDodgeLeadSeconds;
		const float X = XAt(InputAt);
		if (X > EndX || InputAt - Now < -.25) continue;
		if (Cue.RequiredResponse == ERhythmResponseType::HoldBlock && Cue.HoldMarker == ERhythmHoldMarkerType::None) continue; // inside a hold bar
		const float A = Fade(InputAt, X);
		const float AbsUntil = FMath::Abs(float(InputAt - Now));
		const float Pop = (Cue.bStrongBeat ? 1.1f : 1.f) * (AbsUntil <= State->GetPerfectWindowSeconds() ? 1.2f : 1.f);
		const bool bRelease = Cue.RequiredResponse == ERhythmResponseType::HoldBlock && Cue.HoldMarker == ERhythmHoldMarkerType::End;
		const float Size = (bRelease ? 50.f : 64.f) * Scale * Pop;
		Tile(TEXT("T_BS2_BossNote"), FVector2D(X, MidY), FVector2D(Size, Size), FLinearColor(1.f, 1.f, 1.f, A));
		const bool bArea = Cue.DamageShape == EBossAttackDamageShape::GroundBurst;
		const TCHAR* Glyph = bArea ? TEXT("T_BS2_Glyph_Area") :
			Cue.RequiredResponse == ERhythmResponseType::Counter ? TEXT("T_BS2_Glyph_Counter") :
			(Cue.RequiredResponse == ERhythmResponseType::Block || Cue.RequiredResponse == ERhythmResponseType::HoldBlock) ? TEXT("T_BS2_Glyph_Block") :
			TEXT("T_BS2_Glyph_Dodge");
		if (!bRelease) Tile(Glyph, FVector2D(X, MidY), FVector2D(Size, Size) * .58f, FLinearColor(1.f, 1.f, 1.f, A));
		const TCHAR* KeyText =
			bArea ? TEXT("RUN") :
			Cue.RequiredResponse == ERhythmResponseType::Counter ? TEXT("LMB") :
			Cue.RequiredResponse == ERhythmResponseType::Block ? TEXT("RMB") :
			Cue.RequiredResponse == ERhythmResponseType::HoldBlock ? (bRelease ? TEXT("RELEASE") : TEXT("HOLD RMB")) :
			Cue.RequiredResponse == ERhythmResponseType::DodgeOrBlock ? TEXT("SHIFT/RMB") : TEXT("SHIFT");
		KeyCap(FVector2D(X, MidY - Size * .5f - 18.f * Scale), KeyText, Red, A);
	}
	// Approach sound half a beat before the next note (keyed by its impact time).
	if (NextCue && ResponseVisuals && NextUntil <= float(SPB * .5) && NextUntil > 0.f &&
		FMath::Abs(NextCue->ImpactAudioSeconds - LastPromptedImpactAudioSeconds) > .001)
	{
		const FRhythmResponseVisual& Visual = ResponseVisuals->Get(NextCue->RequiredResponse);
		USoundBase* Sound = NextCue->bStrongBeat && ResponseVisuals->StrongApproachSound ? ResponseVisuals->StrongApproachSound.Get()
			: (Visual.PromptSound ? Visual.PromptSound.Get() : ResponseVisuals->ApproachSound.Get());
		if (Sound) UGameplayStatics::PlaySound2D(GetWorld(), Sound, NextCue->bStrongBeat ? 1.f : .72f);
		LastPromptedImpactAudioSeconds = NextCue->ImpactAudioSeconds;
	}

	// ---- 5. Judge ring: red when a boss note is about to arrive, gold in an opening --
	{
		const bool bDefend = NextCue && NextUntil < float(SPB * .75);
		const bool bAttack = !bDefend && OpeningAt(Now) != nullptr;
		const FLinearColor C = bDefend ? Red : bAttack ? Gold : FLinearColor(1.f, .95f, .85f);
		const float Ring = 84.f * Scale * (1.f + .06f * Impulse);
		Tile(TEXT("T_BS2_Judge"), FVector2D(JudgeX, MidY), FVector2D(Ring, Ring), C);
		if (Attacks)
		{
			const float JudgeAge = float(WorldNow - Attacks->LastJudgedWorldSeconds);
			if (JudgeAge >= 0.f && JudgeAge < .25f)
			{
				const FLinearColor G = Attacks->LastGrade == EPlayerAttackGrade::Perfect ? Gold :
					Attacks->LastGrade == EPlayerAttackGrade::Good ? FLinearColor(.5f, .9f, 1.f) : FLinearColor(.6f, .6f, .65f);
				Tile(TEXT("T_BS2_Judge"), FVector2D(JudgeX, MidY), FVector2D(Ring, Ring) * (1.f + 1.2f * JudgeAge),
					FLinearColor(G.R, G.G, G.B, 1.f - JudgeAge / .25f), true);
			}
			const float FillAge = float(WorldNow - Attacks->FillWorldTime);
			if (FillAge >= 0.f && FillAge < .8f)
			{
				const float P = FillAge < .08f ? FMath::Lerp(1.8f, 1.f, FillAge / .08f) : 1.f;
				const float A = FillAge < .55f ? 1.f : 1.f - (FillAge - .55f) / .25f;
				Tile(TEXT("T_BS_Word_Fill"), FVector2D(JudgeX + 120.f * Scale, MidY - 60.f * Scale - 20.f * Scale * FillAge),
					FVector2D(150.f, 47.f) * Scale * P, FLinearColor(1.f, 1.f, 1.f, A));
			}
		}
	}
	BladeAttackTextAnchor = FVector2D(JudgeX, MidY + 62.f * Scale);
}

void ARhythmCombatHUD::DrawTutorial(float Scale,double Now)
{
	APawn* Pawn=PlayerOwner?PlayerOwner->GetPawn():nullptr;
	if(!Pawn || !Canvas || !GEngine) return;
	UCombatTutorialComponent* T=Pawn->FindComponentByClass<UCombatTutorialComponent>();
	if(!T) { T=NewObject<UCombatTutorialComponent>(Pawn,TEXT("CombatTutorial")); T->RegisterComponent(); return; }
	auto Tex=[&](FName Name)->UTexture2D*
	{
		if(const TObjectPtr<UTexture2D>* Found=TutorialArt.Find(Name)) return Found->Get();
		UTexture2D* Loaded=LoadObject<UTexture2D>(nullptr,*FString::Printf(TEXT("/Game/UI/Tutorial/%s.%s"),*Name.ToString(),*Name.ToString()),nullptr,LOAD_NoWarn|LOAD_Quiet);
		TutorialArt.Add(Name,Loaded); return Loaded;
	};
	auto Tile=[&](UTexture2D* Texture,FVector2D Pos,FVector2D Size,FLinearColor Color,bool bAdd=false)
	{
		if(!Texture || !Texture->GetResource()) return false;
		FCanvasTileItem Item(Pos,Texture->GetResource(),Size,Color);
		Item.BlendMode=bAdd?SE_BLEND_Additive:SE_BLEND_Translucent; Canvas->DrawItem(Item); return true;
	};
	auto Text=[&](FVector2D Pos,const FString& S,UFont* Font,float Sc,FLinearColor Color,bool bCentre=false)
	{
		FCanvasTextItem Item(Pos,FText::FromString(S),Font,Color);
		Item.Scale=FVector2D(Sc); Item.bCentreX=bCentre; Item.EnableShadow(FLinearColor(0.f,0.f,0.f,.9f));
		Canvas->DrawItem(Item);
	};
	// Glyph row, right-aligned at X, returns its left edge.
	auto Glyphs=[&](const TArray<FName>& Names,float RightX,float CenterY,float H)
	{
		float X=RightX;
		for(int32 I=Names.Num()-1;I>=0;--I)
		{
			UTexture2D* G=Tex(Names[I]);
			const float W=G&&G->GetSizeY()>0?H*float(G->GetSizeX())/float(G->GetSizeY()):H;
			X-=W;
			if(!Tile(G,FVector2D(X,CenterY-H*.5f),FVector2D(W,H),FLinearColor::White))
				Text(FVector2D(X,CenterY-10.f*Scale),Names[I].ToString().RightChop(8),GEngine->GetMediumFont(),Scale,FLinearColor::White);
			X-=10.f*Scale;
		}
		return X;
	};
	auto Wrap=[](const FString& S,int32 Max)
	{
		TArray<FString> Words,Lines; S.ParseIntoArrayWS(Words); FString Line;
		for(const FString& W:Words){ if(Line.Len()+W.Len()+1>Max && !Line.IsEmpty()){ Lines.Add(Line); Line.Empty(); } Line+=(Line.IsEmpty()?TEXT(""):TEXT(" "))+W; }
		if(!Line.IsEmpty()) Lines.Add(Line); return Lines;
	};
	const FLinearColor Gold(1.f,.75f,.25f), Red(1.f,.35f,.4f);

	if(T->IsCardVisible())
	{
		const float W=860.f*Scale, H=210.f*Scale, L=Canvas->SizeX*.5f-W*.5f, Top=Canvas->SizeY*.06f;
		const float Age=float(Now-T->StepStartWorld), In=FMath::Clamp(Age/.25f,0.f,1.f);
		const float Y0=Top-(1.f-In)*30.f*Scale;
		if(!Tile(Tex(TEXT("T_TUT_Card")),FVector2D(L,Y0),FVector2D(W,H),FLinearColor(1.f,1.f,1.f,In)))
			DrawRect(FLinearColor(.03f,.03f,.05f,.85f*In),L,Y0,W,H);
		const float Flash=FMath::Clamp(1.f-float(Now-T->SuccessFlashWorld)/.5f,0.f,1.f);
		if(Flash>0.f) Tile(Tex(TEXT("T_TUT_Card")),FVector2D(L,Y0),FVector2D(W,H),FLinearColor(Gold.R,Gold.G,Gold.B,.5f*Flash),true);
		// Input glyphs (right), title and body (left).
		const float GlyphLeft=Glyphs(T->Glyphs,L+W-36.f*Scale,Y0+62.f*Scale,72.f*Scale);
		Text(FVector2D(L+36.f*Scale,Y0+22.f*Scale),T->Title,GEngine->GetLargeFont(),1.5f*Scale,Gold);
		const int32 Chars=FMath::Clamp(int32((GlyphLeft-L-50.f*Scale)/(8.2f*Scale)),30,70);
		const TArray<FString> Lines=Wrap(T->Body,Chars);
		for(int32 I=0;I<Lines.Num() && I<3;++I)
			Text(FVector2D(L+36.f*Scale,Y0+78.f*Scale+I*28.f*Scale),Lines[I],GEngine->GetMediumFont(),1.05f*Scale,FLinearColor(.95f,.95f,.97f));
		// Progress pips.
		const float PipY=Y0+H-40.f*Scale;
		for(int32 I=0;I<T->Goal;++I)
		{
			const float X=L+36.f*Scale+I*34.f*Scale;
			DrawRect(FLinearColor(0.f,0.f,0.f,.8f),X-2.f*Scale,PipY-2.f*Scale,26.f*Scale,26.f*Scale);
			DrawRect(I<T->Progress?Gold:FLinearColor(.25f,.26f,.32f),X,PipY,22.f*Scale,22.f*Scale);
		}
		if(T->Goal>0 && T->Progress>=T->Goal) Text(FVector2D(L+60.f*Scale+T->Goal*34.f*Scale,PipY-10.f*Scale),TEXT("NICE!"),GEngine->GetLargeFont(),1.2f*Scale,Gold);
		else if(Now-T->MissWorld<1.4) Text(FVector2D(L+60.f*Scale+T->Goal*34.f*Scale,PipY-2.f*Scale),TEXT("Wait until the icon reaches the diamond, then press."),GEngine->GetMediumFont(),.95f*Scale,Red);
		// Skip: gamepad holds View; mouse holds ALT to show the cursor and clicks SKIP.
		if(T->bGamepad)
		{
			T->SkipButton=FBox2D(ForceInit);
			const float SX=L+W-36.f*Scale, SY=Y0+H-30.f*Scale;
			TArray<FName> Skip; Skip.Add(FName(TEXT("T_TUT_P_View")));
			const float GL=Glyphs(Skip,SX,SY,34.f*Scale);
			Text(FVector2D(GL-128.f*Scale,SY-10.f*Scale),TEXT("HOLD TO SKIP"),GEngine->GetSmallFont(),1.1f*Scale,FLinearColor(.8f,.8f,.85f));
			if(T->SkipHold>0.f) DrawRect(Gold,GL-128.f*Scale,SY+12.f*Scale,(SX-GL+128.f*Scale)*T->SkipHold,4.f*Scale);
		}
		else
		{
			const float BW=132.f*Scale, BH=40.f*Scale, BX=L+W-BW-30.f*Scale, BY=Y0+H-BH-18.f*Scale;
			T->SkipButton=FBox2D(FVector2D(BX,BY),FVector2D(BX+BW,BY+BH));
			const FLinearColor Face=T->bSkipHover?Gold:(T->bCursorMode?FLinearColor(.32f,.30f,.26f,.95f):FLinearColor(.10f,.10f,.14f,.9f));
			DrawRect(FLinearColor(0.f,0.f,0.f,.9f),BX-2.f*Scale,BY-2.f*Scale,BW+4.f*Scale,BH+4.f*Scale);
			DrawRect(Face,BX,BY,BW,BH);
			DrawRect(Gold,BX,BY+BH-3.f*Scale,BW,3.f*Scale);
			Text(FVector2D(BX+BW*.5f,BY+7.f*Scale),TEXT("SKIP  >>"),GEngine->GetMediumFont(),1.15f*Scale,
				T->bSkipHover?FLinearColor(.05f,.05f,.07f):FLinearColor::White,true);
			if(!T->bCursorMode)
				Text(FVector2D(BX-12.f*Scale-150.f*Scale,BY+11.f*Scale),TEXT("hold ALT + click"),GEngine->GetSmallFont(),1.1f*Scale,FLinearColor(.75f,.75f,.8f));
		}
		return;
	}
	T->SkipButton=FBox2D(ForceInit);
	// One-time tip during the real fight.
	const float CAge=float(Now-T->CalloutWorld);
	if(CAge>=0.f && CAge<4.f && !T->Callout.IsEmpty())
	{
		const float A=CAge<.2f?CAge/.2f:CAge>3.5f?(4.f-CAge)/.5f:1.f;
		const float W=620.f*Scale, H=86.f*Scale, L=Canvas->SizeX*.5f-W*.5f, Top=Canvas->SizeY*.08f;
		if(!Tile(Tex(TEXT("T_TUT_Card")),FVector2D(L,Top),FVector2D(W,H),FLinearColor(1.f,1.f,1.f,A)))
			DrawRect(FLinearColor(.03f,.03f,.05f,.85f*A),L,Top,W,H);
		Glyphs(T->CalloutGlyphs,L+W-24.f*Scale,Top+H*.5f,54.f*Scale);
		Text(FVector2D(L+28.f*Scale,Top+H*.5f-14.f*Scale),T->Callout,GEngine->GetMediumFont(),1.2f*Scale,FLinearColor(Gold.R,Gold.G,Gold.B,A));
	}
}
