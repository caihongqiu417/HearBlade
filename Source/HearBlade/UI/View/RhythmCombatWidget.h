#pragma once
#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "UI/Model/RhythmHUDViewModel.h"
#include "RhythmCombatWidget.generated.h"

class URhythmWidgetController;

/** 接收显示数据并更新蓝图资产中的控件 */
UCLASS(Abstract, Blueprintable)

class HEARBLADE_API URhythmCombatWidget : public UUserWidget
{
	GENERATED_BODY()
  public:

	// 处理器连接
	/** 替换控制器绑定，并立即接收当前显示快照 */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void SetWidgetController(URhythmWidgetController* InController);
	UPROPERTY(BlueprintReadOnly, Category = "HUD") TObjectPtr<URhythmWidgetController> WidgetController;
	UPROPERTY(BlueprintReadOnly, Category = "HUD") FRhythmHUDViewModel ViewModel;
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD") void OnViewModelUpdated();
	/** 根据当前显示快照刷新约定名称的控件，由蓝图更新事件调用 */
	UFUNCTION(BlueprintCallable, Category = "HUD") void RefreshViewFromModel();

    // 蓝图样式配置
    // 原版贴图保存在蓝图默认值中，可直接替换图案和裁切范围
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="界面样式")
    TMap<ERhythmDodgeJudgment, FSlateBrush> JudgmentWords;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="界面样式")
    TMap<ERhythmResponseType, FSlateBrush> ResponseIcons;
    // 更新原版卡片、指令和命中装饰，不读取玩法对象
    UFUNCTION(BlueprintCallable, Category="HUD") void RefreshOriginalStyle();

  protected:
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;

  private:
	// 缓存控制器提供的数据，再通知蓝图更新显示
	UFUNCTION() void ReceiveModel(const FRhythmHUDViewModel& Model);
};
