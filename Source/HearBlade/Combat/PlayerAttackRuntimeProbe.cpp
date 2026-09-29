#include "Combat/PlayerBeatAttackComponent.h"
#include "Combat/PlayerRhythmCombatComponent.h"
#include "Combat/BeatSyncedComboComponent.h"
#include "Combat/BossCombatCameraComponent.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Combat/PlayerCombatAnimInstance.h"
#include "GameFramework/PlayerController.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Combat/BossBeatDirector.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Kismet/KismetSystemLibrary.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"

void UPlayerBeatAttackComponent::TickComboProbe()
{
#if !UE_BUILD_SHIPPING
	if(!Clock() || !Clock()->IsRunning() || !Combat.IsValid() || GetWorld()->GetTimeSeconds()<5.) return;
	const double Now=Clock()->GetAudioNowSeconds();
	if(FParse::Param(FCommandLine::Get(),TEXT("ComboMotionCapture")) && IsAttacking() && AttackSerial<=3)
	{
		static double NextCapture=0.; static int32 CaptureIndex=0;
		if(Now>=NextCapture)
		{
			NextCapture=Now+.1;
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/ComboMotion_%02d.png"),CaptureIndex++),false,false);
		}
	}
	const FString Routes[]={TEXT("LLL"),TEXT("LH"),TEXT("LLH"),TEXT("HHHH"),TEXT("HLLH")};
	constexpr int32 RouteCount=UE_ARRAY_COUNT(Routes);
	if(!bProbeStarted)
	{
		bProbeStarted=true;ProbeStartWorld=GetWorld()->GetTimeSeconds();Combat->ResetCombatState();
		for(TActorIterator<ABossBeatDirector> It(GetWorld());It;++It) if(It->BehaviorTreeComponent) It->BehaviorTreeComponent->StopTree(EBTStopMode::Forced);
		for(TActorIterator<AActor> It(GetWorld());It;++It) if(auto* Boss=It->FindComponentByClass<UBeatSyncedComboComponent>())
		{
			ResonanceBoss=Boss;Boss->CancelCombo();Boss->MaxBossHealth=Boss->BossHealth=20000.f;
			GetOwner()->SetActorLocation(It->GetActorLocation()+It->GetActorForwardVector()*165.f);break;
		}
		ProbeNextInputBeat=Clock()->GetBeatTimeSeconds(FMath::CeilToDouble(Clock()->GetCurrentBeat()+.3));
	}
	if(ProbeStage<RouteCount)
	{
		const FString& Route=Routes[ProbeStage];
		if(AttackSerial!=ComboProbeObservedSerial)
		{
			ComboProbeObservedSerial=AttackSerial;
			const FString Expected=Route.Left(InputProbeStep);
			if(CurrentAttack.ToString()!=Expected)
			{
				bComboProbeFailed=true;UE_LOG(LogTemp,Error,TEXT("ComboProbe: expected=%s actual=%s"),*Expected,*CurrentAttack.ToString());
			}
			// Cover an early buffered link and a late link in the real montage route.
			ProbeNextInputBeat=ComboInputAudio+(ProbeStage==0 && InputProbeStep==1?-.25:ProbeStage==1 && InputProbeStep==1?.15:0.);
			// The mashing regression runs after all routes. Extra presses during
			// a route intentionally buffer the first input and would change it.
		}
		if(InputProbeStep<Route.Len() && Now>=ProbeNextInputBeat)
		{
			if(TryAttack(Route[InputProbeStep]=='L'?EPlayerAttackInput::Light:EPlayerAttackInput::Heavy))
			{++InputProbeStep;ProbeNextInputBeat=BIG_NUMBER;}
		}
		if(InputProbeStep==Route.Len() && !IsAttacking() && ComboProbeObservedSerial==AttackSerial)
		{
			UE_LOG(LogTemp,Display,TEXT("ComboProbe: route=%s complete sequence=%s"),*Route,*InputSequence);
			++ProbeStage;InputProbeStep=0;ProbeNextInputBeat=Clock()->GetBeatTimeSeconds(FMath::CeilToDouble(Clock()->GetCurrentBeat()+1.));
			if(ProbeStage==RouteCount) {ProbeInputs=int32(AttackSerial);ArmedBeat=Clock()->GetCurrentBeat();}
		}
	}
	else
	{
		// Simulate sustained 20 Hz clicks; count actual attacks, not accepted calls.
		if(Now>=ProbeNextInputBeat) {TryAttack(EPlayerAttackInput::Light);ProbeNextInputBeat=Now+.05;}
		if(Clock()->GetCurrentBeat()-ArmedBeat>=10.)
		{
			const int32 Swings=int32(AttackSerial)-ProbeInputs;
			if(!bComboProbeFailed && Swings<=11) {UE_LOG(LogTemp,Display,TEXT("ComboProbe: PASS five routes; 20Hz burst produced %d swings in 10 beats"),Swings);}
			else {UE_LOG(LogTemp,Error,TEXT("ComboProbe: FAIL routes=%d swings=%d"),bComboProbeFailed,Swings);}
			UKismetSystemLibrary::QuitGame(this,nullptr,EQuitPreference::Quit,false);
		}
	}
	if(GetWorld()->GetTimeSeconds()-ProbeStartWorld>65.) {UE_LOG(LogTemp,Error,TEXT("ComboProbe: TIMEOUT stage=%d step=%d"),ProbeStage,InputProbeStep);UKismetSystemLibrary::QuitGame(this,nullptr,EQuitPreference::Quit,false);}
#endif
}

