// 连接敌人、行为树和战斗组件，管理行为运行与退出
#include "Combat/Boss/BossBeatDirector.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Combat/Boss/BeatSyncedComboComponent.h"
#include "TimerManager.h"

ABossBeatDirector::ABossBeatDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	BlackboardComponent = CreateDefaultSubobject<UBlackboardComponent>(TEXT("BossBlackboard"));
	BehaviorTreeComponent = CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("BossBehaviorTree"));
}

void ABossBeatDirector::BeginPlay()
{
	Super::BeginPlay();
	if (!IsValid(BossActor) || !BossActor->FindComponentByClass<UBeatSyncedComboComponent>() ||
	    !IsValid(BehaviorTreeAsset) || !IsValid(BehaviorTreeAsset->BlackboardAsset))
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("BossBeatDirector: 请设置 BossActor（含 BeatSyncedCombo）和带黑板的 BehaviorTreeAsset"));
		return;
	}
	if (!BlackboardComponent->InitializeBlackboard(*BehaviorTreeAsset->BlackboardAsset))
	{
		UE_LOG(LogTemp, Warning, TEXT("BossBeatDirector: 黑板初始化失败"));
		return;
	}
	if (BlackboardComponent->GetKeyID(TEXT("BossActor")) == FBlackboard::InvalidKey)
	{
		UE_LOG(LogTemp, Warning, TEXT("BossBeatDirector: 黑板需要名为 BossActor 的 Object 键"));
		return;
	}
	BlackboardComponent->SetValueAsObject(TEXT("BossActor"), BossActor);
	BehaviorTreeComponent->CacheBlackboardComponent(BlackboardComponent);
	UE_LOG(LogTemp, Display, TEXT("BossBeatDirector: Boss=%s Tree=%s"), *GetNameSafe(BossActor),
	       *GetNameSafe(BehaviorTreeAsset));
	// Let every actor finish BeginPlay first, especially the actor starting the music clock.
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(
	    this,
	    [this]()
	    {
		    if (IsValid(BehaviorTreeAsset) && IsValid(BehaviorTreeComponent))
		    {
			    BehaviorTreeComponent->StartTree(*BehaviorTreeAsset, EBTExecutionMode::Looped);
		    }
	    }));
}

void ABossBeatDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BehaviorTreeComponent)
		BehaviorTreeComponent->StopTree(EBTStopMode::Forced);
	Super::EndPlay(EndPlayReason);
}
