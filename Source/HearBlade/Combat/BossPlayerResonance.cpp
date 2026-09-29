#include "Combat/BeatSyncedComboComponent.h"
#include "Combat/BossLocomotionAnimInstance.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

void UBeatSyncedComboComponent::ReceivePlayerStrike(float Damage,float PoiseDamage,AActor* Source,bool bHeavy,FVector ImpactLocation,bool bUppercut)
{
	ApplyBossDamageFrom(Damage,Source,bHeavy,PoiseDamage,ImpactLocation);
	if(bUppercut && BossHealth>0.f && !bPhaseTransitionActive && !bPhaseChangePending)
		StartUppercutFeedback(Source);
	if(bPlayerResonance && bHeavy && BossHealth>0.f && (bQueued || bPlaying))
		StartBossHitReaction(Source,.45f,true);
}

void UBeatSyncedComboComponent::GetOpenings(TArray<FBossOpening>& Out) const
{
	Out.Reset();
	const UBeatClockSubsystem* Clock=GetWorld()?GetWorld()->GetSubsystem<UBeatClockSubsystem>():nullptr;
	if(!Clock || BossHealth<=0.f || bPhaseTransitionActive) return;
	const double SPB=Clock->GetSecondsPerBeat(), Now=Clock->GetAudioNowSeconds();
	// A staggered boss is open until it recovers.
	if(bBossHitReacting && GetWorld())
		Out.Add({EBossOpeningKind::Tail,Now-.05,Now+FMath::Max(0.,BossHitReactionEndWorldSeconds-GetWorld()->GetTimeSeconds())});
	const bool bActive=(bQueued||bPlaying) && IsValid(AttackData) && GetImpactCount()>0;
	if(!bActive)
	{
		// A cancelled combo's tail that never started belongs to nothing.
		if(CachedTail.Kind!=EBossOpeningKind::None && CachedTail.StartAudio<=Now+.05 && CachedTail.EndAudio>Now) Out.Add(CachedTail);
		return;
	}
	const int32 Count=GetImpactCount();
	const double ClipStartElapsed=AttackData->GetElapsedAtAnimationTime(AttackData->ClipStartSeconds);
	TArray<double,TInlineAllocator<16>> Impact, Input;
	for(int32 I=0;I<Count;++I)
	{
		const double Hit=ScheduledStartAudioSeconds+AttackData->GetElapsedAtAnimationTime(GetImpactTime(I))-ClipStartElapsed;
		double Lead=.14;
		if(AttackData->ImpactMarkers.IsValidIndex(I))
		{
			const FAttackImpactMarker& M=GetImpactMarker(I);
			Lead=M.IdealDodgeLeadSeconds;
			if(M.DamageShape==EBossAttackDamageShape::GroundBurst) Lead=0.;
			if(M.RequiredResponse==ERhythmResponseType::HoldBlock && M.HoldMarker==ERhythmHoldMarkerType::End) Lead=0.;
		}
		Impact.Add(Hit); Input.Add(Hit-Lead);
	}
	// The previous combo's tail, clipped so it never overlaps this combo's first note.
	if(CachedTail.Kind!=EBossOpeningKind::None)
	{
		FBossOpening Prev=CachedTail; Prev.EndAudio=FMath::Min(Prev.EndAudio,Input[0]-.3*SPB);
		if(Prev.EndAudio-Prev.StartAudio>=.4*SPB && Prev.StartAudio<Impact[0]) Out.Add(Prev);
	}
	for(int32 I=0;I+1<Count;++I)
	{
		// No opening while a hold is still being held.
		if(AttackData->ImpactMarkers.IsValidIndex(I))
		{
			const FAttackImpactMarker& M=GetImpactMarker(I);
			if(M.RequiredResponse==ERhythmResponseType::HoldBlock && M.HoldMarker!=ERhythmHoldMarkerType::End) continue;
		}
		const double S=Impact[I]+.25*SPB, E=Input[I+1]-.3*SPB;
		if(E-S>=.4*SPB) Out.Add({EBossOpeningKind::Gap,S,E});
	}
	const double TailStart=Impact.Last()+.35*SPB;
	CachedTail={EBossOpeningKind::Tail,TailStart,TailStart+2.5*SPB};
	Out.Add(CachedTail);
}

bool UBeatSyncedComboComponent::GetOpeningAt(double AudioSeconds, FBossOpening& Out) const
{
	TArray<FBossOpening> All; GetOpenings(All);
	// Prefer a Gap when a stagger window overlaps it.
	for(const FBossOpening& O:All) if(O.Kind==EBossOpeningKind::Gap && O.Contains(AudioSeconds)) { Out=O; return true; }
	for(const FBossOpening& O:All) if(O.Contains(AudioSeconds)) { Out=O; return true; }
	Out=FBossOpening(); return false;
}

