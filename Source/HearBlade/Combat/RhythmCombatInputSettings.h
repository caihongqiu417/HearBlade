#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"
#include "RhythmCombatInputSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="节奏战斗输入"))
class HEARBLADE_API URhythmCombatInputSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UPROPERTY(Config, EditAnywhere, Category="闪避") FKey DodgePrimary = EKeys::LeftShift;
	UPROPERTY(Config, EditAnywhere, Category="闪避") FKey DodgeSecondary = EKeys::RightShift;
	UPROPERTY(Config, EditAnywhere, Category="闪避") FKey DodgeGamepad = EKeys::Gamepad_FaceButton_Right;
	UPROPERTY(Config, EditAnywhere, Category="格挡") FKey BlockKeyboardMouse = EKeys::RightMouseButton;
	UPROPERTY(Config, EditAnywhere, Category="格挡") FKey BlockGamepad = EKeys::Gamepad_LeftShoulder;
	UPROPERTY(Config, EditAnywhere, Category="反击") FKey CounterPrimary = EKeys::LeftMouseButton;
	UPROPERTY(Config, EditAnywhere, Category="反击") FKey CounterSecondary = EKeys::F;
	UPROPERTY(Config, EditAnywhere, Category="反击") FKey CounterGamepad = EKeys::Gamepad_RightShoulder;
};
