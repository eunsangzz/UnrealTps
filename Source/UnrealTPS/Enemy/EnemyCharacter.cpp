// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyCharacter.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "../AI/TPSAIController.h"
#include "../Components/HealthComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"

AEnemyCharacter::AEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	AIControllerClass = ATPSAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 420.0f, 0.0f);
	GetCharacterMovement()->MaxAcceleration = 2400.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 800.0f;

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	DamageFlashLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("DamageFlashLight"));
	DamageFlashLight->SetupAttachment(RootComponent);
	DamageFlashLight->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));
	DamageFlashLight->SetLightColor(FLinearColor::Red);
	DamageFlashLight->SetIntensity(5000.0f);
	DamageFlashLight->SetAttenuationRadius(300.0f);
	DamageFlashLight->SetVisibility(false);
}

void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	SpawnLocation = GetActorLocation();
	ConfigureForEnemyType();
	GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;
	SpawnDefaultController();
	HealthComponent->OnDeath.AddDynamic(this, &AEnemyCharacter::HandleDeath);
	HealthComponent->OnDamaged.AddDynamic(this, &AEnemyCharacter::HandleDamaged);
	InitializeAnimation();
	SelectPatrolDestination();
	ChangeState(EEnemyState::Patrol);
}

void AEnemyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	DrawDetectionRange();

	if (CurrentState != EEnemyState::Dead)
	{
		UpdateStateMachine(DeltaTime);
		UpdateLocomotionAnimation();
	}
}

void AEnemyCharacter::DrawDetectionRange() const
{
	if (!bDrawDetectionRange || !GetWorld() || CurrentState == EEnemyState::Dead)
	{
		return;
	}

	const FVector EyeLocation = GetActorLocation() + FVector(0.0f, 0.0f, BaseEyeHeight);
	const FColor DetectionColor = TargetActor ? FColor::Red : FColor::Yellow;

	DrawDebugCone(
		GetWorld(),
		EyeLocation,
		GetActorForwardVector(),
		SightDistance,
		FMath::DegreesToRadians(HorizontalSightAngle * 0.5f),
		FMath::DegreesToRadians(VerticalSightAngle * 0.5f),
		24,
		DetectionColor,
		false,
		0.0f,
		0,
		2.0f);
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
		SightDistance = FMath::Max(SightDistance, 2000.0f);
		HorizontalSightAngle = 110.0f;
		VerticalSightAngle = 100.0f;
		AttackRange = FMath::Max(AttackRange, 180.0f);
	}
}

void AEnemyCharacter::UpdateStateMachine(float DeltaTime)
{
	if (CurrentState == EEnemyState::Idle || CurrentState == EEnemyState::Patrol)
	{
		TryAcquireTarget();
	}
	else if (!TargetActor)
	{
		SelectPatrolDestination();
		ChangeState(EEnemyState::Patrol);
	}
	else
	{
		const bool bCanSeeTarget = CanSeeTarget(TargetActor);
		if (bCanSeeTarget)
		{
			LastKnownTargetLocation = TargetActor->GetActorLocation();
			LastTargetSeenTime = GetWorld()->GetTimeSeconds();
		}

		const float LoseTargetDistance = SightDistance * LoseTargetDistanceMultiplier;
		const bool bTargetTooFar = FVector::DistSquared2D(GetActorLocation(), TargetActor->GetActorLocation())
			> FMath::Square(LoseTargetDistance);
		const bool bMemoryExpired = GetWorld()->GetTimeSeconds() - LastTargetSeenTime > LoseSightGraceDuration;

		if (bTargetTooFar || (!bCanSeeTarget && bMemoryExpired))
		{
			TargetActor = nullptr;
			SelectPatrolDestination();
			ChangeState(EEnemyState::Patrol);
		}
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
		SelectPatrolDestination();
	}

	const bool bFollowingNavigationPath = RequestNavigationMove(PatrolDestination, PatrolAcceptanceRadius);

	if (!bFollowingNavigationPath || GetVelocity().SizeSquared2D() < FMath::Square(PatrolSpeed * 0.35f))
	{
		MoveDirectlyTo(PatrolDestination);
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

	const bool bHasCurrentSight = CanSeeTarget(TargetActor);
	if (bHasCurrentSight)
	{
		LastKnownTargetLocation = TargetActor->GetActorLocation();
		LastTargetSeenTime = GetWorld()->GetTimeSeconds();
	}

	const FVector ChaseDestination = bHasCurrentSight
		? TargetActor->GetActorLocation()
		: LastKnownTargetLocation;

	const bool bFollowingNavigationPath = RequestNavigationMove(ChaseDestination, AttackRange * 0.8f);
	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->SetFocus(TargetActor);
	}

	if (!bFollowingNavigationPath || GetVelocity().SizeSquared2D() < FMath::Square(ChaseSpeed * 0.35f))
	{
		MoveDirectlyTo(ChaseDestination);
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
		LastMoveRequestDestination = FVector(FLT_MAX);
	}
	else if (NewState == EEnemyState::Chase)
	{
		GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;
		LastMoveRequestDestination = FVector(FLT_MAX);
	}

	OnEnemyStateChanged.Broadcast(PreviousState, NewState);
}

