#pragma once

#include "NiagaraComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

// AutoDestroy alone does not finish looping Niagara emitters. Preserve a short
// particle tail, then retire only this transient instance, never attached fire.
inline void RetireCombatFX(UNiagaraComponent* Component, float EmitSeconds, float TailSeconds = .6f)
{
    if (!Component || !Component->GetWorld()) return;
    FTimerHandle StopHandle, DestroyHandle;
    auto& Timers = Component->GetWorld()->GetTimerManager();
    const float StopAt = FMath::Max(.01f, EmitSeconds);
    Timers.SetTimer(StopHandle, FTimerDelegate::CreateWeakLambda(Component,
        [Component]() { Component->Deactivate(); }), StopAt, false);
    Timers.SetTimer(DestroyHandle, FTimerDelegate::CreateWeakLambda(Component,
        [Component]() { Component->DestroyComponent(); }), StopAt + FMath::Max(.01f, TailSeconds), false);
}

inline void RetireCombatFX(UParticleSystemComponent* Component, float EmitSeconds, float TailSeconds = .6f)
{
    if (!Component || !Component->GetWorld()) return;
    FTimerHandle StopHandle, DestroyHandle;
    auto& Timers = Component->GetWorld()->GetTimerManager();
    const float StopAt = FMath::Max(.01f, EmitSeconds);
    Timers.SetTimer(StopHandle, FTimerDelegate::CreateWeakLambda(Component,
        [Component]() { Component->DeactivateSystem(); }), StopAt, false);
    Timers.SetTimer(DestroyHandle, FTimerDelegate::CreateWeakLambda(Component,
        [Component]() { Component->DestroyComponent(); }), StopAt + FMath::Max(.01f, TailSeconds), false);
}

inline bool IsMeshSampledCourtEmber(const UNiagaraSystem* Effect)
{
    if (!Effect) return false;
    const FString Path = Effect->GetPathName();
    return Path == TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/VFX/NS_Embers_System.NS_Embers_System") ||
        Path == TEXT("/Game/Combat/Feedback/NS_PerfectDodge_Ring.NS_PerfectDodge_Ring");
}

inline void SpawnStandalonePerfectSparks(UWorld* World, UNiagaraSystem* Effect,
    const FVector& Location, const FRotator& Rotation, float Scale)
{
    if (!World || !Effect) return;
    // The copied ember system samples User.Spawn skeletal mesh and User.Spawn
    // and color texture. Without either binding it becomes a white spray.
    if (IsMeshSampledCourtEmber(Effect))
    {
        UParticleSystem* Sparks = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/SwordAnim/demo/FX/P_Spark.P_Spark"));
        RetireCombatFX(UGameplayStatics::SpawnEmitterAtLocation(World, Sparks, Location, Rotation,
            FVector(Scale * .65f), true), .10f, .35f);
    }
    else
        RetireCombatFX(UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Effect, Location,
            Rotation, FVector(Scale), true, true), .12f, .45f);
}
