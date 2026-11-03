// 把轻重攻击输入安排到音乐节拍，并推进动画、移动与刀刃接触
#include "Combat/Player/PlayerBeatAttackComponent.h"
#include "HearBladeCharacter.h"
#include "Combat/Feedback/TransientCombatFX.h"
#include "Combat/Feedback/PerfectDodgeFeedbackComponent.h"
#include "Combat/Player/PlayerRhythmCombatComponent.h"
#include "Combat/Player/PlayerCombatAnimInstance.h"
#include "Combat/Boss/BeatSyncedComboComponent.h"
#include "Combat/Camera/BossCombatCameraComponent.h"
#include "Audio/BeatClock/BeatClockSubsystem.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Particles/ParticleSystemComponent.h"
#include "NiagaraSystem.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundBase.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

UPlayerBeatAttackComponent::UPlayerBeatAttackComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	AttackSet = LoadObject<UPlayerAttackSet>(
	    nullptr, TEXT("/Game/Combat/Player/DA_PlayerAttackSet.DA_PlayerAttackSet"));
	HitSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/SW_RhythmPerfect.SW_RhythmPerfect"));
	LightSwingSound =
	    LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/SW_BossBladeWeak.SW_BossBladeWeak"));
	HeavySwingSound =
	    LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/SW_BossBladeStrong.SW_BossBladeStrong"));
	ContactSound = LoadObject<USoundBase>(
	    nullptr,
	    TEXT("/Game/Enemies/DF_DRAGON_KNIGHT/SOUND/SW_DragonKnight_SwordSlice.SW_DragonKnight_SwordSlice"));
	StartSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/SW_ResonanceCrowd.SW_ResonanceCrowd"));
	HitEffect =
	    LoadObject<UParticleSystem>(nullptr, TEXT("/Game/InsaneAirCombo/Particles/Sparks_P.Sparks_P"));
	TrailEffect =
	    LoadObject<UParticleSystem>(nullptr, TEXT("/Game/InsaneAirCombo/Particles/Trail_P.Trail_P"));
	BeatKickSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/SW_BossDrumKick.SW_BossDrumKick"));
	GoodHitSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/SW_RhythmGood.SW_RhythmGood"));
	// 循环音乐层不能当作单次命中音效播放，否则会不断叠加音源

	SnareSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/SW_BossDrumTick.SW_BossDrumTick"));
	FinisherSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/SW_ComboTierUp.SW_ComboTierUp"));
	FinisherPerfectSound =
	    LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/SW_RhythmPerfect.SW_RhythmPerfect"));
	PerfectBurstEffect = LoadObject<UNiagaraSystem>(
	    nullptr, TEXT("/Game/Combat/Feedback/NS_PerfectDodge_Ring.NS_PerfectDodge_Ring"));
}

// 加载各连段的命中音效，排除缺失资源
void UPlayerBeatAttackComponent::LoadHitRushSounds()
{
	if (LightHitSounds.Num())
		return;
	for (int32 I = 1; I <= 4; ++I)
	{
		LightHitSounds.Add(LoadObject<USoundBase>(
		    nullptr,
		    *FString::Printf(TEXT("/Game/Audio/SFX/HitRush/SW_HitRush_Light%d.SW_HitRush_Light%d"), I, I)));
		HeavyHitSounds.Add(LoadObject<USoundBase>(
		    nullptr,
		    *FString::Printf(TEXT("/Game/Audio/SFX/HitRush/SW_HitRush_Heavy%d.SW_HitRush_Heavy%d"), I, I)));
	}
	LightHitSounds.RemoveAll(
	    [](const TObjectPtr<USoundBase>& S)
	    {
		    return !S;
	    });
	HeavyHitSounds.RemoveAll(
	    [](const TObjectPtr<USoundBase>& S)
	    {
		    return !S;
	    });
	FinisherHitSound = LoadObject<USoundBase>(
	    nullptr, TEXT("/Game/Audio/SFX/HitRush/SW_HitRush_Finisher.SW_HitRush_Finisher"));
	KickHitSound =
	    LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/HitRush/SW_HitRush_Kick.SW_HitRush_Kick"));
	OffBeatHitSound = LoadObject<USoundBase>(
	    nullptr, TEXT("/Game/Audio/SFX/HitRush/SW_HitRush_OffBeat.SW_HitRush_OffBeat"));
	// 挥刀使用风声，避免叠加 Boss 武器的金属声
	LightSwingSound = LoadObject<USoundBase>(
	    nullptr, TEXT("/Game/Audio/SFX/HitRush/SW_HitRush_WhooshLight.SW_HitRush_WhooshLight"));
	HeavySwingSound = LoadObject<USoundBase>(
	    nullptr, TEXT("/Game/Audio/SFX/HitRush/SW_HitRush_WhooshHeavy.SW_HitRush_WhooshHeavy"));
}

// 取得当前世界的节拍时钟
UBeatClockSubsystem* UPlayerBeatAttackComponent::Clock() const
{
	return GetWorld() ? GetWorld()->GetSubsystem<UBeatClockSubsystem>() : nullptr;
}

// 缓存战斗组件，加载音效并校验攻击配置
void UPlayerBeatAttackComponent::BeginPlay()
{
	Super::BeginPlay();
	Combat = GetOwner()->FindComponentByClass<UPlayerRhythmCombatComponent>();
	LoadHitRushSounds(); // 开局加载，方便编辑器中新导入的音效直接生效
	FString Error;
	if (!AttackSet || !AttackSet->Validate(Error))
		UE_LOG(LogTemp, Error, TEXT("PlayerAttack: invalid data %s"), *Error);
}

// 退出关卡时清空攻击与共鸣状态
void UPlayerBeatAttackComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	Reset();
	Super::EndPlay(Reason);
}

// 优先使用镜头锁定目标，否则寻找前方附近的存活 Boss
AActor* UPlayerBeatAttackComponent::FindTarget() const
{
	if (const auto* Camera = GetOwner()->FindComponentByClass<UBossCombatCameraComponent>())
		if (auto* Target = Camera->GetCombatTarget())
			return Target;
	AActor* Best = nullptr;
	float BestDistance = 600.f;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		if (const auto* Boss = It->FindComponentByClass<UBeatSyncedComboComponent>();
		    Boss && Boss->BossHealth > 0.f)
		{
			const FVector Offset = It->GetActorLocation() - GetOwner()->GetActorLocation();
			if (Offset.Size2D() < BestDistance &&
			    FVector::DotProduct(Offset.GetSafeNormal2D(), GetOwner()->GetActorForwardVector()) > .25f)
			{
				Best = *It;
				BestDistance = Offset.Size2D();
			}
		}
	return Best;
}

// 接收攻击输入，决定立即出招还是等待连招时机
bool UPlayerBeatAttackComponent::TryAttack(EPlayerAttackInput Input)
{
	if (!Combat.IsValid() || Combat->Health <= 0.f || Combat->IsPhaseCinematic() || !Clock() ||
	    !Clock()->IsRunning())
		return false;
	if (Input == EPlayerAttackInput::Heavy && !bHeavyAttacksEnabled)
		return false; // 当前配置禁用重攻击时，忽略该输入
	// 统一读取音乐时间，后续缓存和节奏评级都使用这次按键的原始时间
	const double Now = Clock()->GetAudioNowSeconds();
	if (IsAttacking())
	{
		// 保存本轮第一次连招输入及按键时间，后续按键不覆盖它
		// 重复按键仍会计数，供连招判定检查是否连续乱按
		if (Now > LatestCueAudio + ChainLateSeconds())
			return false;
		++PressCountSinceCommit;
		if (!bHasBufferedInput)
		{
			BufferedInput = Input;
			BufferedInputTime = Now;
			bHasBufferedInput = true;
		}
		return true;
	}
	// 这里是普通起手缓存，与出招期间的连招缓存分开
	// 等待上限使用游戏时间，避免音乐暂停或恢复后旧输入突然执行
	auto QueuePending = [&]()
	{
		++PressCountSinceCommit;
		if (bHasPendingAttack)
			return true; // 已有等待中的攻击，保留第一次按键时间
		PendingAttack = Input;
		PendingAttackAudio = Now;
		PendingAttackDeadline = GetWorld()->GetTimeSeconds() + 1.0;
		bHasPendingAttack = true; // 暂时不能出招时，保存输入供后续重试
		UE_LOG(LogTemp, Display, TEXT("PlayerAttack: queued ordinary input during recovery"));
		return true;
	};
	if (bHasPendingAttack || Now < RecoveryUntilAudio || Now < NextStartAllowedAudio)
		return QueuePending();
	PressCountSinceCommit = 0; // 新一轮攻击不累计上一轮的按键次数
	if (Combat->PrepareForOrdinaryAttack() && StartAttack(Input, Now))
	{
		bHasPendingAttack = false;
		return true;
	}
	return QueuePending();
}

