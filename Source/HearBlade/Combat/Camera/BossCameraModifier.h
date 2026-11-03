// 根据反馈强度调整镜头位置、旋转与视野
#pragma once

#include "Camera/CameraModifier.h"
#include "BossCameraModifier.generated.h"

UCLASS()

class HEARBLADE_API UBossCameraModifier : public UCameraModifier
{
	GENERATED_BODY()
  public:
	float Kick = 0.f;
	float Shake = 0.f;
	float Elapsed = 0.f;
	float Push = 0.08f;
	float Hold = 0.05f;
	float Recover = 0.3f;
	float ShakeDuration = 0.12f;
	float LateralShift = 0.f;
	float DollyShift = 0.f;
	float YawKick = 0.f;
	float PitchKick = 0.f;
	float SideSign = 1.f;
	float ReboundStrength = 0.f;
	float RollKick = 0.f; // Dutch-angle accent (kick finisher only).
	float HoldShake = 0.f;
	float HoldShakeTarget = 0.f;
	float HoldShakeInterpSpeed = 40.f;
	float HoldImpact = 0.f;
	float HoldImpactElapsed = 1.f;
	float HoldImpactDuration = .06f;
	// Cinematic shot override, blended by ShotWeight (driven by the combat camera
	// component every frame): the launcher's side close-up.
	float ShotWeight = 0.f;
	FVector ShotLocation = FVector::ZeroVector;
	FRotator ShotRotation = FRotator::ZeroRotator;
	float ShotFOV = 60.f;

	void StartHoldShake(float Degrees)
	{
		HoldShakeTarget = FMath::Clamp(Degrees, 0.f, .35f);
		HoldShakeInterpSpeed = 20.f;
	}

	void StopHoldShake(float FadeSeconds)
	{
		HoldShakeTarget = 0.f;
		HoldShakeInterpSpeed = 1.f / FMath::Max(FadeSeconds, .001f);
	}

	void TriggerHoldImpact(float Degrees, float Duration)
	{
		HoldImpact = FMath::Clamp(Degrees, 0.f, .35f);
		HoldImpactDuration = FMath::Max(Duration, .01f);
		HoldImpactElapsed = 0.f;
	}

	void Trigger(float InKick, float InShake, float InPush, float InHold, float InRecover)
	{
		const float ExistingKick = Kick * GetEnvelope();
		Kick = FMath::Abs(InKick) >= FMath::Abs(ExistingKick) ? InKick : ExistingKick;
		Shake = FMath::Max(Shake * GetShakeEnvelope(), InShake);
		Push = InPush;
		Hold = InHold;
		Recover = InRecover;
		Elapsed = 0.f;
	}

	void TriggerPerfectDodge(float InKick, float InShake, float InLateralShift, float InDollyShift,
	                         float InYawKick, float InPitchKick, float InSideSign, float InReboundStrength,
	                         float InPush, float InHold, float InRecover)
	{
		Trigger(InKick, InShake, InPush, InHold, InRecover);
		LateralShift = InLateralShift;
		DollyShift = InDollyShift;
		YawKick = InYawKick;
		PitchKick = InPitchKick;
		SideSign = InSideSign >= 0.f ? 1.f : -1.f;
		ReboundStrength = InReboundStrength;
		RollKick = 0.f;
	}

	void Clear()
	{
		Kick = Shake = Elapsed = LateralShift = DollyShift = YawKick = PitchKick = ReboundStrength =
		    RollKick = HoldShake = HoldShakeTarget = HoldImpact = ShotWeight = 0.f;
		HoldImpactElapsed = 1.f;
	}

	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;

  private:
	float GetEnvelope() const;
	float GetReboundEnvelope() const;
	float GetShakeEnvelope() const;
};
