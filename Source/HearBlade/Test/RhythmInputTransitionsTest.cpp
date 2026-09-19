#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/PlayerRhythmCombatComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Combat/PlayerCombatAnimInstance.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRhythmInputTransitionsTest,
    "HearBlade.Combat.InputTransitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRhythmInputTransitionsTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    AActor* Owner = World->SpawnActor<AActor>();
    UPlayerRhythmCombatComponent* Combat = NewObject<UPlayerRhythmCombatComponent>(Owner);
    Owner->AddInstanceComponent(Combat);
    Combat->RegisterComponent();
    using EAction = UPlayerRhythmCombatComponent::EBufferedRhythmAction;

    TestTrue(TEXT("Initial dodge accepted"), Combat->TryDodge(FVector::ForwardVector));
    TestTrue(TEXT("Counter accepted during dodge startup"), Combat->TryCounter());
    TestTrue(TEXT("Counter queued rather than discarded"), Combat->BufferedAction == EAction::Counter);
    const double OriginalCounterTime = Combat->BufferedInputAudioSeconds;
    Combat->ProcessBufferedInput();
    TestFalse(TEXT("Startup protection remains active"), Combat->bCountering);
    Combat->DodgeStartWorldSeconds -= .13;
    Combat->ProcessBufferedInput();
    TestTrue(TEXT("Counter replaces roll before roll animation finishes"), Combat->bCountering && !Combat->bDodging);
    TestEqual(TEXT("Counter retains button timestamp"), Combat->LastCounterInputAudioSeconds, OriginalCounterTime);

    TestTrue(TEXT("Block queues during counter"), Combat->TryBlockStart());
    const double OriginalBlockTime = Combat->BufferedInputAudioSeconds;
    Combat->CounterStartWorldSeconds -= .13;
    Combat->ProcessBufferedInput();
    TestTrue(TEXT("Block replaces counter"), Combat->bBlocking && !Combat->bCountering);
    TestEqual(TEXT("Block retains button timestamp"), Combat->LastBlockStartAudioSeconds, OriginalBlockTime);
    TestTrue(TEXT("Counter directly exits held guard"), Combat->TryCounter());
    TestTrue(TEXT("Guard does not swallow counter"), Combat->bCountering && !Combat->bBlocking);

    Combat->TryBlockStart();
    Combat->EndBlock();
    Combat->CounterStartWorldSeconds -= .13;
    Combat->ProcessBufferedInput();
    TestFalse(TEXT("Released buffered guard cannot start a stuck hold"), Combat->bBlocking);
    TestTrue(TEXT("Released buffered guard still displays the tap action"), Combat->bBlockReleasePending);

    Combat->TryCounter();
    Combat->CounterStartWorldSeconds = World->GetTimeSeconds();
    Combat->TryDodge(FVector::RightVector);
    Combat->BufferedInputWorldSeconds -= .30;
    Combat->CounterStartWorldSeconds -= .30;
    Combat->ProcessBufferedInput();
    TestFalse(TEXT("Expired buffer cannot trigger an automatic roll"), Combat->bDodging);

    Combat->bCountering = false;
    Combat->TryDodge(FVector::ForwardVector);
    Combat->TryCounter();
    Combat->DodgeStartWorldSeconds -= .13;
    Combat->TryBlockStart();
    Combat->ProcessBufferedInput();
    TestTrue(TEXT("New immediate block supersedes old queued counter"), Combat->bBlocking && !Combat->bCountering);

    TestTrue(TEXT("Counter uses same Perfect window as dodge"),
        Combat->JudgeCounterInput(10.07, 10.0) == ERhythmDodgeJudgment::Perfect);
    TestTrue(TEXT("Counter uses same Good window as dodge"),
        Combat->JudgeCounterInput(10.14, 10.0) == ERhythmDodgeJudgment::Good);
    TestTrue(TEXT("Off-beat input remains Bad"),
        Combat->JudgeCounterInput(10.20, 10.0) == ERhythmDodgeJudgment::Bad);
    Combat->BlockInputHistory = {10.0};
    Combat->bBlocking = false;
    Combat->ResolveBossAvoidance(10.14, 4., .14f, false, FVector::ZeroVector,
        FVector::ForwardVector, ERhythmResponseType::Block, ERhythmHoldMarkerType::None, NAME_None);
    TestTrue(TEXT("A released single block tap earns Perfect"), Combat->LastJudgment == ERhythmDodgeJudgment::Perfect);
    TArray<double> Clicks = {20.0, 20.19};
    TestEqual(TEXT("Extra late click does not overwrite the accurate click"), Combat->ConsumeClosestInput(Clicks, 20.0), 20.0);
    TestEqual(TEXT("A consumed click cannot grade a second note"), Clicks.Num(), 1);
    Combat->bCountering = true;
    Combat->PlayResponseFeedback(ERhythmResponseType::Counter, ERhythmDodgeJudgment::Bad,
        FVector::ZeroVector, FVector::ForwardVector, false);
    TestTrue(TEXT("BAD feedback must not cancel the counter animation"), Combat->bCountering);
    World->DestroyWorld(false);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRhythmAnimationBlendTest,
    "HearBlade.Combat.AnimationBlend", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRhythmAnimationBlendTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    ACharacter* Character = World->SpawnActor<ACharacter>();
    USkeletalMeshComponent* Mesh = Character->GetMesh();
    Mesh->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,
        TEXT("/Game/Characters/Dark_Knight/Dark_Knight_Male/Meshes/SKM_DKM_Full.SKM_DKM_Full")));
    Mesh->SetAnimInstanceClass(UPlayerCombatAnimInstance::StaticClass());
    auto* Anim = Cast<UPlayerCombatAnimInstance>(Mesh->GetAnimInstance());
    if (!TestNotNull(TEXT("Persistent combat animation instance initialized"), Anim))
    {
        World->DestroyWorld(false);
        return false;
    }
    auto Step = [&](int32 Frames)
    {
        for (int32 Frame = 0; Frame < Frames; ++Frame)
        {
            Mesh->TickAnimation(1.f / 60.f, false);
            Mesh->RefreshBoneTransforms();
        }
    };
    Step(2);
    const FTransform IdleHand = Mesh->GetSocketTransform(TEXT("hand_r"), RTS_Component);
    auto* Attack = LoadObject<UAnimSequence>(nullptr,
        TEXT("/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Defense01_CounterAttack.Defense01_CounterAttack"));
    auto* Guard = LoadObject<UAnimSequence>(nullptr,
        TEXT("/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Defense01_Start.Defense01_Start"));
    auto* Roll = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/SwordAnim/Animation/UE5/roll_front.roll_front"));
    Anim->PlayCombatAction(Attack, false, .62f);
    UAnimMontage* First = Anim->GetCurrentActiveMontage();
    TestNotNull(TEXT("Attack uses a montage"), First);
    Step(15);
    TestTrue(TEXT("Slot evaluates the attack at full weight"), Anim->GetSlotMontageGlobalWeight(TEXT("DefaultSlot")) > .95f);
    TestFalse(TEXT("Attack changes the actual hand pose from idle"),
        IdleHand.Equals(Mesh->GetSocketTransform(TEXT("hand_r"), RTS_Component), .01f));
    const FAnimMontageInstance* FirstInstance = Anim->GetActiveInstanceForMontage(First);
    const int32 FirstInstanceID = FirstInstance ? FirstInstance->GetInstanceID() : INDEX_NONE;
    Anim->PlayCombatAction(Guard, false, .4f);
    UAnimMontage* Second = Anim->GetCurrentActiveMontage();
    Step(3);
    // Blending-out instances are removed from the active montage map immediately,
    // but remain in the evaluated instance list until their weight reaches zero.
    FAnimMontageInstance* Outgoing = Anim->GetMontageInstanceForID(FirstInstanceID);
    FAnimMontageInstance* Incoming = Anim->GetActiveInstanceForMontage(Second);
    TestTrue(TEXT("Attack and guard coexist during crossfade"),
        Outgoing && Incoming && Outgoing->GetWeight() > 0.f && Incoming->GetWeight() > 0.f);
    Anim->PlayCombatAction(Roll, false, .62f, 0.f);
    Step(10);
    TestTrue(TEXT("Roll plays in the same persistent instance"), Mesh->GetAnimInstance() == Anim);
    TestTrue(TEXT("Roll slot is fully evaluated"), Anim->GetSlotMontageGlobalWeight(TEXT("DefaultSlot")) > .95f);
    Anim->Montage_Stop(.18f);
    Step(15);
    TestTrue(TEXT("Action weight returns to locomotion"), Anim->GetSlotMontageGlobalWeight(TEXT("DefaultSlot")) < .01f);
    World->DestroyWorld(false);
    return true;
}
#endif
