// 管理玩家输入映射，提供触控界面配置
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HearBladePlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;

/**
 * 管理本机输入映射，提供触控配置
 * 界面创建和销毁交给 HUD
 */
UCLASS(abstract)

class AHearBladePlayerController : public APlayerController
{
	GENERATED_BODY()

  protected:
	/** 输入映射资源 */
	UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** 输入映射资源 */
	UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** 触控界面类，沿用现有蓝图配置 */
	UPROPERTY(EditAnywhere, Category = "Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** 强制显示触控界面，用于在桌面环境检查触控布局 */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;


	/** 在本机设置输入映射 */
	virtual void SetupInputComponent() override;

  public:
    TSubclassOf<UUserWidget> GetTouchControlsClass() const { return MobileControlsWidgetClass; }
	/** 是否使用触控界面 */
	bool ShouldUseTouchControls() const;
};
