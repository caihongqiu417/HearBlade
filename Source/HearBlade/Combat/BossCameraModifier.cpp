#include "Combat/BossCameraModifier.h"
#include "Camera/CameraTypes.h"

float UBossCameraModifier::GetEnvelope() const
{
	if (Elapsed < Push) return FMath::Clamp(Elapsed / FMath::Max(Push, .001f), 0.f, 1.f);
	if (Elapsed < Push + Hold) return 1.f;
	return FMath::Clamp(1.f - (Elapsed - Push - Hold) / FMath::Max(Recover, .001f), 0.f, 1.f);
}

float UBossCameraModifier::GetShakeEnvelope() const
{
	return FMath::Clamp(1.f - Elapsed / FMath::Max(ShakeDuration, .001f), 0.f, 1.f);
}

float UBossCameraModifier::GetReboundEnvelope() const
{
	if (Elapsed < Push + Hold) return GetEnvelope();
	const float T = FMath::Clamp((Elapsed - Push - Hold) / FMath::Max(Recover, .001f), 0.f, 1.f);
	// Ease back through a tiny opposite offset, then settle. This reads as a deliberate camera beat.
	return (1.f - T) - FMath::Sin(T * UE_PI) * ReboundStrength;
}

bool UBossCameraModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Elapsed += DeltaTime;
	InOutPOV.FOV = FMath::Max(55.f, InOutPOV.FOV - Kick * GetEnvelope());
	const float Amplitude = Shake * GetShakeEnvelope();
	InOutPOV.Rotation.Pitch += Amplitude * FMath::Sin(Elapsed * 75.f);
	InOutPOV.Rotation.Yaw += Amplitude * .7f * FMath::Sin(Elapsed * 97.f);
	const float CinematicEnvelope = GetReboundEnvelope();
	InOutPOV.Rotation.Yaw += YawKick * SideSign * CinematicEnvelope;
	InOutPOV.Rotation.Pitch += PitchKick * CinematicEnvelope;
	const FVector Forward = InOutPOV.Rotation.Vector();
	const FVector Right = FRotationMatrix(InOutPOV.Rotation).GetScaledAxis(EAxis::Y);
	InOutPOV.Location += Right * LateralShift * SideSign * CinematicEnvelope;
	InOutPOV.Location += Forward * DollyShift * GetEnvelope();
	return false;
}