// 返回连招允许迟按的时间宽限
double UPlayerBeatAttackComponent::ChainLateSeconds() const
{
	// 较长的重击允许更晚接续，其余动作使用普通连招迟到窗口
	const auto* Node = AttackSet ? AttackSet->Find(CurrentAttack) : nullptr;
	return Node && Node->DurationBeats > 1.2f ? .26 : PlayerAttackTiming::ComboQueueLateSeconds;
}

// 计算出手到接触姿态需要预留的时间
double UPlayerBeatAttackComponent::StrikeLead(const FPlayerAttackNode& Node)
{
	return FMath::Clamp(double(Node.ImpactTime - Node.StrikeStart) / 1.2, .05, .12);
}

// 返回蓄力拍数，当前实现关闭该分支并返回零
double UPlayerBeatAttackComponent::WindupBeats(const FPlayerAttackNode& Node, bool bResonance)
{
	// 当前关闭重攻击蓄力，轻重攻击都按普通命中流程执行
	static const bool bHeavyCharge = false;
	if (!bHeavyCharge || Node.Input != EPlayerAttackInput::Heavy)
		return 0.;
	return FMath::Max(.5,
	                  FMath::RoundToDouble(double(Node.DurationBeats) * (bResonance ? .5 : 1.) * 2.) * .5);
}

// 根据蓄力进度计算应该显示的动画位置
float UPlayerBeatAttackComponent::WindupPosition(double Now, double SampleNow) const
{
	// 备用蓄力曲线分为抬剑、保持、挥击和收招
	// 当前 WindupBeats 返回 0，普通战斗不会进入这条分支
	const auto* Node = AttackSet ? AttackSet->Find(CurrentAttack) : nullptr;
	if (!Node)
		return ActiveClipStart;
	const double Start = ActiveClipStart, Hold = HoldPosition, Impact = Node->ImpactTime;
	const double Creep = Hold + (Impact - Hold) * .06;
	auto U = [](double T, double A, double B)
	{
		return FMath::Clamp((T - A) / FMath::Max(.001, B - A), 0., 1.);
	};
	if (Now < ChamberStartAudio)
		return float(Start);
	if (Now < ChamberEndAudio)
	{
		const double X = U(Now, ChamberStartAudio, ChamberEndAudio);
		return float(FMath::Lerp(Start, Hold, 1. - FMath::Pow(1. - X, 3.)));
	}
	if (Now < SnapStartAudio)
	{
		const double X = U(Now, ChamberEndAudio, SnapStartAudio);
		return float(FMath::Lerp(Hold, Creep, X * X));
	}
	if (Now < ImpactAudio)
	{
		const double X = U(Now, SnapStartAudio, ImpactAudio);
		return float(FMath::Lerp(Creep, Impact, X * X * (.5 + .5 * X)));
	}
	const double X = U(SampleNow, ImpactAudio, EndAudio);
	return float(FMath::Lerp(Impact, double(ActiveClipEnd), 1. - (1. - X) * (1. - X)));
}

