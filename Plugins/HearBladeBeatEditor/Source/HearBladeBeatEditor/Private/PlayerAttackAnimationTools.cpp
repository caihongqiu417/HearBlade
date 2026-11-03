// 调整攻击动画中的骨骼长度数据以匹配参考骨架
#include "BossBlendSpaceTools.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/IAnimationDataController.h"

// 保留动画旋转，将非根骨骼平移校正为参考骨架长度
bool UBossBlendSpaceTools::NormalizePlayerAttackBoneLengths(UAnimSequence* Sequence)
{
	// 只修改项目中的播放副本，保留源动画库
	if (!Sequence || !Sequence->GetPathName().StartsWith(TEXT("/Game/Combat/Player/Animations/")) ||
	    !Sequence->GetSkeleton())
		return false;
	const FReferenceSkeleton& Reference = Sequence->GetSkeleton()->GetReferenceSkeleton();
	const auto* Model = Sequence->GetDataModel();
	if (!Model)
		return false;
	TArray<FName> Names;
	Model->GetBoneTrackNames(Names);
	auto& Controller = Sequence->GetController();
	// 把整次骨骼轨道修改合并处理，完成后统一通知动画数据更新
	Controller.OpenBracket(FText::FromString(TEXT("Restore player bone lengths")), false);
	for (FName Name : Names)
	{
		const int32 Bone = Reference.FindBoneIndex(Name);
		// 保留根骨骼和骨盆的位移，不改变动画原有的移动轨迹
		if (Bone == INDEX_NONE || Name == TEXT("root") || Name == TEXT("pelvis"))
			continue;
		TArray<FTransform> Keys;
		Model->GetBoneTrackTransforms(Name, Keys);
		TArray<FVector> Positions, Scales;
		TArray<FQuat> Rotations;
		for (const FTransform& Key : Keys)
		{
			// 位置和缩放恢复为参考骨架，保留每帧旋转来维持动作姿态
			Positions.Add(Reference.GetRefBonePose()[Bone].GetTranslation());
			Rotations.Add(Key.GetRotation());
			Scales.Add(Reference.GetRefBonePose()[Bone].GetScale3D());
		}
		if (Keys.Num())
			Controller.SetBoneTrackKeys(Name, Positions, Rotations, Scales, false);
	}
	Controller.CloseBracket(false);
	Sequence->MarkPackageDirty();
	return true;
}
