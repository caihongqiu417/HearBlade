#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Async/Future.h"
#include "RhythmCaptureComponent.generated.h"

// Explicit command-line capture only. Never instantiated in normal play.
UCLASS()
class URhythmCaptureComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URhythmCaptureComponent();
	virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* Function) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	void CaptureScreenshot(int32 Width,int32 Height,const TArray<FColor>& Pixels);
	void FinishCapture();
	FDelegateHandle ScreenshotHandle;
	TArray<TFuture<bool>> Writes;
	FString Folder,Manifest;
	double StartTime=0.,NextFrame=0.;
	double RequestedTime=0.;
	bool bStarted=false,bRequested=false;
	int32 FrameCount=0;
	bool bFinished=false;
};
