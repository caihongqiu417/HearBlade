// 处理阶段过场期间的输入、移动和战斗状态锁定
#include "Combat/Player/PlayerDefenseComponent.h"
#include "Combat/Player/PlayerCombatAnimInstance.h"
#include "Combat/Camera/BossCombatCameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"

// 切换演出状态，进入时清理未完成的攻击与防御操作
void UPlayerDefenseComponent::SetPhaseCinematic(bool bEnabled)
{
	if (bPhaseCinematic == bEnabled)
		return;
	// 进入过场时清除进行中的防御和缓冲输入，避免结束后执行旧操作
	if (bEnabled)
	{
		CancelDodgeForTransition();
		FinishHitReaction();
		bPoiseBroken = false;
		bBlocking = bCountering = bBlockRecovering = bBlockReleasePending = false;
		bRecoveryStrikePending = false;
		BufferedAction = EBufferedDefenseAction::None;
		ActiveHoldGroup = NAME_None;
		bActiveHoldBroken = false;
		BlockInputHistory.Reset();
		CounterInputHistory.Reset();
	}
	bPhaseCinematic = bEnabled;
	if (auto* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->ConsumeMovementInputVector();
		if (auto* Movement = Character->GetCharacterMovement())
		{
			if (bEnabled)
			{
				// 保存原移动模式及自定义模式，退出过场时恢复，不固定假设角色处于行走状态
				PhaseSavedMovementMode = Movement->MovementMode;
				PhaseSavedCustomMode = Movement->CustomMovementMode;
				Movement->StopMovementImmediately();
				Movement->DisableMovement();
			}
			else
				Movement->SetMovementMode(EMovementMode(PhaseSavedMovementMode), PhaseSavedCustomMode);
		}
		if (!bEnabled && Character->GetMesh()->GetAnimInstance())
			Character->GetMesh()->GetAnimInstance()->Montage_Stop(.2f);
	}
	if (auto* Camera = GetOwner()->FindComponentByClass<UBossCombatCameraComponent>())
		Camera->StopHoldFeedback(.04f);
	UE_LOG(LogTemp, Display, TEXT("PlayerPhaseCinematic: locked=%d health=%.0f combo=%d resonance=%.1f"),
	       bEnabled, Health, ComboCount, Resonance);
}