void AEnemyCharacter::SelectPatrolDestination()
{
	const FVector PatrolCenter = GetActorLocation();
	const float PatrolAngle = FMath::FRandRange(0.0f, 2.0f * UE_PI);
	const float PatrolDistance = FMath::FRandRange(PatrolRadius * 0.5f, PatrolRadius);
	const FVector RandomOffset(
		FMath::Cos(PatrolAngle) * PatrolDistance,
		FMath::Sin(PatrolAngle) * PatrolDistance,
		0.0f);
	PatrolDestination = PatrolCenter + RandomOffset;

	if (UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetCurrent(GetWorld()))
	{
		FNavLocation NavLocation;
		if (NavigationSystem->GetRandomReachablePointInRadius(PatrolCenter, PatrolRadius, NavLocation))
		{
			PatrolDestination = NavLocation.Location;
		}
	}
}

bool AEnemyCharacter::RequestNavigationMove(const FVector& Destination, float AcceptanceRadius)
{
	AAIController* AIController = Cast<AAIController>(GetController());
	if (!AIController)
	{
		return false;
	}

	const bool bDestinationChanged =
		FVector::DistSquared2D(LastMoveRequestDestination, Destination) > FMath::Square(100.0f);
	const UPathFollowingComponent* PathFollowing = AIController->GetPathFollowingComponent();
	const bool bMoveInactive = !PathFollowing || PathFollowing->GetStatus() == EPathFollowingStatus::Idle;

	if (!bDestinationChanged && !bMoveInactive)
	{
		return true;
	}

	LastMoveRequestDestination = Destination;
	const EPathFollowingRequestResult::Type MoveResult =
		AIController->MoveToLocation(
			Destination,
			AcceptanceRadius,
			true,
			true,
			true,
			false,
			nullptr,
			true);

	return MoveResult != EPathFollowingRequestResult::Failed;
}

void AEnemyCharacter::MoveDirectlyTo(const FVector& Destination)
{
	FVector MoveDirection = Destination - GetActorLocation();
	MoveDirection.Z = 0.0f;

	if (!MoveDirection.IsNearlyZero())
	{
		AddMovementInput(MoveDirection.GetSafeNormal());
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
	PlayAttackAnimation();
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
	LastKnownTargetLocation = PlayerPawn->GetActorLocation();
	LastTargetSeenTime = GetWorld()->GetTimeSeconds();
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
	PlayDeathAnimation();

	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
		AIController->ClearFocus(EAIFocusPriority::Gameplay);
	}

	GetCharacterMovement()->DisableMovement();
	SetActorEnableCollision(false);
}

