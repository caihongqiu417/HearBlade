// 根据节拍更新已导入的舞台物体，不生成或修改网格几何
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoundstageBeatController.generated.h"

// 控制已导入场景物件的运动，不创建或修改网格几何
UCLASS()

class HEARBLADE_API ASoundstageBeatController : public AActor
{
	GENERATED_BODY()
  public:
	ASoundstageBeatController();

	void TriggerAscensionAccent(double Beat)
	{
		AscensionAccentBeat = Beat;
	}

	// 按音乐节拍更新舞台物件、灯光与材质
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soundstage|Rhythm",
	          meta = (ClampMin = "0", ClampMax = "2"))
	float MotionIntensity = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soundstage|Rhythm")
	bool bReducedMotion = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soundstage|Rhythm",
	          meta = (ClampMin = "0", ClampMax = "80"))
	float CrownBounceCm = 28.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soundstage|Sky",
	          meta = (ClampMin = "0", ClampMax = "0.08"))
	float SkyPulseStrength = .025f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soundstage|Rhythm",
	          meta = (ClampMin = "0", ClampMax = "25"))
	float PendulumAngleDegrees = 14.f;

  protected:
	// 查找场景装饰，记录初始变换和材质
	virtual void BeginPlay() override;
	// 退出时恢复场景装饰的原始状态
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
	double AscensionAccentBeat = -10000.;
};
