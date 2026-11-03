// 创建并回收临时战斗特效，避免循环发射器永久保留
#pragma once

#include "NiagaraComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

// 临时特效先停止发射，再等尾部粒子消失后销毁
// 循环发射器不会自行结束，不能只依赖自动销毁
inline void RetireCombatFX(UNiagaraComponent* Component, float EmitSeconds, float TailSeconds = .6f)
{
	if (!Component || !Component->GetWorld())
		return;
	FTimerHandle StopHandle, DestroyHandle;
	auto& Timers = Component->GetWorld()->GetTimerManager();
	const float StopAt = FMath::Max(.01f, EmitSeconds);
	// 弱绑定使组件销毁后不再执行回调
	Timers.SetTimer(StopHandle,
	                FTimerDelegate::CreateWeakLambda(Component,
	                                                 [Component]()
	                                                 {
		                                                 Component->Deactivate();
	                                                 }),
	                StopAt, false);
	Timers.SetTimer(DestroyHandle,
	                FTimerDelegate::CreateWeakLambda(Component,
	                                                 [Component]()
	                                                 {
		                                                 Component->DestroyComponent();
	                                                 }),
	                StopAt + FMath::Max(.01f, TailSeconds), false);
}

// 旧粒子系统也先停止发射，再延迟销毁，避免循环特效一直残留
inline void RetireCombatFX(UParticleSystemComponent* Component, float EmitSeconds, float TailSeconds = .6f)
{
	if (!Component || !Component->GetWorld())
		return;
	FTimerHandle StopHandle, DestroyHandle;
	auto& Timers = Component->GetWorld()->GetTimerManager();
	const float StopAt = FMath::Max(.01f, EmitSeconds);
	Timers.SetTimer(StopHandle,
	                FTimerDelegate::CreateWeakLambda(Component,
	                                                 [Component]()
	                                                 {
		                                                 Component->DeactivateSystem();
	                                                 }),
	                StopAt, false);
	Timers.SetTimer(DestroyHandle,
	                FTimerDelegate::CreateWeakLambda(Component,
	                                                 [Component]()
	                                                 {
		                                                 Component->DestroyComponent();
	                                                 }),
	                StopAt + FMath::Max(.01f, TailSeconds), false);
}

inline bool IsMeshSampledCourtEmber(const UNiagaraSystem* Effect)
{
	if (!Effect)
		return false;
	const FString Path = Effect->GetPathName();
	return Path == TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/VFX/NS_Embers_System.NS_Embers_System") ||
	       Path == TEXT("/Game/Combat/Feedback/NS_PerfectDodge_Ring.NS_PerfectDodge_Ring");
}

inline void SpawnStandalonePerfectSparks(UWorld* World, UNiagaraSystem* Effect, const FVector& Location,
                                         const FRotator& Rotation, float Scale)
{
	if (!World || !Effect)
		return;
	// 这类余烬需要角色网格和颜色贴图，独立生成会显示异常
	// 没有角色来源时改用普通火花
	if (IsMeshSampledCourtEmber(Effect))
	{
		UParticleSystem* Sparks =
		    LoadObject<UParticleSystem>(nullptr, TEXT("/Game/SwordAnim/demo/FX/P_Spark.P_Spark"));
		RetireCombatFX(UGameplayStatics::SpawnEmitterAtLocation(World, Sparks, Location, Rotation,
		                                                        FVector(Scale * .65f), true),
		               .10f, .35f);
	}
	else
		RetireCombatFX(UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Effect, Location, Rotation,
		                                                              FVector(Scale), true, true),
		               .12f, .45f);
}
