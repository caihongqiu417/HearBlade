#include "Combat/PlayerRhythmCombatComponent.h"

void UPlayerRhythmCombatComponent::AwardPlayerAttack(bool bPerfect,float BaseGain)
{
	if(Health<=0.f || !GetWorld()) return;
	const int32 Previous=ComboCount++;
	LastJudgment=bPerfect?ERhythmDodgeJudgment::Perfect:ERhythmDodgeJudgment::Good;
	LastJudgmentWorldSeconds=GetWorld()->GetTimeSeconds();
	LastResonanceGainWorldSeconds=LastJudgmentWorldSeconds;
	Resonance=FMath::Clamp(Resonance+BaseGain*(ComboCount>=8?1.5f:ComboCount>=4?1.25f:1.f),0.f,MaxResonance);
	FRhythmSuccessFeedbackData Feedback;
	Feedback.bPlayerAttack=true;
	Feedback.PreviousCombo=Previous;Feedback.ComboCount=ComboCount;Feedback.FeedbackTier=GetFeedbackTierForCombo(ComboCount);
	Feedback.bSuccess=true;Feedback.bPerfect=bPerfect;Feedback.EventId=++FeedbackEventSerial;
	Feedback.WorldLocation=GetOwner()->GetActorLocation();Feedback.AttackDirection=GetOwner()->GetActorForwardVector();
	// Reuse the music's success stream, with no defensive response cinematics.
	Feedback.Response=ERhythmResponseType::Counter;
	OnRhythmSuccessFeedback.Broadcast(Feedback);
}
