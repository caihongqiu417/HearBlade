// 配置游戏模式默认对象
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HearBladeGameMode.generated.h"

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)

class AHearBladeGameMode : public AGameModeBase
{
	GENERATED_BODY()

  public:
	/** Constructor */
	AHearBladeGameMode();
};