void AEnemyCharacter::HandleDamaged(UHealthComponent* Component, float DamageAmount, AController* InstigatedBy, AActor* DamageCauser)
{
	if (CurrentState != EEnemyState::Dead)
	{
		AActor* Aggressor = InstigatedBy ? InstigatedBy->GetPawn() : DamageCauser;
		if (Aggressor)
		{
			TargetActor = Aggressor;
			LastKnownTargetLocation = Aggressor->GetActorLocation();
			LastTargetSeenTime = GetWorld()->GetTimeSeconds();
			ChangeState(IsTargetInAttackRange() ? EEnemyState::Attack : EEnemyState::Chase);
		}

		StartDamageFlash();
		PlayHitAnimation();
	}
}

void AEnemyCharacter::StartDamageFlash()
{
	SetDamageMaterialParameters(FLinearColor::Red, 1.0f);
	DamageFlashLight->SetVisibility(true);
	GetWorldTimerManager().SetTimer(
		DamageFlashTimerHandle,
		this,
		&AEnemyCharacter::EndDamageFlash,
		DamageFlashDuration,
		false);
}

void AEnemyCharacter::EndDamageFlash()
{
	SetDamageMaterialParameters(FLinearColor::White, 0.0f);
	DamageFlashLight->SetVisibility(false);
}

void AEnemyCharacter::SetDamageMaterialParameters(const FLinearColor& Color, float Strength)
{
	USkeletalMeshComponent* EnemyMesh = GetMesh();
	if (!EnemyMesh)
	{
		return;
	}

	if (DamageMaterials.IsEmpty())
	{
		for (int32 MaterialIndex = 0; MaterialIndex < EnemyMesh->GetNumMaterials(); ++MaterialIndex)
		{
			if (UMaterialInstanceDynamic* Material = EnemyMesh->CreateAndSetMaterialInstanceDynamic(MaterialIndex))
			{
				DamageMaterials.Add(Material);
			}
		}
	}

	for (UMaterialInstanceDynamic* Material : DamageMaterials)
	{
		if (!Material)
		{
			continue;
		}

		if (Strength <= 0.0f)
		{
			Material->ClearParameterValues();
			continue;
		}

		Material->SetVectorParameterValue(TEXT("DamageColor"), Color);
		Material->SetVectorParameterValue(TEXT("TintColor"), Color);
		Material->SetVectorParameterValue(TEXT("Color"), Color);
		Material->SetScalarParameterValue(TEXT("DamageAmount"), Strength);
		Material->SetScalarParameterValue(TEXT("HitFlash"), Strength);
	}
}

void AEnemyCharacter::InitializeAnimation()
{
	if (!bUseSingleNodeLocomotion || !LocomotionBlendSpace)
	{
		return;
	}

	GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	GetMesh()->PlayAnimation(LocomotionBlendSpace, true);
}

void AEnemyCharacter::UpdateLocomotionAnimation()
{
	if (!bUseSingleNodeLocomotion || bPlayingActionAnimation)
	{
		return;
	}

	if (UAnimSingleNodeInstance* SingleNodeInstance = GetMesh()->GetSingleNodeInstance())
	{
		const float Speed = GetVelocity().Size2D();
		SingleNodeInstance->SetBlendSpacePosition(FVector(Speed, 0.0f, 0.0f));
	}
}

void AEnemyCharacter::PlayAttackAnimation()
{
	if (AttackMontage)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_Play(AttackMontage);
		}
		return;
	}

	PlayActionAnimation(AttackAnimation, true);
}

void AEnemyCharacter::PlayHitAnimation()
{
	PlayActionAnimation(HitAnimation, true);
}

void AEnemyCharacter::PlayDeathAnimation()
{
	GetWorldTimerManager().ClearTimer(AnimationTimerHandle);
	bPlayingActionAnimation = true;

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		if (DeathAnimation && !bUseSingleNodeLocomotion)
		{
			AnimInstance->PlaySlotAnimationAsDynamicMontage(DeathAnimation, TEXT("DefaultSlot"), 0.1f, 0.2f);
			return;
		}
	}

	if (DeathAnimation)
	{
		GetMesh()->PlayAnimation(DeathAnimation, false);
	}
}

