// 处理显式启动的截图采集任务，不作为正常战斗组件自动创建
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Async/Future.h"
#include "RhythmCaptureComponent.generated.h"

// 只由指定命令行参数启用截图，普通游戏不创建此组件
UCLASS()

class URhythmCaptureComponent : public UActorComponent
{
	GENERATED_BODY()
  public:
	URhythmCaptureComponent();
	// 按截图任务推进等待和采集，完成后结束任务
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* Function) override;
	// 停止截图流程并清理状态
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

  private:
	// 按当前截图任务保存画面
	void CaptureScreenshot(int32 Width, int32 Height, const TArray<FColor>& Pixels);
	// 停止音画采集，等待后台写入完成并保存帧时间表
	void FinishCapture();
	FDelegateHandle ScreenshotHandle;
	TArray<TFuture<bool>> Writes;
	FString Folder, Manifest;
	double StartTime = 0., NextFrame = 0.;
	double RequestedTime = 0.;
	bool bStarted = false, bRequested = false;
	int32 FrameCount = 0;
	bool bFinished = false;
};