void UPlayerBeatAttackComponent::TickRuntimeProbe()
{
#if !UE_BUILD_SHIPPING
	if(FParse::Param(FCommandLine::Get(),TEXT("PlayerRootMotionProbe")))
	{
		if(!Clock() || !Clock()->IsRunning() || !Combat.IsValid() || GetWorld()->GetTimeSeconds()<5.) return;
		if(!bProbeStarted)
		{
			bProbeStarted=true; ProbeStartWorld=GetWorld()->GetTimeSeconds(); Combat->ResetCombatState();
			for(TActorIterator<ABossBeatDirector> It(GetWorld());It;++It) if(It->BehaviorTreeComponent) It->BehaviorTreeComponent->StopTree(EBTStopMode::Forced);
			for(TActorIterator<AActor> It(GetWorld());It;++It) if(auto* Boss=It->FindComponentByClass<UBeatSyncedComboComponent>())
			{
				Boss->CancelCombo();
				GetOwner()->SetActorLocation(It->GetActorLocation()+It->GetActorForwardVector()*500.f);
				GetOwner()->SetActorRotation((-It->GetActorForwardVector()).Rotation()); break;
			}
			RootProbeStart=GetOwner()->GetActorLocation();
			bComboProbeFailed=!TryAttack(EPlayerAttackInput::Light);
		}
		if(GetWorld()->GetTimeSeconds()-ProbeStartWorld>1.8)
		{
			const float Distance=FVector::Dist2D(RootProbeStart,GetOwner()->GetActorLocation());
			if(!bComboProbeFailed && Distance>20.f && Distance<80.f) {UE_LOG(LogTemp,Display,TEXT("RootMotionProbe: PASS continuous-slice authored travel %.1f cm; no target-distance push"),Distance);}
			else {UE_LOG(LogTemp,Error,TEXT("RootMotionProbe: FAIL started=%d travel=%.1f"),!bComboProbeFailed,Distance);}
			UKismetSystemLibrary::QuitGame(this,nullptr,EQuitPreference::Quit,false);
		}
		return;
	}
	if(FParse::Param(FCommandLine::Get(),TEXT("PlayerLocomotionProbe")))
	{
		if(GetWorld()->GetTimeSeconds()<5.) return;
		auto* C=Cast<ACharacter>(GetOwner());if(!C) return;
		if(!bProbeStarted)
		{
			bProbeStarted=true;ProbeStartWorld=GetWorld()->GetTimeSeconds();ProbeStage=-1;Combat->ResetCombatState();
			for(TActorIterator<ABossBeatDirector> It(GetWorld());It;++It) if(It->BehaviorTreeComponent) It->BehaviorTreeComponent->StopTree(EBTStopMode::Forced);
			for(TActorIterator<AActor> It(GetWorld());It;++It) if(auto* Boss=It->FindComponentByClass<UBeatSyncedComboComponent>()) {ResonanceBoss=Boss;Boss->CancelCombo();break;}
			if(auto* Camera=GetOwner()->FindComponentByClass<UBossCombatCameraComponent>();Camera && ResonanceBoss.IsValid())
			{
				// The probe stops the encounter AI; retain its explicit test lock.
				Camera->SetCombatTarget(ResonanceBoss->GetOwner());Camera->SetComponentTickEnabled(false);
			}
		}
		const double Elapsed=GetWorld()->GetTimeSeconds()-ProbeStartWorld;
		const int32 Stage=FMath::FloorToInt(Elapsed/1.2);
		if(Stage<9 && ResonanceBoss.IsValid())
		{
			if(Stage!=ProbeStage)
			{
				ProbeStage=Stage;InputProbeStep=0;C->GetCharacterMovement()->StopMovementImmediately();
				const auto* Boss=ResonanceBoss->GetOwner();C->SetActorLocation(Boss->GetActorLocation()+Boss->GetActorForwardVector()*900.f);
				C->SetActorRotation((Boss->GetActorLocation()-C->GetActorLocation()).Rotation());
			}
			const double Angle=(Stage%8)*UE_PI/4.;
			C->AddMovementInput(C->GetActorForwardVector()*FMath::Cos(Angle)+C->GetActorRightVector()*FMath::Sin(Angle),1.f,true);
			if(FMath::Fmod(Elapsed,1.2)>.65 && InputProbeStep++==0)
			{
				auto* Anim=Cast<UPlayerCombatAnimInstance>(C->GetMesh()->GetAnimInstance());
				UE_LOG(LogTemp,Display,TEXT("LocomotionProbe: direction=%d speed=%.1f max=%.1f blend=%s"),Stage,C->GetVelocity().Size2D(),C->GetCharacterMovement()->MaxWalkSpeed,Anim?*Anim->LocomotionPosition.ToString():TEXT("missing"));
				if(Stage==8 && C->GetCharacterMovement()->MaxWalkSpeed<640.f) bComboProbeFailed=true;
				if(Stage==2 || Stage==8) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/PlayerGait_%d.png"),Stage),false,false);
			}
		}
		else if(Elapsed>11.8)
		{
			if(!bComboProbeFailed && C->GetVelocity().Size2D()<5.f) {UE_LOG(LogTemp,Display,TEXT("LocomotionProbe: PASS eight directions, pursuit and stop"));}
			else {UE_LOG(LogTemp,Error,TEXT("LocomotionProbe: FAIL pursuit=%d stopSpeed=%.1f"),bComboProbeFailed,C->GetVelocity().Size2D());}
			UKismetSystemLibrary::QuitGame(this,nullptr,EQuitPreference::Quit,false);
		}
		return;
	}
	if(FParse::Param(FCommandLine::Get(),TEXT("PlayerComboProbe"))) { TickComboProbe(); return; }
	if(FParse::Param(FCommandLine::Get(),TEXT("PlayerDefenseContactProbe"))) { TickDefenseContactProbe(); return; }
	if(FParse::Param(FCommandLine::Get(),TEXT("PlayerAttackInputProbe"))) { TickInputProbe(); return; }
	if(!FParse::Param(FCommandLine::Get(),TEXT("PlayerAttackProbe")) || !Clock() || !Clock()->IsRunning() || !Combat.IsValid()) return;
	const double Beat=Clock()->GetCurrentBeat();
	if(!bProbeStarted)
	{
		if(GetWorld()->GetTimeSeconds()<5.) return;
		bProbeStarted=true;ProbeStartWorld=GetWorld()->GetTimeSeconds();
		Combat->ResetCombatState();
		for(TActorIterator<ABossBeatDirector> It(GetWorld());It;++It) if(It->BehaviorTreeComponent) It->BehaviorTreeComponent->StopTree(EBTStopMode::Forced);
		for(TActorIterator<AAIController> It(GetWorld());It;++It) if(It->BrainComponent) It->BrainComponent->StopLogic(TEXT("Player attack runtime probe"));
		for(TActorIterator<AActor> It(GetWorld());It;++It) if(auto* Boss=It->FindComponentByClass<UBeatSyncedComboComponent>())
		{
			Boss->CancelCombo();Boss->bEnableSecondPhase=false;Boss->MaxBossHealth=Boss->BossHealth=10000.f;
			ResonanceBoss=Boss;ProbeInitialBossHealth=Boss->BossHealth;
			const FVector Forward=It->GetActorForwardVector();
			GetOwner()->SetActorLocation(It->GetActorLocation()+Forward*180.f+FVector(0,0,-10));
			GetOwner()->SetActorRotation((-Forward).Rotation());
			if(auto* Camera=GetOwner()->FindComponentByClass<UBossCombatCameraComponent>()) Camera->SetCombatTarget(*It);
			break;
		}
		ProbeNextInputBeat=FMath::CeilToDouble(Beat)+1.;
		UE_LOG(LogTemp,Display,TEXT("PlayerAttackProbe: START"));
	}
	if(ProbeStage==2 && !IsResonanceActive())
	{
		UE_LOG(LogTemp,Display,TEXT("PlayerAttackProbe: COMPLETE inputs=%d resonance=%.1f"),ProbeInputs,Combat->Resonance);
		ProbeStage=3;
	}
	if(ProbeStage==3 || GetWorld()->GetTimeSeconds()-ProbeStartWorld>55.)
	{
		if(ProbeStage!=3) UE_LOG(LogTemp,Error,TEXT("PlayerAttackProbe: TIMEOUT"));
		UKismetSystemLibrary::QuitGame(this,Cast<APlayerController>(Cast<APawn>(GetOwner())->GetController()),EQuitPreference::Quit,false);
		return;
	}
	if(!ResonanceBoss.IsValid()) return;
	if(ProbeStage==0 && Beat>=ProbeNextInputBeat)
	{
		if(TryAttack(EPlayerAttackInput::Light)) ++ProbeInputs;
		ProbeNextInputBeat+=.5;
		if(ProbeInputs>=30 || Combat->Resonance>=Combat->MaxResonance)
		{
			UE_LOG(LogTemp,Display,TEXT("PlayerAttackProbe: ordinary inputs=%d damage=%.1f resonance=%.1f"),ProbeInputs,ProbeInitialBossHealth-ResonanceBoss->BossHealth,Combat->Resonance);
			if(ProbeInitialBossHealth==ResonanceBoss->BossHealth) UE_LOG(LogTemp,Error,TEXT("PlayerAttackProbe: no ordinary hits"));
			Combat->Resonance=Combat->MaxResonance;
			TryActivateResonance();ProbeStage=1;
		}
	}
	if(ProbeStage==1 && IsResonanceActive())
	{
		ProbeStage=2;ProbeInputs=0;ProbeNextInputBeat=Beat+.5;
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/PlayerResonance_Start.png"),false,false);
	}
	if(ProbeStage==2 && IsResonanceActive() && Beat>=ProbeNextInputBeat)
	{
		const TCHAR* Pattern=TEXT("LLLLH LHLH HLLH");
		FString Inputs(Pattern);Inputs.ReplaceInline(TEXT(" "),TEXT(""));
		const auto Input=Inputs[ProbeInputs%Inputs.Len()]==TCHAR('H')?EPlayerAttackInput::Heavy:EPlayerAttackInput::Light;
		if(TryAttack(Input)) ++ProbeInputs;
		ProbeNextInputBeat+=Input==EPlayerAttackInput::Heavy?1.:.5;
		if(ProbeInputs==8) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/PlayerResonance_Combo.png"),false,false);
	}
	if(ProbeStage==2 && !IsResonanceActive())
	{
		UE_LOG(LogTemp,Display,TEXT("PlayerAttackProbe: COMPLETE inputs=%d resonance=%.1f damage=%.1f"),ProbeInputs,Combat->Resonance,ProbeInitialBossHealth-ResonanceBoss->BossHealth);
		ProbeStage=3;
	}
	if(ProbeStage==3 || GetWorld()->GetTimeSeconds()-ProbeStartWorld>55.)
	{
		if(ProbeStage!=3) UE_LOG(LogTemp,Error,TEXT("PlayerAttackProbe: TIMEOUT"));
		UKismetSystemLibrary::QuitGame(this,Cast<APlayerController>(Cast<APawn>(GetOwner())->GetController()),EQuitPreference::Quit,false);
	}
#endif
}

void UPlayerBeatAttackComponent::TickInputProbe()
{
	// Compatibility alias for the current combo/cadence regression.
	TickComboProbe();
}
