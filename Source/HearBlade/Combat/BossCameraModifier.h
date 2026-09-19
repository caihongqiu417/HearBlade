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
	void Trigger(float InKick, float InShake, float InPush, float InHold, float InRecover)
	{
		const float ExistingKick = Kick * GetEnvelope();
		Kick = FMath::Abs(InKick) >= FMath::Abs(ExistingKick) ? InKick : ExistingKick;
		Shake = FMath::Max(Shake * GetShakeEnvelope(), InShake);
		Push = InPush; Hold = InHold; Recover = InRecover; Elapsed = 0.f;
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
	}
	void Clear() { Kick = Shake = Elapsed = LateralShift = DollyShift = YawKick = PitchKick = ReboundStrength = 0.f; }
	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
private:
	float GetEnvelope() const;
	float GetReboundEnvelope() const;
	float GetShakeEnvelope() const;
};
