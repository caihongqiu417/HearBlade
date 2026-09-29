#include "Combat/BeatSyncedComboComponent.h"
#include "Combat/PlayerRhythmCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "Misc/CommandLine.h"
#include "Components/BoxComponent.h"

bool UBeatSyncedComboComponent::IsGroundBurstRecovery() const
{
	const auto* Clock=GetWorld()?GetWorld()->GetSubsystem<UBeatClockSubsystem>():nullptr;
	return BossHealth>0.f && Clock && Clock->IsRunning() && Clock->GetAudioNowSeconds()<GroundBurstRecoveryEndAudio;
}

float UBeatSyncedComboComponent::GetGroundBurstRemainingBeats() const
{
	const auto* Clock=GetWorld()?GetWorld()->GetSubsystem<UBeatClockSubsystem>():nullptr;
	return Clock && GroundBurstImpact!=INDEX_NONE && !bGroundBurstResolved && !bGroundBurstHarmless
		? FMath::Max(0.,(GroundBurstTargetAudio-Clock->GetAudioNowSeconds())/Clock->GetSecondsPerBeat()) : -1.f;
}

bool UBeatSyncedComboComponent::TryRecoveryStrike(AActor* Player)
{
	if(!Player || !IsGroundBurstRecovery()) return false;
	const auto* Clock=GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	const double Now=Clock->GetAudioNowSeconds();
	if(Now-LastRecoveryStrikeAudio<Clock->GetSecondsPerBeat()*.75) return false;
	const FVector Offset=GetOwner()->GetActorLocation()-Player->GetActorLocation();
	if(Offset.Size2D()>300.f || FMath::Abs(Offset.Z)>160.f || FVector::DotProduct(Player->GetActorForwardVector(),Offset.GetSafeNormal2D())<.35f) return false;
	FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(RecoveryStrike),false,Player);
	Params.AddIgnoredActor(GetOwner());
	if(GetWorld()->LineTraceSingleByChannel(Hit,Player->GetActorLocation(),GetOwner()->GetActorLocation(),ECC_Visibility,Params)) return false;
	LastRecoveryStrikeAudio=Now;
	ApplyBossDamageFrom(CounterDamage*.5f,Player);
	UE_LOG(LogTemp,Display,TEXT("GroundBurstRecovery: strike distance=%.1f damage=%.1f resonance=8"),Offset.Size2D(),CounterDamage*.5f);
	return true;
}

