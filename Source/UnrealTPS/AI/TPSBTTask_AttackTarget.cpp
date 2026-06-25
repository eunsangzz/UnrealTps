// Copyright Epic Games, Inc. All Rights Reserved.

#include "TPSBTTask_AttackTarget.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "../Enemy/EnemyCharacter.h"

UTPSBTTask_AttackTarget::UTPSBTTask_AttackTarget()
{
	NodeName = TEXT("Attack Target");
	bNotifyTick = true;
}

EBTNodeResult::Type UTPSBTTask_AttackTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AEnemyCharacter* Enemy = OwnerComp.GetAIOwner() ? Cast<AEnemyCharacter>(OwnerComp.GetAIOwner()->GetPawn()) : nullptr;
	if (!Enemy || !Enemy->GetTargetActor() || !Enemy->IsTargetInAttackRange() || Enemy->GetEnemyState() == EEnemyState::Dead)
	{
		return EBTNodeResult::Failed;
	}

	Enemy->SetAIState(EEnemyState::Attack);
	return EBTNodeResult::InProgress;
}

void UTPSBTTask_AttackTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	AEnemyCharacter* Enemy = AIController ? Cast<AEnemyCharacter>(AIController->GetPawn()) : nullptr;
	if (!Enemy || !Enemy->GetTargetActor() || Enemy->GetEnemyState() == EEnemyState::Dead)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	if (!Enemy->IsTargetInAttackRange())
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	AIController->StopMovement();
	AIController->SetFocus(Enemy->GetTargetActor());
	Enemy->PerformAttack();
}
