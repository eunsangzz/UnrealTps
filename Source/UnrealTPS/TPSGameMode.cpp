// Copyright Epic Games, Inc. All Rights Reserved.

#include "TPSGameMode.h"

#include "Character/TPSCharacter.h"
#include "Components/HealthComponent.h"
#include "Enemy/EnemyCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
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

AActor* ATPSGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	TArray<AActor*> PlayerStarts;
	UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), PlayerStarts);

	if (!PlayerStarts.IsEmpty())
	{
		return PlayerStarts[FMath::RandRange(0, PlayerStarts.Num() - 1)];
	}

	return Super::ChoosePlayerStart_Implementation(Player);
}

void ATPSGameMode::SpawnInitialEnemies()
{
	CurrentWave = 1;
	SpawnWaveEnemies(InitialEnemyCount);
}

void ATPSGameMode::SpawnWaveEnemies(int32 EnemyCount)
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	UWorld* World = GetWorld();
	if (!PlayerPawn || !World)
	{
		InitializeCombatRound();
		return;
	}

	const FVector PlayerLocation = PlayerPawn->GetActorLocation();
	for (int32 EnemyIndex = 0; EnemyIndex < EnemyCount; ++EnemyIndex)
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

	InitializeCombatRound();
}

void ATPSGameMode::StartNextWave()
{
	if (CombatState != ETPSCombatState::Preparing)
	{
		return;
	}

	++CurrentWave;
	const int32 EnemyCount = InitialEnemyCount + (CurrentWave - 1) * EnemiesAddedPerWave;
	SpawnWaveEnemies(EnemyCount);
}

void ATPSGameMode::ScheduleNextWave()
{
	if (CombatState != ETPSCombatState::Playing)
	{
		return;
	}

	CombatState = ETPSCombatState::Preparing;
	if (NextWaveDelay <= 0.0f)
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &ATPSGameMode::StartNextWave);
	}
	else
	{
		GetWorldTimerManager().SetTimer(
			NextWaveTimerHandle,
			this,
			&ATPSGameMode::StartNextWave,
			NextWaveDelay,
			false);
	}
}

void ATPSGameMode::InitializeCombatRound()
{
	CombatState = ETPSCombatState::Playing;
	RemainingEnemyCount = 0;

	if (ATPSCharacter* PlayerCharacter = Cast<ATPSCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		if (UHealthComponent* PlayerHealth = PlayerCharacter->GetHealthComponent())
		{
			PlayerHealth->OnDeath.AddUniqueDynamic(this, &ATPSGameMode::HandlePlayerDeath);
		}
	}

	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		AEnemyCharacter* Enemy = *It;
		if (!Enemy || !Enemy->HealthComponent || Enemy->HealthComponent->IsDead())
		{
			continue;
		}

		++RemainingEnemyCount;
		Enemy->HealthComponent->OnDeath.AddUniqueDynamic(this, &ATPSGameMode::HandleEnemyDeath);
	}

	if (RemainingEnemyCount == 0)
	{
		ScheduleNextWave();
	}
}

void ATPSGameMode::HandlePlayerDeath(
	UHealthComponent* HealthComponent,
	AController* InstigatedBy,
	AActor* DamageCauser)
{
	FinishCombat(ETPSCombatState::Defeat);
}

void ATPSGameMode::HandleEnemyDeath(
	UHealthComponent* HealthComponent,
	AController* InstigatedBy,
	AActor* DamageCauser)
{
	if (CombatState != ETPSCombatState::Playing)
	{
		return;
	}

	RemainingEnemyCount = FMath::Max(0, RemainingEnemyCount - 1);
	if (RemainingEnemyCount == 0)
	{
		ScheduleNextWave();
	}
}

void ATPSGameMode::FinishCombat(ETPSCombatState Result)
{
	const bool bCanFinish = CombatState == ETPSCombatState::Playing
		|| (Result == ETPSCombatState::Defeat && CombatState == ETPSCombatState::Preparing);
	if (!bCanFinish)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(NextWaveTimerHandle);
	CombatState = Result;

	if (ATPSCharacter* PlayerCharacter = Cast<ATPSCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		PlayerCharacter->HandleCombatEnded();
	}
}

void ATPSGameMode::RestartCombat()
{
	if (CombatState != ETPSCombatState::Victory && CombatState != ETPSCombatState::Defeat)
	{
		return;
	}

	const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	if (!CurrentLevelName.IsEmpty())
	{
		UGameplayStatics::OpenLevel(this, FName(*CurrentLevelName));
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
