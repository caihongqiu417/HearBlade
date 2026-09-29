#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/AttackData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearBladeAttackTimingTest,"HearBlade.Combat.AuthoredAttackTiming",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FHearBladeAttackTimingTest::RunTest(const FString& Parameters)
{
	for(const TCHAR* Name:{TEXT("Combo01"),TEXT("Combo02"),TEXT("Combo03")})
	{
		const FString Path=FString::Printf(TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_DragonKnight_%s_Beat"),Name);
		const auto* Data=LoadObject<UAttackDataAsset>(nullptr,*Path);
		if(!TestNotNull(Path,Data)) continue;
		// Sample through transitions and recovery, including either side of the hold.
		for(float Time=Data->ClipStartSeconds;Time<Data->GetClipEndSeconds();Time+=.013f)
		{
			const float Restored=Data->GetAnimationTimeAtElapsed(Data->GetElapsedAtAnimationTime(Time));
			if(!TestTrue(FString::Printf(TEXT("%s timeline round trip at %.3f"),Name,Time),FMath::Abs(Restored-Time)<.0001f)) break;
		}
		if(Data->ChargeHoldSeconds>0.f)
		{
			const float HoldStart=Data->GetElapsedAtAnimationTime(Data->ChargePoseAnimationSeconds);
			TestEqual(TEXT("Hold preserves the authored pose"),Data->GetAnimationTimeAtElapsed(HoldStart+Data->ChargeHoldSeconds*.5f),Data->ChargePoseAnimationSeconds);
		}
		for(int32 I=1;I<Data->ImpactMarkers.Num();++I)
		{
			const auto& Hit=Data->ImpactMarkers[I];const auto& Prev=Data->ImpactMarkers[I-1];
			if(Hit.DamageShape!=EBossAttackDamageShape::GroundBurst) continue;
			const float Gap=Data->GetElapsedAtAnimationTime(Hit.AnimationTimeSeconds)-Data->GetElapsedAtAnimationTime(Prev.AnimationTimeSeconds);
			const double TargetGap=(Hit.BeatOffset-Prev.BeatOffset)*60./Data->ReferenceBPM;
			TestTrue(TEXT("Area landing matches beat after charge hold"),FMath::Abs(Gap-TargetGap)<.001);
			TestTrue(TEXT("Area warning meets configured minimum"),Gap+.001>=Hit.GroundBurst.WarningBeats*60./Data->ReferenceBPM);
		}
	}
	return !HasAnyErrors();
}
#endif
