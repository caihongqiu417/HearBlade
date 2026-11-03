// 保存攻击片段和命中标记，并在动画时间与实际播放时间之间转换
// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/Data/AttackData.h"

float UAttackDataAsset::GetClipEndSeconds() const
{
	if (!AttackMontage)
		return 0.0f;
	const float Length = AttackMontage->GetPlayLength();
	return ClipEndSeconds > ClipStartSeconds ? FMath::Min(ClipEndSeconds, Length) : Length;
}

// 按命中标记定位速度段，缺少分段值时使用默认速度，并限制最小速度
float UAttackDataAsset::GetRateAtAnimationTime(float AnimationTime) const
{
	int32 Segment = 0;
	for (const FAttackImpactMarker& Marker : ImpactMarkers)
	{
		if (AnimationTime < Marker.AnimationTimeSeconds)
			break;
		++Segment;
	}
	return FMath::Max(0.01f, SegmentPlayRates.IsValidIndex(Segment) ? SegmentPlayRates[Segment] : PlayRate);
}

float UAttackDataAsset::GetElapsedAtAnimationTime(float AnimationTime) const
{
	return GetMotionElapsedAtAnimationTime(AnimationTime) +
	       (ChargeHoldSeconds > 0.f && AnimationTime > ChargePoseAnimationSeconds ? ChargeHoldSeconds : 0.f);
}

// 从播放经过时间反求动画位置；蓄力停顿期间保持在指定姿势
float UAttackDataAsset::GetAnimationTimeAtElapsed(float ElapsedSeconds) const
{
	if (ChargeHoldSeconds > 0.f)
	{
		const float HoldStart = GetMotionElapsedAtAnimationTime(ChargePoseAnimationSeconds);
		if (ElapsedSeconds >= HoldStart && ElapsedSeconds < HoldStart + ChargeHoldSeconds)
			return ChargePoseAnimationSeconds;
		if (ElapsedSeconds >= HoldStart + ChargeHoldSeconds)
			ElapsedSeconds -= ChargeHoldSeconds;
	}
	return GetMotionAnimationTimeAtElapsed(ElapsedSeconds);
}

// 逐段累加动画时长除以播放速度的结果，不包含额外蓄力停顿
float UAttackDataAsset::GetMotionElapsedAtAnimationTime(float AnimationTime) const
{
	float Elapsed = 0.0f;
	float Previous = 0.0f;
	int32 Segment = 0;
	for (const FAttackImpactMarker& Marker : ImpactMarkers)
	{
		const float End = FMath::Min(AnimationTime, Marker.AnimationTimeSeconds);
		if (End > Previous)
		{
			const float Rate = SegmentPlayRates.IsValidIndex(Segment) ? SegmentPlayRates[Segment] : PlayRate;
			Elapsed += (End - Previous) / FMath::Max(0.01f, Rate);
		}
		if (AnimationTime <= Marker.AnimationTimeSeconds)
			return Elapsed;
		Previous = Marker.AnimationTimeSeconds;
		++Segment;
	}
	const float Rate = SegmentPlayRates.IsValidIndex(Segment) ? SegmentPlayRates[Segment] : PlayRate;
	return Elapsed + FMath::Max(0.0f, AnimationTime - Previous) / FMath::Max(0.01f, Rate);
}

// 逐段扣除播放时长，找到目标段后换算为动画位置
float UAttackDataAsset::GetMotionAnimationTimeAtElapsed(float ElapsedSeconds) const
{
	float Remaining = FMath::Max(0.0f, ElapsedSeconds);
	float Previous = 0.0f;
	int32 Segment = 0;
	for (const FAttackImpactMarker& Marker : ImpactMarkers)
	{
		const float Rate =
		    FMath::Max(0.01f, SegmentPlayRates.IsValidIndex(Segment) ? SegmentPlayRates[Segment] : PlayRate);
		const float Duration = (Marker.AnimationTimeSeconds - Previous) / Rate;
		if (Remaining < Duration)
			return Previous + Remaining * Rate;
		Remaining -= Duration;
		Previous = Marker.AnimationTimeSeconds;
		++Segment;
	}
	const float Rate =
	    FMath::Max(0.01f, SegmentPlayRates.IsValidIndex(Segment) ? SegmentPlayRates[Segment] : PlayRate);
	return Previous + Remaining * Rate;
}
