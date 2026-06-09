// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyTypes.h"
#include "EnemyCharacter.generated.h"

class ATPSAIController;
class UAnimationAsset;
class UAnimMontage;
class UAnimSequenceBase;
class UBlendSpace;
class UHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnEnemyStateChangedSignature,
	EEnemyState, PreviousState,
	EEnemyState, NewState);

UCLASS()
class UNREALTPS_API AEnemyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AEnemyCharacter();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintPure, Category = "Enemy")
	EEnemyState GetEnemyState() const { return CurrentState; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	EEnemyType GetEnemyType() const { return EnemyType; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	TObjectPtr<UHealthComponent> HealthComponent;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events")
	FOnEnemyStateChangedSignature OnEnemyStateChanged;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	EEnemyType EnemyType = EEnemyType::Melee;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	EEnemyState CurrentState = EEnemyState::Idle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Perception", meta = (ClampMin = "0.0"))
	float SightDistance = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Perception", meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float HorizontalSightAngle = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Perception", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float VerticalSightAngle = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.0"))
	float AttackRange = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.01"))
	float AttackCooldown = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.0"))
	float AttackDamage = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Patrol", meta = (ClampMin = "0.0"))
	float PatrolRadius = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Patrol", meta = (ClampMin = "0.0"))
	float PatrolAcceptanceRadius = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Patrol", meta = (ClampMin = "0.0"))
	float IdleDuration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Movement", meta = (ClampMin = "0.0"))
	float PatrolSpeed = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Movement", meta = (ClampMin = "0.0"))
	float ChaseSpeed = 420.0f;

private:
	void ConfigureForEnemyType();
	void UpdateStateMachine(float DeltaTime);
	void UpdateIdle(float DeltaTime);
	void UpdatePatrol();
	void UpdateChase();
	void UpdateAttack();
	void ChangeState(EEnemyState NewState);
	void SelectPatrolDestination();
	void PerformAttack();
	void InitializeAnimation();
	void UpdateLocomotionAnimation();
	void PlayAttackAnimation();
	void PlayHitAnimation();
	void PlayDeathAnimation();
	void PlayActionAnimation(UAnimSequenceBase* Animation, bool bRestoreLocomotion);
	void RestoreLocomotionAnimation();

	bool TryAcquireTarget();
	bool CanSeeTarget(const AActor* Target) const;
	bool IsTargetInAttackRange() const;
	bool HasLineOfSightToTarget(const AActor* Target) const;

	UFUNCTION()
	void HandleDeath(UHealthComponent* Component, AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION()
	void HandleDamaged(UHealthComponent* Component, float DamageAmount, AController* InstigatedBy, AActor* DamageCauser);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UBlendSpace> LocomotionBlendSpace;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> AttackAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> HitAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> DeathAnimation;

	bool bUseSingleNodeLocomotion = false;

private:
	UPROPERTY()
	TObjectPtr<AActor> TargetActor;

	FVector SpawnLocation = FVector::ZeroVector;
	FVector PatrolDestination = FVector::ZeroVector;
	float IdleElapsed = 0.0f;
	double LastAttackTime = -1000.0;
	bool bPlayingActionAnimation = false;
	FTimerHandle AnimationTimerHandle;
};

UCLASS()
class UNREALTPS_API AMeleeEnemyCharacter : public AEnemyCharacter
{
	GENERATED_BODY()

public:
	AMeleeEnemyCharacter();
};

UCLASS()
class UNREALTPS_API ARangedEnemyCharacter : public AEnemyCharacter
{
	GENERATED_BODY()

public:
	ARangedEnemyCharacter();
};
