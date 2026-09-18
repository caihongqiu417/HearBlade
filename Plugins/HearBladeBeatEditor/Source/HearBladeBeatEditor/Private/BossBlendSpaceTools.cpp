#include "BossBlendSpaceTools.h"

#include "Animation/BlendSpace.h"

int32 UBossBlendSpaceTools::RebuildBlendSpace(UBlendSpace* BlendSpace)
{
	if (!IsValid(BlendSpace)) return 0;
	BlendSpace->Modify();
	BlendSpace->ResampleData();
	BlendSpace->MarkPackageDirty();
	return BlendSpace->GetBlendSpaceData().Triangles.Num();
}
