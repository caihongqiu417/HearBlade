// Fill out your copyright notice in the Description page of Project Settings.


#include "Combat/AttackData.h"

float UAttackDataAsset::GetClipEndSeconds() const
{
	if (!AttackMontage) return 0.0f;
	const float Length = AttackMontage->GetPlayLength();
	return ClipEndSeconds > ClipStartSeconds ? FMath::Min(ClipEndSeconds, Length) : Length;
}

float UAttackDataAsset::GetRateAtAnimationTime(float AnimationTime) const
{
	int32 Segment = 0;
	for (const FAttackImpactMarker& Marker : ImpactMarkers)
	{
		if (AnimationTime < Marker.AnimationTimeSeconds) break;
		++Segment;
	}
	return FMath::Max(0.01f, SegmentPlayRates.IsValidIndex(Segment) ? SegmentPlayRates[Segment] : PlayRate);
}

float UAttackDataAsset::GetElapsedAtAnimationTime(float AnimationTime) const
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
		if (AnimationTime <= Marker.AnimationTimeSeconds) return Elapsed;
		Previous = Marker.AnimationTimeSeconds;
		++Segment;
	}
	const float Rate = SegmentPlayRates.IsValidIndex(Segment) ? SegmentPlayRates[Segment] : PlayRate;
	return Elapsed + FMath::Max(0.0f, AnimationTime - Previous) / FMath::Max(0.01f, Rate);
}

float UAttackDataAsset::GetAnimationTimeAtElapsed(float ElapsedSeconds) const
{
	float Remaining = FMath::Max(0.0f, ElapsedSeconds);
	float Previous = 0.0f;
	int32 Segment = 0;
	for (const FAttackImpactMarker& Marker : ImpactMarkers)
	{
		const float Rate = FMath::Max(0.01f, SegmentPlayRates.IsValidIndex(Segment) ? SegmentPlayRates[Segment] : PlayRate);
		const float Duration = (Marker.AnimationTimeSeconds - Previous) / Rate;
		if (Remaining < Duration) return Previous + Remaining * Rate;
		Remaining -= Duration;
		Previous = Marker.AnimationTimeSeconds;
		++Segment;
	}
	const float Rate = FMath::Max(0.01f, SegmentPlayRates.IsValidIndex(Segment) ? SegmentPlayRates[Segment] : PlayRate);
	return Previous + Remaining * Rate;
}