void UBeatSyncedComboComponent::StartUppercutFeedback(AActor* Source)
{
	if(!GetWorld() || !GetOwner() || BossHealth<=0.f || bPhaseTransitionActive) return;
	EndUppercutFeedback();
	// This is a real stagger, while the visual lift stays on the skeletal meshes.
	// The collision capsule remains grounded so follow-up strikes still intersect.
	StartBossHitReaction(Source,FMath::Max(.95f,HeavyHitReactionSeconds),true); // covers the air time
	TArray<USkeletalMeshComponent*> Meshes;
	GetOwner()->GetComponents(Meshes);
	for(USkeletalMeshComponent* Mesh:Meshes)
	{
		if(!IsValid(Mesh)) continue;
		bool bHasSkeletalParent=false;
		for(USceneComponent* Parent=Mesh->GetAttachParent();Parent;Parent=Parent->GetAttachParent())
			if(Cast<USkeletalMeshComponent>(Parent)) { bHasSkeletalParent=true; break; }
		if(!bHasSkeletalParent) { UppercutMeshBases.Add(Mesh,Mesh->GetRelativeLocation()); UppercutMeshRots.Add(Mesh,Mesh->GetRelativeRotation()); }
	}
	UppercutStartWorldSeconds=GetWorld()->GetTimeSeconds();
	GetWorld()->GetTimerManager().SetTimer(UppercutTimer,this,&UBeatSyncedComboComponent::TickUppercutFeedback,1.f/120.f,true);
	UE_LOG(LogTemp,Display,TEXT("BossUppercut: stagger and visual lift meshes=%d"),UppercutMeshBases.Num());
}

void UBeatSyncedComboComponent::TickUppercutFeedback()
{
	if(!GetWorld()) return;
	// Launch arc: fast rise (ease-out), a short hang at the apex, gravity-like fall
	// (ease-in), then a small landing bounce. The body leans back with the lift.
	constexpr float Rise=.24f, Hang=.14f, Fall=.34f, Bounce=.14f, Height=125.f, BounceH=12.f, MaxTilt=16.f;
	constexpr float Duration=Rise+Hang+Fall+Bounce;
	const float Age=float(GetWorld()->GetTimeSeconds()-UppercutStartWorldSeconds);
	if(Age>=Duration || BossHealth<=0.f || bPhaseTransitionActive) { EndUppercutFeedback(); return; }
	float Lift=0.f, Tilt=0.f;
	if(Age<Rise) { const float U=Age/Rise; Lift=Height*(1.f-FMath::Square(1.f-U)); Tilt=MaxTilt*U; }
	else if(Age<Rise+Hang) { const float U=(Age-Rise)/Hang; Lift=Height*(1.f-.06f*U*U); Tilt=MaxTilt; }
	else if(Age<Rise+Hang+Fall) { const float U=(Age-Rise-Hang)/Fall; Lift=Height*.94f*(1.f-U*U); Tilt=MaxTilt*(1.f-.6f*U); }
	else { const float U=(Age-Rise-Hang-Fall)/Bounce; Lift=BounceH*FMath::Sin(U*PI); Tilt=MaxTilt*.4f*(1.f-U); }
	// Lean back = rotate about the actor's right axis, top moving away from the hit.
	const FQuat Lean(FVector::RightVector,FMath::DegreesToRadians(-Tilt));
	for(const auto& Entry:UppercutMeshBases)
		if(USkeletalMeshComponent* Mesh=Entry.Key.Get())
		{
			Mesh->SetRelativeLocation(Entry.Value+FVector(0.f,0.f,Lift));
			if(const FRotator* Base=UppercutMeshRots.Find(Entry.Key)) Mesh->SetRelativeRotation(Lean*Base->Quaternion());
		}
}

void UBeatSyncedComboComponent::EndUppercutFeedback()
{
	if(GetWorld()) GetWorld()->GetTimerManager().ClearTimer(UppercutTimer);
	for(const auto& Entry:UppercutMeshBases)
		if(USkeletalMeshComponent* Mesh=Entry.Key.Get()) Mesh->SetRelativeLocation(Entry.Value);
	for(const auto& Entry:UppercutMeshRots)
		if(USkeletalMeshComponent* Mesh=Entry.Key.Get()) Mesh->SetRelativeRotation(Entry.Value);
	UppercutMeshBases.Empty(); UppercutMeshRots.Empty();
	UppercutStartWorldSeconds=-1000.;
}

