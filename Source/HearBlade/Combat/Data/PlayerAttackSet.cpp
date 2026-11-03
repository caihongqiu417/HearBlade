#include "Combat/Data/PlayerAttackSet.h"
#include "Animation/AnimSequence.h"

// 攻击节点查询与校验

// 按标识查找连招节点，不存在时返回空指针
const FPlayerAttackNode* UPlayerAttackSet::Find(FName Id) const
{
	return Nodes.FindByPredicate(
	    [Id](const auto& Node)
	    {
		    return Node.Id == Id;
	    });
}

// 检查节点标识、动作时间范围和轻重分支是否有效
bool UPlayerAttackSet::Validate(FString& Error) const
{
	TSet<FName> SeenNodeIds;
	const auto ReportInvalidNode = [&Error](FName NodeId)
	{
		Error = FString::Printf(TEXT("Invalid player attack node: %s"), *NodeId.ToString());
		return false;
	};

	for (const FPlayerAttackNode& Node : Nodes)
	{
		// 检查节点名称和动画是否有效
		if (Node.Id.IsNone() || SeenNodeIds.Contains(Node.Id) || !Node.Animation)
		{
			return ReportInvalidNode(Node.Id);
		}

		// 命中点必须位于片段内部，片段结束不能超过动画长度
		if (Node.ClipStart < 0.f || Node.ImpactTime <= Node.ClipStart || Node.ClipEnd <= Node.ImpactTime ||
		    Node.ClipEnd > Node.Animation->GetPlayLength() + .001f)
		{
			return ReportInvalidNode(Node.Id);
		}

		// 挥击在命中前开始，下一招的衔接点在命中后、片段结束前
		if (Node.StrikeStart <= Node.ClipStart || Node.StrikeStart >= Node.ImpactTime ||
		    Node.LinkTime <= Node.ImpactTime || Node.LinkTime > Node.ClipEnd)
		{
			return ReportInvalidNode(Node.Id);
		}

		if (Node.StrikeSeconds <= 0.f || Node.DurationBeats <= 0.f)
		{
			return ReportInvalidNode(Node.Id);
		}

		if (Node.bUppercut && Node.Input != EPlayerAttackInput::Heavy)
		{
			return ReportInvalidNode(Node.Id);
		}

		// 配置了下一招时，检查对应节点是否存在
		if (!Node.NextLight.IsNone() && !Find(Node.NextLight))
		{
			return ReportInvalidNode(Node.Id);
		}
		if (!Node.NextHeavy.IsNone() && !Find(Node.NextHeavy))
		{
			return ReportInvalidNode(Node.Id);
		}

		SeenNodeIds.Add(Node.Id);
	}

	// 起手使用固定的轻重击根节点，分支都有效也不能缺少这两个入口
	if (!Find(TEXT("L")) || !Find(TEXT("H")))
	{
		Error = TEXT("Missing L/H roots");
		return false;
	}
	return true;
}

// 输入时机评级

// 根据输入与目标时刻的偏差，得到完美、良好或失误评价
EPlayerAttackGrade PlayerAttackTiming::Judge(double ErrorSeconds)
{
	return FMath::Abs(ErrorSeconds) <= .060 + 1.e-7   ? EPlayerAttackGrade::Perfect
	       : FMath::Abs(ErrorSeconds) <= .120 + 1.e-7 ? EPlayerAttackGrade::Good
	                                                  : EPlayerAttackGrade::Bad;
}

// 返回连续攻击之间的基础拍数间隔
double PlayerAttackTiming::CadenceBeats(EPlayerAttackInput Input, bool Resonance)
{
	// 更密集的连击由专门的动作配置支持，不在这里统一加速
	return 1.;
}

// 以命中时刻为基准，将音乐时间换算为动画位置
float PlayerAttackTiming::ClipPosition(double AudioNow, double ImpactAudio, float ClipStart, float ImpactTime,
                                       float ClipEnd, float PlayRate)
{
	// 命中时刻由实际起始时间和配置的接触帧确定
	return FMath::Clamp(ImpactTime + float(AudioNow - ImpactAudio) * PlayRate, ClipStart, ClipEnd);
}

// 音乐时间到动画位置

