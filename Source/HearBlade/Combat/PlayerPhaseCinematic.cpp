#include "Combat/PlayerRhythmCombatComponent.h"
#include "Combat/PlayerCombatAnimInstance.h"
#include "Combat/BossCombatCameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UPlayerRhythmCombatComponent::SetPhaseCinematic(bool bEnabled)
{
	if(bPhaseCinematic==bEnabled) return;
	if(bEnabled)
	{
		CancelDodgeForTransition(); FinishHitReaction();
		bPoiseBroken=false; bBlocking=bCountering=bBlockRecovering=bBlockReleasePending=false;
		bRecoveryStrikePending=false; BufferedAction=EBufferedRhythmAction::None;
		ActiveHoldGroup=NAME_None; bActiveHoldBroken=false;
		BlockInputHistory.Reset();CounterInputHistory.Reset();
	}
	bPhaseCinematic=bEnabled;
	if(auto* Character=Cast<ACharacter>(GetOwner()))
	{
		Character->ConsumeMovementInputVector();
		if(auto* Movement=Character->GetCharacterMovement())
		{
			if(bEnabled)
			{
				PhaseSavedMovementMode=Movement->MovementMode;PhaseSavedCustomMode=Movement->CustomMovementMode;
				Movement->StopMovementImmediately(); Movement->DisableMovement();
			}
			else Movement->SetMovementMode(EMovementMode(PhaseSavedMovementMode),PhaseSavedCustomMode);
		}
		if(!bEnabled && Character->GetMesh()->GetAnimInstance()) Character->GetMesh()->GetAnimInstance()->Montage_Stop(.2f);
	}
	if(auto* Camera=GetOwner()->FindComponentByClass<UBossCombatCameraComponent>()) Camera->StopHoldFeedback(.04f);
	UE_LOG(LogTemp,Display,TEXT("PlayerPhaseCinematic: locked=%d health=%.0f combo=%d resonance=%.1f"),bEnabled,Health,ComboCount,Resonance);
}
