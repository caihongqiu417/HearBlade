// 指定默认战斗 HUD，具体界面由 HUD 创建
// Copyright Epic Games, Inc. All Rights Reserved.

#include "HearBladeGameMode.h"
#include "UI/HUD/RhythmCombatHUD.h"

AHearBladeGameMode::AHearBladeGameMode()
{
	HUDClass = ARhythmCombatHUD::StaticClass();
}
