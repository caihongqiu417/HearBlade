#include "Input/HearBladeInputComponent.h"
#include "HearBladeCharacter.h"
#include "HearBlade.h"
#include "Combat/Player/PlayerRhythmCombatComponent.h"
#include "Combat/Player/PlayerBeatAttackComponent.h"
#include "Combat/Camera/BossCombatCameraComponent.h"
#include "Combat/Data/RhythmCombatInputSettings.h"
#include "UI/HUD/RhythmCombatHUD.h"
#include "UI/Controller/RhythmWidgetController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

UHearBladeInputComponent::UHearBladeInputComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// 临时动作只创建一次，按键继续读取项目设置中的原配置
void UHearBladeInputComponent::CreateCombatActions()
{
    // 重新取得角色控制权时复用已有动作，避免重复创建映射
    if (DefenseMapping) return;
    const auto MakeAction = [this](const TCHAR* Name)
    {
        auto* Action = NewObject<UInputAction>(this, Name);
        Action->ValueType = EInputActionValueType::Boolean;
        return Action;
    };
    LockOnAction = MakeAction(TEXT("BossLockOnAction"));
    DodgeAction = MakeAction(TEXT("RhythmDodgeAction"));
    BlockAction = MakeAction(TEXT("RhythmBlockAction"));
    CounterAction = MakeAction(TEXT("RhythmCounterAction"));
    LightAttackAction = MakeAction(TEXT("LightAttackAction"));
    HeavyAttackAction = MakeAction(TEXT("HeavyAttackAction"));
    ResonanceAction = MakeAction(TEXT("ResonanceAction"));
    MousePrimaryCombatAction = MakeAction(TEXT("MousePrimaryCombat"));
    MouseSecondaryCombatAction = MakeAction(TEXT("MouseSecondaryCombat"));
    RestartAction = MakeAction(TEXT("RestartAction"));
    // 锁定、闪避和攻防分别组织映射，绑定与解绑时成组处理
    LockOnMapping = NewObject<UInputMappingContext>(this, TEXT("BossLockOnMapping"));
    DodgeMapping = NewObject<UInputMappingContext>(this, TEXT("RhythmDodgeMapping"));
    DefenseMapping = NewObject<UInputMappingContext>(this, TEXT("RhythmDefenseMapping"));
    const auto* Keys = GetDefault<URhythmCombatInputSettings>();
    LockOnMapping->MapKey(LockOnAction, EKeys::MiddleMouseButton);
    LockOnMapping->MapKey(LockOnAction, EKeys::Gamepad_RightThumbstick);
    DodgeMapping->MapKey(DodgeAction, Keys->DodgePrimary);
    DodgeMapping->MapKey(DodgeAction, Keys->DodgeSecondary);
    DodgeMapping->MapKey(DodgeAction, Keys->DodgeGamepad);

    // 鼠标左右键先判断攻防窗口，避免一次按键同时触发攻击和防御
    DefenseMapping->MapKey(MousePrimaryCombatAction, EKeys::LeftMouseButton);
    DefenseMapping->MapKey(MouseSecondaryCombatAction, EKeys::RightMouseButton);
    DefenseMapping->MapKey(LightAttackAction, Keys->LightAttackGamepad);
    DefenseMapping->MapKey(HeavyAttackAction, EKeys::F);
    DefenseMapping->MapKey(HeavyAttackAction, Keys->HeavyAttackGamepad);
    DefenseMapping->MapKey(ResonanceAction, Keys->ResonanceKey);
    DefenseMapping->MapKey(ResonanceAction, Keys->ResonanceGamepad);
    DefenseMapping->MapKey(BlockAction, Keys->BlockGamepad);
    // 鼠标已由攻防分派处理，单独弹反只绑定其他按键，避免一次按键执行两遍
    if (!Keys->CounterPrimary.IsMouseButton()) DefenseMapping->MapKey(CounterAction, Keys->CounterPrimary);
    if (Keys->CounterSecondary != Keys->CounterPrimary && !Keys->CounterSecondary.IsMouseButton())
        DefenseMapping->MapKey(CounterAction, Keys->CounterSecondary);
    DefenseMapping->MapKey(CounterAction, Keys->CounterGamepad);
    DefenseMapping->MapKey(RestartAction, EKeys::Enter);
}

