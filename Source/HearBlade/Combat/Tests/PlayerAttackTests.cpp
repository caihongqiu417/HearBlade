// 验证攻击节奏计算和正式攻击资源，覆盖连招时间与节点配置
// 验证主角攻击时间计算与攻击资产配置
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/Data/PlayerAttackSet.h"
#include "Combat/Data/RhythmCombatInputSettings.h"
#include "Animation/AnimSequence.h"
#include "Sound/SoundBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerAttackTimingTest, "HearBlade.Combat.PlayerAttackTiming",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerAttackTimingTest::RunTest(const FString& Parameters)
{
	// 先检查评级窗口边界，再检查拍点安排、动画位置和连招缓存范围
	TestEqual(TEXT("Perfect inclusive early edge"), PlayerAttackTiming::Judge(-.060),
	          EPlayerAttackGrade::Perfect);
	TestEqual(TEXT("Good outside perfect"), PlayerAttackTiming::Judge(.061), EPlayerAttackGrade::Good);
	TestEqual(TEXT("Good inclusive late edge"), PlayerAttackTiming::Judge(.120), EPlayerAttackGrade::Good);
	TestEqual(TEXT("Off-beat between half beats"), PlayerAttackTiming::Judge(.13), EPlayerAttackGrade::Bad);
	TestFalse(TEXT("Windup remains ordinary attack"), PlayerAttackTiming::IsDefenseInputWindow(.301, .14));
	TestTrue(TEXT("Defense window includes entry"), PlayerAttackTiming::IsDefenseInputWindow(.300, .14));
	TestTrue(TEXT("Defense window includes late edge"), PlayerAttackTiming::IsDefenseInputWindow(-.120, .14));
	TestFalse(TEXT("Expired threat returns to ordinary attack"),
	          PlayerAttackTiming::IsDefenseInputWindow(-.121, .14));
	TestTrue(TEXT("Long lead cue preserves its response window"),
	         PlayerAttackTiming::IsDefenseInputWindow(.4, .3));
	for (double BPM : {80., 104., 140.})
		for (double Phase : {0., .01, .12, .49, .51, .99})
			for (bool Heavy : {false, true})
			{
				const double SPB = 60. / BPM, StartBeat = 8. + Phase;
				const double Beat = PlayerAttackTiming::NextImpactBeat(StartBeat, SPB, .16f, Heavy);
				TestTrue(TEXT("No retroactive attack"), (Beat - StartBeat) * SPB >= .16 / 1.25 - 1.e-6);
				TestTrue(TEXT("Musical grid"),
				         FMath::Abs(Beat * (Heavy ? 1. : 2.) -
				                    FMath::RoundToDouble(Beat * (Heavy ? 1. : 2.))) < 1.e-6);
			}
	const auto* Keys = GetDefault<URhythmCombatInputSettings>();
	TestEqual(TEXT("Normal light cadence is a full beat"),
	          PlayerAttackTiming::CadenceBeats(EPlayerAttackInput::Light, false), 1.);
	TestEqual(TEXT("Normal heavy cadence is one beat"),
	          PlayerAttackTiming::CadenceBeats(EPlayerAttackInput::Heavy, false), 1.);
	for (double Start : {10., 10.10, 10.25, 10.45, 10.65})
	{
		FPlayerAttackNode Node;
		Node.ClipStart = .1f;
		Node.StrikeStart = .2f;
		Node.ImpactTime = .3f;
		Node.ClipEnd = .7f;
		const double Impact = Start + Node.ImpactTime - Node.ClipStart;
		auto Position = [&](double Now)
		{
			return PlayerAttackTiming::ClipPosition(Now, Impact, Node.ClipStart, Node.ImpactTime,
			                                        Node.ClipEnd, 1.f);
		};
		TestTrue(TEXT("First frame advances immediately"),
		         Position(Start + 1. / 60.) > Node.ClipStart + .016f);
		for (double Elapsed = .02; Elapsed < .58; Elapsed += .02)
			TestTrue(TEXT("No slowdown or raised-sword hold anywhere in swing"),
			         FMath::IsNearlyEqual(Position(Start + Elapsed) - Position(Start + Elapsed - .02), .02f,
			                              1.e-6f));
		TestTrue(TEXT("Collision time still matches contact pose"),
		         FMath::IsNearlyEqual(Position(Impact), Node.ImpactTime));
	}
	{
		FPlayerAttackNode Node;
		Node.ClipStart = 1.f;
		Node.ImpactTime = 1.3f;
		Node.ClipEnd = 2.f;
		auto Position = [&](double Now)
		{
			return PlayerAttackTiming::ContinuousComboPosition(Now, 10., 10.2, 10.58, Node);
		};
		TestEqual(TEXT("Continuous slice begins at authored boundary"), Position(10.), Node.ClipStart);
		TestEqual(TEXT("Continuous slice reaches contact"), Position(10.2), Node.ImpactTime);
		TestEqual(TEXT("Continuous slice ends at next boundary"), Position(10.58), Node.ClipEnd);
		TestTrue(TEXT("Continuous slice never reverses before impact"), Position(10.15) > Position(10.1));
		TestTrue(TEXT("Continuous slice never pauses after impact"), Position(10.5) > Position(10.4));
	}
	TestFalse(TEXT("Early spam cannot reserve next combo step"),
	          PlayerAttackTiming::CanQueue(9.699, 10., false));
	// 提前和迟到边界都要覆盖，避免刚好处于边界的输入被错误丢弃
	TestTrue(TEXT("Combo input opens 300 ms before the next cue"),
	         PlayerAttackTiming::CanQueue(9.7, 10., false));
	TestFalse(TEXT("One window holds exactly one command"), PlayerAttackTiming::CanQueue(10., 10., true));
	TestTrue(TEXT("Combo input remains open 180 ms after the cue"),
	         PlayerAttackTiming::CanQueue(10.18, 10., false));
	TestFalse(TEXT("Late input cannot extend old chain"), PlayerAttackTiming::CanQueue(10.181, 10., false));
	for (bool Resonance : {false, true})
	{
		double Previous = -1000.;
		for (int32 I = 0; I < 100; ++I)
		{
			const double Cadence = PlayerAttackTiming::CadenceBeats(
			    I % 2 ? EPlayerAttackInput::Heavy : EPlayerAttackInput::Light, Resonance);
			const double Impact = PlayerAttackTiming::ScheduleImpact(10. + I * .01, Previous, Cadence);
			TestTrue(TEXT("Burst input/cancels cannot compress committed beat spacing"),
			         Impact - Previous >= Cadence - 1.e-7);
			Previous = Impact;
		}
	}
	TestEqual(TEXT("Left mouse ordinary light"), Keys->LightAttackKey, EKeys::LeftMouseButton);
	TestEqual(TEXT("Right mouse ordinary heavy"), Keys->HeavyAttackKey, EKeys::RightMouseButton);
	TestEqual(TEXT("Right mouse contextual block"), Keys->BlockKeyboardMouse, EKeys::RightMouseButton);
	TestEqual(TEXT("Left mouse contextual counter"), Keys->CounterPrimary, EKeys::LeftMouseButton);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerAttackAssetTest, "HearBlade.Combat.PlayerAttackAssets",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerAttackAssetTest::RunTest(const FString& Parameters)
{
	// 加载正式资源检查节点和动画，而不是仅检查临时构造的数据
	const auto* Data = LoadObject<UPlayerAttackSet>(nullptr, TEXT("/Game/Combat/Player/DA_PlayerAttackSet"));
	for (const TCHAR* SoundPath :
	     {TEXT("/Game/Audio/SFX/SW_BossBladeWeak"), TEXT("/Game/Audio/SFX/SW_BossBladeStrong"),
	      TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/SOUND/SW_DragonKnight_SwordSlice")})
		TestNotNull(TEXT("Attack audio resolves"), LoadObject<USoundBase>(nullptr, SoundPath));
	if (!TestNotNull(TEXT("Player attack asset"), Data))
		return false;
	FString Error;
	TestTrue(TEXT("Graph validates: ") + Error, Data->Validate(Error));
	const auto* Player = LoadObject<UAnimSequence>(
	    nullptr, TEXT("/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Anim_DKM_Attack_01"));
	for (const auto& Node : Data->Nodes)
	{
		TestTrue(*Node.Id.ToString(),
		         Node.Animation && Player && Node.Animation->GetSkeleton() == Player->GetSkeleton());
		TestTrue(TEXT("Root locked to avoid visual teleport"),
		         Node.Animation && Node.Animation->bForceRootLock);
		TestTrue(TEXT("Attack root motion is enabled"), Node.Animation && Node.Animation->bEnableRootMotion);
		if (Node.Animation)
		{
			const auto Root = Node.Animation->ExtractRootMotionFromRange(
			    Node.ClipStart, Node.ClipEnd, FAnimExtractContext(Node.ClipEnd, true));
			TestTrue(TEXT("Authored attack contains real capsule travel"),
			         Root.GetTranslation().Size2D() > 1.f);
		}
	}
	const auto* L = Data->Find(TEXT("L"));
	const auto* LL = Data->Find(TEXT("LL"));
	const auto* LLL = Data->Find(TEXT("LLL"));
	TestTrue(TEXT("Light chain uses one continuous combo source"),
	         L && LL && LLL && L->Animation == LL->Animation && LL->Animation == LLL->Animation);
	TestTrue(TEXT("L ends exactly where LL begins"),
	         L && LL && FMath::IsNearlyEqual(L->ClipEnd, LL->ClipStart));
	TestTrue(TEXT("LL ends exactly where LLL begins"),
	         LL && LLL && FMath::IsNearlyEqual(LL->ClipEnd, LLL->ClipStart));
	const auto* H = Data->Find(TEXT("H"));
	const auto* HH = Data->Find(TEXT("HH"));
	TestTrue(TEXT("Heavy chain uses one continuous combo source"), H && HH && H->Animation == HH->Animation);
	TestTrue(TEXT("H ends exactly where HH begins"),
	         H && HH && FMath::IsNearlyEqual(H->ClipEnd, HH->ClipStart));
	for (const TCHAR* Route :
	     {TEXT("LLLL"), TEXT("LH"), TEXT("HHHH"), TEXT("LLLLH"), TEXT("LHLH"), TEXT("HLLH")})
	{
		const FPlayerAttackNode* Node = nullptr;
		for (const TCHAR Ch : FString(Route))
		{
			const FName Id = Node ? (Ch == 'L' ? Node->NextLight : Node->NextHeavy)
			                      : FName(Ch == 'L' ? TEXT("L") : TEXT("H"));
			Node = Data->Find(Id);
			if (!TestNotNull(FString(Route) + TEXT(" path"), Node))
				break;
		}
		if (Node)
			TestTrue(TEXT("Route ends in finisher"), Node->bFinisher);
	}
	return !HasAnyErrors();
}
#endif
