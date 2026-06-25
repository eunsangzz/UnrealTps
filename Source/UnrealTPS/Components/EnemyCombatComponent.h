// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../Enemy/EnemyTypes.h"
#include "EnemyCombatComponent.generated.h"

class AEnemyCharacter;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UNREALTPS_API UEnemyCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyCombatComponent();

	void ConfigureForEnemyType(EEnemyType EnemyType);
	bool IsTargetInAttackRange(const AEnemyCharacter* Enemy, const AActor* Target) const;
	void PerformAttack(AEnemyCharacter* Enemy, AActor* Target);

	float GetAttackRange() const { return AttackRange; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.0"))
	float AttackRange = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.01"))
	float AttackCooldown = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.0"))
	float AttackDamage = 15.0f;

private:
	double LastAttackTime = -1000.0;
};