bool UBeatSyncedComboComponent::HasSafeAreaExit(const FVector& Center, float Radius, const AActor* Target, double AvailableSeconds) const
{
	if (!Target || !GetWorld()) return false;
	const UCapsuleComponent* Capsule = Target->FindComponentByClass<UCapsuleComponent>();
	const float R = Capsule ? Capsule->GetScaledCapsuleRadius() : 42.f;
	const float H = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.f;
	if (FVector::DistSquared2D(Target->GetActorLocation(),Center)>FMath::Square(Radius+R+5.f)) return true;
	const ACharacter* Character=Cast<ACharacter>(Target);
	const UCharacterMovementComponent* Movement=Character?Character->GetCharacterMovement():nullptr;
	// Defensive animations temporarily use MOVE_None. Predict the walking speed
	// available after recovery, not the instantaneous disabled movement mode.
	const float Speed=Movement?Movement->MaxWalkSpeed:200.f;
	if (AvailableSeconds<0.)
	{
		const auto* Clock=GetWorld()->GetSubsystem<UBeatClockSubsystem>();
		AvailableSeconds=Clock?Clock->GetSecondsPerBeat()*4.:2.;
	}
	// Leave a readable reaction margin; do not assume an instant full-speed dodge.
	const auto* Combat=Target->FindComponentByClass<UPlayerRhythmCombatComponent>();
	const double TravelBudget=FMath::Max(0.,AvailableSeconds-.55)*Speed+(Combat?Combat->GetEscapeDodgeBonus(Speed,AvailableSeconds):0.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GroundBurstExit), false, Target);
	Params.AddIgnoredActor(GetOwner());
	int32 TimeRejected=0,GroundRejected=0,ObstacleRejected=0;
	for (int32 I=0; I<16; ++I)
	{
		const float A = I * UE_TWO_PI / 16.f;
		const FVector End = Center + FVector(FMath::Cos(A), FMath::Sin(A), 0) * (Radius+R+60.f);
		const float Distance=FVector::Dist2D(Target->GetActorLocation(),End);
		if (Distance>TravelBudget) {++TimeRejected;continue;}
		const int32 Steps=FMath::Max(1,FMath::CeilToInt(Distance/60.f));
		FVector Previous=Target->GetActorLocation(); bool bSafe=true;
		for(int32 Step=1;Step<=Steps;++Step)
		{
			const FVector Sample=FMath::Lerp(Target->GetActorLocation(),End,float(Step)/Steps);
			FHitResult Ground,Obstacle;
			const float FloorZ=Previous.Z-H;
			const FVector Top(Sample.X,Sample.Y,FloorZ+100.f),Bottom(Sample.X,Sample.Y,FloorZ-100.f);
			if (!GetWorld()->LineTraceSingleByChannel(Ground,Top,Bottom,ECC_Visibility,Params)||
				Ground.ImpactNormal.Z<(Movement?Movement->GetWalkableFloorZ():.7f)||
				FMath::Abs(Ground.ImpactPoint.Z-FloorZ)>(Movement?Movement->MaxStepHeight:45.f)) {++GroundRejected;bSafe=false;break;}
			const FVector Safe=Ground.ImpactPoint+FVector(0,0,H+3.f);
			if(GetWorld()->SweepSingleByChannel(Obstacle,Previous,Safe,FQuat::Identity,ECC_Pawn,
				FCollisionShape::MakeCapsule(R,H-2.f),Params)) {++ObstacleRejected;bSafe=false;break;}
			Previous=Safe;
		}
		if(bSafe) return true;
	}
	UE_LOG(LogTemp,Display,TEXT("GroundBurstExit: radius=%.0f seconds=%.3f speed=%.1f budget=%.1f time=%d ground=%d obstacle=%d"),Radius,AvailableSeconds,Speed,TravelBudget,TimeRejected,GroundRejected,ObstacleRejected);
	return false;
}

void UBeatSyncedComboComponent::ClearGroundBurst()
{
	GroundBurstImpact = INDEX_NONE;
	bGroundBurstResolved = false;
	GroundBurstExitAudio = -1.0;
	bGroundBurstWasInside = false;
	GroundBurstBoundaryCrossings=0;
	ActiveGroundBurstRadius=0.f;
	bGroundBurstHarmless=false;
}

bool UBeatSyncedComboComponent::GetGroundBurstPreview(FVector& Center, float& Radius, float& Progress) const
{
	if (GroundBurstImpact == INDEX_NONE || bGroundBurstResolved || !AttackData) return false;
	Center = GroundBurstCenter;
	Radius = ActiveGroundBurstRadius;
	if(bGroundBurstHarmless) return false;
	Progress = FMath::Clamp(static_cast<float>((GroundBurstPreviousAudio-GroundBurstStartAudio) /
		FMath::Max(.01,GroundBurstTargetAudio-GroundBurstStartAudio)),0.f,1.f);
	return true;
}

