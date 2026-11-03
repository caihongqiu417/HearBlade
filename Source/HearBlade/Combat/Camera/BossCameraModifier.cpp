// 根据反馈强度调整镜头位置、旋转与视野
#include "Combat/Camera/BossCameraModifier.h"
#include "Camera/CameraTypes.h"

float UBossCameraModifier::GetEnvelope() const
{
	// 平滑进入和退出镜头偏移，避免突然跳变
	if (Elapsed < Push)
		return FMath::SmoothStep(0.f, 1.f, Elapsed / FMath::Max(Push, .001f));
	if (Elapsed < Push + Hold)
		return 1.f;
	return 1.f - FMath::SmoothStep(0.f, 1.f, (Elapsed - Push - Hold) / FMath::Max(Recover, .001f));
}

float UBossCameraModifier::GetShakeEnvelope() const
{
	return FMath::Clamp(1.f - Elapsed / FMath::Max(ShakeDuration, .001f), 0.f, 1.f);
}

float UBossCameraModifier::GetReboundEnvelope() const
{
	if (Elapsed < Push + Hold)
		return GetEnvelope();
	const float T = FMath::Clamp((Elapsed - Push - Hold) / FMath::Max(Recover, .001f), 0.f, 1.f);
	// 先向反方向轻微回弹，再恢复原视角
	return (1.f - T) - FMath::Sin(T * UE_PI) * ReboundStrength;
}

bool UBossCameraModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Elapsed += DeltaTime;
	HoldImpactElapsed += DeltaTime;
	HoldShake = FMath::FInterpTo(HoldShake, HoldShakeTarget, DeltaTime, HoldShakeInterpSpeed);
	if (ShotWeight > 0.f)
	{
		// 先混合特写镜头，再叠加命中反馈
		const float W = FMath::Clamp(ShotWeight, 0.f, 1.f);
		InOutPOV.Location = FMath::Lerp(InOutPOV.Location, ShotLocation, W);
		InOutPOV.Rotation =
		    FQuat::Slerp(InOutPOV.Rotation.Quaternion(), ShotRotation.Quaternion(), W).Rotator();
		InOutPOV.FOV = FMath::Lerp(InOutPOV.FOV, ShotFOV, W);
	}
	InOutPOV.FOV = FMath::Max(55.f, InOutPOV.FOV - Kick * GetEnvelope());
	const float HoldImpactEnvelope =
	    FMath::Clamp(1.f - HoldImpactElapsed / FMath::Max(HoldImpactDuration, .001f), 0.f, 1.f);
	const float Amplitude = Shake * GetShakeEnvelope() + HoldShake + HoldImpact * HoldImpactEnvelope;
	InOutPOV.Rotation.Pitch += Amplitude * FMath::Sin(Elapsed * 75.f);
	InOutPOV.Rotation.Yaw += Amplitude * .7f * FMath::Sin(Elapsed * 97.f);
	const float CinematicEnvelope = GetReboundEnvelope();
	InOutPOV.Rotation.Yaw += YawKick * SideSign * CinematicEnvelope;
	InOutPOV.Rotation.Pitch += PitchKick * CinematicEnvelope;
	InOutPOV.Rotation.Roll += RollKick * SideSign * CinematicEnvelope;
	const FVector Forward = InOutPOV.Rotation.Vector();
	const FVector Right = FRotationMatrix(InOutPOV.Rotation).GetScaledAxis(EAxis::Y);
	InOutPOV.Location += Right * LateralShift * SideSign * CinematicEnvelope;
	InOutPOV.Location += Forward * DollyShift * GetEnvelope();
	return false;
}