// 安排一击：先沿轻重分支选择动作，再确定命中音乐时间，最后准备动画片段与连招窗口
// 命中时间由节拍安排，靠近目标的距离和方向由位移逻辑处理
bool UPlayerBeatAttackComponent::StartAttack(EPlayerAttackInput Input, double InputAudio, bool bChained,
                                             double TargetImpactAudio)
{
	if (!AttackSet || !Clock())
		return false;
	if (!Combat.IsValid() || !Combat->PrepareForOrdinaryAttack())
		return false;
	const double Now = Clock()->GetAudioNowSeconds(), SPB = Clock()->GetSecondsPerBeat();
	// 已安排命中拍点的连招可以提前起手，普通起手仍受最早出招时间限制
	if (TargetImpactAudio < 0. && Now < NextStartAllowedAudio)
		return false;
	// 已缓存的按键属于原来的连招分支，不能因执行延迟而丢失
	// 没有缓存的按键则需要落在普通接续窗口内
	const FPlayerAttackNode* Previous =
	    (bChained || PlayerAttackTiming::CanQueue(InputAudio, LastChainAudio, false))
	        ? AttackSet->Find(ChainNode)
	        : nullptr;
	FName Next = Input == EPlayerAttackInput::Light ? TEXT("L") : TEXT("H");
	if (Previous)
	{
		const FName Branch = Input == EPlayerAttackInput::Light ? Previous->NextLight : Previous->NextHeavy;
		const auto* Candidate = AttackSet->Find(Branch);
		if (Candidate && (!Candidate->bResonanceOnly || IsResonanceActive()))
			Next = Branch;
		else
			Previous = nullptr;
	}
	const auto* Node = AttackSet->Find(Next);
	if (!Node || !Node->Animation)
		return false;
	// 记录当前可见姿态，衔接同一段动画时从这个位置继续
	const auto* Playing = AttackSet->Find(CurrentAttack);
	const float PlayingPos = LastRootMotionPosition;
	const double PreviousImpact = ImpactAudio;
	const bool bPlayingVisible = Playing && !bAttackVisualEnded;
	const double Windup = WindupBeats(*Node, IsResonanceActive());
	const bool bWindup = Windup > 0.;
	auto* Character = Cast<ACharacter>(GetOwner());
	auto* Anim =
	    Character ? Cast<UPlayerCombatAnimInstance>(Character->GetMesh()->GetAnimInstance()) : nullptr;
	if (!Anim || Character->GetCharacterMovement()->IsFalling())
		return false;
	// 先确定武器接触的音乐时刻，再安排起手和收招
	const double Beat0 = Clock()->GetBeatTimeSeconds(0.);
	double Target = TargetImpactAudio;
	if (Target < 0.)
	{
		// 普通起手对齐整拍，共鸣期间对齐半拍
		// 稍早按下就等待拍点，稍晚按下保留最短起手时间，太早则交回待执行缓存
		// 按得更快不会缩短固定的连招节拍间隔
		const double RootGrid = (IsResonanceActive() ? .5 : 1.) * SPB;
		const double Nearest = Beat0 + FMath::RoundToDouble((Now - Beat0) / RootGrid) * RootGrid;
		if (Nearest - Now >= .033 && Nearest - Now <= .15)
			Target = Nearest;
		else if (Now - Nearest >= -.033 && Now - Nearest <= .12)
			Target = Now + .033;
		else
		{
			Target = Beat0 + FMath::CeilToDouble((Now - Beat0) / RootGrid) * RootGrid;
			if (!bWindup && Target - Now > .15)
				return false; // 尚未到起手时机，由调用者继续保留输入
		}
		// 备用蓄力分支以按键拍点为起点，命中安排在蓄力结束后
		if (bWindup)
			Target = Beat0 + FMath::RoundToDouble((Now - Beat0) / SPB) * SPB + Windup * SPB;
	}
	ImpactAudio = FMath::Max(Target, Now + .033);
	const double StartupSeconds = ImpactAudio - Now;
	const float StrikeFrom = FMath::Clamp(Node->StrikeStart, Node->ClipStart, Node->ImpactTime - .01f);
	// 晚按时缩短实际播放的前摇片段，避免把整段动画突然加速
	const float PreviousClipEnd = ActiveClipEnd;
	ActiveClipStart = FMath::Min(FMath::Max(StrikeFrom, float(Node->ImpactTime - StartupSeconds * 1.5)),
	                             Node->ImpactTime - .005f);
	bool bContinuesPose = false;
	if (bWindup)
	{
		// 同一动画中接续蓄力时，沿用当前姿态，不回到片段起点
		bContinuesPose = bPlayingVisible && Playing->Animation == Node->Animation &&
		                 PlayingPos >= Node->ClipStart - .4f && PlayingPos < Node->StrikeStart;
		ActiveClipStart = bContinuesPose ? PlayingPos : Node->ClipStart;
	}
	// 优先寻找同一动画内连续的下一招，找不到时使用同类按键分支
	// 这样能提前准备收招姿态，重击连段也不会每击都退回待机
	auto Usable = [this](FName Id) -> const FPlayerAttackNode*
	{
		const auto* N = AttackSet->Find(Id);
		return N && (!N->bResonanceOnly || IsResonanceActive()) ? N : nullptr;
	};
	// 蓄力需要独立抬剑，不能当作普通连续片段直接衔接
	auto Flows = [this, Node, bWindup](const FPlayerAttackNode* N)
	{
		return !bWindup && N && WindupBeats(*N, IsResonanceActive()) <= 0. &&
		       N->Animation == Node->Animation && FMath::IsNearlyEqual(N->ClipStart, Node->ClipEnd, .02f);
	};
	const FPlayerAttackNode* SameInput =
	    Usable(Node->Input == EPlayerAttackInput::Heavy ? Node->NextHeavy : Node->NextLight);
	const FPlayerAttackNode* OtherInput =
	    Usable(Node->Input == EPlayerAttackInput::Heavy ? Node->NextLight : Node->NextHeavy);
	const FPlayerAttackNode* NextGuess = Flows(SameInput)    ? SameInput
	                                     : Flows(OtherInput) ? OtherInput
	                                     : SameInput         ? SameInput
	                                                         : OtherInput;
	const bool bFlowsOn = Flows(NextGuess);
	ActiveClipEnd = bFlowsOn
	                    ? FMath::Clamp(NextGuess->StrikeStart, Node->ClipEnd, NextGuess->ImpactTime - .01f)
	                    : Node->ClipEnd;
	const bool bSeamless = bContinuesPose || (Previous && Previous->Animation == Node->Animation &&
	                                          FMath::IsNearlyEqual(PreviousClipEnd, ActiveClipStart, .02f));
	AttackMontage = Anim->BeginBeatAttack(Node->Animation, ActiveClipStart, bSeamless ? .03f : .07f);
	if (!AttackMontage)
		return false;
	// 蒙太奇创建成功后才提交这一击，并清空上一击的命中记录
	CurrentAttack = Next;
	++AttackSerial;
	HitActors.Reset();
	bImpactPresented = bRewarded = false;
	HitStopSeconds = 0.f;
	HitStopStartAudio = -1000.;
	LastRootMotionPosition = ActiveClipStart;
	Character->GetCharacterMovement()->StopMovementImmediately();
	bSwingPresented = false;
	bAttackVisualEnded = false;
	StartAudio = Now;
	LastAudio = Now;
	bHasBufferedInput = false;
	// 普通连招固定隔一拍命中，共鸣期间固定隔半拍
	// 按键快慢只影响评级和等待，不改变这条节奏
	const double Cadence = bWindup ? 1. : (IsResonanceActive() ? .5 : 1.);
	// 整数间隔使用整拍网格，半拍间隔使用半拍网格
	const double CueGrid = FMath::IsNearlyEqual(FMath::Frac(Cadence), 0., .01) ? 1. : .5;
	const double Grid = bWindup ? 1. : Cadence;
	AttackPlayRate = 1.f;
	SwingStartAudio = Now + StartupSeconds * .55;
	bWindupAttack = bWindup;
	NextChargeCueAudio = -1.;
	if (bWindup)
	{
		HoldPosition = FMath::Clamp(Node->StrikeStart, ActiveClipStart, Node->ImpactTime - .01f);
		// 刚命中就接续蓄力时，短暂保留接触姿态
		double Pre = bPlayingVisible && Now - PreviousImpact < .05 ? .06 : 0.;
		double Chamber = HoldPosition > ActiveClipStart
		                     ? FMath::Max(.06, double(HoldPosition - ActiveClipStart) / 1.15)
		                     : 0.;
		double Snap = FMath::Clamp(double(Node->ImpactTime - HoldPosition) / 2.2, .035, .08);
		const double Budget = ImpactAudio - Now;
		if (Pre + Chamber + Snap > Budget)
		{
			const double K = Budget / (Pre + Chamber + Snap);
			Pre *= K;
			Chamber *= K;
			Snap *= K;
		} // 按键晚于目标时刻
		ChamberStartAudio = Now + Pre;
		ChamberEndAudio = ChamberStartAudio + Chamber;
		SnapStartAudio = ImpactAudio - Snap;
		SwingStartAudio = SnapStartAudio; // 蓄力期间继续朝向目标
		// 备用蓄力分支按整拍安排视觉提示，不在命中前重复补发
		NextChargeCueAudio = Beat0 + FMath::CeilToDouble((Now + .1 - Beat0) / SPB) * SPB;
		if (NextChargeCueAudio >= ImpactAudio - .1)
			NextChargeCueAudio = -1.;
	}
	const double StrikeDuration = ImpactAudio - SwingStartAudio;
	// 从本次命中拍点推算下一击，给下一击预留起手时间
	const double NextLead = NextGuess ? StrikeLead(*NextGuess) : 0.;
	double CueBeat = FMath::RoundToDouble(((ImpactAudio - Beat0) / SPB + Cadence) / CueGrid) * CueGrid;
	// 备用蓄力重击后的轻击安排在命中后的半拍
	if (bWindup)
		CueBeat = FMath::CeilToDouble(((ImpactAudio - Beat0) / SPB + .5 - .02) * 2.) * .5;
	while (Beat0 + CueBeat * SPB - NextLead < ImpactAudio + .12)
		CueBeat += CueGrid;
	ComboInputAudio = Beat0 + CueBeat * SPB;
	// 本轮只有一个接续拍点，早按等待，稍晚按立即起手
	// 不再向后选择其他拍点，避免同一条连招忽快忽慢
	LatestCueAudio = ComboInputAudio;
	LinkAudio = ComboInputAudio;
	// 收招持续到下一击需要起手的时刻
	EndAudio = LatestCueAudio - NextLead;
	// 终结技或不连续片段使用较短收招，不把动画拉长到整拍
	// 画面结束后，连招输入仍可保留到窗口关闭
	if (!bFlowsOn)
		EndAudio =
		    FMath::Min(EndAudio, ImpactAudio + FMath::Max(.2, double(ActiveClipEnd - Node->ImpactTime) + .1));
	NextStartAllowedAudio = EndAudio - .025; // 允许一帧的调度误差，不提前下一击的目标拍点
	RecoveryUntilAudio = EndAudio + (Node->bFinisher ? .06 : 0.);
	// 终结技命中后保留最短收招，再允许下一轮普通起手
	if (!bFlowsOn)
	{
		NextStartAllowedAudio = FMath::Min(NextStartAllowedAudio, ImpactAudio + .22);
		RecoveryUntilAudio = FMath::Min(RecoveryUntilAudio, ImpactAudio + .22);
	}
	// 备用蓄力分支单独记录重击按键时刻，普通分支使用连招拍点
	HeavyPressAudio = bWindup ? ImpactAudio + (FMath::Max(1., Windup) - Windup) * SPB : ComboInputAudio;
	// 备用蓄力连段在衔接前继续收招，避免中间短暂退回待机
	if (bWindup && (Usable(Node->NextHeavy) || Usable(Node->NextLight)))
		EndAudio = FMath::Max(EndAudio, FMath::Min(ComboInputAudio, HeavyPressAudio) + .04);
	// 评级使用原始按键时间；同一轮重复按键会降为失误，但不会加快连招
	const double InputBeat = (InputAudio - Clock()->GetBeatTimeSeconds(0.)) / SPB;
	const double IdealInput =
	    Previous ? LastChainAudio
		         : Clock()->GetBeatTimeSeconds(FMath::RoundToDouble(InputBeat / Grid) * Grid);
	LastErrorMs = (InputAudio - IdealInput) * 1000.;
	LastGrade = PlayerAttackTiming::Judge(LastErrorMs / 1000.);
	if (PressCountSinceCommit > 1)
		LastGrade = EPlayerAttackGrade::Bad; // 连续乱按降低评级
	UE_LOG(LogTemp, Display, TEXT("PlayerAttack: judged presses=%d errorMs=%.1f grade=%d"),
	       PressCountSinceCommit, LastErrorMs, int(LastGrade));
	PressCountSinceCommit = 0;
	LastJudgedWorldSeconds = GetWorld()->GetTimeSeconds();
	if (!Previous)
		InputSequence.Empty();
	InputSequence += Input == EPlayerAttackInput::Light ? TEXT("L ") : TEXT("H ");
	if (LastGrade == EPlayerAttackGrade::Bad)
		Combat->ComboCount = 0;
	OnAttackJudged.Broadcast(LastGrade, LastErrorMs, Next);
	AttackTarget = FindTarget();
	if (AttackTarget.IsValid())
	{
		const FRotator Face =
		    (AttackTarget->GetActorLocation() - Character->GetActorLocation()).GetSafeNormal2D().Rotation();
		if (FMath::Abs(FRotator::NormalizeAxis(Face.Yaw - Character->GetActorRotation().Yaw)) < 75.f)
			Character->SetActorRotation(FRotator(0.f, Face.Yaw, 0.f));
	}
	if (Trail)
	{
		Trail->DeactivateSystem();
		Trail = nullptr;
	}
	if (TrailEffect)
		Trail = UGameplayStatics::SpawnEmitterAttached(TrailEffect, Character->GetMesh(), TEXT("hand_r"),
		                                               FVector::ZeroVector, FRotator::ZeroRotator,
		                                               EAttachLocation::SnapToTarget, true);
	// 保存本次出招的初始剑位置，避免把上一招结束到本招开始的跳变算成挥击
	if (auto* Sword = GetOwner()->FindComponentByClass<UStaticMeshComponent>())
	{
		LastBladeBase = Sword->GetComponentLocation();
		LastBladeTip = LastBladeBase +
		               Sword->GetComponentTransform().TransformVectorNoScale(FVector::YAxisVector) * 150.f;
	}
	SampleLegs(LastLegA, LastLegB);
	UE_LOG(
	    LogTemp, Display,
	    TEXT(
	        "PlayerAttack: start id=%llu node=%s grade=%d errorMs=%.2f impact=%.6f end=%.6f leadMs=%.1f strikeMs=%.1f link=%.6f"),
	    AttackSerial, *Next.ToString(), int(LastGrade), LastErrorMs, ImpactAudio, EndAudio,
	    (SwingStartAudio - Now) * 1000., StrikeDuration * 1000., LinkAudio);
	return true;
}

