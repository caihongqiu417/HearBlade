// 配置不同应对类型对应的界面与视觉资源
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/Data/AttackData.h"
#include "RhythmResponseVisualData.generated.h"

class UTexture2D;
class USoundBase;

USTRUCT(BlueprintType)

struct FRhythmResponseVisual
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UTexture2D> Icon = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor Color = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<USoundBase> PromptSound = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.4", ClampMax = "2.0"))
	float IconScale = 1.0f;
};

UCLASS(BlueprintType)

class HEARBLADE_API URhythmResponseVisualData : public UPrimaryDataAsset
{
	GENERATED_BODY()
  public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "指令") FRhythmResponseVisual Dodge;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "指令") FRhythmResponseVisual Block;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "指令") FRhythmResponseVisual Counter;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "指令") FRhythmResponseVisual HoldBlock;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "持续格挡")
	TObjectPtr<UTexture2D> HoldStartIcon = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "持续格挡")
	TObjectPtr<UTexture2D> HoldTrackTexture = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "持续格挡")
	TObjectPtr<UTexture2D> HoldEndIcon = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "提示")
	TObjectPtr<USoundBase> ApproachSound = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "提示")
	TObjectPtr<USoundBase> StrongApproachSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "判定", meta = (ClampMin = "10", ClampMax = "250"))
	float PerfectWindowMs = 60.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "判定", meta = (ClampMin = "20", ClampMax = "350"))
	float GoodWindowMs = 120.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "持续格挡",
	          meta = (ClampMin = "0", ClampMax = "250"))
	float HoldReleaseGraceMs = 90.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "持续格挡", meta = (ClampMin = "0"))
	float PerfectHoldResonanceBonus = 20.0f;

	const FRhythmResponseVisual& Get(ERhythmResponseType Type) const
	{
		switch (Type)
		{
		case ERhythmResponseType::Block:
			return Block;
		case ERhythmResponseType::Counter:
			return Counter;
		case ERhythmResponseType::HoldBlock:
			return HoldBlock;
		default:
			return Dodge;
		}
	}
};