void UBeatSyncedComboComponent::PlayPlayerHitFeedback(AActor* Source,float Strength,float FreezeSeconds)
{
	if(!GetWorld() || !GetOwner() || BossHealth<=0.f) return;
	Strength=FMath::Clamp(Strength,0.f,2.f);
	// Armour flash: reuse the strong beat overlay pulse as a white hit flash.
	if(const auto* Clock=GetWorld()->GetSubsystem<UBeatClockSubsystem>()) TriggerBeatOverlay(true,Clock->GetAudioNowSeconds());
	// Shove away from the player. Skip while an attack is committed so warped
	// boss swings keep their authored landing.
	if(auto* Boss=Cast<ACharacter>(GetOwner()); Boss && Source && !bQueued && !bPlaying && !bPhaseTransitionActive)
	{
		FVector Away=Boss->GetActorLocation()-Source->GetActorLocation(); Away.Z=0.f;
		if(Away.Normalize()) Boss->LaunchCharacter(Away*(160.f+220.f*Strength),true,false);
	}
	// Shudder along the hit direction for the freeze (plus a little tail).
	if(auto* Boss=Cast<ACharacter>(GetOwner()); Boss && Boss->GetMesh() && Source)
	{
		USkeletalMeshComponent* Body=Boss->GetMesh();
		if(!bHitShaking) HitShakeBase=Body->GetRelativeLocation();
		FVector Dir=Boss->GetActorLocation()-Source->GetActorLocation(); Dir.Z=0.f;
		HitShakeDir=Boss->GetActorTransform().InverseTransformVectorNoScale(Dir.GetSafeNormal());
		HitShakeAmplitude=1.5f+1.5f*Strength; // cm
		HitShakeDuration=FreezeSeconds+.06f; HitShakeElapsed=0.f; bHitShaking=true;
		GetWorld()->GetTimerManager().SetTimer(HitShakeTimer,FTimerDelegate::CreateUObject(this,&UBeatSyncedComboComponent::TickHitShake),1.f/120.f,true);
	}
	// Hit-stop: freeze the boss meshes for a few frames, then restore exactly the
	// rate that was there (resonance also scales these meshes).
	if(FreezeSeconds<=0.f) return;
	TArray<USkeletalMeshComponent*> Meshes; GetOwner()->GetComponents(Meshes);
	for(auto* Mesh:Meshes)
	{
		if(!IsValid(Mesh) || Mesh->GlobalAnimRateScale<=.06f) continue; // already frozen
		const float Previous=Mesh->GlobalAnimRateScale;
		Mesh->GlobalAnimRateScale=.05f; // near-freeze, not a hard stop
		FTimerHandle Handle;
		TWeakObjectPtr<USkeletalMeshComponent> Weak=Mesh;
		GetWorld()->GetTimerManager().SetTimer(Handle,FTimerDelegate::CreateWeakLambda(this,[Weak,Previous]()
		{
			if(auto* M=Weak.Get(); M && M->GlobalAnimRateScale<=.06f) M->GlobalAnimRateScale=Previous;
		}),FreezeSeconds,false);
	}
}