// 返回攻击期间允许的移动比例
float UPlayerBeatAttackComponent::GetMovementScale() const
{
	const auto* Node = AttackSet ? AttackSet->Find(CurrentAttack) : nullptr;
	if (bAttackVisualEnded)
		return 1.f;
	return Node ? 0.f : 1.f; // 攻击期间由动画根位移负责移动
}

// 根据当前连招节点生成下一步按键提示
FString UPlayerBeatAttackComponent::GetComboPrompt() const
{
	if (!Clock() || !AttackSet)
		return FString();
	const double Now = Clock()->GetAudioNowSeconds();
	const auto* Node = AttackSet->Find(CurrentAttack);
	if (!Node)
		return Now < RecoveryUntilAudio
		           ? TEXT("RECOVERY")
				   : (bHeavyAttacksEnabled ? TEXT("L L L L   |   H H H H   |   L H   |   H L L H")
				                           : TEXT("L L L L"));
	FString Choices;
	for (bool Heavy : {false, true})
	{
		if (Heavy && !bHeavyAttacksEnabled)
			continue;
		const auto* Branch = AttackSet->Find(Heavy ? Node->NextHeavy : Node->NextLight);
		if (Branch && (!Branch->bResonanceOnly || IsResonanceActive()))
			Choices += Heavy ? TEXT("RMB ") : TEXT("LMB ");
	}
	if (bHasBufferedInput)
		return InputSequence + TEXT(" > ") +
		       (BufferedInput == EPlayerAttackInput::Light ? TEXT("L  QUEUED") : TEXT("H  QUEUED"));
	if (Choices.IsEmpty())
		return InputSequence + TEXT("  FINISHER");
	if (Now > LatestCueAudio + ChainLateSeconds())
		return InputSequence + TEXT("  CHAIN CLOSED");
	return InputSequence +
	       (Now >= FMath::Max(ImpactAudio, ComboInputAudio - PlayerAttackTiming::ComboQueueEarlySeconds)
	            ? TEXT("  LINK NOW: ")
				: TEXT("  NEXT BEAT: ")) +
	       Choices;
}

// 检查防御能否打断当前攻击，允许时结束攻击动作
bool UPlayerBeatAttackComponent::CancelForDefense(bool bDodge)
{
	// 先取消待执行的普通攻击，防御结束后不会补出之前排队的攻击
	bHasPendingAttack = false;
	if (!IsAttacking())
		return true;
	if (Combat.IsValid() && Combat->IsMouseDefenseWindow())
	{
		CancelAttack();
		return true;
	}
	const auto* Node = AttackSet->Find(CurrentAttack);
	// 重击蓄力期间允许用闪避取消
	if (Clock()->GetAudioNowSeconds() < ImpactAudio &&
	    (!bDodge || (Node->Input == EPlayerAttackInput::Heavy && !bWindupAttack)))
		return false;
	CancelAttack();
	return true;
}

// 停止本次攻击的动画、轨迹和输入缓存
void UPlayerBeatAttackComponent::CancelAttack()
{
	if (IsAttacking())
		UE_LOG(LogTemp, Display, TEXT("PlayerAttack: end id=%llu node=%s audio=%.3f expectedEnd=%.3f"),
		       AttackSerial, *CurrentAttack.ToString(), Clock() ? Clock()->GetAudioNowSeconds() : 0.,
		       EndAudio);
	// 动画已经开始淡出时，不再次用更短时间重启淡出，避免姿态突然跳回待机
	if (auto* C = Cast<ACharacter>(GetOwner()); C && !bAttackVisualEnded)
		if (auto* Anim = Cast<UPlayerCombatAnimInstance>(C->GetMesh()->GetAnimInstance()))
			Anim->EndBeatAttack(AttackMontage);
	AttackMontage = nullptr;
	CurrentAttack = NAME_None;
	ChainNode = NAME_None;
	bHasBufferedInput = false;
	PressCountSinceCommit = 0;
	bWindupAttack = false;
	NextChargeCueAudio = -1.;
	if (Trail)
	{
		Trail->DeactivateSystem();
		Trail = nullptr;
	}
}

// 受击等外部中断发生时清理攻击状态
void UPlayerBeatAttackComponent::NotifyInterrupted()
{
	bHasPendingAttack = false;
	CancelAttack();
	InputSequence.Empty();
}

// 重置攻击、缓存和共鸣状态
void UPlayerBeatAttackComponent::Reset()
{
	bHasPendingAttack = false;
	CancelAttack();
	LastCommittedImpactBeat = -1000.;
	NextStartAllowedAudio = -1000.;
	RecoveryUntilAudio = -1000.;
	InputSequence.Empty();
	EndResonance();
	SetResonanceState(EPlayerResonanceState::Filling);
}

