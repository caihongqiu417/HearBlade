// 根据音乐重音配置选择攻击强调拍点
#include "Combat/Boss/BeatSyncedComboComponent.h"
#include "Combat/Data/MusicEmphasisData.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "Engine/World.h"

int32 UBeatSyncedComboComponent::SelectMusicEmphasisBeat(const UAttackDataAsset* Data,
                                                         float& OutPriority) const
{
	OutPriority = 0.f;
	const auto* Clock = GetWorld() ? GetWorld()->GetSubsystem<UBeatClockSubsystem>() : nullptr;
	const auto* Emphasis = MusicEmphasisData.Get();
	if (!Data || Data->ImpactMarkers.IsEmpty() || !Clock || !Clock->IsRunning() || !Emphasis ||
	    !Emphasis->bEnabled ||
	    Emphasis->Music.ToSoftObjectPath() != FSoftObjectPath(Clock->GetPlayingMusicSound()))
		return INDEX_NONE;
	const double FirstDelay = Data->GetElapsedAtAnimationTime(Data->ImpactMarkers[0].AnimationTimeSeconds) -
	                          Data->GetElapsedAtAnimationTime(Data->ClipStartSeconds);
	// 用连招末击寻找音乐重音，首击时间需要扣除中间各击的播放时长
	const double TailGap = Data->GetElapsedAtAnimationTime(Data->ImpactMarkers.Last().AnimationTimeSeconds) -
	                       Data->GetElapsedAtAnimationTime(Data->ImpactMarkers[0].AnimationTimeSeconds);
	const double Lead = FMath::Max<double>(
	    SchedulingLeadSeconds,
	    bBlinkRequested
	        ? FMath::Max(BlinkSettings.WarningBeats, StrongBeatChargeBeats) * Clock->GetSecondsPerBeat() + .05
	        : 0.);
	int32 First = FMath::Max(
	    0, FMath::CeilToInt(Clock->GetCurrentBeat() + (FirstDelay + Lead) / Clock->GetSecondsPerBeat()));
	const int32 Bar = FMath::Max(1, Clock->GetBeatsPerBar());
	// 指定小节内拍位时按整小节搜索，否则逐拍搜索
	const int32 Step = Data->FirstImpactBeatInBar > 0 ? Bar : 1;
	if (Step > 1)
		First += (FMath::Clamp(Data->FirstImpactBeatInBar, 1, Bar) - 1 - First % Bar + Bar) % Bar;
	int32 Best = INDEX_NONE;
	for (int32 Beat = First; Beat <= First + FMath::Clamp(Emphasis->MaxExtraWaitBeats, 0.f, 8.f);
	     Beat += Step)
	{
		const double FinalSource =
		    Clock->AudioTimeToMusicSourceSeconds(Clock->GetBeatTimeSeconds(Beat) + TailGap);
		// 重音越强越优先，等待越久分数越低
		const float Priority = Emphasis->ScoreAt(FinalSource) - .035f * (Beat - First);
		if (Priority > OutPriority)
		{
			OutPriority = Priority;
			Best = Beat;
		}
	}
	return Best;
}
