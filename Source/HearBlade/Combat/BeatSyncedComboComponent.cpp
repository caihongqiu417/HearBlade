#include "Combat/BeatSyncedComboComponent.h"

#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "Combat/AttackData.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

UBeatSyncedComboComponent::UBeatSyncedComboComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

int32 UBeatSyncedComboComponent::GetImpactCount() const
{
	return AttackData && !AttackData->ImpactMarkers.IsEmpty() ? AttackData->ImpactMarkers.Num() : 1;
}

float UBeatSyncedComboComponent::GetImpactTime(int32 Index) const
{
	return AttackData->ImpactMarkers.IsEmpty()
		? AttackData->AuthoredImpactTime
		: AttackData->ImpactMarkers[Index].AnimationTimeSeconds;
}

bool UBeatSyncedComboComponent::QueueCombo()
{
	if (bQueued || bPlaying || !IsValid(AttackData) || !IsValid(AttackData->AttackMontage))
	{
		return false;
	}

	UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	if (!Clock || !Clock->IsRunning() || AttackData->PlayRate <= 0.0f)
	{
		return false;
	}

	const float MontageLength = AttackData->AttackMontage->GetPlayLength();
	float PreviousTime = -1.0f;
	double PreviousBeatOffset = -1.0;
	for (int32 Index = 0; Index < GetImpactCount(); ++Index)
	{
		const float Time = GetImpactTime(Index);
		const double Offset = AttackData->ImpactMarkers.IsEmpty() ? 0.0 : AttackData->ImpactMarkers[Index].BeatOffset;
		if (Time < 0.0f || Time > MontageLength || Time <= PreviousTime || Offset < PreviousBeatOffset || (Index == 0 && Offset != 0))
		{
			UE_LOG(LogTemp, Warning, TEXT("BeatSyncedCombo: AttackData 中的命中时间或拍数无效"));
			return false;
		}
		PreviousTime = Time;
		PreviousBeatOffset = Offset;
	}

	const double FirstImpactDelay = GetImpactTime(0) / AttackData->PlayRate;
	const double EarliestImpactTime = Clock->GetAudioNowSeconds() + FirstImpactDelay + MinimumLeadSeconds;
	const double BeatDuration = Clock->GetSecondsPerBeat();
	for (int32 Index = 1; Index < GetImpactCount(); ++Index)
	{
		const double AttackGap = (GetImpactTime(Index) - GetImpactTime(0)) / AttackData->PlayRate;
		const double BeatGap = AttackData->ImpactMarkers[Index].BeatOffset * BeatDuration;
		if (FMath::Abs(AttackGap - BeatGap) > 0.05)
		{
			UE_LOG(LogTemp, Warning, TEXT("BeatSyncedCombo: 第 %d 击与目标拍点相差 %.3f 秒；请调整动画间隔或 PlayRate"),
				Index + 1, AttackGap - BeatGap);
		}
	}
	int32 CandidateBeat = FMath::Max(0, FMath::CeilToInt(Clock->GetCurrentBeat() +
		(FirstImpactDelay + MinimumLeadSeconds) / BeatDuration));
	while (Clock->GetBeatTimeSeconds(CandidateBeat) < EarliestImpactTime)
	{
		++CandidateBeat;
	}
	if (FirstImpactBeatInBar >= 0)
	{
		const int32 BeatsPerBar = Clock->GetBeatsPerBar();
		const int32 Desired = FirstImpactBeatInBar % BeatsPerBar;
		while (CandidateBeat % BeatsPerBar != Desired)
		{
			++CandidateBeat;
		}
	}

	FirstImpactBeat = CandidateBeat;
	ScheduledStartAudioSeconds = Clock->GetBeatTimeSeconds(FirstImpactBeat) - FirstImpactDelay;
	NextImpactIndex = 0;
	bQueued = true;
	return true;
}

void UBeatSyncedComboComponent::CancelCombo()
{
	if (bPlaying && IsValid(AttackData) && IsValid(AttackData->AttackMontage))
	{
		TArray<USkeletalMeshComponent*> Meshes;
		GetOwner()->GetComponents<USkeletalMeshComponent>(Meshes);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (IsValid(Mesh) && IsValid(Mesh->GetSkeletalMeshAsset()) &&
				Mesh->GetSkeletalMeshAsset()->GetSkeleton() == AttackData->AttackMontage->GetSkeleton() &&
				(MeshComponentTag.IsNone() || Mesh->ComponentHasTag(MeshComponentTag)))
			{
				Mesh->Stop();
			}
		}
	}
	bQueued = false;
	bPlaying = false;
	NextImpactIndex = 0;
	RootMotionMesh.Reset();
}

