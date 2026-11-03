// 根据主角攻击结果更新连击、共鸣和成功反馈
#include "Combat/Player/PlayerDefenseComponent.h"

// 根据玩家攻击评级增加共鸣，并记录本次收益
void UPlayerDefenseComponent::AwardPlayerAttack(bool bPerfect, float BaseGain)
{
	if (Health <= 0.f || !GetWorld())
		return;
	// 保存奖励前的连击数，反馈同时携带变化前后的值
	const int32 Previous = ComboCount++;
	LastJudgment = bPerfect ? ERhythmDodgeJudgment::Perfect : ERhythmDodgeJudgment::Good;
	LastJudgmentWorldSeconds = GetWorld()->GetTimeSeconds();
	LastResonanceGainWorldSeconds = LastJudgmentWorldSeconds;
	// 连击达到 4 次和 8 次时提高共鸣收益，最终限制在合法范围
	Resonance = FMath::Clamp(Resonance + BaseGain * (ComboCount >= 8   ? 1.5f
	                                                 : ComboCount >= 4 ? 1.25f
	                                                                   : 1.f),
	                         0.f, MaxResonance);
	FRhythmSuccessFeedbackData Feedback;
	Feedback.bPlayerAttack = true;
	Feedback.PreviousCombo = Previous;
	Feedback.ComboCount = ComboCount;
	Feedback.FeedbackTier = GetFeedbackTierForCombo(ComboCount);
	Feedback.bSuccess = true;
	Feedback.bPerfect = bPerfect;
	Feedback.EventId = ++FeedbackEventSerial;
	Feedback.WorldLocation = GetOwner()->GetActorLocation();
	Feedback.AttackDirection = GetOwner()->GetActorForwardVector();
	// 攻击奖励共用成功事件，接收方通过 bPlayerAttack 区分攻击和防御
	Feedback.Response = ERhythmResponseType::Counter;
	OnRhythmSuccessFeedback.Broadcast(Feedback);
}