// 分别调整准备段和收招段的进度，使命中点附近的播放速度连续
float PlayerAttackTiming::ContinuousComboPosition(double AudioNow, double StartAudio, double ImpactAudio,
                                                  double EndAudio, const FPlayerAttackNode& Node)
{
	// 两段曲线在命中点使用相同斜率，避免准备段切换到收招段时突然变速
	// 确保开始、命中和结束之间有有效间隔，才能计算各段播放速度
	const double StartMusicTime = StartAudio,
	             ImpactMusicTime = FMath::Max(ImpactAudio, StartMusicTime + .001),
	             EndMusicTime = FMath::Max(EndAudio, ImpactMusicTime + .001);
	// 动画位置以片段自身的秒数表示，与音乐时间分别计算
	const double StartAnimationTime = Node.ClipStart, ImpactAnimationTime = Node.ImpactTime,
	             EndAnimationTime = Node.ClipEnd;
	// 分段平均播放速度为动画时长与音乐时长之比
	const double WindupPlayRate =
	                 (ImpactAnimationTime - StartAnimationTime) / (ImpactMusicTime - StartMusicTime),
	             RecoveryPlayRate =
	                 (EndAnimationTime - ImpactAnimationTime) / (EndMusicTime - ImpactMusicTime);
	// 命中点使用两段播放速度的调和平均值，保持连接处速度连续
	const double ImpactPlayRate =
	    WindupPlayRate > 0. && RecoveryPlayRate > 0.
	        ? 2. * WindupPlayRate * RecoveryPlayRate / (WindupPlayRate + RecoveryPlayRate)
	        : 0.;
	// 三次插值同时约束端点位置与播放速度；段内进度限制为 0 到 1
	auto InterpolateAnimationTime = [](double StartPosition, double EndPosition, double StartPlayRate,
	                                   double EndPlayRate, double SegmentDuration, double SegmentProgress)
	{
		const double ProgressSquared = SegmentProgress * SegmentProgress,
		             ProgressCubed = ProgressSquared * SegmentProgress;
		return (2. * ProgressCubed - 3. * ProgressSquared + 1.) * StartPosition +
		       (ProgressCubed - 2. * ProgressSquared + SegmentProgress) * SegmentDuration * StartPlayRate +
		       (-2. * ProgressCubed + 3. * ProgressSquared) * EndPosition +
		       (ProgressCubed - ProgressSquared) * SegmentDuration * EndPlayRate;
	};
	if (AudioNow <= ImpactMusicTime)
		return float(InterpolateAnimationTime(
		    StartAnimationTime, ImpactAnimationTime, WindupPlayRate, ImpactPlayRate,
		    ImpactMusicTime - StartMusicTime,
		    FMath::Clamp((AudioNow - StartMusicTime) / (ImpactMusicTime - StartMusicTime), 0., 1.)));
	return float(InterpolateAnimationTime(
	    ImpactAnimationTime, EndAnimationTime, ImpactPlayRate, RecoveryPlayRate,
	    EndMusicTime - ImpactMusicTime,
	    FMath::Clamp((AudioNow - ImpactMusicTime) / (EndMusicTime - ImpactMusicTime), 0., 1.)));
}

// 保证新攻击不会早于上一击要求的最小间隔
double PlayerAttackTiming::ScheduleImpact(double CandidateBeat, double PreviousImpactBeat, double Cadence)
{
	return FMath::Max(CandidateBeat, PreviousImpactBeat + Cadence);
}

// 接招与防御窗口

// 只在允许的时间窗口内接收一次连招输入
bool PlayerAttackTiming::CanQueue(double InputAudio, double ImpactAudio, bool AlreadyQueued)
{
	return !AlreadyQueued && FMath::IsFinite(InputAudio) && FMath::IsFinite(ImpactAudio) &&
	       InputAudio >= ImpactAudio - ComboQueueEarlySeconds - 1.e-7 &&
	       InputAudio <= ImpactAudio + ComboQueueLateSeconds + 1.e-7;
}

// 检查当前时间是否落在允许接收防御输入的范围
bool PlayerAttackTiming::IsDefenseInputWindow(double SecondsUntilImpact, double IdealLeadSeconds)
{
	return FMath::IsFinite(SecondsUntilImpact) && FMath::IsFinite(IdealLeadSeconds) &&
	       SecondsUntilImpact >= -.12 - 1.e-7 &&
	       SecondsUntilImpact <= FMath::Max(.30, IdealLeadSeconds + .12) + 1.e-7;
}

// 结合准备时长寻找下一目标拍点，轻攻击允许使用半拍
double PlayerAttackTiming::NextImpactBeat(double CurrentBeat, double SecondsPerBeat, float Windup, bool Heavy)
{
	// Grid 的单位为拍：重攻击对齐整拍，轻攻击对齐半拍
	const double Grid = Heavy ? 1. : .5;
	// 准备时长先换算为拍数，再向上取最近网格；微小容差避免浮点误差多推进一格
	return FMath::CeilToDouble(
	           (CurrentBeat + FMath::Max(.06, double(Windup) / 1.25) / SecondsPerBeat - 1.e-7) / Grid) *
	       Grid;
}
