// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyCombatComponent.h"

#include "Kismet/GameplayStatics.h"
#include "../Enemy/EnemyCharacter.h"

UEnemyCombatComponent::UEnemyCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyCombatComponent::ConfigureForEnemyType(EEnemyType EnemyType)
{
	if (EnemyType == EEnemyType::Ranged)
	{
		AttackRange = FMath::Max(AttackRange, 1800.0f);
		AttackCooldown = FMath::Max(AttackCooldown, 1.5f);
		AttackDamage = FMath::Max(AttackDamage, 10.0f);
	}
	else
	{
		AttackRange = FMath::Max(AttackRange, 180.0f);
	}
}

bool UEnemyCombatComponent::IsTargetInAttackRange(const AEnemyCharacter* Enemy, const AActor* Target) const
{
	return Enemy && Target && FVector::DistSquared(Enemy->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(AttackRange);
}

void UEnemyCombatComponent::PerformAttack(AEnemyCharacter* Enemy, AActor* Target)
{
	const UWorld* World = Enemy ? Enemy->GetWorld() : nullptr;
	if (!World || !Enemy || !Target || World->GetTimeSeconds() - LastAttackTime < AttackCooldown)
	{
		return;
	}

	if (!Enemy->HasLineOfSightToTarget(Target))
	{
		return;
	}

	LastAttackTime = World->GetTimeSeconds();
	Enemy->PlayAttackAnimation();
	UGameplayStatics::ApplyDamage(Target, AttackDamage, Enemy->GetController(), Enemy, nullptr);
}
