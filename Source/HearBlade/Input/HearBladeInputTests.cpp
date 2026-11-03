// 验证重复绑定、失去控制和重新控制时的输入生命周期
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Input/HearBladeInputComponent.h"
#include "HearBladeCharacter.h"
#include "Combat/Player/PlayerDefenseComponent.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearBladeInputLifecycleTest, "HearBlade.Input.Lifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHearBladeInputLifecycleTest::RunTest(const FString&)
{
    // 使用独立世界创建正式角色蓝图，测试结束时统一销毁
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    ON_SCOPE_EXIT { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); };
    UClass* Type = LoadClass<AHearBladeCharacter>(nullptr,
        TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
    if (!TestNotNull(TEXT("正式角色蓝图加载"), Type)) return false;
    auto* Character = World->SpawnActor<AHearBladeCharacter>(Type);
    auto* Player = World->SpawnActor<APlayerController>();
    Player->Possess(Character);
    auto* Input = Character->FindComponentByClass<UHearBladeInputComponent>();
    if (!TestNotNull(TEXT("角色包含输入组件"), Input)) return false;
    auto* Bindings = NewObject<UEnhancedInputComponent>(Character);
    auto* Move = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Move.IA_Move"));
    Input->Bind(Bindings, nullptr, Move, nullptr, nullptr);
    const int32 Count = Bindings->GetActionEventBindings().Num();
    Input->Bind(Bindings, nullptr, Move, nullptr, nullptr);
    TestEqual(TEXT("重复初始化不重复注册回调"), Bindings->GetActionEventBindings().Num(), Count);
    TestEqual(TEXT("所有操作都通过 IA 绑定"), Bindings->KeyBindings.Num(), 0);
    const auto HasKey = [](const UInputMappingContext* Mapping, const UInputAction* Action, FKey Key)
    {
        return Mapping->GetMappings().ContainsByPredicate([&](const FEnhancedActionKeyMapping& Item)
            { return Item.Action == Action && Item.Key == Key; });
    };
    TestTrue(TEXT("左键保留攻防分派"), HasKey(Input->DefenseMapping, Input->MousePrimaryCombatAction, EKeys::LeftMouseButton));
    TestTrue(TEXT("右键保留攻防分派"), HasKey(Input->DefenseMapping, Input->MouseSecondaryCombatAction, EKeys::RightMouseButton));
    TestTrue(TEXT("回车重开使用 IA"), HasKey(Input->DefenseMapping, Input->RestartAction, EKeys::Enter));
    TestTrue(TEXT("保留 F 重击"), HasKey(Input->DefenseMapping, Input->HeavyAttackAction, EKeys::F));
    TestTrue(TEXT("保留手柄锁定"), HasKey(Input->LockOnMapping, Input->LockOnAction, EKeys::Gamepad_RightThumbstick));

    // 解除输入时只删除自己的回调，保留其他系统登记的回调
    auto* ExternalAction = NewObject<UInputAction>();
    Bindings->BindAction(ExternalAction, ETriggerEvent::Started, Character, &ACharacter::Jump);
    Input->bMouseSecondaryHeld = true;
    Player->UnPossess();
    TestFalse(TEXT("失去控制后清除右键按住状态"), Input->bMouseSecondaryHeld);
    TestEqual(TEXT("只留下外部注册的回调"), Bindings->GetActionEventBindings().Num(), 1);
    TestFalse(TEXT("解除旧输入组件引用"), Input->BoundInput.IsValid());
    Player->Possess(Character);
    Input->Bind(Bindings, nullptr, Move, nullptr, nullptr);
    TestEqual(TEXT("重新取得控制后恢复绑定"), Bindings->GetActionEventBindings().Num(), Count + 1);
    Input->Unbind();
    return true;
}
#endif
