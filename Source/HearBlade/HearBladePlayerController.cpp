// 管理玩家输入映射，提供触控界面配置
// Copyright Epic Games, Inc. All Rights Reserved.

#include "HearBladePlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "HearBlade.h"
#include "Widgets/Input/SVirtualJoystick.h"

void AHearBladePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// 只为本机玩家启用输入映射
	if (IsLocalPlayerController())
	{
		// 添加基础输入映射
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		        ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				if (CurrentContext) Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// 非触控模式才启用额外的视角映射
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					if (CurrentContext) Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

bool AHearBladePlayerController::ShouldUseTouchControls() const
{
	// 移动平台或强制触控配置决定是否显示触控界面
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