// 按音乐时间推进动作、接触检测与连招接续
void UPlayerBeatAttackComponent::TickAttack(double Now, float Delta)
{
	const auto* Node = AttackSet->Find(CurrentAttack);
	if (!Node)
	{
		CancelAttack();
		return;
	}
	// 每次攻击只播放一次挥刀声，和实际命中声音分开
	if (!bSwingPresented && Now >= ImpactAudio - .07)
	{
		bSwingPresented = true;
		USoundBase* Swing = Node->Input == EPlayerAttackInput::Heavy ? HeavySwingSound : LightSwingSound;
		if (Swing)
			UGameplayStatics::PlaySoundAtLocation(this, Swing, GetOwner()->GetActorLocation(), .46f,
			                                      Node->bFinisher ? .95f : 1.05f);
	}
	if (bWindupAttack && NextChargeCueAudio > 0. && Now >= NextChargeCueAudio)
	{
		// 备用蓄力提示只产生手部闪光，不重复播放声音
		const double SPB = Clock()->GetSecondsPerBeat();
		const bool bLast = NextChargeCueAudio + SPB >= ImpactAudio - .1;
		if (auto* C = Cast<ACharacter>(GetOwner()))
			if (UParticleSystem* Spark =
			        LoadObject<UParticleSystem>(nullptr, TEXT("/Game/SwordAnim/demo/FX/P_Spark.P_Spark")))
				RetireCombatFX(UGameplayStatics::SpawnEmitterAtLocation(
				                   GetWorld(), Spark, C->GetMesh()->GetSocketLocation(TEXT("hand_r")),
				                   C->GetActorRotation(), FVector(bLast ? .45f : .3f), true),
				               .03f, .12f);
		NextChargeCueAudio = bLast ? -1. : NextChargeCueAudio + SPB;
	}
	if (auto* C = Cast<ACharacter>(GetOwner()))
	{
		if (auto* Anim = C->GetMesh()->GetAnimInstance(); Anim && !bAttackVisualEnded)
		{
			// 按绝对音乐时间采样动画，避免逐帧累加造成节奏漂移
			// 命中停顿只调整姿态采样，下一击的目标拍点保持不变
			const double Natural = FMath::Max(0., double(ActiveClipEnd - Node->ImpactTime));
			const double FreezeEnd = HitStopSeconds > 0.f
			                             ? FMath::Max(ImpactAudio, HitStopStartAudio + double(HitStopSeconds))
			                             : ImpactAudio;
			// 当前不使用收招后停住等待的分支，收招一直推进到 EndAudio
			const bool bSpare = false;
			const double VisualEnd = bSpare ? FreezeEnd + Natural * 1.3 : EndAudio;
			double SampleNow = Now;
			if (HitStopSeconds > 0.f && Now >= HitStopStartAudio && VisualEnd > HitStopStartAudio + .02)
			{
				// 先短暂停在接触姿态，再平滑追上原定收招结束时间
				// 停顿长度受剩余收招时间限制，避免下一拍仍无法恢复动作
				const double Span = VisualEnd - HitStopStartAudio;
				const double Freeze = FMath::Min(double(HitStopSeconds) * .6, Span * .25);
				if (Now < HitStopStartAudio + Freeze)
					SampleNow = HitStopStartAudio;
				else
				{
					const double V = FMath::Clamp(
					    (Now - HitStopStartAudio - Freeze) / FMath::Max(.001, Span - Freeze), 0., 1.);
					SampleNow = HitStopStartAudio + Span * (1. - FMath::Pow(1. - V, 1.4));
				}
			}
			FPlayerAttackNode Active = *Node;
			Active.ClipStart = ActiveClipStart;
			Active.ClipEnd = ActiveClipEnd;
			float Position;
			if (bWindupAttack)
				Position = WindupPosition(Now, SampleNow);
			else if (bSpare && Now >= ImpactAudio)
			{
				const double U =
				    FMath::Clamp((Now - FreezeEnd) / FMath::Max(.001, VisualEnd - FreezeEnd), 0., 1.);
				Position = float(
				    FMath::Lerp(double(Node->ImpactTime), double(ActiveClipEnd), 1. - (1. - U) * (1. - U)));
			}
			else
				Position = PlayerAttackTiming::ContinuousComboPosition(bSpare ? Now : SampleNow, StartAudio,
				                                                       ImpactAudio, VisualEnd, Active);
			if (Position > LastRootMotionPosition && Node->Animation)
			{
				// 动画位置由代码设置，因此这里手动提取本帧经过的根位移
				const FTransform LocalRoot = Node->Animation->ExtractRootMotionFromRange(
				    LastRootMotionPosition, Position, FAnimExtractContext(Position, true));
				const FTransform WorldRoot = C->GetMesh()->ConvertLocalRootMotionToWorld(LocalRoot);
				FVector Travel = WorldRoot.GetTranslation();
				Travel.Z = 0.; // 攻击只应用水平位移，落地和重力仍由移动组件处理
				const FQuat Facing = FRotator(0.f, WorldRoot.GetRotation().Rotator().Yaw, 0.f).Quaternion() *
				                     C->GetActorQuat();
				FHitResult Hit;
				C->GetCharacterMovement()->SafeMoveUpdatedComponent(Travel, Facing, true, Hit);
				// 即使被墙挡住，也记下已处理的动画位置，下一帧不补走这段位移
				LastRootMotionPosition = Position;
			}
			Anim->Montage_SetPosition(AttackMontage, Position);
			C->GetMesh()->TickAnimation(0.f, false);
			// 先让骨骼更新到当前动画位置，再同步武器，后面的检测使用这一帧刀刃位置
			C->GetMesh()->RefreshBoneTransforms();
			if (auto* Player = Cast<AHearBladeCharacter>(C))
				Player->SyncAttackWeapon();
		}
		if (Now < SwingStartAudio && AttackTarget.IsValid())
			C->SetActorRotation(FMath::RInterpTo(
			    C->GetActorRotation(),
			    (AttackTarget->GetActorLocation() - C->GetActorLocation()).GetSafeNormal2D().Rotation(),
			    Delta, 12.f));
	}
	// 命中回调可能打断或切换动作，检测后核对编号，避免继续处理旧的一击
	const uint64 SerialBeforeSweep = AttackSerial;
	SweepBlade(Now);
	if (!IsAttacking() || AttackSerial != SerialBeforeSweep)
		return;
	if (Trail && Now >= ImpactAudio + .08)
	{
		Trail->DeactivateSystem();
		Trail = nullptr;
	}
	// 到达命中拍点但没有接触时仍完成挥空表现，不因此生成伤害
	if (!bImpactPresented && Now >= ImpactAudio)
	{
		bImpactPresented = true;
		PresentImpact(false, GetOwner()->GetActorLocation());
		UE_LOG(LogTemp, Display, TEXT("PlayerAttack: impact id=%llu errorMs=%.2f"), AttackSerial,
		       (Now - ImpactAudio) * 1000.);
#if !UE_BUILD_SHIPPING
		if (AttackSerial == 8 && FParse::Param(FCommandLine::Get(), TEXT("PlayerAttackProbe")))
			FScreenshotRequest::RequestScreenshot(
			    FPaths::ProjectSavedDir() / TEXT("Screenshots/PlayerAttack_Corrected.png"), false, false);
#endif
	}
	LastAudio = Now;
	// 先确定缓存按键对应的下一招，再等到它的起手时刻
	// 伤害拍点与按键原始时间分开保存，不能把等待时间当成按键误差
	const auto* Branch =
	    bHasBufferedInput
	        ? AttackSet->Find(BufferedInput == EPlayerAttackInput::Light ? Node->NextLight : Node->NextHeavy)
	        : nullptr;
	const bool bCanChain = Branch && (!Branch->bResonanceOnly || IsResonanceActive());
	const auto* Upcoming =
	    bCanChain ? Branch
		          : (bHasBufferedInput
	                     ? AttackSet->Find(BufferedInput == EPlayerAttackInput::Light ? TEXT("L") : TEXT("H"))
	                     : nullptr);
	// 备用重击蓄力从按键拍点开始，命中另加蓄力拍数
	const double UpWindup = Upcoming ? WindupBeats(*Upcoming, IsResonanceActive()) : 0.;
	// 早按的输入只等待固定接续拍点，不立即开始下一次挥击
	const double Slot = ComboInputAudio; // 下一击仍安排在既定拍点，不因连续按键加快
	const double FireAt = UpWindup > 0. ? HeavyPressAudio : (Upcoming ? Slot - StrikeLead(*Upcoming) : 0.);
	if (bHasBufferedInput && Upcoming && Now >= FireAt)
	{
		const auto Input = BufferedInput;
		const double Timestamp = BufferedInputTime;
		const FName Finished = CurrentAttack;
		const double FinishedAt = UpWindup > 0. ? HeavyPressAudio : Slot;
		const double TargetAudio =
		    UpWindup > 0. ? HeavyPressAudio + UpWindup * Clock()->GetSecondsPerBeat() : Slot;
		ChainNode = Finished;
		LastChainAudio = FinishedAt;
		if (bCanChain && StartAttack(Input, Timestamp, true, TargetAudio))
			return;
		// 当前分支不能接这个键时，尝试同一拍点重新起手
		// 重新起手也失败就取消旧动作，避免留下无效的连招状态
		UAnimMontage* OldMontage = AttackMontage.Get();
		CurrentAttack = NAME_None;
		ChainNode = NAME_None;
		bHasBufferedInput = false;
		InputSequence.Empty();
		if (StartAttack(Input, Timestamp, false, TargetAudio))
			return;
		AttackMontage = OldMontage;
		CurrentAttack = Finished;
		CancelAttack();
		return;
	}
	if (Now >= EndAudio)
	{
		const auto* Light = AttackSet->Find(Node->NextLight);
		const auto* Heavy = AttackSet->Find(Node->NextHeavy);
		const bool CanLink = (Light && (!Light->bResonanceOnly || IsResonanceActive())) ||
		                     (Heavy && (!Heavy->bResonanceOnly || IsResonanceActive()));
		// 收招结束后立即淡出动画，逻辑上的连招窗口可以继续保留
		// 当前不启用停住姿态等待按键的分支
		const bool bHoldForLink = false;
		if (!bAttackVisualEnded && !bHoldForLink)
		{
			if (auto* C = Cast<ACharacter>(GetOwner()))
				if (auto* Anim = Cast<UPlayerCombatAnimInstance>(C->GetMesh()->GetAnimInstance()))
					Anim->EndBeatAttack(AttackMontage, .28f);
			bAttackVisualEnded = true;
		}
		// 终结技期间已经缓存的按键仍可尝试重新起手，超过窗口才清除
		if ((!CanLink && !bHasBufferedInput) || Now > LatestCueAudio + ChainLateSeconds())
			CancelAttack();
	}
}

// 记录破绽中的有效命中，并检查是否达到填满条件
void UPlayerBeatAttackComponent::RegisterOpeningHit(const FBossOpening& Opening, bool bGap)
{
	OpeningHitMarks.Add({ImpactAudio, bGap});
	if (OpeningHitMarks.Num() > 24)
		OpeningHitMarks.RemoveAt(0);
	LastOpeningHitWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : -1000.;
	bLastOpeningHitGap = bGap;
	// 换到新的破绽后重新计数，不能把上一个破绽的命中带过来
	if (!FMath::IsNearlyEqual(OpeningKey, Opening.StartAudio, .01))
	{
		OpeningKey = Opening.StartAudio;
		OpeningHits = 0;
		bOpeningFilled = false;
	}
	++OpeningHits;
	// 按破绽拍数计算填满所需命中次数，至少两次
	const double SPB = Clock() ? Clock()->GetSecondsPerBeat() : .5;
	const int32 Needed = FMath::Max(2, FMath::FloorToInt((Opening.EndAudio - Opening.StartAudio) / SPB) + 1);
	if (!bOpeningFilled && OpeningHits >= Needed)
	{
		bOpeningFilled = true;
		FillWorldTime = LastOpeningHitWorldTime;
		FillAudio = ImpactAudio;
		bFillGap = bGap;
		ReceiveReward(EPlayerAttackGrade::Perfect, bGap ? 8.f : 5.f, true);
		// 填满破绽时不额外播放升级提示音
		UE_LOG(LogTemp, Display, TEXT("PlayerAttack: FILL gap=%d hits=%d"), bGap ? 1 : 0, OpeningHits);
	}
}

