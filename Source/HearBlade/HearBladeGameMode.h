// 配置游戏模式默认对象
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HearBladeGameMode.generated.h"

// 配置第三人称游戏的默认规则
UCLASS(abstract)

class AHearBladeGameMode : public AGameModeBase
{
	GENERATED_BODY()

  public:
	// 初始化游戏规则
	AHearBladeGameMode();
};
