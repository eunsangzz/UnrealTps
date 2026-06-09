// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

class AController;
class UDamageType;
class UHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FOnHealthChangedSignature,
	UHealthComponent*, HealthComponent,
	float, CurrentHealth,
	float, MaxHealth,
	float, HealthDelta,
	AActor*, DamageCauser);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOnDamagedSignature,
	UHealthComponent*, HealthComponent,
	float, DamageAmount,
	AController*, InstigatedBy,
	AActor*, DamageCauser);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnDeathSignature,
	UHealthComponent*, HealthComponent,
	AController*, InstigatedBy,
	AActor*, DamageCauser);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UNREALTPS_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Health")
	float TakeDamage(float DamageAmount, AController* InstigatedBy = nullptr, AActor* DamageCauser = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Health")
	float Heal(float HealAmount);

	UFUNCTION(BlueprintCallable, Category = "Health")
	void Die(AController* InstigatedBy = nullptr, AActor* DamageCauser = nullptr);

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return bIsDead; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	float CurrentHealth = 100.0f;

	UPROPERTY(BlueprintAssignable, Category = "Health|Events")
	FOnHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Health|Events")
	FOnDamagedSignature OnDamaged;

	UPROPERTY(BlueprintAssignable, Category = "Health|Events")
	FOnDeathSignature OnDeath;

private:
	UFUNCTION()
	void HandleOwnerTakeAnyDamage(
		AActor* DamagedActor,
		float DamageAmount,
		const UDamageType* DamageType,
		AController* InstigatedBy,
		AActor* DamageCauser);

	bool bIsDead = false;
};