// 取得双腿检测端点，供踢击接触检测使用
void UPlayerBeatAttackComponent::SampleLegs(FVector OutA[2], FVector OutB[2]) const
{
	const auto* C = Cast<ACharacter>(GetOwner());
	const USkeletalMeshComponent* Mesh = C ? C->GetMesh() : nullptr;
	for (int32 I = 0; I < 2; ++I)
	{
		OutA[I] = Mesh ? Mesh->GetSocketLocation(I ? TEXT("calf_r") : TEXT("calf_l")) : FVector::ZeroVector;
		OutB[I] = Mesh ? Mesh->GetSocketLocation(I ? TEXT("ball_r") : TEXT("ball_l")) : FVector::ZeroVector;
	}
}

// 接触检测：沿刀刃的运动检查碰撞，并记录本次已命中的对象，避免一击逐帧重复结算
void UPlayerBeatAttackComponent::SweepBlade(double Now)
{
	const auto* Node = AttackSet->Find(CurrentAttack);
	auto* Sword = GetOwner()->FindComponentByClass<UStaticMeshComponent>();
	if (!Sword || !Node)
		return;
	// 读取已经同步的剑姿态，握持偏移由武器组件维护
	const FVector Base = Sword->GetComponentLocation();
	const FVector Tip =
	    Base + Sword->GetComponentTransform().TransformVectorNoScale(FVector::YAxisVector) * 150.f;
	// 只处理经过命中前后 40 毫秒的帧区间，LastAudio 让跨过窗口的低帧率帧仍能检查
	// 这是音乐时间窗口加轨迹检测，不是动画通知开启的碰撞体
	if (Now >= ImpactAudio - .04 && LastAudio <= ImpactAudio + .04)
	{
		FCollisionQueryParams Query(SCENE_QUERY_STAT(PlayerBlade), false, GetOwner());
		// 先检查武器轨迹与 Boss 胶囊是否接触，再检查两者之间的遮挡
		// 不要求 Boss 胶囊阻挡 Visibility 通道
		TArray<FHitResult> Contacts;
		if (AActor* Target = AttackTarget.Get(); Target && !HitActors.Contains(Target))
		{
			if (auto* Capsule = Target->FindComponentByClass<UCapsuleComponent>())
			{
				const FVector Center = Capsule->GetComponentLocation();
				const float Radius = Capsule->GetScaledCapsuleRadius();
				const FVector HalfAxis =
				    FVector::UpVector * FMath::Max(0.f, Capsule->GetScaledCapsuleHalfHeight() - Radius);
				float Closest = MAX_flt;
				FVector Contact = Center;
				// 剑和踢击都检查上一帧到当前帧的经过路径，减少高速动作漏检
				FVector LegA[2], LegB[2];
				SampleLegs(LegA, LegB);

				struct FSweep
				{
					FVector A0, B0, A1, B1;
				};

				TArray<FSweep, TInlineAllocator<3>> Sweeps;
				Sweeps.Add({LastBladeBase, LastBladeTip, Base, Tip});
				if (Node->bKick)
					for (int32 L = 0; L < 2; ++L)
						Sweeps.Add({LastLegA[L], LastLegB[L], LegA[L], LegB[L]});
				for (const FSweep& S : Sweeps)
					for (int32 I = 0; I <= 10; ++I)
					{
						FVector OnBlade, OnBody;
						FMath::SegmentDistToSegmentSafe(FMath::Lerp(S.A0, S.B0, I / 10.f),
						                                FMath::Lerp(S.A1, S.B1, I / 10.f), Center - HalfAxis,
						                                Center + HalfAxis, OnBlade, OnBody);
						const float Distance = FVector::Distance(OnBlade, OnBody);
						if (Distance < Closest)
						{
							Closest = Distance;
							Contact = OnBlade;
						}
					}
				Query.AddIgnoredActor(Target);
				FHitResult Wall;
				if (FParse::Param(FCommandLine::Get(), TEXT("PlayerAttackProbe")) && AttackSerial <= 3)
				{
					UE_LOG(
					    LogTemp, Display,
					    TEXT(
					        "PlayerAttackProbe: blade id=%llu node=%s closest=%.1f radius=%.1f base=%s tip=%s target=%s"),
					    AttackSerial, *CurrentAttack.ToString(), Closest, Radius, *Base.ToCompactString(),
					    *Tip.ToCompactString(), *Center.ToCompactString());
				}
				const bool Blocked = GetWorld()->LineTraceSingleByChannel(
				    Wall, GetOwner()->GetActorLocation(), Center, ECC_Visibility, Query);
				if (Closest <= Radius + 16.f && !Blocked)
				{
					FHitResult Hit(Target, Capsule, Contact, FVector::UpVector);
					Hit.ImpactPoint = Contact;
					Contacts.Add(Hit);
				}
			}
		}
		for (const auto& Hit : Contacts)
		{
			AActor* Target = Hit.GetActor();
			if (!Target || HitActors.Contains(Target))
				continue;
			auto* Boss = Target->FindComponentByClass<UBeatSyncedComboComponent>();
			if (!Boss || Boss->BossHealth <= 0 || Boss->IsPhaseTransitionActive())
				continue;
			// 先登记本击已经命中的目标，再触发伤害和回调，防止重复接触重复扣血
			HitActors.Add(Target);
			// 先记录接触时 Boss 是否存在破绽，再扣血和削韧
			// 本次攻击造成的硬直不能反过来增加本次伤害
			FBossOpening Opening;
			bool bOpen = Boss->GetOpeningAt(ImpactAudio, Opening);
			if (bOpen && Opening.Kind == EBossOpeningKind::Sidestep)
			{
				const FVector BossToPlayer =
				    (GetOwner()->GetActorLocation() - Boss->GetOwner()->GetActorLocation()).GetSafeNormal2D();
				bOpen = FVector::DotProduct(Boss->GetOwner()->GetActorForwardVector(), BossToPlayer) < .45f;
			}
			const bool bGap = bOpen && Opening.Kind == EBossOpeningKind::Gap;
			const bool bSidestep = bOpen && Opening.Kind == EBossOpeningKind::Sidestep;
			const float OpenMul = bGap ? 2.f : bSidestep ? 1.5f : bOpen ? 1.2f : 1.f;
			const float Damage = Node->Damage * OpenMul;
			// 第三次重击触发挑飞表现，其余重击仍走普通受击处理
			const bool bLauncher = Node->Input == EPlayerAttackInput::Heavy && Node->Id == FName(TEXT("HHH"));
			Boss->ReceivePlayerStrike(Damage, Node->PoiseDamage * (bGap ? 1.5f : 1.f), GetOwner(),
			                          Node->Input == EPlayerAttackInput::Heavy || Node->bFinisher,
			                          Hit.ImpactPoint, bLauncher);
			// 一次挥击只发放一次命中奖励，命中记录与奖励标记分别控制
			if (!bRewarded)
			{
				bRewarded = true;
				float Gain = Node->Input == EPlayerAttackInput::Heavy ? 6.f : 4.f;
				if (LastGrade == EPlayerAttackGrade::Good)
					Gain *= .5f;
				if (Node->bFinisher && LastGrade == EPlayerAttackGrade::Perfect)
					Gain += 4.f;
				ReceiveReward(LastGrade, Gain * OpenMul, true);
			}
			if (bOpen)
				RegisterOpeningHit(Opening, bGap);
			PresentImpact(true, Hit.ImpactPoint, Target);
			OnAttackHit.Broadcast(Target, Damage, LastGrade);
			UE_LOG(LogTemp, Display, TEXT("PlayerAttack: HIT id=%llu target=%s damage=%.1f resonance=%.1f"),
			       AttackSerial, *Target->GetName(), Damage, Combat->Resonance);
		}
	}
	LastBladeBase = Base;
	LastBladeTip = Tip;
	SampleLegs(LastLegA, LastLegB);
}

