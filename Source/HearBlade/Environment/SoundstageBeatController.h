// 按音乐拍数更新场景物件的位置、旋转和缩放
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoundstageBeatController.generated.h"

// 开始运行时按标签登记物件，每帧跟随音乐，退出时恢复初始变换
UCLASS()

class HEARBLADE_API ASoundstageBeatController : public AActor
{
	GENERATED_BODY()
  public:
	ASoundstageBeatController();

	// 阶段变化的节拍强调
	void TriggerAscensionAccent(double Beat)
	{
		AscensionAccentBeat = Beat;
	}

	// 按音乐拍数更新物件变换和天空材质
	virtual void Tick(float DeltaSeconds) override;

	// 场景运动参数
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soundstage|Rhythm",
	          meta = (ClampMin = "0", ClampMax = "2"))
	// 所有场景物件的运动倍率，零表示不运动
	float MotionIntensity = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soundstage|Rhythm")
	// 开启后将物件运动和天空变化强度降低到原来的五分之一
	bool bReducedMotion = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soundstage|Rhythm",
	          meta = (ClampMin = "0", ClampMax = "80"))
	float CrownBounceCm = 28.f;

	// 天空材质参数
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

	// 运行时物件记录
	struct FMovingProp
	{
		TWeakObjectPtr<AActor> Actor;
		FTransform RestTransform;
		FName Mode;
	};

	// 已登记的物件与材质
	TArray<FMovingProp> Props;
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UMaterialInstanceDynamic>> SkyMaterials;
	// 平滑后的拍点强度，用于弹起和缩放，不是音乐音量
	float Envelope = 0.f;
	// Boss 阶段变化的起始拍号，用于短暂增强场景弹动
	double AscensionAccentBeat = -10000.;
};
