// 验证节奏输入状态切换与动画混合
#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/Player/PlayerDefenseComponent.h"
#include "Combat/Boss/BeatSyncedComboComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Combat/Player/PlayerCombatAnimInstance.h"
#include "Combat/Boss/BossLocomotionAnimInstance.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRhythmInputTransitionsTest, "HearBlade.Combat.InputTransitions",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRhythmInputTransitionsTest::RunTest(const FString& Parameters)
{
	// 在独立世界中逐项切换防御状态，检查缓存、松键和中断后的恢复
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AActor* Owner = World->SpawnActor<AActor>();
	UPlayerDefenseComponent* Combat = NewObject<UPlayerDefenseComponent>(Owner);
	Owner->AddInstanceComponent(Combat);
	Combat->RegisterComponent();
	TestNotNull(TEXT("Player light blood effect loaded"), Combat->BloodHitEffect.Get());
	TestNotNull(TEXT("Player poise-break blood effect loaded"), Combat->BloodPoiseBreakEffect.Get());
	Combat->Resonance = 10.0f;
	Combat->ResonanceDecayDelaySeconds = 0.0f;
	Combat->ResonanceDecayPerSecond = 6.0f;
	Combat->LastResonanceGainWorldSeconds = -1000.0;
	Combat->TickComponent(0.5f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Resonance decays continuously after its grace period"), Combat->Resonance, 7.0f);
	Combat->Resonance = 0.0f;
	using EAction = UPlayerDefenseComponent::EBufferedDefenseAction;

	TestTrue(TEXT("Initial dodge accepted"), Combat->RequestDodge(FVector::ForwardVector));
	TestTrue(TEXT("Counter accepted during dodge startup"), Combat->RequestParry());
	TestTrue(TEXT("Counter queued rather than discarded"), Combat->BufferedAction == EAction::Counter);
	const double OriginalCounterTime = Combat->BufferedInputAudioSeconds;
	Combat->ProcessDefenseBuffer();
	TestFalse(TEXT("Startup protection remains active"), Combat->bCountering);
	Combat->DodgeStartWorldSeconds -= .13;
	Combat->ProcessDefenseBuffer();
	TestTrue(TEXT("Counter replaces roll before roll animation finishes"),
	         Combat->bCountering && !Combat->bDodging);
	TestEqual(TEXT("Counter retains button timestamp"), Combat->LastCounterInputAudioSeconds,
	          OriginalCounterTime);

	TestTrue(TEXT("Block queues during counter"), Combat->RequestBlock());
	const double OriginalBlockTime = Combat->BufferedInputAudioSeconds;
	Combat->CounterStartWorldSeconds -= .13;
	Combat->ProcessDefenseBuffer();
	TestTrue(TEXT("Block replaces counter"), Combat->bBlocking && !Combat->bCountering);
	TestEqual(TEXT("Block retains button timestamp"), Combat->LastBlockStartAudioSeconds, OriginalBlockTime);
	TestTrue(TEXT("Counter directly exits held guard"), Combat->RequestParry());
	TestTrue(TEXT("Guard does not swallow counter"), Combat->bCountering && !Combat->bBlocking);

	Combat->RequestBlock();
	Combat->ReleaseBlock();
	Combat->CounterStartWorldSeconds -= .13;
	Combat->ProcessDefenseBuffer();
	TestFalse(TEXT("Released buffered guard cannot start a stuck hold"), Combat->bBlocking);
	TestTrue(TEXT("Released buffered guard still displays the tap action"), Combat->bBlockReleasePending);

	Combat->RequestParry();
	Combat->CounterStartWorldSeconds = World->GetTimeSeconds();
	Combat->RequestDodge(FVector::RightVector);
	Combat->BufferedInputWorldSeconds -= .30;
	Combat->CounterStartWorldSeconds -= .30;
	Combat->ProcessDefenseBuffer();
	TestFalse(TEXT("Expired buffer cannot trigger an automatic roll"), Combat->bDodging);

	Combat->bCountering = false;
	Combat->RequestDodge(FVector::ForwardVector);
	Combat->RequestParry();
	Combat->DodgeStartWorldSeconds -= .13;
	Combat->RequestBlock();
	Combat->ProcessDefenseBuffer();
	TestTrue(TEXT("New immediate block supersedes old queued counter"),
	         Combat->bBlocking && !Combat->bCountering);

	TestTrue(TEXT("Counter uses same Perfect window as dodge"),
	         Combat->JudgeParryInput(10.07, 10.0) == ERhythmDodgeJudgment::Perfect);
	TestTrue(TEXT("Counter uses same Good window as dodge"),
	         Combat->JudgeParryInput(10.14, 10.0) == ERhythmDodgeJudgment::Good);
	TestTrue(TEXT("Off-beat input remains Bad"),
	         Combat->JudgeParryInput(10.20, 10.0) == ERhythmDodgeJudgment::Bad);
	Combat->BlockInputHistory = {10.0};
	Combat->bBlocking = false;
	Combat->ResolveEnemyMiss(10.14, 4., .14f, false, FVector::ZeroVector, FVector::ForwardVector,
	                             ERhythmResponseType::Block, ERhythmHoldMarkerType::None, NAME_None);
	TestTrue(TEXT("A released single block tap earns Perfect"),
	         Combat->LastJudgment == ERhythmDodgeJudgment::Perfect);
	TArray<double> Clicks = {20.0, 20.19};
	TestEqual(TEXT("Extra late click does not overwrite the accurate click"),
	          Combat->ConsumeClosestDefenseInput(Clicks, 20.0), 20.0);
	TestEqual(TEXT("A consumed click cannot grade a second note"), Clicks.Num(), 1);
	Combat->bCountering = true;
	Combat->PlayResponseFeedback(ERhythmResponseType::Counter, ERhythmDodgeJudgment::Bad, FVector::ZeroVector,
	                             FVector::ForwardVector, false);
	TestTrue(TEXT("BAD feedback must not cancel the counter animation"), Combat->bCountering);

	Combat->bCountering = false;
	Combat->BufferedAction = EAction::None;
	Combat->Poise = Combat->MaxPoise;
	Combat->ApplyPoiseDamage(Combat->BadPoiseDamage, FVector(100, 0, 0), -FVector::ForwardVector);
	Combat->ApplyPoiseDamage(Combat->BadPoiseDamage, FVector(100, 0, 0), -FVector::ForwardVector);
	TestFalse(TEXT("Poise damage below zero does not interrupt actions"), Combat->bPoiseBroken);
	Combat->ApplyPoiseDamage(Combat->BadPoiseDamage, FVector(100, 0, 0), -FVector::ForwardVector);
	TestTrue(TEXT("Repeated BAD judgments break poise"), Combat->bPoiseBroken);
	TestEqual(TEXT("Broken poise remains empty during hit stun"), Combat->Poise, 0.0f);
	TestTrue(TEXT("Input during hit lock is accepted into the buffer"), Combat->RequestParry());
	TestTrue(TEXT("Buffered counter waits during hit lock"),
	         Combat->BufferedAction == EAction::Counter && !Combat->bCountering);
	Combat->PoiseBreakStartWorldSeconds -= Combat->PoiseBreakInputLockSeconds + .01;
	Combat->ProcessDefenseBuffer();
	TestTrue(TEXT("Buffered counter exits hit stun after the lock"),
	         Combat->bCountering && !Combat->bPoiseBroken);
	TestEqual(TEXT("Poise resets after hit stun"), Combat->Poise, Combat->MaxPoise);

	ACharacter* HitCharacter = World->SpawnActor<ACharacter>();
	UPlayerDefenseComponent* HitCombat = NewObject<UPlayerDefenseComponent>(HitCharacter);
	HitCharacter->AddInstanceComponent(HitCombat);
	HitCombat->RegisterComponent();
	HitCharacter->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));
	HitCombat->StartHitReaction(FVector(100.0f, 0.0f, 85.0f), FVector::ForwardVector, false,
	                            FVector(-100.0f, 0.0f, 0.0f));
	TestTrue(TEXT("Every ordinary damaging contact can start a hit reaction"), HitCombat->IsHitReacting());
	TestTrue(TEXT("Hit waits for the turn before playing the backward clip"), HitCombat->bHitTurnPending);
	TestTrue(TEXT("Hit turns toward the incoming attack"),
	         FMath::Abs(FMath::FindDeltaAngleDegrees(HitCombat->HitFacingRotation.Yaw, 180.0f)) < 1.0f);
	HitCombat->HitTurnEndWorldSeconds = -1.0;
	HitCombat->TickComponent(.08f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Turning finishes before the backward hit animation"), HitCombat->bHitTurnPending);
	TestTrue(TEXT("Character faces the blade source"),
	         FMath::Abs(FMath::FindDeltaAngleDegrees(HitCharacter->GetActorRotation().Yaw, 180.0f)) < 1.0f);
	HitCombat->StartPoiseBreak(FVector(100, 0, 85), FVector::ForwardVector, FVector(-100, 0, 0));
	TestTrue(TEXT("Counter buffers immediately after a failed defense breaks poise"),
	         HitCombat->RequestParry());
	const double BufferedCounterTime = HitCombat->BufferedInputAudioSeconds;
	HitCombat->BufferedInputWorldSeconds -= .29;
	HitCombat->PoiseBreakStartWorldSeconds -= .29;
	HitCombat->HitInputUnlockWorldSeconds -= .29;
	HitCombat->ProcessDefenseBuffer();
	TestTrue(TEXT("Counter resumes after 280 ms lock despite 260 ms normal buffer"),
	         HitCombat->bCountering && !HitCombat->bHitReacting && !HitCombat->bPoiseBroken);
	TestEqual(TEXT("Recovered counter preserves original rhythm timestamp"),
	          HitCombat->LastCounterInputAudioSeconds, BufferedCounterTime);
	HitCombat->CounterStartWorldSeconds -= .13;
	TestTrue(TEXT("Next guard works after the recovered counter"), HitCombat->RequestBlock());
	HitCombat->ReleaseBlock();
	TestFalse(TEXT("Recovered guard tap does not stick"), HitCombat->bBlocking);
	HitCombat->RequestBlock();
	HitCombat->BlockStartWorldSeconds = World->GetTimeSeconds() - 2.;
	HitCombat->DefenseAnimationStartWorldSeconds = BIG_NUMBER;
	HitCombat->ReleaseBlock();
	TestFalse(TEXT("Releasing looping guard never waits on the animation sentinel"),
	          HitCombat->bBlockReleasePending);
	HitCombat->Health = 0.f;
	TestFalse(TEXT("Defeated player cannot silently keep dodging"),
	          HitCombat->RequestDodge(FVector::ForwardVector));
	TestFalse(TEXT("Defeated player cannot counter"), HitCombat->RequestParry());
	TestFalse(TEXT("Defeated player cannot guard"), HitCombat->RequestBlock());
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRhythmAnimationBlendTest, "HearBlade.Combat.AnimationBlend",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRhythmAnimationBlendTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ACharacter* Character = World->SpawnActor<ACharacter>();
	USkeletalMeshComponent* Mesh = Character->GetMesh();
	Mesh->SetSkeletalMesh(LoadObject<USkeletalMesh>(
	    nullptr, TEXT("/Game/Characters/Dark_Knight/Dark_Knight_Male/Meshes/SKM_DKM_Full.SKM_DKM_Full")));
	Mesh->SetAnimInstanceClass(UPlayerCombatAnimInstance::StaticClass());
	auto* Anim = Cast<UPlayerCombatAnimInstance>(Mesh->GetAnimInstance());
	if (!TestNotNull(TEXT("Persistent combat animation instance initialized"), Anim))
	{
		World->DestroyWorld(false);
		return false;
	}
	if (TestNotNull(TEXT("Player directional hit Blend Space loaded"), Anim->DirectionalHit.Get()))
	{
		bool bFrontalHitRecoilFound = false;
		for (const FBlendSample& Sample : Anim->DirectionalHit->GetBlendSamples())
		{
			if (Sample.Animation && Sample.Animation->GetName().Contains(TEXT("Hit_Alert_Fwd")) &&
			    FMath::IsNearlyEqual(Sample.SampleValue.X, 1.0f) && FMath::IsNearlyZero(Sample.SampleValue.Y))
				bFrontalHitRecoilFound = true;
		}
		TestTrue(TEXT("Frontal hit selects backward recoil sample"), bFrontalHitRecoilFound);
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
	auto* Attack = LoadObject<UAnimSequence>(
	    nullptr,
	    TEXT(
	        "/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Defense01_CounterAttack.Defense01_CounterAttack"));
	auto* Guard = LoadObject<UAnimSequence>(
	    nullptr,
	    TEXT("/Game/Characters/Dark_Knight/Dark_Knight_Male/Animations/Defense01_Start.Defense01_Start"));
	auto* Roll =
	    LoadObject<UAnimSequence>(nullptr, TEXT("/Game/SwordAnim/Animation/UE5/roll_front.roll_front"));
	// 验证普通攻击状态或旧蒙太奇指针不会压住新的格挡和反击动画

	auto* Ordinary = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Combat/Player/Animations/combo01_1"));
	UAnimMontage* Lead = Anim->BeginBeatAttack(Ordinary, .4f);
	Step(8);
	const auto* LeadInstance = Anim->GetActiveInstanceForMontage(Lead);
	const int32 LeadID = LeadInstance ? LeadInstance->GetInstanceID() : INDEX_NONE;
	const float LeadPosition = Anim->Montage_GetPosition(Lead);
	auto* NextOrdinary = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Combat/Player/Animations/combo01_2"));
	UAnimMontage* Follow = Anim->BeginBeatAttack(NextOrdinary, .1f);
	Step(3);
	const auto* MovingOut = Anim->GetMontageInstanceForID(LeadID);
	const auto* MovingIn = Anim->GetActiveInstanceForMontage(Follow);
	TestTrue(TEXT("Ordinary combo crossfade retains both poses"),
	         MovingOut && MovingIn && MovingOut->GetWeight() > 0.f && MovingIn->GetWeight() > 0.f);
	TestTrue(TEXT("Outgoing follow-through keeps moving during combo crossfade"),
	         MovingOut && MovingOut->GetPosition() > LeadPosition + .02f);
	Anim->BeginBeatAttack(Ordinary, .1f);
	Step(4);
	Anim->PlayCombatAction(Guard, false, .4f);
	Step(5);
	auto* GuardMontage = Anim->GetDefenseMontage();
	TestNotNull(TEXT("Guard replaces ordinary attack montage"), GuardMontage);
	TestTrue(TEXT("Guard montage time advances"),
	         GuardMontage && Anim->Montage_GetPosition(GuardMontage) > .02f);
	TestTrue(TEXT("Guard evaluates in the slot"),
	         Anim->GetSlotMontageGlobalWeight(TEXT("DefaultSlot")) > .9f);
	const FTransform GuardHand = Mesh->GetSocketTransform(TEXT("hand_r"), RTS_Component);
	TestFalse(TEXT("Guard has a real changed bone pose"), IdleHand.Equals(GuardHand, .01f));
	Anim->BeginBeatAttack(Ordinary, .1f);
	Step(4);
	Anim->PlayCombatAction(Attack, false, .62f);
	Step(6);
	auto* CounterMontage = Anim->GetDefenseMontage();
	TestTrue(TEXT("Counter montage time advances after ordinary attack"),
	         CounterMontage && Anim->Montage_GetPosition(CounterMontage) > .02f);
	TestFalse(TEXT("Counter visibly differs from guard"),
	          GuardHand.Equals(Mesh->GetSocketTransform(TEXT("hand_r"), RTS_Component), .01f));
	Anim->PlayCombatAction(Attack, false, .62f);
	UAnimMontage* First = Anim->GetCurrentActiveMontage();
	TestNotNull(TEXT("Attack uses a montage"), First);
	Step(15);
	TestTrue(TEXT("Slot evaluates the attack at full weight"),
	         Anim->GetSlotMontageGlobalWeight(TEXT("DefaultSlot")) > .95f);
	TestFalse(TEXT("Attack changes the actual hand pose from idle"),
	          IdleHand.Equals(Mesh->GetSocketTransform(TEXT("hand_r"), RTS_Component), .01f));
	const FAnimMontageInstance* FirstInstance = Anim->GetActiveInstanceForMontage(First);
	const int32 FirstInstanceID = FirstInstance ? FirstInstance->GetInstanceID() : INDEX_NONE;
	Anim->PlayCombatAction(Guard, false, .4f);
	UAnimMontage* Second = Anim->GetCurrentActiveMontage();
	Step(3);
	// 淡出蒙太奇可能已退出活动映射，但权重归零前仍参与姿态计算

	FAnimMontageInstance* Outgoing = Anim->GetMontageInstanceForID(FirstInstanceID);
	FAnimMontageInstance* Incoming = Anim->GetActiveInstanceForMontage(Second);
	TestTrue(TEXT("Attack and guard coexist during crossfade"),
	         Outgoing && Incoming && Outgoing->GetWeight() > 0.f && Incoming->GetWeight() > 0.f);
	Anim->PlayCombatAction(Roll, false, .62f, 0.f);
	Step(10);
	TestTrue(TEXT("Roll plays in the same persistent instance"), Mesh->GetAnimInstance() == Anim);
	TestTrue(TEXT("Roll slot is fully evaluated"),
	         Anim->GetSlotMontageGlobalWeight(TEXT("DefaultSlot")) > .95f);
	Anim->Montage_Stop(.18f);
	Step(15);
	TestTrue(TEXT("Action weight returns to locomotion"),
	         Anim->GetSlotMontageGlobalWeight(TEXT("DefaultSlot")) < .01f);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossDirectionalHitBlendTest, "HearBlade.Combat.BossDirectionalHitBlend",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBossDirectionalHitBlendTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ACharacter* Character = World->SpawnActor<ACharacter>();
	UBeatSyncedComboComponent* BossCombat = NewObject<UBeatSyncedComboComponent>(Character);
	TestNotNull(TEXT("Boss light metal blood effect loaded"), BossCombat->BloodHitMetalEffect.Get());
	TestNotNull(TEXT("Boss heavy metal blood effect loaded"), BossCombat->BloodHeavyMetalEffect.Get());
	Character->AddInstanceComponent(BossCombat);
	BossCombat->RegisterComponent();
	// 测试没有进入 BeginPlay，需要先恢复满血，避免直接触发转阶段
	BossCombat->BossHealth = BossCombat->MaxBossHealth;
	BossCombat->ApplyBossDamage(20.0f);
	TestEqual(TEXT("Boss damage path subtracts health"), BossCombat->BossHealth,
	          BossCombat->MaxBossHealth - 20.0f);
	TestTrue(TEXT("An idle Boss visibly reacts to damage"), BossCombat->IsHitReacting());
	USkeletalMeshComponent* Mesh = Character->GetMesh();
	Mesh->SetSkeletalMesh(LoadObject<USkeletalMesh>(
	    nullptr,
	    TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/MESHES/UE5/SK_Dragon_knight_UE5.SK_Dragon_knight_UE5")));
	Mesh->SetAnimInstanceClass(UBossLocomotionAnimInstance::StaticClass());
	UBossLocomotionAnimInstance* Anim = Cast<UBossLocomotionAnimInstance>(Mesh->GetAnimInstance());
	if (!TestNotNull(TEXT("Boss native animation instance initialized"), Anim))
	{
		World->DestroyWorld(false);
		return false;
	}
	TestNotNull(TEXT("Eight-way locomotion Blend Space loaded"), Anim->Locomotion.Get());
	TestNotNull(TEXT("Four-way directional hit Blend Space loaded"), Anim->DirectionalHit.Get());
	auto Step = [&](int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			Mesh->TickAnimation(1.f / 60.f, false);
			Mesh->RefreshBoneTransforms();
		}
	};
	Anim->SetLocomotion(0.0f, FVector::ZeroVector, 0.0f);
	Step(2);
	const FTransform IdleChest = Mesh->GetSocketTransform(TEXT("spine_03"), RTS_Component);
	Anim->StartDirectionalHit(FVector2D(-1.0f, 0.0f), .58f);
	Step(10);
	TestTrue(TEXT("Boss enters directional hit reaction"), Anim->IsDirectionalHitActive());
	TestFalse(TEXT("Directional hit changes the evaluated body pose"),
	          IdleChest.Equals(Mesh->GetSocketTransform(TEXT("spine_03"), RTS_Component), .01f));
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerEightWayTest, "HearBlade.Combat.PlayerEightWay",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerEightWayTest::RunTest(const FString& Parameters)
{
	// 先检查正式混合空间的采样数量，再验证各方向和追击使用的动画
	auto* BS = LoadObject<UBlendSpace>(nullptr, TEXT("/Game/Combat/Player/BS_PlayerEightWay"));
	if (!TestNotNull(TEXT("Eight-direction locomotion asset loads"), BS))
		return false;
	TestEqual(TEXT("Idle, eight walks, eight runs and eight pursuit samples"), BS->GetBlendSamples().Num(),
	          25);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	auto* Character = World->SpawnActor<ACharacter>();
	auto* Mesh = Character->GetMesh();
	Mesh->SetSkeletalMesh(LoadObject<USkeletalMesh>(
	    nullptr, TEXT("/Game/Characters/Dark_Knight/Dark_Knight_Male/Meshes/SKM_DKM_Full")));
	Mesh->SetAnimInstanceClass(UPlayerCombatAnimInstance::StaticClass());
	auto* Anim = Cast<UPlayerCombatAnimInstance>(Mesh->GetAnimInstance());
	auto Step = [&]()
	{
		Mesh->TickAnimation(1.f / 60.f, false);
		Mesh->RefreshBoneTransforms();
	};
	for (int32 I = 0; I < 20; ++I)
		Step();
	const FTransform IdleFoot = Mesh->GetSocketTransform(TEXT("foot_l"), RTS_Component);
	for (int32 Direction = 0; Direction < 8; ++Direction)
	{
		const FVector Point(FMath::Cos(Direction * UE_PI / 4.), FMath::Sin(Direction * UE_PI / 4.), 0);
		const FBlendSample* Sample = BS->GetBlendSamples().FindByPredicate(
		    [&](const auto& S)
		    {
			    return S.SampleValue.Equals(Point, .002);
		    });
		TestTrue(TEXT("Every run direction uses its own run clip"),
		         Sample && Sample->Animation && Sample->Animation->GetName().StartsWith(TEXT("strafe_run_")));
		Anim->LocomotionPosition = Point;
		for (int32 I = 0; I < 30; ++I)
			Step();
		const FTransform Foot = Mesh->GetSocketTransform(TEXT("foot_l"), RTS_Component);
		TestFalse(TEXT("Directional run produces an actual non-idle foot pose"), Foot.Equals(IdleFoot, .01f));
		for (int32 I = 0; I < 8; ++I)
			Step();
		TestFalse(TEXT("Run cycle continues advancing"),
		          Foot.Equals(Mesh->GetSocketTransform(TEXT("foot_l"), RTS_Component), .01f));
	}
	const auto* Pursuit = BS->GetBlendSamples().FindByPredicate(
	    [](const auto& S)
	    {
		    return S.SampleValue.Equals(FVector(1.3, 0, 0), .002);
	    });
	TestTrue(TEXT("Pursuit uses the dedicated run_fast clip"),
	         Pursuit && Pursuit->Animation && Pursuit->Animation->GetName() == TEXT("run_fast"));
	World->DestroyWorld(false);
	return !HasAnyErrors();
}
#endif
