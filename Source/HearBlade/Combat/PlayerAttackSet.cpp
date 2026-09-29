#include "Combat/PlayerAttackSet.h"
#include "Animation/AnimSequence.h"

const FPlayerAttackNode* UPlayerAttackSet::Find(FName Id) const
{
	return Nodes.FindByPredicate([Id](const auto& Node) { return Node.Id == Id; });
}

bool UPlayerAttackSet::Validate(FString& Error) const
{
	TSet<FName> Seen;
	for (const auto& Node : Nodes)
	{
		if (Node.Id.IsNone() || Seen.Contains(Node.Id) || !Node.Animation ||
			Node.ClipStart < 0.f || Node.ImpactTime <= Node.ClipStart || Node.ClipEnd <= Node.ImpactTime ||
			Node.StrikeStart <= Node.ClipStart || Node.StrikeStart >= Node.ImpactTime || Node.LinkTime <= Node.ImpactTime || Node.LinkTime > Node.ClipEnd || Node.StrikeSeconds <= 0.f ||
			(Node.bUppercut && Node.Input != EPlayerAttackInput::Heavy) ||
			Node.ClipEnd > Node.Animation->GetPlayLength() + .001f || Node.DurationBeats <= 0.f ||
			(!Node.NextLight.IsNone() && !Find(Node.NextLight)) || (!Node.NextHeavy.IsNone() && !Find(Node.NextHeavy)))
		{ Error = FString::Printf(TEXT("Invalid player attack node: %s"), *Node.Id.ToString()); return false; }
		Seen.Add(Node.Id);
	}
	if (!Find(TEXT("L")) || !Find(TEXT("H"))) { Error=TEXT("Missing L/H roots"); return false; }
	return true;
}

EPlayerAttackGrade PlayerAttackTiming::Judge(double ErrorSeconds)
{
	return FMath::Abs(ErrorSeconds) <= .060 + 1.e-7 ? EPlayerAttackGrade::Perfect :
		FMath::Abs(ErrorSeconds) <= .120 + 1.e-7 ? EPlayerAttackGrade::Good : EPlayerAttackGrade::Bad;
}

double PlayerAttackTiming::CadenceBeats(EPlayerAttackInput Input, bool Resonance)
{
	// Dense subdivisions require an authored route, never a global speed-up.
	return 1.;
}

float PlayerAttackTiming::ClipPosition(double AudioNow,double ImpactAudio,float ClipStart,float ImpactTime,float ClipEnd,float PlayRate)
{
	// ImpactAudio is derived from the actual start and authored contact frame.
	return FMath::Clamp(ImpactTime+float(AudioNow-ImpactAudio)*PlayRate,ClipStart,ClipEnd);
}

float PlayerAttackTiming::ContinuousComboPosition(double AudioNow,double StartAudio,double ImpactAudio,double EndAudio,const FPlayerAttackNode& Node)
{
	// Monotone cubic through (start, contact, end). The windup and the recovery are
	// time-warped by different amounts so contact and the next cue land on time; a
	// piecewise-linear warp made playback speed jump at contact, which read as a
	// stutter. The shared tangent at contact keeps the speed continuous.
	const double T0=StartAudio, T1=FMath::Max(ImpactAudio,T0+.001), T2=FMath::Max(EndAudio,T1+.001);
	const double P0=Node.ClipStart, P1=Node.ImpactTime, P2=Node.ClipEnd;
	const double S1=(P1-P0)/(T1-T0), S2=(P2-P1)/(T2-T1);
	const double M1=S1>0. && S2>0. ? 2.*S1*S2/(S1+S2) : 0.; // Harmonic mean: never overshoots.
	auto Hermite=[](double A,double B,double MA,double MB,double H,double U)
	{
		const double U2=U*U, U3=U2*U;
		return (2.*U3-3.*U2+1.)*A+(U3-2.*U2+U)*H*MA+(-2.*U3+3.*U2)*B+(U3-U2)*H*MB;
	};
	if(AudioNow<=T1)
		return float(Hermite(P0,P1,S1,M1,T1-T0,FMath::Clamp((AudioNow-T0)/(T1-T0),0.,1.)));
	return float(Hermite(P1,P2,M1,S2,T2-T1,FMath::Clamp((AudioNow-T1)/(T2-T1),0.,1.)));
}

double PlayerAttackTiming::ScheduleImpact(double CandidateBeat,double PreviousImpactBeat,double Cadence)
{
	return FMath::Max(CandidateBeat,PreviousImpactBeat+Cadence);
}

bool PlayerAttackTiming::CanQueue(double InputAudio,double ImpactAudio,bool AlreadyQueued)
{
	return !AlreadyQueued && FMath::IsFinite(InputAudio) && FMath::IsFinite(ImpactAudio) &&
		InputAudio >= ImpactAudio-ComboQueueEarlySeconds-1.e-7 &&
		InputAudio <= ImpactAudio+ComboQueueLateSeconds+1.e-7;
}


bool PlayerAttackTiming::IsDefenseInputWindow(double SecondsUntilImpact,double IdealLeadSeconds)
{
	return FMath::IsFinite(SecondsUntilImpact) && FMath::IsFinite(IdealLeadSeconds) &&
		SecondsUntilImpact>=-.12-1.e-7 && SecondsUntilImpact<=FMath::Max(.30,IdealLeadSeconds+.12)+1.e-7;
}

double PlayerAttackTiming::NextImpactBeat(double CurrentBeat, double SecondsPerBeat, float Windup, bool Heavy)
{
	const double Grid = Heavy ? 1. : .5;
	return FMath::CeilToDouble((CurrentBeat + FMath::Max(.06, double(Windup)/1.25)/SecondsPerBeat - 1.e-7)/Grid)*Grid;
}