// 记录攻击收益，更新共鸣与命中反馈状态
void UPlayerBeatAttackComponent::ReceiveReward(EPlayerAttackGrade Grade, float Amount, bool bHit)
{
	if (!Combat.IsValid() || !bHit || Grade == EPlayerAttackGrade::Bad)
		return;
	const float Previous = Combat->Resonance;
	Combat->AwardPlayerAttack(Grade == EPlayerAttackGrade::Perfect, IsResonanceActive() ? 0.f : Amount);
	LastResonanceGain = Combat->Resonance - Previous;
	LastHitWorldSeconds = GetWorld()->GetTimeSeconds();
	Combat->LastJudgmentErrorMs = LastErrorMs;
}

// 按命中结果和评级组合动作停顿、声音、特效与镜头
void UPlayerBeatAttackComponent::PresentImpact(bool bHit, FVector Location, AActor* Target)
{
	if (!bHit)
		return;
	const auto* Node = AttackSet->Find(CurrentAttack);
	const bool bHeavy = Node && Node->Input == EPlayerAttackInput::Heavy;
	const bool bFinisher = Node && Node->bFinisher;
	const bool bKick = Node && Node->bKick;
	const bool bLauncher = bHeavy && Node->Id == FName(TEXT("HHH"));
	const bool bPerfect = LastGrade == EPlayerAttackGrade::Perfect,
	           bGood = LastGrade == EPlayerAttackGrade::Good;
	// 节奏评级调整命中表现强度，偏拍仍有反馈，踩拍收尾最强

	const float Grade = bPerfect ? 1.f : bGood ? .7f : .4f;
	const float Power = Grade * (bFinisher ? 1.6f : 1.f) * (IsResonanceActive() ? 1.25f : 1.f);
	const int32 Step = FMath::Clamp(InputSequence.Len() / 2 - 1, 0, 4); // 轻击第一段到第三段对应索引零到二
	const float Pitch = 1.f + .06f * Step;
	const FVector Forward = GetOwner()->GetActorForwardVector();
	UWorld* World = GetWorld();

	// 先记录命中停顿，玩家姿态按音乐时间恢复，敌人网格单独恢复速度

	HitStopSeconds = (bPerfect ? .07f
	                  : bGood  ? .05f
	                           : .03f) +
	                 (bFinisher ? .03f : 0.f) + (bKick ? .05f : 0.f) + (bHeavy ? .02f : 0.f) +
	                 (bLauncher ? .03f : 0.f);
	HitStopEase = (bPerfect ? 2.1f : bGood ? 1.8f : 1.5f) + (bKick ? .3f : 0.f);
	// 检测略早发生时仍停在配置的接触姿态
	HitStopStartAudio = Clock() ? FMath::Max(Clock()->GetAudioNowSeconds(), ImpactAudio) : -1000.;

	// 根据轻重击和连段选择命中声音，再按节奏评级调整音量


	{
		const TArray<TObjectPtr<USoundBase>>& Set = bHeavy ? HeavyHitSounds : LightHitSounds;
		USoundBase* Main = bFinisher && FinisherHitSound ? FinisherHitSound.Get()
		                   : bKick && KickHitSound       ? KickHitSound.Get()
		                   : Set.Num()                   ? Set[FMath::Min(Step, Set.Num() - 1)].Get()
		                                                 : nullptr;
		if (!bPerfect && !bGood && OffBeatHitSound)
			Main = OffBeatHitSound;
		if (Main)
			UGameplayStatics::PlaySound2D(this, Main, bPerfect ? .78f : bGood ? .68f : .60f, 1.f);
		// 只播放选中的命中音，不继续叠加刀刃和铃声音层
	}

	// 记录界面需要的命中文字、位置与强度，强力命中可附带短暂闪白


	{
		static const TCHAR* HeavyWords[] = {TEXT("BAM!"), TEXT("WHAM!"), TEXT("SMASH!"), TEXT("KRAKOOM!")};
		static const TCHAR* LightWords[] = {TEXT("POW!"), TEXT("BAP!"), TEXT("PAK!")};
		const bool bBig = bHeavy || bFinisher || bKick;
		ImpactFxWorld = Location;
		ImpactFxWorldTime = World ? World->GetTimeSeconds() : -1000.;
		ImpactFxPower = FMath::Clamp(Power, .4f, 1.8f);
		bImpactFxHeavy = bBig;
		bImpactFxPerfect = bPerfect;
		bImpactFxFrame = bBig && bPerfect; // 只有完美的强力命中触发轻微闪白
		bImpactFxLaunch = bLauncher;
		ImpactFxWord = (!bPerfect && !bGood) ? FString()
		               : bFinisher           ? FString(bHeavy ? TEXT("KRAKOOM!") : TEXT("BOOM!"))
		               : bKick               ? FString(TEXT("THWACK!"))
		               : bLauncher           ? FString(TEXT("LAUNCH!"))
		               : bHeavy              ? FString(HeavyWords[Step % 4])
		               : bPerfect            ? FString(LightWords[Step % 3])
		                                     : FString();
		if ((bHeavy || bFinisher) && !bKick)
			if (auto* Sword = GetOwner()->FindComponentByClass<UStaticMeshComponent>())
			{
				TipBurstWorld =
				    Sword->GetComponentLocation() +
				    Sword->GetComponentTransform().TransformVectorNoScale(FVector::YAxisVector) * 120.f;
				TipBurstWorldTime = ImpactFxWorldTime;
			}
	}

	// 在接触点生成火花，大小随节奏评级变化
	if (HitEffect)
		RetireCombatFX(UGameplayStatics::SpawnEmitterAtLocation(
		                   World, HitEffect, Location, Forward.Rotation(), FVector(.8f + 1.1f * Power), true),
		               .03f, .12f);
	// 重击和收尾使用界面刀尖爆发效果
	if ((bPerfect || bFinisher) && !bHeavy && !bFinisher && PerfectBurstEffect)
	{
		// 短暂发射后停止，保留粒子的消散过程
		UParticleSystem* Burst =
		    LoadObject<UParticleSystem>(nullptr, TEXT("/Game/SwordAnim/demo/FX/P_Spark.P_Spark"));
		if (Burst)
			RetireCombatFX(UGameplayStatics::SpawnEmitterAtLocation(
			                   World, Burst, Location, Forward.Rotation(), FVector(.65f + .6f * Power), true),
			               .03f, .15f);
	}

	// 强拍踢击使用完整镜头反馈，其他拍点按概率选择较强反馈

	bool bKickDownbeat = false;
	if (const auto* KC = Clock(); bKick && KC && KC->GetSecondsPerBeat() > 0.)
		bKickDownbeat =
		    FMath::RoundToInt((ImpactAudio - KC->GetBeatTimeSeconds(0.)) / KC->GetSecondsPerBeat()) %
		        FMath::Max(1, KC->GetBeatsPerBar()) ==
		    0;
	const float KickWeight = bKickDownbeat ? 1.f : .6f;
	const bool bKickCamera = bKick && FMath::FRand() < (bKickDownbeat ? .95f : bPerfect ? .7f : .5f);
	// 敌人的闪光、推移和停顿由表现函数处理，韧性与受击由 ReceivePlayerStrike 处理
	if (Target)
		if (auto* Boss = Target->FindComponentByClass<UBeatSyncedComboComponent>())
		{
			Boss->PlayPlayerHitFeedback(GetOwner(), Power * (bKick ? 1.5f : 1.f), HitStopSeconds + .02f);
			// 踢击停顿后再执行击退
			if (bKick)
				Boss->StartKickKnockback(GetOwner(), (110.f + 30.f * Grade) + 60.f * KickWeight, .26f);
		}

	// 按节奏评级调整镜头视角和震动
	if (IsResonanceActive())
		if (auto* FX = GetOwner()->FindComponentByClass<UPerfectDodgeFeedbackComponent>())
			FX->PlayResonanceAttack(bFinisher);
	if (auto* Camera = GetOwner()->FindComponentByClass<UBossCombatCameraComponent>(); Camera && bKick)
	// 踢击拉近镜头并向踢出方向侧倾
	{
		{
			if (bKickCamera) // 强拍使用完整踢击镜头，其他拍点减弱
				Camera->TriggerKickAccent(2.f + 2.5f * KickWeight + 1.5f * Grade * KickWeight,
				                          1.f + 1.5f * KickWeight + 1.5f * Grade * KickWeight,
				                          .1f + .25f * KickWeight + .2f * Grade * KickWeight,
				                          .14f * KickWeight + .08f * Grade * KickWeight, Location,
				                          HitStopSeconds + .06f * KickWeight);
			else // 普通轻击的轻微拉近
				Camera->TriggerKickAccent(2.5f + 1.5f * Grade, 1.f + 1.f * Grade, .15f + .1f * Grade, 0.f,
				                          Location, HitStopSeconds);
		}
	}
	else if (Camera && bLauncher)
	{
		// 挑飞使用跟随 Boss 上升的侧面特写
		if (Target)
			Camera->TriggerLaunchShot(Target, 1.3f);
		Camera->TriggerKickAccent(1.5f + 1.f * Grade, .4f + .4f * Grade, 0.f, 0.f, Location,
		                          HitStopSeconds); // 此处不额外抬高镜头俯仰
	}
	else if (Camera && bHeavy)
		// 重击使用拉近和轻微侧倾，强度低于踢击
		Camera->TriggerKickAccent(2.f + 1.3f * Grade, .5f + .7f * Grade, .1f + .08f * Grade, 0.f, Location,
		                          HitStopSeconds);
	else if (Camera)
		// 普通命中只做小幅镜头变化，主要冲击感由动作停顿、受击、声音和闪光提供


		// 普通命中不使用持续震荡，只平滑调整视角和俯仰

		Camera->TriggerPerfectDodgeFeedback(.25f + .4f * Power, 0.f, 1.f, Location, Forward, bPerfect, 0.f,
		                                    0.f, 0.f, .02f + .03f * Power,
		                                    .08f); // 降低轻击的镜头变化

	// 更新屏幕效果并触发手柄震动
	PunchStartWorld = World ? World->GetTimeSeconds() : -1000.;
	PunchStrength = Power * (bKick ? 1.35f : bHeavy ? 1.3f : 1.f);
	if (auto* PC = Cast<APlayerController>(Cast<APawn>(GetOwner()) ? Cast<APawn>(GetOwner())->GetController()
	                                                               : nullptr))
		PC->PlayDynamicForceFeedback(FMath::Clamp(.35f + .4f * Power, 0.f, 1.f), .08f + .06f * Power, true,
		                             true, true, true);
}

