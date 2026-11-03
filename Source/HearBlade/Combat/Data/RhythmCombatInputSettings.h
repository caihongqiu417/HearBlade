// 定义战斗输入配置
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"
#include "RhythmCombatInputSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "节奏战斗输入"))

class HEARBLADE_API URhythmCombatInputSettings : public UDeveloperSettings
{
	GENERATED_BODY()
  public:
	virtual FName GetCategoryName() const override
	{
		return TEXT("Game");
	}

	UPROPERTY(Config, EditAnywhere, Category = "闪避") FKey DodgePrimary = EKeys::LeftShift;
	UPROPERTY(Config, EditAnywhere, Category = "闪避") FKey DodgeSecondary = EKeys::RightShift;
	UPROPERTY(Config, EditAnywhere, Category = "闪避") FKey DodgeGamepad = EKeys::Gamepad_FaceButton_Right;
	UPROPERTY(Config, EditAnywhere, Category = "格挡") FKey BlockKeyboardMouse = EKeys::RightMouseButton;
	UPROPERTY(Config, EditAnywhere, Category = "格挡") FKey BlockGamepad = EKeys::Gamepad_LeftShoulder;
	UPROPERTY(Config, EditAnywhere, Category = "反击") FKey CounterPrimary = EKeys::LeftMouseButton;
	UPROPERTY(Config, EditAnywhere, Category = "反击") FKey CounterSecondary = EKeys::Q;
	UPROPERTY(Config, EditAnywhere, Category = "反击") FKey CounterGamepad = EKeys::Gamepad_RightShoulder;
	UPROPERTY(Config, EditAnywhere, Category = "攻击") FKey LightAttackKey = EKeys::LeftMouseButton;
	UPROPERTY(Config, EditAnywhere, Category = "攻击")
	FKey LightAttackGamepad = EKeys::Gamepad_FaceButton_Left;
	UPROPERTY(Config, EditAnywhere, Category = "攻击") FKey HeavyAttackKey = EKeys::RightMouseButton;
	UPROPERTY(Config, EditAnywhere, Category = "攻击")
	FKey HeavyAttackGamepad = EKeys::Gamepad_FaceButton_Top;
	UPROPERTY(Config, EditAnywhere, Category = "共鸣") FKey ResonanceKey = EKeys::R;
	UPROPERTY(Config, EditAnywhere, Category = "共鸣") FKey ResonanceGamepad = EKeys::Gamepad_LeftTrigger;
};
