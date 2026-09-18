#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "BossBlendSpaceTools.generated.h"

class UBlendSpace;

UCLASS()
class HEARBLADEBEATEDITOR_API UBossBlendSpaceTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Rebuilds the triangulation after samples have been populated by an editor script.
	UFUNCTION(BlueprintCallable, Category="HearBlade|Boss Animation")
	static int32 RebuildBlendSpace(UBlendSpace* BlendSpace);
};
