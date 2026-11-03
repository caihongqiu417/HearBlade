#pragma once
#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "UI/Model/RhythmHUDViewModel.h"
#include "RhythmCombatWidget.generated.h"

class URhythmWidgetController;

/** 接收显示数据并更新蓝图资产中的控件 */
UCLASS(Abstract, Blueprintable)

class HEARBLADE_API URhythmCombatWidget : public UUserWidget
{
	GENERATED_BODY()
  public:
	/** 替换控制器绑定，并立即接收当前显示快照 */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void SetWidgetController(URhythmWidgetController* InController);
	UPROPERTY(BlueprintReadOnly, Category = "HUD") TObjectPtr<URhythmWidgetController> WidgetController;
	UPROPERTY(BlueprintReadOnly, Category = "HUD") FRhythmHUDViewModel ViewModel;
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD") void OnViewModelUpdated();
	/** 根据当前显示快照刷新约定名称的控件，由蓝图更新事件调用 */
	UFUNCTION(BlueprintCallable, Category = "HUD") void RefreshViewFromModel();

  protected:
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;

  private:
	UFUNCTION() void ReceiveModel(const FRhythmHUDViewModel& Model);
};
