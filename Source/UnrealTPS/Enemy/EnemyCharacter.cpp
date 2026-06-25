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
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "../AI/TPSAIController.h"
#include "../Components/EnemyCombatComponent.h"
#include "../Components/EnemyPerceptionComponent.h"
#include "../Components/HealthComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
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
	CombatComponent = CreateDefaultSubobject<UEnemyCombatComponent>(TEXT("CombatComponent"));
	PerceptionComponent = CreateDefaultSubobject<UEnemyPerceptionComponent>(TEXT("PerceptionComponent"));
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
}

void AEnemyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	DrawDetectionRange();

	if (CurrentState != EEnemyState::Dead)
	{
		UpdateLocomotionAnimation();
	}
}

void AEnemyCharacter::DrawDetectionRange() const
{
	if (PerceptionComponent)
	{
		PerceptionComponent->DrawDetectionRange(this);
	}
}

float AEnemyCharacter::GetSightDistance() const
{
	return PerceptionComponent ? PerceptionComponent->GetSightDistance() : 0.0f;
}

AActor* AEnemyCharacter::GetTargetActor() const
{
	return PerceptionComponent ? PerceptionComponent->GetTargetActor() : nullptr;
}

const FVector& AEnemyCharacter::GetLastKnownTargetLocation() const
{
	static const FVector ZeroVector = FVector::ZeroVector;
	return PerceptionComponent ? PerceptionComponent->GetLastKnownTargetLocation() : ZeroVector;
}

float AEnemyCharacter::GetAttackRange() const
{
	return CombatComponent ? CombatComponent->GetAttackRange() : 0.0f;
}

