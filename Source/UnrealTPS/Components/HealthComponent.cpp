// Copyright Epic Games, Inc. All Rights Reserved.

#include "HealthComponent.h"

#include "GameFramework/Actor.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	MaxHealth = FMath::Max(1.0f, MaxHealth);
	CurrentHealth = MaxHealth;
	bIsDead = false;

	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddDynamic(this, &UHealthComponent::HandleOwnerTakeAnyDamage);
	}
}

float UHealthComponent::TakeDamage(float DamageAmount, AController* InstigatedBy, AActor* DamageCauser)
{
	if (bIsDead || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);
	const float AppliedDamage = PreviousHealth - CurrentHealth;

	if (AppliedDamage <= 0.0f)
	{
		return 0.0f;
	}

	OnDamaged.Broadcast(this, AppliedDamage, InstigatedBy, DamageCauser);
	OnHealthChanged.Broadcast(this, CurrentHealth, MaxHealth, -AppliedDamage, DamageCauser);

	if (CurrentHealth <= 0.0f)
	{
		Die(InstigatedBy, DamageCauser);
	}

	return AppliedDamage;
}

float UHealthComponent::Heal(float HealAmount)
{
	if (bIsDead || HealAmount <= 0.0f || CurrentHealth >= MaxHealth)
	{
		return 0.0f;
	}

	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth + HealAmount, 0.0f, MaxHealth);
	const float AppliedHealing = CurrentHealth - PreviousHealth;

	if (AppliedHealing > 0.0f)
	{
		OnHealthChanged.Broadcast(this, CurrentHealth, MaxHealth, AppliedHealing, nullptr);
	}

	return AppliedHealing;
}

void UHealthComponent::Die(AController* InstigatedBy, AActor* DamageCauser)
{
	if (bIsDead)
	{
		return;
	}

	const float PreviousHealth = CurrentHealth;
	bIsDead = true;
	CurrentHealth = 0.0f;

	if (PreviousHealth > 0.0f)
	{
		OnHealthChanged.Broadcast(this, CurrentHealth, MaxHealth, -PreviousHealth, DamageCauser);
	}

	OnDeath.Broadcast(this, InstigatedBy, DamageCauser);
}

float UHealthComponent::GetHealthPercent() const
{
	return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
}

void UHealthComponent::HandleOwnerTakeAnyDamage(
	AActor* DamagedActor,
	float DamageAmount,
	const UDamageType* DamageType,
	AController* InstigatedBy,
	AActor* DamageCauser)
{
	TakeDamage(DamageAmount, InstigatedBy, DamageCauser);
}
