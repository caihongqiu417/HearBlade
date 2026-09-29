#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoundstageBeatController.generated.h"

// Animates imported Blender prop actors; never constructs or deforms mesh geometry.
UCLASS()
class HEARBLADE_API ASoundstageBeatController : public AActor
{
    GENERATED_BODY()
public:
    ASoundstageBeatController();
	void TriggerAscensionAccent(double Beat) { AscensionAccentBeat=Beat; }
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Soundstage|Rhythm", meta=(ClampMin="0", ClampMax="2"))
    float MotionIntensity = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Soundstage|Rhythm")
    bool bReducedMotion = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Soundstage|Rhythm", meta=(ClampMin="0", ClampMax="80"))
    float CrownBounceCm = 28.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Soundstage|Sky", meta=(ClampMin="0", ClampMax="0.08"))
    float SkyPulseStrength = .025f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Soundstage|Rhythm", meta=(ClampMin="0", ClampMax="25"))
    float PendulumAngleDegrees = 14.f;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    struct FMovingProp
    {
        TWeakObjectPtr<AActor> Actor;
        FTransform RestTransform;
        FName Mode;
    };
    TArray<FMovingProp> Props;
    UPROPERTY(Transient)
    TArray<TObjectPtr<class UMaterialInstanceDynamic>> SkyMaterials;
    float Envelope = 0.f;
	double AscensionAccentBeat=-10000.;
};
