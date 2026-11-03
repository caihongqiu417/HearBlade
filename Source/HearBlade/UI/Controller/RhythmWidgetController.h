#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UI/Model/RhythmHUDViewModel.h"
#include "RhythmWidgetController.generated.h"

class APlayerController;
class UPlayerDefenseComponent;
class UBeatSyncedComboComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRhythmHUDModelChanged, const FRhythmHUDViewModel&, Model);

// 读取玩法状态，生成并广播界面显示数据
UCLASS(BlueprintType)

class HEARBLADE_API URhythmWidgetController : public UObject
{
	GENERATED_BODY()
  public:
	// 通过绑定的玩家控制器取得当前世界
	virtual UWorld* GetWorld() const override;
	/** 绑定玩家控制器并初始化玩法数据引用 */
	void Initialize(APlayerController* InPlayer);
	/** 停止音频分析，释放对象引用并清空显示状态 */
	void Shutdown();
	/** 刷新显示快照并广播；DeltaSeconds 为本帧时长，单位为秒 */
	void Update(float DeltaSeconds);
	UPROPERTY(BlueprintReadOnly, Category = "HUD") FRhythmHUDViewModel ViewModel;
	UPROPERTY(BlueprintAssignable, Category = "HUD") FRhythmHUDModelChanged OnViewModelChanged;
	/** 立即生成并广播当前显示状态，供新界面初始化 */
	UFUNCTION(BlueprintCallable, Category = "HUD") void BroadcastInitialValues();
	/** 设置教程跳过区域的屏幕像素边界；无效范围清除区域 */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void SetTutorialSkipScreenRect(FVector2D Minimum, FVector2D Maximum);
	/** 玩家死亡后重新载入当前关卡，输入和蓝图按钮共用这个入口 */
	UFUNCTION(BlueprintCallable, Category="HUD") void RestartLevel();

  private:
	// 玩法对象引用：只读取已有状态，不拥有这些角色
	TWeakObjectPtr<APlayerController> Player;
	TWeakObjectPtr<UPlayerDefenseComponent> Combat;
	TWeakObjectPtr<UBeatSyncedComboComponent> Boss;
	TArray<FText> History;
	// 上次收录的判定时间，防止每帧把同一结果加入历史
	double LastJudgment = -1000.;
	// 下一次允许查找 Boss 的游戏时间
	double NextBossSearch = 0.;
	// 频谱分析的生命周期与平滑缓存
	bool bSpectrumStarted = false;
	float SpectrumPeak = .01f;
	TArray<float> SmoothedSpectrum;
	void Rebind();
	void UpdatePlayerPanel(float Delta);
	void UpdateJudgments();
	void UpdateBossPanel(float Delta);
	void UpdateRhythmTrack(float Delta);
	void UpdateTutorial();
	void UpdateLockMarker();
};
