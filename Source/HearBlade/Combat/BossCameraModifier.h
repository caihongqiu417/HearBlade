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
	void Trigger(float InKick, float InShake, float InPush, float InHold, float InRecover)
	{
		Kick = FMath::Max(Kick * GetEnvelope(), InKick);
		Shake = FMath::Max(Shake * GetShakeEnvelope(), InShake);
		Push = InPush; Hold = InHold; Recover = InRecover; Elapsed = 0.f;
	}
	void Clear() { Kick = Shake = Elapsed = 0.f; }
	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
private:
	float GetEnvelope() const;
	float GetShakeEnvelope() const;
};