void AEnemyCharacter::ConfigureForEnemyType()
{
	if (PerceptionComponent)
	{
		PerceptionComponent->ConfigureForEnemyType(EnemyType);
	}

	if (CombatComponent)
	{
		CombatComponent->ConfigureForEnemyType(EnemyType);
	}
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

void AEnemyCharacter::SetAIState(EEnemyState NewState)
{
	ChangeState(NewState);
}

void AEnemyCharacter::SetAITarget(AActor* NewTarget)
{
	if (PerceptionComponent)
	{
		PerceptionComponent->SetTarget(NewTarget, GetWorld());
	}
}

void AEnemyCharacter::ClearAITarget()
{
	if (PerceptionComponent)
	{
		PerceptionComponent->ClearTarget();
	}
	LastMoveRequestDestination = FVector(FLT_MAX);
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

bool AEnemyCharacter::HasReachedPatrolDestination() const
{
	return FVector::DistSquared2D(GetActorLocation(), PatrolDestination) <= FMath::Square(PatrolAcceptanceRadius);
}

bool AEnemyCharacter::RequestAIMove(const FVector& Destination, float AcceptanceRadius)
{
	return RequestNavigationMove(Destination, AcceptanceRadius);
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

	if (MoveDirection.IsNearlyZero())
	{
		return;
	}

	const FVector NormalizedDirection = MoveDirection.GetSafeNormal();
	const float DesiredSpeed = CurrentState == EEnemyState::Chase ? ChaseSpeed : PatrolSpeed;
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	MovementComponent->MaxWalkSpeed = DesiredSpeed;

	AddMovementInput(NormalizedDirection);

	// Some navigation layouts accept a move request without producing useful velocity.
	const float SpeedAlongDirection = FVector::DotProduct(MovementComponent->Velocity, NormalizedDirection);
	if (SpeedAlongDirection < DesiredSpeed * 0.8f)
	{
		MovementComponent->Velocity.X = NormalizedDirection.X * DesiredSpeed;
		MovementComponent->Velocity.Y = NormalizedDirection.Y * DesiredSpeed;
	}
}

void AEnemyCharacter::PerformAttack()
{
	if (CombatComponent)
	{
		CombatComponent->PerformAttack(this, GetTargetActor());
	}
}

bool AEnemyCharacter::CanSeeTarget(const AActor* Target) const
{
	return PerceptionComponent && PerceptionComponent->CanSeeTarget(this, Target);
}

bool AEnemyCharacter::IsTargetInAttackRange() const
{
	return CombatComponent && CombatComponent->IsTargetInAttackRange(this, GetTargetActor());
}

bool AEnemyCharacter::ShouldLoseTarget() const
{
	return PerceptionComponent && PerceptionComponent->ShouldLoseTarget(this);
}

bool AEnemyCharacter::HasLineOfSightToTarget(const AActor* Target) const
{
	return PerceptionComponent && PerceptionComponent->HasLineOfSightToTarget(this, Target);
}

void AEnemyCharacter::HandleDeath(UHealthComponent* Component, AController* InstigatedBy, AActor* DamageCauser)
{
	ChangeState(EEnemyState::Dead);
	ClearAITarget();
	PlayDeathAnimation();

	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
		AIController->ClearFocus(EAIFocusPriority::Gameplay);
	}

	GetCharacterMovement()->DisableMovement();
	SetActorEnableCollision(false);

	if (CorpseLifetime > 0.0f)
	{
		SetLifeSpan(CorpseLifetime);
	}
}

void AEnemyCharacter::HandleDamaged(UHealthComponent* Component, float DamageAmount, AController* InstigatedBy, AActor* DamageCauser)
{
	if (CurrentState != EEnemyState::Dead)
	{
		AActor* Aggressor = InstigatedBy ? InstigatedBy->GetPawn() : DamageCauser;
		if (Aggressor)
		{
			SetAITarget(Aggressor);
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
	if (!bUseSingleNodeLocomotion || (!LocomotionBlendSpace && !LocomotionAnimation))
	{
		return;
	}

	GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	UAnimationAsset* Animation = LocomotionBlendSpace ? Cast<UAnimationAsset>(LocomotionBlendSpace) : LocomotionAnimation.Get();
	GetMesh()->PlayAnimation(Animation, true);
}

void AEnemyCharacter::UpdateLocomotionAnimation()
{
	if (!bUseSingleNodeLocomotion || bPlayingActionAnimation)
	{
		return;
	}

	if (!LocomotionBlendSpace)
	{
		if (LocomotionAnimation)
		{
			GetMesh()->PlayAnimation(LocomotionAnimation, true);
		}
		return;
	}

	if (UAnimSingleNodeInstance* SingleNodeInstance = GetMesh()->GetSingleNodeInstance())
	{
		const float Speed = GetVelocity().Size2D();
		const float DesiredSpeed = CurrentState == EEnemyState::Chase ? ChaseSpeed : PatrolSpeed;
		const FBlendParameter& SpeedParameter = LocomotionBlendSpace->GetBlendParameter(0);
		const float NormalizedSpeed = DesiredSpeed > KINDA_SMALL_NUMBER
			? FMath::Clamp(Speed / DesiredSpeed, 0.0f, 1.0f)
			: 0.0f;
		const float BlendInput = FMath::Lerp(SpeedParameter.Min, SpeedParameter.Max, NormalizedSpeed);

		SingleNodeInstance->SetBlendSpacePosition(FVector(BlendInput, 0.0f, 0.0f));
		SingleNodeInstance->SetPlaying(true);

		const float PlayRate = Speed > 5.0f
			? FMath::GetMappedRangeValueClamped(
				FVector2D(0.0f, FMath::Max(DesiredSpeed, 1.0f)),
				FVector2D(0.9f, 1.25f),
				Speed)
			: 1.0f;
		SingleNodeInstance->SetPlayRate(PlayRate);
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
	if (CurrentState == EEnemyState::Dead || (!LocomotionBlendSpace && !LocomotionAnimation))
	{
		return;
	}

	bPlayingActionAnimation = false;
	UAnimationAsset* Animation = LocomotionBlendSpace ? Cast<UAnimationAsset>(LocomotionBlendSpace) : LocomotionAnimation.Get();
	GetMesh()->PlayAnimation(Animation, true);
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
	bUseSingleNodeLocomotion = true;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> WraithMesh(
		TEXT("/Game/ParagonWraith/Characters/Heroes/Wraith/Meshes/Wraith.Wraith"));
	if (WraithMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(WraithMesh.Object);
		GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
		GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> WraithLocomotion(
		TEXT("/Game/ParagonWraith/Characters/Heroes/Wraith/Animations/Locomotion_Combat/Idle_Combat.Idle_Combat"));
	LocomotionAnimation = WraithLocomotion.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> WraithAttack(
		TEXT("/Game/ParagonWraith/Characters/Heroes/Wraith/Animations/Fire_A_Slow.Fire_A_Slow"));
	AttackAnimation = WraithAttack.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> WraithHit(
		TEXT("/Game/ParagonWraith/Characters/Heroes/Wraith/Animations/HitReact_Front.HitReact_Front"));
	HitAnimation = WraithHit.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> WraithDeath(
		TEXT("/Game/ParagonWraith/Characters/Heroes/Wraith/Animations/Death_Forward.Death_Forward"));
	DeathAnimation = WraithDeath.Object;
}
