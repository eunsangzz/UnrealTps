// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyCharacter.h"

#include "AIController.h"
#include "NavigationSystem.h"
#include "../AI/TPSAIController.h"
#include "../Components/HealthComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

AEnemyCharacter::AEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	AIControllerClass = ATPSAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 420.0f, 0.0f);

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
}

void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	SpawnLocation = GetActorLocation();
	ConfigureForEnemyType();
	GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;
	HealthComponent->OnDeath.AddDynamic(this, &AEnemyCharacter::HandleDeath);
	ChangeState(EEnemyState::Idle);
}

void AEnemyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CurrentState != EEnemyState::Dead)
	{
		UpdateStateMachine(DeltaTime);
	}
}

void AEnemyCharacter::ConfigureForEnemyType()
{
	if (EnemyType == EEnemyType::Ranged)
	{
		SightDistance = FMath::Max(SightDistance, 3500.0f);
		HorizontalSightAngle = FMath::Max(HorizontalSightAngle, 130.0f);
		VerticalSightAngle = FMath::Max(VerticalSightAngle, 130.0f);
		AttackRange = FMath::Max(AttackRange, 1800.0f);
		AttackCooldown = FMath::Max(AttackCooldown, 1.5f);
		AttackDamage = FMath::Max(AttackDamage, 10.0f);
	}
	else
	{
		SightDistance = FMath::Max(SightDistance, 1500.0f);
		AttackRange = FMath::Max(AttackRange, 180.0f);
	}
}

void AEnemyCharacter::UpdateStateMachine(float DeltaTime)
{
	if (CurrentState == EEnemyState::Idle || CurrentState == EEnemyState::Patrol)
	{
		TryAcquireTarget();
	}
	else if (!TargetActor || !CanSeeTarget(TargetActor))
	{
		TargetActor = nullptr;
		ChangeState(EEnemyState::Idle);
	}

	switch (CurrentState)
	{
	case EEnemyState::Idle:
		UpdateIdle(DeltaTime);
		break;
	case EEnemyState::Patrol:
		UpdatePatrol();
		break;
	case EEnemyState::Chase:
		UpdateChase();
		break;
	case EEnemyState::Attack:
		UpdateAttack();
		break;
	default:
		break;
	}
}

void AEnemyCharacter::UpdateIdle(float DeltaTime)
{
	IdleElapsed += DeltaTime;
	if (IdleElapsed >= IdleDuration)
	{
		SelectPatrolDestination();
		ChangeState(EEnemyState::Patrol);
	}
}

void AEnemyCharacter::UpdatePatrol()
{
	if (FVector::DistSquared2D(GetActorLocation(), PatrolDestination) <= FMath::Square(PatrolAcceptanceRadius))
	{
		ChangeState(EEnemyState::Idle);
		return;
	}

	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->MoveToLocation(PatrolDestination, PatrolAcceptanceRadius);
	}
}

void AEnemyCharacter::UpdateChase()
{
	if (!TargetActor)
	{
		ChangeState(EEnemyState::Idle);
		return;
	}

	if (IsTargetInAttackRange())
	{
		ChangeState(EEnemyState::Attack);
		return;
	}

	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->MoveToActor(TargetActor, AttackRange * 0.8f);
		AIController->SetFocus(TargetActor);
	}
}

void AEnemyCharacter::UpdateAttack()
{
	if (!TargetActor)
	{
		ChangeState(EEnemyState::Idle);
		return;
	}

	if (!IsTargetInAttackRange())
	{
		ChangeState(EEnemyState::Chase);
		return;
	}

	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
		AIController->SetFocus(TargetActor);
	}

	PerformAttack();
}