void UBeatSyncedComboComponent::UpdateGroundBurst(double AudioNow)
{
	if (!AttackData || !AttackTarget.IsValid()) return;
	if (GroundBurstImpact == INDEX_NONE)
	{
		for (int32 I=0; I<GetImpactCount(); ++I)
		{
			if (GetImpactMarker(I).DamageShape != EBossAttackDamageShape::GroundBurst) continue;
			const double Start = ScheduledStartAudioSeconds + AttackData->GetElapsedAtAnimationTime(I>0 ? GetImpactTime(I-1) : AttackData->ClipStartSeconds)
				- AttackData->GetElapsedAtAnimationTime(AttackData->ClipStartSeconds);
			if (AudioNow < Start) return;
			GroundBurstCenter = GetOwner()->GetActorLocation();
			if (const UCapsuleComponent* C = GetOwner()->FindComponentByClass<UCapsuleComponent>()) GroundBurstCenter.Z -= C->GetScaledCapsuleHalfHeight();
			const double TargetAudio=ScheduledStartAudioSeconds+AttackData->GetElapsedAtAnimationTime(GetImpactTime(I))-AttackData->GetElapsedAtAnimationTime(AttackData->ClipStartSeconds);
			const auto* Clock=GetWorld()->GetSubsystem<UBeatClockSubsystem>();
			const double RequiredWarning=Clock?Clock->GetSecondsPerBeat()*GetImpactMarker(I).GroundBurst.WarningBeats:0.;
#if !UE_BUILD_SHIPPING
			if(!bGroundBurstTestInjected&&FParse::Param(FCommandLine::Get(),TEXT("GroundBurstBlockedTest")))
			{
				bGroundBurstTestInjected=true;
				for(int32 Wall=0;Wall<4;++Wall)
				{
					auto* Barrier=GetWorld()->SpawnActor<AActor>();
					auto* Box=NewObject<UBoxComponent>(Barrier);Barrier->SetRootComponent(Box);
					Box->SetBoxExtent(Wall<2?FVector(10,130,180):FVector(130,10,180));
					Box->SetCollisionProfileName(TEXT("BlockAll"));Box->RegisterComponent();
					const FVector Offset=Wall<2?FVector(Wall==0?110:-110,0,0):FVector(0,Wall==2?110:-110,0);
					Barrier->SetActorLocation(AttackTarget->GetActorLocation()+Offset);
					Barrier->SetLifeSpan(12.f);
				}
				UE_LOG(LogTemp,Display,TEXT("GroundBurstTest: temporary escape blockers injected"));
			}
#endif
			ActiveGroundBurstRadius=GetImpactMarker(I).GroundBurst.RadiusCm;
			const bool bEnoughWarning=TargetAudio-Start+.001>=RequiredWarning;
			// With dodge-through, the dodge itself is the exit: keep the full radius.
			bool bExit=bGroundBurstDodgeThrough || HasSafeAreaExit(GroundBurstCenter,ActiveGroundBurstRadius,AttackTarget.Get(),TargetAudio-AudioNow);
			while(!bExit && ActiveGroundBurstRadius>300.f)
			{
				ActiveGroundBurstRadius=FMath::Max(300.f,ActiveGroundBurstRadius-40.f);
				bExit=HasSafeAreaExit(GroundBurstCenter,ActiveGroundBurstRadius,AttackTarget.Get(),TargetAudio-AudioNow);
			}
			if (!bEnoughWarning || !bExit)
			{
				bGroundBurstHarmless=true;
				ActiveGroundBurstRadius=0.f;
				UE_LOG(LogTemp, Warning, TEXT("GroundBurst: harmless finish warningSeconds=%.3f required=%.3f safeExit=%d"),TargetAudio-Start,RequiredWarning,bExit);
			}
			GroundBurstImpact=I; GroundBurstStartAudio=Start;
			GroundBurstTargetAudio=ScheduledStartAudioSeconds+AttackData->GetElapsedAtAnimationTime(GetImpactTime(I))-AttackData->GetElapsedAtAnimationTime(AttackData->ClipStartSeconds);
			GroundBurstPreviousLocation=AttackTarget->GetActorLocation(); GroundBurstPreviousAudio=AudioNow;
			const UCapsuleComponent* InitialCapsule=AttackTarget->FindComponentByClass<UCapsuleComponent>();
			const float InitialBoundary=ActiveGroundBurstRadius+(InitialCapsule?InitialCapsule->GetScaledCapsuleRadius():42.f);
			bGroundBurstWasInside=FVector::DistSquared2D(GroundBurstPreviousLocation,GroundBurstCenter)<=FMath::Square(InitialBoundary);
			ClearWarping();
			UE_LOG(LogTemp, Display, TEXT("GroundBurst: warning center=%s radius=%.0f authoredRadius=%.0f seconds=%.3f targetAudio=%.3f harmless=%d"), *GroundBurstCenter.ToString(),ActiveGroundBurstRadius,GetImpactMarker(I).GroundBurst.RadiusCm,TargetAudio-Start,GroundBurstTargetAudio,bGroundBurstHarmless);
			break;
		}
	}
	if (GroundBurstImpact == INDEX_NONE || bGroundBurstResolved) return;
#if !UE_BUILD_SHIPPING
	if(!bGroundBurstTestInjected&&AudioNow-GroundBurstStartAudio>1. && FParse::Param(FCommandLine::Get(),TEXT("GroundBurstDeathTest")))
	{
		bGroundBurstTestInjected=true;
		ApplyBossDamage(MaxBossHealth*2.f);
		UE_LOG(LogTemp,Display,TEXT("GroundBurstTest: killed boss during warning, activeArea=%d playing=%d"),GroundBurstImpact,bPlaying);
		return;
	}
	if(!bGroundBurstTestInjected&&GroundBurstTargetAudio-AudioNow<.1 && GroundBurstTargetAudio>AudioNow && FParse::Param(FCommandLine::Get(),TEXT("GroundBurstHitchTest")))
	{
		bGroundBurstTestInjected=true;
		UE_LOG(LogTemp,Display,TEXT("GroundBurstTest: injecting 250ms game-thread hitch before impact"));
		FPlatformProcess::Sleep(.25f);
	}
#endif
	const FVector Current=AttackTarget->GetActorLocation();
	const UCapsuleComponent* Capsule=AttackTarget->FindComponentByClass<UCapsuleComponent>();
	const float Boundary=ActiveGroundBurstRadius+(Capsule?Capsule->GetScaledCapsuleRadius():42.f);
	const FVector2D From(GroundBurstPreviousLocation-GroundBurstCenter), Delta(Current-GroundBurstPreviousLocation);
	const bool Inside=FVector::DistSquared2D(Current,GroundBurstCenter)<=FMath::Square(Boundary);
	const bool WasInside=From.SizeSquared()<=FMath::Square(Boundary);
	if(Inside!=WasInside) ++GroundBurstBoundaryCrossings;
	if (WasInside && !Inside && Delta.SizeSquared()>UE_SMALL_NUMBER)
	{
		const double A=Delta.SizeSquared(), B=2.*FVector2D::DotProduct(From,Delta), C=From.SizeSquared()-FMath::Square(Boundary);
		const double T=FMath::Clamp((-B+FMath::Sqrt(FMath::Max(0.,B*B-4.*A*C)))/(2.*A),0.,1.);
		GroundBurstExitAudio=FMath::Lerp(GroundBurstPreviousAudio,AudioNow,T);
	}
	if (Inside) GroundBurstExitAudio=-1.0;
	// Reward eligibility is fixed when the warning starts. Entering a safe area
	// deliberately, or repeatedly crossing its edge, must not manufacture rewards.
	// Retain the bracketing sample when this frame crosses the damage timestamp.
	if (AudioNow < GroundBurstTargetAudio) { GroundBurstPreviousLocation=Current; GroundBurstPreviousAudio=AudioNow; }
}