bool UBeatSyncedComboComponent::StartPlayback(double AudioNowSeconds)
{
	const float StartPosition = FMath::Max(0.0f, static_cast<float>(
		(AudioNowSeconds - ScheduledStartAudioSeconds) * AttackData->PlayRate));
	if (StartPosition >= AttackData->AttackMontage->GetPlayLength())
	{
		return false;
	}

	TArray<USkeletalMeshComponent*> Meshes;
	GetOwner()->GetComponents<USkeletalMeshComponent>(Meshes);
	int32 PlayedCount = 0;
	RootMotionMesh.Reset();
	LastRootMotionPosition = StartPosition;
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (!IsValid(Mesh) || !IsValid(Mesh->GetSkeletalMeshAsset()) ||
			Mesh->GetSkeletalMeshAsset()->GetSkeleton() != AttackData->AttackMontage->GetSkeleton() ||
			(!MeshComponentTag.IsNone() && !Mesh->ComponentHasTag(MeshComponentTag)))
		{
			continue;
		}
		Mesh->PlayAnimation(AttackData->AttackMontage, false);
		if (UAnimSingleNodeInstance* Instance = Mesh->GetSingleNodeInstance())
		{
			Instance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
			Instance->SetPlayRate(AttackData->PlayRate);
			Instance->SetPosition(StartPosition, false);
			if (!RootMotionMesh.IsValid())
			{
				RootMotionMesh = Mesh;
			}
			++PlayedCount;
		}
	}
	if (PlayedCount == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("BeatSyncedCombo: Boss 上没有匹配 Montage 骨架的 SkeletalMeshComponent"));
		return false;
	}
	OnComboStarted.Broadcast(FirstImpactBeat);
	return true;
}

void UBeatSyncedComboComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UBeatClockSubsystem* Clock = GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	if (!Clock || !Clock->IsRunning())
	{
		return;
	}
	if (bAutoQueueWhenClockStarts && !bTriedAutoQueue)
	{
		bTriedAutoQueue = true;
		QueueCombo();
	}
	const double Now = Clock->GetAudioNowSeconds();
	if (bQueued && Now >= ScheduledStartAudioSeconds)
	{
		bQueued = false;
		bPlaying = StartPlayback(Now);
	}
	if (!bPlaying)
	{
		return;
	}

	const double AnimationPosition = (Now - ScheduledStartAudioSeconds) * AttackData->PlayRate;
	const float CurrentRootPosition = FMath::Clamp(static_cast<float>(AnimationPosition), 0.0f,
		AttackData->AttackMontage->GetPlayLength());
	if (RootMotionMesh.IsValid() && CurrentRootPosition > LastRootMotionPosition)
	{
		const FTransform LocalDelta = AttackData->AttackMontage->ExtractRootMotionFromTrackRange(
			LastRootMotionPosition, CurrentRootPosition, FAnimExtractContext(CurrentRootPosition, true));
		const FTransform WorldDelta = RootMotionMesh->ConvertLocalRootMotionToWorld(LocalDelta);
		GetOwner()->AddActorWorldTransform(WorldDelta, true);
		LastRootMotionPosition = CurrentRootPosition;
	}
	while (NextImpactIndex < GetImpactCount() && AnimationPosition >= GetImpactTime(NextImpactIndex))
	{
		const double Offset = AttackData->ImpactMarkers.IsEmpty() ? 0.0 : AttackData->ImpactMarkers[NextImpactIndex].BeatOffset;
		const double TargetBeat = FirstImpactBeat + Offset;
		const float BeatError = static_cast<float>(Now - Clock->GetBeatTimeSeconds(TargetBeat));
		OnImpact.Broadcast(NextImpactIndex, TargetBeat, BeatError);
		++NextImpactIndex;
	}
	if (AnimationPosition >= AttackData->AttackMontage->GetPlayLength())
	{
		bPlaying = false;
		RootMotionMesh.Reset();
		OnComboFinished.Broadcast();
	}
}
