// 处理显式启动的截图采集任务，不作为正常战斗组件自动创建
#include "Combat/Debug/RhythmCaptureComponent.h"
#include "Engine/GameEngine.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "AudioMixerBlueprintLibrary.h"
#include "ImageUtils.h"
#include "Async/Async.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

URhythmCaptureComponent::URhythmCaptureComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

// 按截图任务推进等待和采集，完成后结束任务
void URhythmCaptureComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* Function)
{
	Super::TickComponent(DeltaTime, TickType, Function);
	if (bFinished)
		return;
	if (!bStarted)
	{
		auto* Engine = Cast<UGameEngine>(GEngine);
		if (!Engine || !Engine->SceneViewport.IsValid())
			return;
		Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("PhaseTwoCapture") /
		                                           FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
		IFileManager::Get().MakeDirectory(*Folder, true);
		ScreenshotHandle = UGameViewportClient::OnScreenshotCaptured().AddUObject(
		    this, &URhythmCaptureComponent::CaptureScreenshot);
		bStarted = true;
		StartTime = FPlatformTime::Seconds();
		NextFrame = StartTime;
		UAudioMixerBlueprintLibrary::StartRecordingOutput(this, 65.f);
		UE_LOG(LogTemp, Display, TEXT("PhaseTwoCapture: started %s"), *Folder);
	}
	const double Now = FPlatformTime::Seconds();
	if (Now - StartTime >= 65.)
	{
		FinishCapture();
		return;
	}
	// 上一张截图尚未回调时不重复请求，避免时间记录与图片错配
	if (Now >= NextFrame && !bRequested)
	{
		RequestedTime = Now - StartTime;
		bRequested = true;
		FScreenshotRequest::RequestScreenshot(TEXT("PhaseTwoVideoFrame"), false, false);
		NextFrame = Now + 1. / 30.;
	}
}

// 按当前截图任务保存画面
void URhythmCaptureComponent::CaptureScreenshot(int32 Width, int32 Height, const TArray<FColor>& Pixels)
{
	if (!bRequested || bFinished)
		return;
	bRequested = false;
	const FString Name = FString::Printf(TEXT("frame_%06d.jpg"), FrameCount++);
	Manifest += FString::Printf(TEXT("%s,%.6f\n"), *Name, RequestedTime);
	const FString Path = Folder / Name;
	// 复制像素后在后台压缩写盘，避免文件写入阻塞正常帧更新
	Writes.Add(Async(EAsyncExecution::ThreadPool,
	                 [Pixels, Width, Height, Path]()
	                 {
		                 TArray64<uint8> Data;
		                 return FImageUtils::CompressImage(Data, TEXT("jpg"),
						                                   FImageView(Pixels.GetData(), Width, Height), 88) &&
						        FFileHelper::SaveArrayToFile(Data, *Path);
	                 }));
}

// 停止音画采集，等待后台写入完成并保存帧时间表
void URhythmCaptureComponent::FinishCapture()
{
	if (bFinished || !bStarted)
		return;
	bFinished = true;
	UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotHandle);
	UAudioMixerBlueprintLibrary::StopRecordingOutput(this, EAudioRecordingExportType::WavFile,
	                                                 TEXT("game_audio"), Folder);
	bool bSaved = true;
	// 结束采集时等待所有写入完成，再保存图片与时间的对应表
	for (auto& Write : Writes)
		bSaved &= Write.Get();
	Writes.Empty();
	FFileHelper::SaveStringToFile(Manifest, *(Folder / TEXT("frames.csv")));
	UE_LOG(LogTemp, Display, TEXT("PhaseTwoCapture: finished frames=%d saved=%d folder=%s"), FrameCount,
	       bSaved, *Folder);
}

// 停止截图流程并清理状态
void URhythmCaptureComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	FinishCapture();
	Super::EndPlay(Reason);
}