void UHearBladeInputComponent::Bind(UInputComponent* Input, UInputAction* Jump, UInputAction* Move,
                                  UInputAction* Look, UInputAction* MouseLook)
{
    if (!Input || BoundInput.Get() == Input) return;
    // 更换输入组件前先清理旧绑定和按住状态
    Unbind();
    Character = Cast<AHearBladeCharacter>(GetOwner());
    auto* Enhanced = Cast<UEnhancedInputComponent>(Input);
    if (!Character || !Enhanced)
    {
        UE_LOG(LogHearBlade, Error, TEXT("%s：角色输入需要 EnhancedInputComponent"), *GetNameSafe(GetOwner()));
        return;
    }
    // 保存操作要调用的组件，按键触发时直接转交
    RhythmCombat = Character->FindComponentByClass<UPlayerRhythmCombatComponent>();
    BeatAttack = Character->FindComponentByClass<UPlayerBeatAttackComponent>();
    CombatCamera = Character->FindComponentByClass<UBossCombatCameraComponent>();
    BoundInput = Input;
    CreateCombatActions();
    // 记录每次绑定的句柄，失去控制时只删除自己的回调
    const auto BindAction = [this, Enhanced](UInputAction* Action, ETriggerEvent Event, auto Callback)
    {
        if (Action) BindingHandles.Add(Enhanced->BindAction(Action, Event, this, Callback).GetHandle());
    };
    // 基础操作沿用角色蓝图的 IA，跳跃区分按下和松开
    BindAction(Jump, ETriggerEvent::Started, &UHearBladeInputComponent::JumpStarted);
    BindAction(Jump, ETriggerEvent::Completed, &UHearBladeInputComponent::JumpEnded);
    // 移动松开或取消时也发送零输入，避免保留上一次方向
    BindAction(Move, ETriggerEvent::Triggered, &UHearBladeInputComponent::Move);
    BindAction(Move, ETriggerEvent::Completed, &UHearBladeInputComponent::Move);
    BindAction(Move, ETriggerEvent::Canceled, &UHearBladeInputComponent::Move);
    BindAction(MouseLook, ETriggerEvent::Triggered, &UHearBladeInputComponent::Look);
    BindAction(Look, ETriggerEvent::Triggered, &UHearBladeInputComponent::Look);

    // 战斗按键只转交意图，动作条件和输入缓冲仍由战斗组件处理
    BindAction(LockOnAction, ETriggerEvent::Started, &UHearBladeInputComponent::ToggleCombatLock);
    BindAction(DodgeAction, ETriggerEvent::Started, &UHearBladeInputComponent::Dodge);
    // 鼠标需要区分攻防窗口，右键还需要记录按住和松开
    BindAction(MousePrimaryCombatAction, ETriggerEvent::Started, &UHearBladeInputComponent::MousePrimaryCombatPressed);
    BindAction(MouseSecondaryCombatAction, ETriggerEvent::Completed, &UHearBladeInputComponent::MouseSecondaryCombatReleased);
    BindAction(MouseSecondaryCombatAction, ETriggerEvent::Canceled, &UHearBladeInputComponent::MouseSecondaryCombatReleased);
    BindAction(MouseSecondaryCombatAction, ETriggerEvent::Started, &UHearBladeInputComponent::MouseSecondaryCombatPressed);
    // 手柄和独立按键直接发起对应动作，格挡松开时结束防御
    BindAction(LightAttackAction, ETriggerEvent::Started, &UHearBladeInputComponent::LightAttack);
    BindAction(HeavyAttackAction, ETriggerEvent::Started, &UHearBladeInputComponent::HeavyAttack);
    BindAction(ResonanceAction, ETriggerEvent::Started, &UHearBladeInputComponent::ActivateResonance);
    BindAction(BlockAction, ETriggerEvent::Started, &UHearBladeInputComponent::BlockStarted);
    BindAction(BlockAction, ETriggerEvent::Completed, &UHearBladeInputComponent::BlockEnded);
    BindAction(BlockAction, ETriggerEvent::Canceled, &UHearBladeInputComponent::BlockEnded);
    BindAction(CounterAction, ETriggerEvent::Started, &UHearBladeInputComponent::Counter);
    BindAction(RestartAction, ETriggerEvent::Started, &UHearBladeInputComponent::RestartPressed);

    // 回调绑定完成后，再启用本机玩家的按键映射
    if (auto* Player = Cast<APlayerController>(Character->GetController()))
    {
        InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Player->GetLocalPlayer());
        if (InputSubsystem.IsValid())
        {
            InputSubsystem->AddMappingContext(LockOnMapping, 1);
            InputSubsystem->AddMappingContext(DodgeMapping, 2);
            InputSubsystem->AddMappingContext(DefenseMapping, 2);
        }
    }
}

void UHearBladeInputComponent::Move(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    if (Character) Character->DoMove(Axis.X, Axis.Y);
}

void UHearBladeInputComponent::Look(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    if (Character) Character->DoLook(Axis.X, Axis.Y);
}

