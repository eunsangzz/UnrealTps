// Copyright Epic Games, Inc. All Rights Reserved.

#include "TPSBTTask_Patrol.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "TPSAIController.h"
#include "../Enemy/EnemyCharacter.h"

UTPSBTTask_Patrol::UTPSBTTask_Patrol()
{
	NodeName = TEXT("Patrol");
	bNotifyTick = true;
}

EBTNodeResult::Type UTPSBTTask_Patrol::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AEnemyCharacter* Enemy = OwnerComp.GetAIOwner() ? Cast<AEnemyCharacter>(OwnerComp.GetAIOwner()->GetPawn()) : nullptr;
	if (!Enemy || Enemy->GetEnemyState() == EEnemyState::Dead)
	{
		return EBTNodeResult::Failed;
	}

	Enemy->SetAIState(EEnemyState::Patrol);
	Enemy->SelectPatrolDestination();
	return EBTNodeResult::InProgress;
}

void UTPSBTTask_Patrol::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (BlackboardComp && BlackboardComp->GetValueAsObject(ATPSAIController::TargetActorKey))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	AEnemyCharacter* Enemy = OwnerComp.GetAIOwner() ? Cast<AEnemyCharacter>(OwnerComp.GetAIOwner()->GetPawn()) : nullptr;
	if (!Enemy || Enemy->GetEnemyState() == EEnemyState::Dead)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	if (Enemy->HasReachedPatrolDestination())
	{
		Enemy->SelectPatrolDestination();
	}

	const FVector Destination = Enemy->GetPatrolDestination();
	const bool bFollowingNavigationPath = Enemy->RequestAIMove(Destination, Enemy->GetPatrolAcceptanceRadius());
	if (!bFollowingNavigationPath || Enemy->GetVelocity().SizeSquared2D() < FMath::Square(Enemy->GetPatrolSpeed() * 0.35f))
	{
		Enemy->MoveDirectlyTo(Destination);
	}
}
