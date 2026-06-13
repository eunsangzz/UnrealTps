// Copyright Epic Games, Inc. All Rights Reserved.

#include "TPSGameMode.h"

#include "Character/TPSCharacter.h"
#include "Enemy/EnemyCharacter.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "UI/TPSHUD.h"

ATPSGameMode::ATPSGameMode()
{
	DefaultPawnClass = ATPSCharacter::StaticClass();
	HUDClass = ATPSHUD::StaticClass();
}

void ATPSGameMode::StartPlay()
{
	Super::StartPlay();

	GetWorldTimerManager().SetTimerForNextTick(this, &ATPSGameMode::SpawnInitialEnemies);
}

void ATPSGameMode::SpawnInitialEnemies()
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	UWorld* World = GetWorld();
	if (!PlayerPawn || !World)
	{
		return;
	}

	const FVector PlayerLocation = PlayerPawn->GetActorLocation();
	for (int32 EnemyIndex = 0; EnemyIndex < InitialEnemyCount; ++EnemyIndex)
	{
		FVector SpawnLocation;
		if (!FindEnemySpawnLocation(PlayerLocation, SpawnLocation))
		{
			continue;
		}

		const FRotator SpawnRotation = (PlayerLocation - SpawnLocation).Rotation();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AMeleeEnemyCharacter* SpawnedEnemy = World->SpawnActor<AMeleeEnemyCharacter>(
			AMeleeEnemyCharacter::StaticClass(),
			SpawnLocation,
			FRotator(0.0f, SpawnRotation.Yaw, 0.0f),
			SpawnParameters);

		if (SpawnedEnemy && !SpawnedEnemy->GetController())
		{
			SpawnedEnemy->SpawnDefaultController();
		}
	}
}

bool ATPSGameMode::FindEnemySpawnLocation(const FVector& PlayerLocation, FVector& OutLocation) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float MinDistance = FMath::Min(MinimumSpawnDistance, MaximumSpawnDistance);
	const float MaxDistance = FMath::Max(MinimumSpawnDistance, MaximumSpawnDistance);

	for (int32 Attempt = 0; Attempt < 20; ++Attempt)
	{
		const float Angle = FMath::FRandRange(0.0f, 2.0f * UE_PI);
		const float Distance = FMath::FRandRange(MinDistance, MaxDistance);
		const FVector CandidateLocation = PlayerLocation + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Distance;

		if (UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetCurrent(World))
		{
			FNavLocation NavLocation;
			if (NavigationSystem->ProjectPointToNavigation(CandidateLocation, NavLocation, FVector(500.0f, 500.0f, 2000.0f)))
			{
				OutLocation = NavLocation.Location + FVector(0.0f, 0.0f, 100.0f);
				return true;
			}
		}

		FHitResult GroundHit;
		const FVector TraceStart = CandidateLocation + FVector(0.0f, 0.0f, 5000.0f);
		const FVector TraceEnd = CandidateLocation - FVector(0.0f, 0.0f, 5000.0f);
		if (World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility))
		{
			OutLocation = GroundHit.ImpactPoint + FVector(0.0f, 0.0f, 100.0f);
			return true;
		}
	}

	return false;
}
