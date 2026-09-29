#include "BossBlendSpaceTools.h"

#include "Animation/BlendSpace.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"

bool UBossBlendSpaceTools::AssignIdenticalSkeleton(UAnimSequence* Sequence, USkeleton* Target)
{
	if(!Sequence || !Sequence->GetSkeleton() || !Target) return false;
	const FReferenceSkeleton& Source=Sequence->GetSkeleton()->GetReferenceSkeleton();
	const FReferenceSkeleton& Dest=Target->GetReferenceSkeleton();
	if(Source.GetNum()!=Dest.GetNum()) return false;
	for(int32 I=0;I<Source.GetNum();++I)
		if(Source.GetBoneName(I)!=Dest.GetBoneName(I) || Source.GetParentIndex(I)!=Dest.GetParentIndex(I) ||
			!Source.GetRefBonePose()[I].Equals(Dest.GetRefBonePose()[I],.001f)) return false;
	Sequence->Modify(); Sequence->SetSkeleton(Target); Sequence->PostEditChange(); Sequence->MarkPackageDirty();
	return true;
}

int32 UBossBlendSpaceTools::RebuildBlendSpace(UBlendSpace* BlendSpace)
{
	if (!IsValid(BlendSpace)) return 0;
	BlendSpace->Modify();
	BlendSpace->ResampleData();
	BlendSpace->MarkPackageDirty();
	return BlendSpace->GetBlendSpaceData().Triangles.Num();
}