void AEnemyCharacter::PlayActionAnimation(UAnimSequenceBase* Animation, bool bRestoreLocomotion)
{
	if (!Animation || CurrentState == EEnemyState::Dead)
	{
		return;
	}

	if (!bUseSingleNodeLocomotion)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->PlaySlotAnimationAsDynamicMontage(Animation, TEXT("DefaultSlot"), 0.1f, 0.15f);
		}
		return;
	}

	bPlayingActionAnimation = true;
	GetMesh()->PlayAnimation(Animation, false);

	if (bRestoreLocomotion)
	{
		GetWorldTimerManager().SetTimer(
			AnimationTimerHandle,
			this,
			&AEnemyCharacter::RestoreLocomotionAnimation,
			FMath::Max(0.1f, Animation->GetPlayLength()),
			false);
	}
}

void AEnemyCharacter::RestoreLocomotionAnimation()
{
	if (CurrentState == EEnemyState::Dead || !LocomotionBlendSpace)
	{
		return;
	}

	bPlayingActionAnimation = false;
	GetMesh()->PlayAnimation(LocomotionBlendSpace, true);
}

AMeleeEnemyCharacter::AMeleeEnemyCharacter()
{
	EnemyType = EEnemyType::Melee;
	bUseSingleNodeLocomotion = true;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 88.0f);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MinionMesh(
		TEXT("/Game/ParagonMinions/Characters/Minions/Down_Minions/Meshes/Minion_Lane_Melee_Dawn.Minion_Lane_Melee_Dawn"));
	if (MinionMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MinionMesh.Object);
		GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
		GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	}

	static ConstructorHelpers::FObjectFinder<UBlendSpace> MinionLocomotion(
		TEXT("/Game/ParagonMinions/Characters/Minions/Down_Minions/Animations/Melee/Blendspaces/IdleToRun_A_Combat.IdleToRun_A_Combat"));
	LocomotionBlendSpace = MinionLocomotion.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> MinionAttack(
		TEXT("/Game/ParagonMinions/Characters/Minions/Down_Minions/Animations/Melee/Attack_A.Attack_A"));
	AttackAnimation = MinionAttack.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> MinionHit(
		TEXT("/Game/ParagonMinions/Characters/Minions/Down_Minions/Animations/Melee/HitReact_Front.HitReact_Front"));
	HitAnimation = MinionHit.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> MinionDeath(
		TEXT("/Game/ParagonMinions/Characters/Minions/Down_Minions/Animations/Melee/Death_A.Death_A"));
	DeathAnimation = MinionDeath.Object;
}

ARangedEnemyCharacter::ARangedEnemyCharacter()
{
	EnemyType = EEnemyType::Ranged;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> WraithMesh(
		TEXT("/Game/ParagonWraith/Characters/Heroes/Wraith/Meshes/Wraith.Wraith"));
	if (WraithMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(WraithMesh.Object);
		GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
		GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	}

	static ConstructorHelpers::FClassFinder<UAnimInstance> WraithAnimBlueprint(
		TEXT("/Game/ParagonWraith/Characters/Heroes/Wraith/Wraith_AnimBlueprint"));
	if (WraithAnimBlueprint.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(WraithAnimBlueprint.Class);
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> WraithAttack(
		TEXT("/Game/ParagonWraith/Characters/Heroes/Wraith/Animations/Fire_A_Slow_Montage.Fire_A_Slow_Montage"));
	AttackMontage = WraithAttack.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> WraithHit(
		TEXT("/Game/ParagonWraith/Characters/Heroes/Wraith/Animations/HitReact_Front.HitReact_Front"));
	HitAnimation = WraithHit.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> WraithDeath(
		TEXT("/Game/ParagonWraith/Characters/Heroes/Wraith/Animations/Death_Forward.Death_Forward"));
	DeathAnimation = WraithDeath.Object;
}
