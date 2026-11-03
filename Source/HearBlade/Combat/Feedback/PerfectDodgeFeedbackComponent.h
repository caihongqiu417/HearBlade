// 管理成功应对后的残影、瞬时效果和音效
#pragma once

#include "Components/ActorComponent.h"
#include "Combat/Player/PlayerDefenseComponent.h"
#include "PerfectDodgeFeedbackComponent.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPerfectDodgeFeedbackConfig;
class UPoseableMeshComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;

// 接收防御判定结果，播放声音、残影、光环与镜头反馈
// 复用残影对象，并在结束时恢复临时修改的显示状态
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))

class HEARBLADE_API UPerfectDodgeFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()
  public:
	UPerfectDodgeFeedbackComponent();
	// 播放共鸣攻击对应的反馈
	void PlayResonanceAttack(bool bBurst);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perfect Dodge")
	TObjectPtr<UPerfectDodgeFeedbackConfig> Config;
	// 绑定防御结果事件并预先创建可复用残影
	virtual void BeginPlay() override;
	// 解除反馈事件，恢复屏幕状态并清理临时效果
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// 更新残影淡出、冲击帧恢复和持续格挡外壳
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

  private:
	// 防御音效与攻击共用 HitRush 资源组
	UPROPERTY(Transient) TObjectPtr<class USoundBase> DodgeHitSound;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> BlockHitSound;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> CounterHitSound;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> HoldHitSound;
	UPROPERTY(Transient) TObjectPtr<class USoundBase> TierUpHitSound;
	// 按防御方式和评级选择声音、残影与镜头反馈
	// 组合声音、残影、光环和镜头，应用反馈强度与显示配置
	UFUNCTION() void HandleRhythmFeedback(const FRhythmSuccessFeedbackData& Feedback);
	void PlayPerfectFeedback(const FPerfectDodgeFeedbackData& Feedback, float TierScale, bool bKeyPerfect,
	                         ERhythmResponseType Response = ERhythmResponseType::DodgeOrBlock);
	// 保存屏幕效果原值并启动短暂冲击帧
	void StartImpactFrame();
	// 恢复冲击帧修改的屏幕效果
	void RestoreImpactFrame();
	// 按配置和延迟生成扩散光环
	void SpawnRing(const FPerfectDodgeFeedbackData& Feedback, float DelaySeconds);
	// 从对象池取出残影，复制角色当前姿态
	void SpawnGhosts(float Strength);
	// 清理残影与光环等临时显示
	void ClearTransientVisuals();
	// 跟随角色姿态更新外壳，并按拍点和接触调整亮度
	void UpdateHoldArmorVisual(float DeltaTime);
	// 为身体和披风准备持续格挡外壳
	void CreateHoldArmorVisual();
	// 销毁持续格挡外壳并清空引用
	void DestroyHoldArmorVisual();

	struct FGhostVisual
	{
		TWeakObjectPtr<UPoseableMeshComponent> Mesh;
		TWeakObjectPtr<UMaterialInstanceDynamic> Material;
		float Delay = 0.f;
	};

	struct FRingVisual
	{
		TWeakObjectPtr<UStaticMeshComponent> Mesh;
		TWeakObjectPtr<UMaterialInstanceDynamic> Material;
		double Start = 0.0;
	};

	TArray<FGhostVisual> Ghosts;
	// 提前创建并复用残影网格，避免完美闪避时创建骨骼网格造成卡顿

	TArray<FGhostVisual> GhostPool;
	// 补齐残影对象池，后续闪避直接复用
	void EnsureGhostPool();
	TArray<FRingVisual> Rings;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> PostProcessMID;

	// 身体和披风分别创建同步外壳，持续格挡时两部分一起显示


	struct FHoldArmorVisual
	{
		TWeakObjectPtr<USkeletalMeshComponent> Source;
		TWeakObjectPtr<USkeletalMeshComponent> Mesh;
		TWeakObjectPtr<UMaterialInstanceDynamic> Material;
		TWeakObjectPtr<UMaterialInterface> PreviousOverlay;
	};

	TArray<FHoldArmorVisual> HoldArmorVisuals;
	double HoldArmorImpactWorldSeconds = -1000.0;
	bool bHoldArmorWasActive = false;
	double EffectStartWorldSeconds = -1000.0;
	float ActiveDuration = 0.15f;
	float ActiveStrength = 1.0f;
	double LastImpactFrameWorldSeconds = -1000.0;
	double ImpactFrameEndWorldSeconds = -1000.0;
	FVector4 SavedSaturation = FVector4(1, 1, 1, 1);
	FVector4 SavedContrast = FVector4(1, 1, 1, 1);
	bool bSavedSaturationOverride = false;
	bool bSavedContrastOverride = false;
	bool bImpactFrameActive = false;
};