void UBeatSyncedComboComponent::BeginPlayerResonance(double StartBeat,AActor* Player)
{
	if(bPlayerResonance || BossHealth<=0.f || bPhaseTransitionActive) return;
	PreResonanceAttack=AttackData;
	CancelCombo(); EndBossHitReaction();
	bPlayerResonance=true; PlayerResonanceStartBeat=StartBeat; PlayerResonanceAttempt=0;
	SetAttackTarget(Player);
	ResonanceSavedRates.Reset();
	TArray<USkeletalMeshComponent*> Meshes; GetOwner()->GetComponents(Meshes);
	for(auto* Mesh:Meshes)
	{
		ResonanceSavedRates.Add(Mesh,Mesh->GlobalAnimRateScale); Mesh->GlobalAnimRateScale=.65f;
		if(auto* Anim=Cast<UBossLocomotionAnimInstance>(Mesh->GetAnimInstance())) Anim->SetLocomotion(0.f,FVector::ZeroVector,0.f);
	}
	const auto* Template=LoadObject<UAttackDataAsset>(nullptr,TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/BLUEPRINTS/DA_DragonKnight_Combo01_Beat"));
	if(Template && Template->ImpactMarkers.Num())
	{
		ResonanceCounterAttack=DuplicateObject<UAttackDataAsset>(Template,this);
		ResonanceCounterAttack->ImpactMarkers.SetNum(1);
		auto& Hit=ResonanceCounterAttack->ImpactMarkers[0];
		Hit.RequiredResponse=ERhythmResponseType::DodgeOrBlock; Hit.HoldMarker=ERhythmHoldMarkerType::None;
		Hit.Cinematic.bEnabled=false; Hit.Damage=12.f; Hit.PromptLeadBeats=4.f;
		ResonanceCounterAttack->ResponsePatterns.Reset(); ResonanceCounterAttack->SegmentPlayRates.Reset();
		ResonanceCounterAttack->PlayRate=.65f;
		ResonanceCounterAttack->ClipEndSeconds=FMath::Min(Hit.AnimationTimeSeconds+.35f,Template->GetClipEndSeconds());
		ResonanceCounterAttack->bWarpToTarget=false;
	}
	UE_LOG(LogTemp,Display,TEXT("PlayerResonance: boss suppression begin beat=%.3f"),StartBeat);
}

void UBeatSyncedComboComponent::TickPlayerResonance()
{
	if(!bPlayerResonance) return;
	const auto* Clock=GetWorld()->GetSubsystem<UBeatClockSubsystem>();
	if(!Clock || !Clock->IsRunning() || BossHealth<=0 || bPhaseTransitionActive || Clock->GetCurrentBeat()>=PlayerResonanceStartBeat+16.) { EndPlayerResonance(); return; }
	const double Beat=Clock->GetCurrentBeat();
	if(PlayerResonanceAttempt<2 && Beat>=PlayerResonanceStartBeat+4.+PlayerResonanceAttempt*4.)
	{
		const double Target=PlayerResonanceStartBeat+8.+PlayerResonanceAttempt*4.; ++PlayerResonanceAttempt;
		if(ResonanceCounterAttack && !bPlaying && !bQueued)
		{
			EndBossHitReaction();
			if(QueueAttack(ResonanceCounterAttack))
			{
				const double Windup=ResonanceCounterAttack->GetElapsedAtAnimationTime(GetImpactTime(0))-ResonanceCounterAttack->GetElapsedAtAnimationTime(ResonanceCounterAttack->ClipStartSeconds);
				FirstImpactBeat=FMath::RoundToInt(Target);
				ScheduledFirstImpactAudioSeconds=Clock->GetBeatTimeSeconds(Target);
				ScheduledStartAudioSeconds=ScheduledFirstImpactAudioSeconds-Windup;
				UE_LOG(LogTemp,Display,TEXT("PlayerResonance: slow counter targetBeat=%.0f"),Target);
			}
		}
	}
}

void UBeatSyncedComboComponent::EndPlayerResonance()
{
	if(!bPlayerResonance) return;
	CancelCombo(); EndBossHitReaction();
	for(const auto& Entry:ResonanceSavedRates) if(Entry.Key.IsValid()) Entry.Key->GlobalAnimRateScale=Entry.Value;
	ResonanceSavedRates.Reset(); AttackData=PreResonanceAttack; PreResonanceAttack=nullptr; ResonanceCounterAttack=nullptr;
	bPlayerResonance=false;
	UE_LOG(LogTemp,Display,TEXT("PlayerResonance: boss restored"));
}

void UBeatSyncedComboComponent::TickHitShake()
{
	auto* Boss=Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Body=Boss?Boss->GetMesh():nullptr;
	HitShakeElapsed+=1.f/120.f;
	// The launcher owns the mesh offset while it runs; never fight it (or snap it
	// back to the pre-launch base when the shudder ends).
	if(UppercutMeshBases.Num()) { bHitShaking=false; if(GetWorld()) GetWorld()->GetTimerManager().ClearTimer(HitShakeTimer); return; }
	if(!Body || HitShakeElapsed>=HitShakeDuration)
	{
		if(Body) Body->SetRelativeLocation(HitShakeBase);
		bHitShaking=false; if(GetWorld()) GetWorld()->GetTimerManager().ClearTimer(HitShakeTimer);
		return;
	}
	// Alternate sides every ~22 ms (about 45 Hz), first push away from the hit.
	const float Decay=FMath::Square(1.f-HitShakeElapsed/HitShakeDuration);
	// Smooth 25 Hz sine instead of a square wave (hard side flips looked like frame drops).
	const float Wave=FMath::Sin(HitShakeElapsed*2.f*UE_PI*25.f);
	Body->SetRelativeLocation(HitShakeBase+HitShakeDir*HitShakeAmplitude*Decay*Wave);
}
