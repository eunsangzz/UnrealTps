// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "TPSAIController.generated.h"

class UBehaviorTree;
class UBehaviorTreeComponent;
class UBlackboardData;

UCLASS()
class UNREALTPS_API ATPSAIController : public AAIController
{
	GENERATED_BODY()

public:
	ATPSAIController();

	virtual void Tick(float DeltaTime) override;

	static const FName TargetActorKey;
	static const FName LastKnownTargetLocationKey;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
	TObjectPtr<UBlackboardData> BlackboardAsset;

protected:
	virtual void OnPossess(APawn* InPawn) override;

private:
	void StartEnemyBehavior();
	void UpdateBlackboardTarget();
	UBehaviorTree* GetBehaviorTree();
	UBlackboardData* GetBlackboardData();

	UPROPERTY(Transient)
	TObjectPtr<UBehaviorTreeComponent> BehaviorTreeComponent;

	UPROPERTY(Transient)
	TObjectPtr<UBehaviorTree> RuntimeBehaviorTree;

	UPROPERTY(Transient)
	TObjectPtr<UBlackboardData> RuntimeBlackboardData;
};
