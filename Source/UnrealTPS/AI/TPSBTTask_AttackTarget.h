// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "TPSBTTask_AttackTarget.generated.h"

UCLASS()
class UNREALTPS_API UTPSBTTask_AttackTarget : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UTPSBTTask_AttackTarget();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