// 衰减命中造成的色散与暗角，结束后恢复原值
void UPlayerBeatAttackComponent::TickScreenPunch()
{
	auto* Player = Cast<AHearBladeCharacter>(GetOwner());
	UCameraComponent* Camera = Player ? Player->GetFollowCamera() : nullptr;
	if (!Camera || !GetWorld())
		return;
	auto& PP = Camera->PostProcessSettings;
	if (BaseFringe < 0.f)
	{
		BaseFringe = PP.bOverride_SceneFringeIntensity ? PP.SceneFringeIntensity : 0.f;
		BaseVignette = PP.bOverride_VignetteIntensity ? PP.VignetteIntensity : .4f;
	}
	const float Age = float(GetWorld()->GetTimeSeconds() - PunchStartWorld),
	            Duration = .08f + .04f * PunchStrength;
	const float Env = Age >= 0.f && Age < Duration ? FMath::Square(1.f - Age / Duration) : 0.f;
	// 限制色散和暗角强度，减少连续命中时的闪烁
	PP.bOverride_SceneFringeIntensity = true;
	PP.SceneFringeIntensity = BaseFringe + Env * .6f * PunchStrength;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = BaseVignette + Env * .12f * PunchStrength;
}

// 切换共鸣状态并通知监听者
void UPlayerBeatAttackComponent::SetResonanceState(EPlayerResonanceState State)
{
	if (State == ResonanceState)
		return;
	ResonanceState = State;
	OnResonanceChanged.Broadcast(State);
	UE_LOG(LogTemp, Display, TEXT("PlayerResonance: state=%d beat=%.4f"), int(State),
	       Clock() ? Clock()->GetCurrentBeat() : 0.);
}

// 检查共鸣条件，通过后等待指定拍点启动
bool UPlayerBeatAttackComponent::TryActivateResonance()
{
	if (!Combat.IsValid() || Combat->Health <= 0 || Combat->IsPhaseCinematic() || !Clock() ||
	    !Clock()->IsRunning())
		return false;
	if (Combat->Resonance >= Combat->MaxResonance && ResonanceState == EPlayerResonanceState::Filling)
		SetResonanceState(EPlayerResonanceState::Ready);
	if (ResonanceState != EPlayerResonanceState::Ready)
		return false;
	AActor* Target = FindTarget();
	auto* Boss = Target ? Target->FindComponentByClass<UBeatSyncedComboComponent>() : nullptr;
	if (!Boss || Boss->BossHealth <= 0 || Boss->IsPhaseTransitionActive())
		return false;
	ResonanceBoss = Boss;
	const int32 Bar = Clock()->GetBeatsPerBar();
	// 按下共鸣键后预约下一小节开始，实际消耗和效果到该拍再启动
	ArmedBeat = (FMath::FloorToDouble(Clock()->GetCurrentBeat() / Bar) + 1.) * Bar;
	SetResonanceState(EPlayerResonanceState::Armed);
	return true;
}

// 返回当前共鸣剩余拍数
float UPlayerBeatAttackComponent::GetRemainingBeats() const
{
	return IsResonanceActive() && Clock()
	           ? float(FMath::Clamp(ActiveStartBeat + 16. - Clock()->GetCurrentBeat(), 0., 16.))
			   : 0.f;
}

// 结束共鸣并通知配合的 Boss 恢复状态
void UPlayerBeatAttackComponent::EndResonance()
{
	if (ResonanceState != EPlayerResonanceState::Active && ResonanceState != EPlayerResonanceState::Armed)
		return;
	const bool WasActive = IsResonanceActive();
	if (ResonanceBoss.IsValid())
		ResonanceBoss->EndPlayerResonance();
	ResonanceBoss.Reset();
	// 只在共鸣已经启动后清空能量，预约阶段取消则保留就绪状态
	if (WasActive && Combat.IsValid())
		Combat->Resonance = 0.f;
	SetResonanceState(WasActive ? EPlayerResonanceState::Filling : EPlayerResonanceState::Ready);
}

// 逐帧推进：把当前音乐时间换算为动画位置，再处理根位移、接触与攻击结束
// StartAudio 和 ImpactAudio 确定攻击的时间范围；动画位置用于提取本帧位移
void UPlayerBeatAttackComponent::TickComponent(float Delta, ELevelTick TickType,
                                               FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(Delta, TickType, TickFunction);
	TickRuntimeProbe();
	TickScreenPunch();
	if (!Combat.IsValid() || !Clock())
		return;
	if (Combat->Health <= 0 || Combat->IsPhaseCinematic() || !Clock()->IsRunning())
	{
		Reset();
		return;
	}
	// 动作空闲后重试暂存攻击，超过有效期则丢弃，防止很久后突然出招
	if (bHasPendingAttack && !IsAttacking())
	{
		const double AudioNow = Clock()->GetAudioNowSeconds();
		if (GetWorld()->GetTimeSeconds() > PendingAttackDeadline)
			bHasPendingAttack = false;
		else if (AudioNow >= RecoveryUntilAudio && AudioNow >= NextStartAllowedAudio &&
		         Combat->PrepareForOrdinaryAttack() && StartAttack(PendingAttack, PendingAttackAudio))
			bHasPendingAttack = false;
	}
	if (Combat->IsPerformingAction() && IsAttacking())
		CancelAttack();
	if (IsAttacking())
		TickAttack(Clock()->GetAudioNowSeconds(), Delta);
	if (ResonanceState == EPlayerResonanceState::Filling && Combat->Resonance >= Combat->MaxResonance)
		SetResonanceState(EPlayerResonanceState::Ready);
	if (ResonanceState == EPlayerResonanceState::Armed || IsResonanceActive())
	{
		if (!ResonanceBoss.IsValid() || ResonanceBoss->BossHealth <= 0 ||
		    ResonanceBoss->IsPhaseTransitionActive())
		{
			EndResonance();
			return;
		}
		if (ResonanceState == EPlayerResonanceState::Armed && Clock()->GetCurrentBeat() >= ArmedBeat)
		{
			ActiveStartBeat = ArmedBeat;
			SetResonanceState(EPlayerResonanceState::Active);
			ResonanceBoss->BeginPlayerResonance(ActiveStartBeat, GetOwner());
			if (auto* FX = GetOwner()->FindComponentByClass<UPerfectDodgeFeedbackComponent>())
				FX->PlayResonanceAttack(true);
			if (StartSound)
				UGameplayStatics::PlaySound2D(this, StartSound, .65f);
			if (auto* Camera = GetOwner()->FindComponentByClass<UBossCombatCameraComponent>())
				Camera->TriggerPerfectDodgeFeedback(5.f, .3f, 1.3f, GetOwner()->GetActorLocation(),
				                                    GetOwner()->GetActorForwardVector(), true, 0.f, 8.f, 0.f,
				                                    .1f, .4f);
		}
		if (IsResonanceActive())
		{
			// 能量显示按剩余拍数递减，音乐速度变化时仍保持十六拍持续时间
			Combat->Resonance = Combat->MaxResonance * GetRemainingBeats() / 16.f;
			if (GetRemainingBeats() <= 0.f)
				EndResonance();
		}
	}
}
