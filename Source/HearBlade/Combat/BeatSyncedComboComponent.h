#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BeatSyncedComboComponent.generated.h"

class UAttackDataAsset;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnComboImpact, int32, ImpactIndex, double, TargetBeat, float, BeatErrorSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnComboStarted, int32, FirstImpactBeat);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnComboFinished);

// 把完整 Montage 的第一击预定在音乐拍点；后续击保持原动画的连贯性。
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class HEARBLADE_API UBeatSyncedComboComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBeatSyncedComboComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="节拍连招")
	TObjectPtr<UAttackDataAsset> AttackData;

	// -1 表示任意拍；0 表示每小节第一拍。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="节拍连招", meta=(ClampMin="-1"))
	int32 FirstImpactBeatInBar = -1;

	// 为动画准备留出的额外时间，避免刚好错过目标拍。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="节拍连招", meta=(ClampMin="0.0"))
	float MinimumLeadSeconds = 0.1f;

	// 空标签：播放 Owner 上所有与 Montage 骨架相同的 SkeletalMeshComponent。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="节拍连招")
	FName MeshComponentTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="节拍连招")
	bool bAutoQueueWhenClockStarts = false;

	UPROPERTY(BlueprintAssignable, Category="节拍连招")
	FOnComboStarted OnComboStarted;

	UPROPERTY(BlueprintAssignable, Category="节拍连招")
	FOnComboImpact OnImpact;

	UPROPERTY(BlueprintAssignable, Category="节拍连招")
	FOnComboFinished OnComboFinished;

	UFUNCTION(BlueprintCallable, Category="节拍连招")
	bool QueueCombo();

	UFUNCTION(BlueprintCallable, Category="节拍连招")
	void CancelCombo();

	UFUNCTION(BlueprintPure, Category="节拍连招")
	bool IsComboQueuedOrPlaying() const { return bQueued || bPlaying; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	float GetImpactTime(int32 Index) const;
	int32 GetImpactCount() const;
	bool StartPlayback(double AudioNowSeconds);

	bool bQueued = false;
	bool bPlaying = false;
	bool bTriedAutoQueue = false;
	int32 FirstImpactBeat = 0;
	int32 NextImpactIndex = 0;
	double ScheduledStartAudioSeconds = 0.0;
	float LastRootMotionPosition = 0.0f;
	TWeakObjectPtr<USkeletalMeshComponent> RootMotionMesh;
};
