// 定义音乐重音标记及其选择配置
#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Sound/SoundBase.h"
#include "MusicEmphasisData.generated.h"
USTRUCT(BlueprintType)

struct FMusicEmphasisPoint
{
	GENERATED_BODY()
	// 重音在源音乐中的秒数，与游戏启动时间无关
	UPROPERTY(EditAnywhere, BlueprintReadWrite) double SourceSeconds = 0.;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1")) float Strength = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Reason;
};
// 保存歌曲重音的位置与强度，帮助攻击选择合适的目标拍点
UCLASS(BlueprintType)

class HEARBLADE_API UMusicEmphasisData : public UDataAsset
{
	GENERATED_BODY()
  public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<USoundBase> Music;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SourceWavFilename = TEXT("Paramore_AintItFun_UE.wav");
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FMusicEmphasisPoint> Points;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "8"))
	// 为等待合适重音最多额外延后多少拍
	float MaxExtraWaitBeats = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01", ClampMax = "0.25"))
	float MatchToleranceSeconds = .12f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1"))
	float MinimumStrength = .55f;

	// 查找时间容差内的有效重音，越接近重音且强度越高，得分越高
	float ScoreAt(double SourceTime) const
	{
		if (!bEnabled)
			return 0.f;
		float Best = 0.f;
		const double Tolerance = FMath::Clamp(MatchToleranceSeconds, .01f, .25f);
		for (const auto& Point : Points)
		{
			// 用源音乐秒数比较重音距离，低于最低强度的标记不参与选择
			const double Error = FMath::Abs(Point.SourceSeconds - SourceTime);
			if (Point.bEnabled && Point.Strength >= MinimumStrength && Error <= Tolerance)
				Best = FMath::Max(Best, static_cast<float>(Point.Strength * (1. - .35 * Error / Tolerance)));
		}
		return Best;
	}
};
