// Copyright Epic Games, Inc. All Rights Reserved.

#include "TPSAIController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Kismet/GameplayStatics.h"
#include "../Enemy/EnemyCharacter.h"
#include "TPSBTTask_AttackTarget.h"
#include "TPSBTTask_ChaseTarget.h"
#include "TPSBTTask_Patrol.h"

const FName ATPSAIController::TargetActorKey(TEXT("TargetActor"));
const FName ATPSAIController::LastKnownTargetLocationKey(TEXT("LastKnownTargetLocation"));

ATPSAIController::ATPSAIController()
{
	bAttachToPawn = true;
	PrimaryActorTick.bCanEverTick = true;

	BehaviorTreeComponent = CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("BehaviorTreeComponent"));
	Blackboard = CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComponent"));
	BrainComponent = BehaviorTreeComponent;
}

void ATPSAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	StartEnemyBehavior();
}

void ATPSAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateBlackboardTarget();
}

void ATPSAIController::StartEnemyBehavior()
{
	if (!Cast<AEnemyCharacter>(GetPawn()))
	{
		return;
	}

	UBlackboardData* EffectiveBlackboard = GetBlackboardData();
	UBehaviorTree* EffectiveBehaviorTree = GetBehaviorTree();
	if (!EffectiveBlackboard || !EffectiveBehaviorTree)
	{
		return;
	}

	UBlackboardComponent* BlackboardComponent = Blackboard.Get();
	if (UseBlackboard(EffectiveBlackboard, BlackboardComponent))
	{
		Blackboard = BlackboardComponent;
		RunBehaviorTree(EffectiveBehaviorTree);
		UpdateBlackboardTarget();
	}
}

void ATPSAIController::UpdateBlackboardTarget()
{
	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetPawn());
	if (!Enemy || !Blackboard)
	{
		return;
	}

	if (Enemy->GetEnemyState() == EEnemyState::Dead)
	{
		Blackboard->ClearValue(TargetActorKey);
		return;
	}

	AActor* TargetActor = Enemy->GetTargetActor();
	if (TargetActor)
	{
		if (Enemy->CanSeeTarget(TargetActor))
		{
			Enemy->SetAITarget(TargetActor);
		}

		if (Enemy->ShouldLoseTarget())
		{
			Enemy->ClearAITarget();
			Blackboard->ClearValue(TargetActorKey);
			return;
		}
	}
	else if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (Enemy->CanSeeTarget(PlayerPawn))
		{
			Enemy->SetAITarget(PlayerPawn);
			TargetActor = PlayerPawn;
		}
	}

	if (TargetActor)
	{
		Blackboard->SetValueAsObject(TargetActorKey, TargetActor);
		Blackboard->SetValueAsVector(LastKnownTargetLocationKey, Enemy->GetLastKnownTargetLocation());
	}
	else
	{
		Blackboard->ClearValue(TargetActorKey);
	}
}

UBehaviorTree* ATPSAIController::GetBehaviorTree()
{
	if (BehaviorTreeAsset)
	{
		return BehaviorTreeAsset;
	}

	if (RuntimeBehaviorTree)
	{
		return RuntimeBehaviorTree;
	}

	RuntimeBehaviorTree = NewObject<UBehaviorTree>(this, TEXT("BT_RuntimeEnemy"));
	RuntimeBehaviorTree->BlackboardAsset = GetBlackboardData();

	UBTComposite_Selector* RootSelector = NewObject<UBTComposite_Selector>(RuntimeBehaviorTree, TEXT("RootSelector"));
	RuntimeBehaviorTree->RootNode = RootSelector;

	FBTCompositeChild AttackChild;
	AttackChild.ChildTask = NewObject<UTPSBTTask_AttackTarget>(RootSelector, TEXT("AttackTarget"));
	RootSelector->Children.Add(AttackChild);

	FBTCompositeChild ChaseChild;
	ChaseChild.ChildTask = NewObject<UTPSBTTask_ChaseTarget>(RootSelector, TEXT("ChaseTarget"));
	RootSelector->Children.Add(ChaseChild);

	FBTCompositeChild PatrolChild;
	PatrolChild.ChildTask = NewObject<UTPSBTTask_Patrol>(RootSelector, TEXT("Patrol"));
	RootSelector->Children.Add(PatrolChild);

	return RuntimeBehaviorTree;
}

UBlackboardData* ATPSAIController::GetBlackboardData()
{
	if (BlackboardAsset)
	{
		return BlackboardAsset;
	}

	if (BehaviorTreeAsset && BehaviorTreeAsset->BlackboardAsset)
	{
		return BehaviorTreeAsset->BlackboardAsset;
	}

	if (RuntimeBlackboardData)
	{
		return RuntimeBlackboardData;
	}

	RuntimeBlackboardData = NewObject<UBlackboardData>(this, TEXT("BB_RuntimeEnemy"));

	FBlackboardEntry TargetActorEntry;
	TargetActorEntry.EntryName = TargetActorKey;
	UBlackboardKeyType_Object* TargetActorType = NewObject<UBlackboardKeyType_Object>(RuntimeBlackboardData);
	TargetActorType->BaseClass = AActor::StaticClass();
	TargetActorEntry.KeyType = TargetActorType;
	RuntimeBlackboardData->Keys.Add(TargetActorEntry);

	FBlackboardEntry LastKnownLocationEntry;
	LastKnownLocationEntry.EntryName = LastKnownTargetLocationKey;
	LastKnownLocationEntry.KeyType = NewObject<UBlackboardKeyType_Vector>(RuntimeBlackboardData);
	RuntimeBlackboardData->Keys.Add(LastKnownLocationEntry);

	return RuntimeBlackboardData;
}