void AEnemyCharacter::ChangeState(EEnemyState NewState)
{
	if (CurrentState == NewState)
	{
		return;
	}

	const EEnemyState PreviousState = CurrentState;
	CurrentState = NewState;

	if (NewState == EEnemyState::Idle)
	{
		IdleElapsed = 0.0f;
		GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;

		if (AAIController* AIController = Cast<AAIController>(GetController()))
		{
			AIController->StopMovement();
			AIController->ClearFocus(EAIFocusPriority::Gameplay);
		}
	}
	else if (NewState == EEnemyState::Patrol)
	{
		GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;
	}
	else if (NewState == EEnemyState::Chase)
	{
		GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;
	}

	OnEnemyStateChanged.Broadcast(PreviousState, NewState);
}

void AEnemyCharacter::SelectPatrolDestination()
{
	PatrolDestination = SpawnLocation;

	if (UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetCurrent(GetWorld()))
	{
		FNavLocation NavLocation;
		if (NavigationSystem->GetRandomReachablePointInRadius(SpawnLocation, PatrolRadius, NavLocation))
		{
			PatrolDestination = NavLocation.Location;
		}
	}
}

void AEnemyCharacter::PerformAttack()
{
	const UWorld* World = GetWorld();
	if (!World || !TargetActor || World->GetTimeSeconds() - LastAttackTime < AttackCooldown)
	{
		return;
	}

	if (!HasLineOfSightToTarget(TargetActor))
	{
		return;
	}

	LastAttackTime = World->GetTimeSeconds();
	UGameplayStatics::ApplyDamage(TargetActor, AttackDamage, GetController(), this, nullptr);
}

bool AEnemyCharacter::TryAcquireTarget()
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn || !CanSeeTarget(PlayerPawn))
	{
		return false;
	}

	TargetActor = PlayerPawn;
	ChangeState(IsTargetInAttackRange() ? EEnemyState::Attack : EEnemyState::Chase);
	return true;
}

bool AEnemyCharacter::CanSeeTarget(const AActor* Target) const
{
	if (!Target)
	{
		return false;
	}

	const FVector EyeLocation = GetActorLocation() + FVector(0.0f, 0.0f, BaseEyeHeight);
	const FVector TargetLocation = Target->GetActorLocation();
	const FVector ToTarget = TargetLocation - EyeLocation;

	if (ToTarget.SizeSquared() > FMath::Square(SightDistance))
	{
		return false;
	}

	const FVector LocalDirection = GetActorTransform().InverseTransformVectorNoScale(ToTarget.GetSafeNormal());
	const float HorizontalAngle = FMath::Abs(FMath::RadiansToDegrees(FMath::Atan2(LocalDirection.Y, LocalDirection.X)));
	const float VerticalAngle = FMath::Abs(FMath::RadiansToDegrees(
		FMath::Atan2(LocalDirection.Z, FVector2D(LocalDirection.X, LocalDirection.Y).Size())));

	if (HorizontalAngle > HorizontalSightAngle * 0.5f || VerticalAngle > VerticalSightAngle * 0.5f)
	{
		return false;
	}

	return HasLineOfSightToTarget(Target);
}

bool AEnemyCharacter::IsTargetInAttackRange() const
{
	return TargetActor && FVector::DistSquared(GetActorLocation(), TargetActor->GetActorLocation()) <= FMath::Square(AttackRange);
}

bool AEnemyCharacter::HasLineOfSightToTarget(const AActor* Target) const
{
	if (!Target || !GetWorld())
	{
		return false;
	}

	const FVector TraceStart = GetActorLocation() + FVector(0.0f, 0.0f, BaseEyeHeight);
	const FVector TraceEnd = Target->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);

	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyLineOfSight), true, this);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);

	return !bHit || Hit.GetActor() == Target;
}

void AEnemyCharacter::HandleDeath(UHealthComponent* Component, AController* InstigatedBy, AActor* DamageCauser)
{
	ChangeState(EEnemyState::Dead);
	TargetActor = nullptr;

	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
		AIController->ClearFocus(EAIFocusPriority::Gameplay);
	}

	GetCharacterMovement()->DisableMovement();
	SetActorEnableCollision(false);
}
