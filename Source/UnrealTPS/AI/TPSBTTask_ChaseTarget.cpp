// Copyright Epic Games, Inc. All Rights Reserved.

#include "TPSBTTask_ChaseTarget.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "TPSAIController.h"
#include "../Enemy/EnemyCharacter.h"

UTPSBTTask_ChaseTarget::UTPSBTTask_ChaseTarget()
{
	NodeName = TEXT("Chase Target");
	bNotifyTick = true;
}

EBTNodeResult::Type UTPSBTTask_ChaseTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AEnemyCharacter* Enemy = OwnerComp.GetAIOwner() ? Cast<AEnemyCharacter>(OwnerComp.GetAIOwner()->GetPawn()) : nullptr;
	if (!Enemy || !Enemy->GetTargetActor() || Enemy->IsTargetInAttackRange() || Enemy->GetEnemyState() == EEnemyState::Dead)
	{
		return EBTNodeResult::Failed;
	}

	Enemy->SetAIState(EEnemyState::Chase);
	return EBTNodeResult::InProgress;
}

void UTPSBTTask_ChaseTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	AEnemyCharacter* Enemy = AIController ? Cast<AEnemyCharacter>(AIController->GetPawn()) : nullptr;
	if (!Enemy || !Enemy->GetTargetActor() || Enemy->GetEnemyState() == EEnemyState::Dead)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	if (Enemy->IsTargetInAttackRange())
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	AActor* TargetActor = Enemy->GetTargetActor();
	const FVector Destination = Enemy->CanSeeTarget(TargetActor)
		? TargetActor->GetActorLocation()
		: Enemy->GetLastKnownTargetLocation();

	AIController->SetFocus(TargetActor);

	const float AcceptanceRadius = Enemy->GetAttackRange() * 0.8f;
	const bool bFollowingNavigationPath = Enemy->RequestAIMove(Destination, AcceptanceRadius);
	if (!bFollowingNavigationPath || Enemy->GetVelocity().SizeSquared2D() < FMath::Square(Enemy->GetChaseSpeed() * 0.35f))
	{
		Enemy->MoveDirectlyTo(Destination);
	}
}