// 保持原来的跳跃绑定，触控入口仍使用角色的 DoJumpStart 和 DoJumpEnd
void UHearBladeInputComponent::JumpStarted() { if (Character) Character->Jump(); }
void UHearBladeInputComponent::JumpEnded() { if (Character) Character->StopJumping(); }

void UHearBladeInputComponent::ToggleCombatLock()
{
	if (CombatCamera)
		CombatCamera->ToggleLockOn();
}

void UHearBladeInputComponent::Dodge()
{
	if (Character) Character->DoDodge();
}

void UHearBladeInputComponent::BlockStarted()
{
	if (RhythmCombat) bBlockHeld = RhythmCombat->TryBlockStart();
}

void UHearBladeInputComponent::BlockEnded()
{
    bBlockHeld = false;
	if (RhythmCombat)
		RhythmCombat->EndBlock();
}

void UHearBladeInputComponent::Counter()
{
	if (RhythmCombat)
		RhythmCombat->TryCounter();
}

void UHearBladeInputComponent::LightAttack()
{
	if (BeatAttack)
		BeatAttack->TryAttack(EPlayerAttackInput::Light);
}

void UHearBladeInputComponent::MousePrimaryCombatPressed()
{
	const bool Defense = RhythmCombat && RhythmCombat->IsMouseDefenseWindow();
	UE_LOG(LogTemp, Display, TEXT("MouseCombat: LMB -> %s"), Defense ? TEXT("COUNTER") : TEXT("LIGHT"));
	if (Defense)
		Counter();
	else
		LightAttack();
}

void UHearBladeInputComponent::MouseSecondaryCombatReleased()
{
	// 只有右键确实启动过格挡，松开时才结束格挡
	if (bMouseSecondaryOwnsBlock && RhythmCombat)
		RhythmCombat->EndBlock();
	bMouseSecondaryOwnsBlock = false;
	bMouseSecondaryHeld = false;
}

void UHearBladeInputComponent::MouseSecondaryCombatPressed()
{
	const bool Defense = RhythmCombat && RhythmCombat->IsMouseDefenseWindow();
	UE_LOG(LogTemp, Display, TEXT("MouseCombat: RMB -> %s"), Defense ? TEXT("BLOCK") : TEXT("HEAVY"));
	bMouseSecondaryOwnsBlock = false;
	bMouseSecondaryHeld = true;
	if (Defense)
		bMouseSecondaryOwnsBlock = RhythmCombat->TryBlockStart();
	else
		HeavyAttack();
}

void UHearBladeInputComponent::HeavyAttack()
{
	if (BeatAttack)
		BeatAttack->TryAttack(EPlayerAttackInput::Heavy);
}

void UHearBladeInputComponent::ActivateResonance()
{
	if (BeatAttack)
		BeatAttack->TryActivateResonance();
}


// 提前按住右键时，防御窗口出现后接续格挡
void UHearBladeInputComponent::UpdateHeldActions()
{
    if (bMouseSecondaryHeld && !bMouseSecondaryOwnsBlock && RhythmCombat && RhythmCombat->IsMouseDefenseWindow())
        bMouseSecondaryOwnsBlock = RhythmCombat->TryBlockStart();
}

void UHearBladeInputComponent::RestartPressed()
{
    if (!Character) return;
    auto* Player = Cast<APlayerController>(Character->GetController());
    auto* HUD = Player ? Cast<ARhythmCombatHUD>(Player->GetHUD()) : nullptr;
    if (HUD)
        if (auto* Controller = HUD->GetWidgetController()) Controller->RestartLevel();
}

void UHearBladeInputComponent::Unbind()
{
	// 先结束按住操作，避免重新控制角色后仍保持格挡或跳跃
    if ((bBlockHeld || bMouseSecondaryOwnsBlock) && RhythmCombat) RhythmCombat->EndBlock();
    bBlockHeld = bMouseSecondaryOwnsBlock = bMouseSecondaryHeld = false;
    if (Character)
    {
        Character->DoMove(0.f, 0.f);
        Character->StopJumping();
    }
    // 只清除本组件登记的回调，保留其他系统的输入
    if (auto* Enhanced = Cast<UEnhancedInputComponent>(BoundInput.Get()))
        for (uint32 Handle : BindingHandles) Enhanced->RemoveBindingByHandle(Handle);
    BindingHandles.Reset();
    BoundInput.Reset();
    // 使用绑定时保存的子系统，角色失去控制后也能移除原来的映射
    if (InputSubsystem.IsValid())
    {
        InputSubsystem->RemoveMappingContext(LockOnMapping);
        InputSubsystem->RemoveMappingContext(DodgeMapping);
        InputSubsystem->RemoveMappingContext(DefenseMapping);
    }
    InputSubsystem.Reset();
}

void UHearBladeInputComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    Unbind();
    Super::EndPlay(Reason);
}
