// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyPerceptionComponent.h"

#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "../Enemy/EnemyCharacter.h"

UEnemyPerceptionComponent::UEnemyPerceptionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyPerceptionComponent::ConfigureForEnemyType(EEnemyType EnemyType)
{
	if (EnemyType == EEnemyType::Ranged)
	{
		SightDistance = FMath::Max(SightDistance, 3500.0f);
		HorizontalSightAngle = FMath::Max(HorizontalSightAngle, 130.0f);
		VerticalSightAngle = FMath::Max(VerticalSightAngle, 130.0f);
	}
	else
	{
		SightDistance = FMath::Max(SightDistance, 2000.0f);
		HorizontalSightAngle = 110.0f;
		VerticalSightAngle = 100.0f;
	}
}

void UEnemyPerceptionComponent::DrawDetectionRange(const AEnemyCharacter* Enemy) const
{
	if (!bDrawDetectionRange || !Enemy || !Enemy->GetWorld() || Enemy->GetEnemyState() == EEnemyState::Dead)
	{
		return;
	}

	const FVector EyeLocation = Enemy->GetActorLocation() + FVector(0.0f, 0.0f, Enemy->BaseEyeHeight);
	const FColor DetectionColor = TargetActor ? FColor::Red : FColor::Yellow;

	DrawDebugCone(
		Enemy->GetWorld(),
		EyeLocation,
		Enemy->GetActorForwardVector(),
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

bool UEnemyPerceptionComponent::TryAcquireTarget(AEnemyCharacter* Enemy)
{
	if (!Enemy)
	{
		return false;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(Enemy, 0);
	if (!PlayerPawn || !CanSeeTarget(Enemy, PlayerPawn))
	{
		return false;
	}

	SetTarget(PlayerPawn, Enemy->GetWorld());
	return true;
}

bool UEnemyPerceptionComponent::CanSeeTarget(const AEnemyCharacter* Enemy, const AActor* Target) const
{
	if (!Enemy || !Target)
	{
		return false;
	}

	const FVector EyeLocation = Enemy->GetActorLocation() + FVector(0.0f, 0.0f, Enemy->BaseEyeHeight);
	const FVector TargetLocation = Target->GetActorLocation();
	const FVector ToTarget = TargetLocation - EyeLocation;

	if (ToTarget.SizeSquared() > FMath::Square(SightDistance))
	{
		return false;
	}

	const FVector LocalDirection = Enemy->GetActorTransform().InverseTransformVectorNoScale(ToTarget.GetSafeNormal());
	const float HorizontalAngle = FMath::Abs(FMath::RadiansToDegrees(FMath::Atan2(LocalDirection.Y, LocalDirection.X)));
	const float VerticalAngle = FMath::Abs(FMath::RadiansToDegrees(
		FMath::Atan2(LocalDirection.Z, FVector2D(LocalDirection.X, LocalDirection.Y).Size())));

	if (HorizontalAngle > HorizontalSightAngle * 0.5f || VerticalAngle > VerticalSightAngle * 0.5f)
	{
		return false;
	}

	return HasLineOfSightToTarget(Enemy, Target);
}

bool UEnemyPerceptionComponent::HasLineOfSightToTarget(const AEnemyCharacter* Enemy, const AActor* Target) const
{
	if (!Enemy || !Target || !Enemy->GetWorld())
	{
		return false;
	}

	const FVector TraceStart = Enemy->GetActorLocation() + FVector(0.0f, 0.0f, Enemy->BaseEyeHeight);
	const FVector TraceEnd = Target->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);

	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyLineOfSight), true, Enemy);
	const bool bHit = Enemy->GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);

	return !bHit || Hit.GetActor() == Target;
}

bool UEnemyPerceptionComponent::ShouldLoseTarget(const AEnemyCharacter* Enemy) const
{
	if (!Enemy || !TargetActor || !Enemy->GetWorld())
	{
		return false;
	}

	const float LoseTargetDistance = SightDistance * LoseTargetDistanceMultiplier;
	const bool bTargetTooFar = FVector::DistSquared2D(Enemy->GetActorLocation(), TargetActor->GetActorLocation())
		> FMath::Square(LoseTargetDistance);
	const bool bMemoryExpired = Enemy->GetWorld()->GetTimeSeconds() - LastTargetSeenTime > LoseSightGraceDuration;

	return bTargetTooFar || (!CanSeeTarget(Enemy, TargetActor) && bMemoryExpired);
}

void UEnemyPerceptionComponent::SetTarget(AActor* NewTarget, const UWorld* World)
{
	TargetActor = NewTarget;
	if (!TargetActor)
	{
		return;
	}

	LastKnownTargetLocation = TargetActor->GetActorLocation();
	LastTargetSeenTime = World ? World->GetTimeSeconds() : LastTargetSeenTime;
}

void UEnemyPerceptionComponent::ClearTarget()
{
	TargetActor = nullptr;
}
