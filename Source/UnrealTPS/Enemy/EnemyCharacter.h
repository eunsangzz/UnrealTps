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
class UMaterialInstanceDynamic;
class UPointLightComponent;

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

	UFUNCTION(BlueprintPure, Category = "Enemy|Perception")
	float GetSightDistance() const { return SightDistance; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	TObjectPtr<UHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Damage")
	TObjectPtr<UPointLightComponent> DamageFlashLight;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events")
	FOnEnemyStateChangedSignature OnEnemyStateChanged;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	EEnemyType EnemyType = EEnemyType::Melee;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	EEnemyState CurrentState = EEnemyState::Idle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Perception", meta = (ClampMin = "0.0"))
	float SightDistance = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Perception", meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float HorizontalSightAngle = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Perception", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float VerticalSightAngle = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Perception", meta = (ClampMin = "0.0"))
	float LoseSightGraceDuration = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Perception", meta = (ClampMin = "1.0"))
	float LoseTargetDistanceMultiplier = 1.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Debug")
	bool bDrawDetectionRange = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Damage", meta = (ClampMin = "0.01"))
	float DamageFlashDuration = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Death", meta = (ClampMin = "0.0"))
	float CorpseLifetime = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.0"))
	float AttackRange = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.01"))
	float AttackCooldown = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.0"))
	float AttackDamage = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Patrol", meta = (ClampMin = "0.0"))
	float PatrolRadius = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Patrol", meta = (ClampMin = "0.0"))
	float PatrolAcceptanceRadius = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Patrol", meta = (ClampMin = "0.0"))
	float IdleDuration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Movement", meta = (ClampMin = "0.0"))
	float PatrolSpeed = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Movement", meta = (ClampMin = "0.0"))
	float ChaseSpeed = 600.0f;

private:
	void ConfigureForEnemyType();
	void DrawDetectionRange() const;
	void UpdateStateMachine(float DeltaTime);
	void UpdateIdle(float DeltaTime);
	void UpdatePatrol();
	void UpdateChase();
	void UpdateAttack();
	void ChangeState(EEnemyState NewState);
	void SelectPatrolDestination();
	bool RequestNavigationMove(const FVector& Destination, float AcceptanceRadius);
	void MoveDirectlyTo(const FVector& Destination);
	void PerformAttack();
	void InitializeAnimation();
	void UpdateLocomotionAnimation();
	void PlayAttackAnimation();
	void PlayHitAnimation();
	void StartDamageFlash();
	void EndDamageFlash();
	void SetDamageMaterialParameters(const FLinearColor& Color, float Strength);
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
	TObjectPtr<UAnimSequenceBase> LocomotionAnimation;

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

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DamageMaterials;

	FVector SpawnLocation = FVector::ZeroVector;
	FVector PatrolDestination = FVector::ZeroVector;
	FVector LastKnownTargetLocation = FVector::ZeroVector;
	FVector LastMoveRequestDestination = FVector(FLT_MAX);
	float IdleElapsed = 0.0f;
	double LastTargetSeenTime = -1000.0;
	double LastAttackTime = -1000.0;
	bool bPlayingActionAnimation = false;
	FTimerHandle AnimationTimerHandle;
	FTimerHandle DamageFlashTimerHandle;
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