void UBeatSyncedComboComponent::ResolveGroundBurst(int32 ImpactIndex, double AudioNow)
{
	if (GroundBurstImpact != ImpactIndex || bGroundBurstResolved || !AttackTarget.IsValid()) return;
	bGroundBurstResolved=true;
	const auto* Clock=GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	GroundBurstRecoveryEndAudio=FMath::Max(AudioNow,ScheduledStartAudioSeconds+AttackData->GetElapsedAtAnimationTime(AttackData->GetClipEndSeconds())-AttackData->GetElapsedAtAnimationTime(AttackData->ClipStartSeconds))+(Clock?Clock->GetSecondsPerBeat():.577)*GroundBurstRecoveryBeats;
	UE_LOG(LogTemp,Display,TEXT("GroundBurstRecovery: open untilAudio=%.3f"),GroundBurstRecoveryEndAudio);
	if(bGroundBurstHarmless) return;
	const FAttackImpactMarker& Marker=GetImpactMarker(ImpactIndex);
	const UCapsuleComponent* Capsule=AttackTarget->FindComponentByClass<UCapsuleComponent>();
	const float Boundary=ActiveGroundBurstRadius+(Capsule?Capsule->GetScaledCapsuleRadius():42.f);
	const double T=FMath::Clamp((GroundBurstTargetAudio-GroundBurstPreviousAudio)/FMath::Max(.0001,AudioNow-GroundBurstPreviousAudio),0.,1.);
	const FVector AtImpact=FMath::Lerp(GroundBurstPreviousLocation,AttackTarget->GetActorLocation(),T);
	const bool Inside=FVector::DistSquared2D(AtImpact,GroundBurstCenter)<=FMath::Square(Boundary);
	if (auto* Player=AttackTarget->FindComponentByClass<UPlayerRhythmCombatComponent>())
	{
		const double DodgeError=Player->GetLastDodgeInputAudioSeconds()-GroundBurstTargetAudio;
		const bool bDodgedThrough=bGroundBurstDodgeThrough && Inside && DodgeError>=-.35 && DodgeError<=GroundBurstLateGraceSeconds+.001;
		if (bDodgedThrough)
		{
			// On-beat dodge (within the escape window) = full reward; loose = partial. Always the full dodge moment.
			const bool bOnBeat=FMath::Abs(DodgeError)<=Marker.GroundBurst.EscapeWindowSeconds;
			Player->AwardRhythmicEscape(bOnBeat?Marker.GroundBurst.EscapeReward:Marker.GroundBurst.EscapeReward*.4f,static_cast<float>(DodgeError));
			UE_LOG(LogTemp,Display,TEXT("GroundBurst: dodged through errorMs=%.1f onBeat=%d"),DodgeError*1000.,bOnBeat?1:0);
		}
		else if (Inside) Player->ApplyGroundBurstDamage(Marker.Damage,GroundBurstCenter);
		else if (bGroundBurstWasInside)
		{
			// Every real escape (inside at warning, outside at detonation) gets the
			// full dodge moment. A last-moment single exit keeps the full reward;
			// leaving early or edge-dancing still reads as a dodge, for less resonance.
			const bool bTimed=GroundBurstBoundaryCrossings==1 && GroundBurstExitAudio>=GroundBurstTargetAudio-Marker.GroundBurst.EscapeWindowSeconds && GroundBurstExitAudio<=GroundBurstTargetAudio;
			Player->AwardRhythmicEscape(bTimed?Marker.GroundBurst.EscapeReward:Marker.GroundBurst.EscapeReward*.4f,
				GroundBurstExitAudio>=0.?static_cast<float>(GroundBurstExitAudio-GroundBurstTargetAudio):0.f);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("GroundBurst: resolved index=%d inside=%d errorMs=%.1f crossings=%d exitErrorMs=%.1f"),ImpactIndex,Inside,(AudioNow-GroundBurstTargetAudio)*1000.,GroundBurstBoundaryCrossings,GroundBurstExitAudio>=0.?(GroundBurstExitAudio-GroundBurstTargetAudio)*1000.:-9999.);
}
