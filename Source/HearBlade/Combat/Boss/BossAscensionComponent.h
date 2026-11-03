// 管理敌人阶段转换的动画、临时镜头和表现对象，音乐时间仍由节拍时钟提供
#pragma once

#include "Components/ActorComponent.h"
#include "BossAscensionComponent.generated.h"

class UStaticMeshComponent;
class UNiagaraComponent;
class UAnimMontage;
class ACameraActor;
class ACharacter;

// 管理演出期间的临时镜头和动画，音乐时间仍由节拍系统提供
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))

class HEARBLADE_API UBossAscensionComponent : public UActorComponent
{
	GENERATED_BODY()
  public:
	UBossAscensionComponent();
	// 检查演出条件，保存原状态后接管角色和镜头
	bool BeginAscension(ACharacter* Player, FVector Contact, bool bFromClash);
	// 统一处理完成和中断，归还镜头与角色控制
	void FinishPresentation(bool bCompleted);
	// 推进演出阶段、角色动作与镜头，到结束点后恢复战斗
	virtual void TickComponent(float DeltaTime, ELevelTick Type, FActorComponentTickFunction* Tick) override;
	// 退出关卡时结束演出，清理临时对象
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UPROPERTY(EditAnywhere, Category = "Ascension") float BodyScaleMultiplier = 1.10f;
	UPROPERTY(EditAnywhere, Category = "Ascension") float CameraReturnSeconds = .45f;

	bool IsPresenting() const
	{
		return bPresenting;
	}

	// 返回当前演出的完成比例
	float GetProgress() const;

  private:
	// 创建演出使用的临时模型和材质
	void CreateForm();
	// 按演出进度更新临时模型和光效
	void UpdateForm(float DeltaTime, double Beat);
	// 销毁演出创建的临时模型和效果
	void DestroyForm();
	// 为演出中的角色播放指定动画
	void PlayAscensionMotion();
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> HaloParts;
	UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraComponent>> Flames;
	UPROPERTY(Transient) TArray<TObjectPtr<UAnimMontage>> PresentationMontages;
	UPROPERTY(Transient) TObjectPtr<ACameraActor> Camera;
	TWeakObjectPtr<ACharacter> PlayerActor;
	TWeakObjectPtr<AActor> PreviousView;
	FVector ContactPoint = FVector::ZeroVector;
	FVector InitialCamera = FVector::ZeroVector;
	FRotator InitialCameraRotation = FRotator::ZeroRotator;
	FVector InitialBossScale = FVector::OneVector;
	float InitialCameraFOV = 90.f;
	float AppliedScale = 1.f;
	float AllowedScale = 1.f;
	float Reveal = 0.f;
	float ReturnElapsed = 0.f;
	bool bPresenting = false, bReturning = false, bTransformed = false, bMotionStarted = false,
	     bBurstPlayed = false, bClash = false;
	double StartBeat = 0., EndBeat = 0., StartWorld = 0.;
	int32 CaptureIndex = 0;
};
