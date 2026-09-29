#include "BossBlendSpaceTools.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/IAnimationDataController.h"

bool UBossBlendSpaceTools::NormalizePlayerAttackBoneLengths(UAnimSequence* Sequence)
{
	// Repair only our playback copies; never change the user's source library.
	if (!Sequence || !Sequence->GetPathName().StartsWith(TEXT("/Game/Combat/Player/Animations/")) || !Sequence->GetSkeleton()) return false;
	const FReferenceSkeleton& Reference = Sequence->GetSkeleton()->GetReferenceSkeleton();
	const auto* Model = Sequence->GetDataModel();
	if (!Model) return false;
	TArray<FName> Names; Model->GetBoneTrackNames(Names);
	auto& Controller = Sequence->GetController();
	Controller.OpenBracket(FText::FromString(TEXT("Restore player bone lengths")), false);
	for (FName Name : Names)
	{
		const int32 Bone = Reference.FindBoneIndex(Name);
		if (Bone == INDEX_NONE || Name == TEXT("root") || Name == TEXT("pelvis")) continue;
		TArray<FTransform> Keys; Model->GetBoneTrackTransforms(Name, Keys);
		TArray<FVector> Positions, Scales; TArray<FQuat> Rotations;
		for (const FTransform& Key : Keys)
		{
			Positions.Add(Reference.GetRefBonePose()[Bone].GetTranslation());
			Rotations.Add(Key.GetRotation());
			Scales.Add(Reference.GetRefBonePose()[Bone].GetScale3D());
		}
		if (Keys.Num()) Controller.SetBoneTrackKeys(Name, Positions, Rotations, Scales, false);
	}
	Controller.CloseBracket(false);
	Sequence->MarkPackageDirty();
	return true;
}
