#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PlayerAttackSet.generated.h"

UENUM(BlueprintType)
enum class EPlayerAttackInput : uint8 { Light, Heavy };
UENUM(BlueprintType)
enum class EPlayerAttackGrade : uint8 { Perfect, Good, Bad };
UENUM(BlueprintType)
enum class EPlayerResonanceState : uint8 { Filling, Ready, Armed, Active };

USTRUCT(BlueprintType)
struct FPlayerAttackNode
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EPlayerAttackInput Input = EPlayerAttackInput::Light;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<class UAnimSequence> Animation;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ClipStart = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ImpactTime = .2f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StrikeStart = .1f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float LinkTime = .35f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StrikeSeconds = .12f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ClipEnd = .4f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float DurationBeats = .5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Damage = 16.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float PoiseDamage = 12.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float AdvanceCm = 40.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bFinisher = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bResonanceOnly = false;
	// Contact is made with a leg (calf..ball), not the blade: hit test and presentation differ.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bKick = false;
	// A sword uppercut interrupts and briefly lifts the Boss visual on contact.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bUppercut = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName NextLight;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName NextHeavy;
};

UCLASS(BlueprintType)
class HEARBLADE_API UPlayerAttackSet : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FPlayerAttackNode> Nodes;
	const FPlayerAttackNode* Find(FName Id) const;
	bool Validate(FString& Error) const;
};

namespace PlayerAttackTiming
{
	inline constexpr double ComboQueueEarlySeconds = .30;
	inline constexpr double ComboQueueLateSeconds = .18;
	HEARBLADE_API EPlayerAttackGrade Judge(double ErrorSeconds);
	HEARBLADE_API double CadenceBeats(EPlayerAttackInput Input, bool Resonance);
	HEARBLADE_API double ScheduleImpact(double CandidateBeat, double PreviousImpactBeat, double Cadence);
	HEARBLADE_API bool CanQueue(double InputAudio, double ImpactAudio, bool AlreadyQueued);
	HEARBLADE_API float ClipPosition(double AudioNow, double ImpactAudio, float ClipStart, float ImpactTime, float ClipEnd, float PlayRate);
	HEARBLADE_API float ContinuousComboPosition(double AudioNow, double StartAudio, double ImpactAudio, double EndAudio, const FPlayerAttackNode& Node);
	HEARBLADE_API bool IsDefenseInputWindow(double SecondsUntilImpact, double IdealLeadSeconds);
	// Beat position and timeline are separate from original input judgment.
	HEARBLADE_API double NextImpactBeat(double CurrentBeat, double SecondsPerBeat, float Windup, bool Heavy);
}
