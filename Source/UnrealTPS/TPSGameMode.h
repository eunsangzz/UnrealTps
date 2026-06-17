// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TPSGameMode.generated.h"

UENUM(BlueprintType)
enum class ETPSCombatState : uint8
{
	Preparing,
	Playing,
	Victory,
	Defeat
};

UCLASS()
class UNREALTPS_API ATPSGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATPSGameMode();

	virtual void StartPlay() override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	UFUNCTION(BlueprintPure, Category = "Combat")
	ETPSCombatState GetCombatState() const { return CombatState; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	int32 GetRemainingEnemyCount() const { return RemainingEnemyCount; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	int32 GetCurrentWave() const { return CurrentWave; }

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void RestartCombat();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Spawning", meta = (ClampMin = "0"))
	int32 InitialEnemyCount = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Spawning", meta = (ClampMin = "0"))
	int32 EnemiesAddedPerWave = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Spawning", meta = (ClampMin = "0.0"))
	float NextWaveDelay = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Spawning", meta = (ClampMin = "0.0"))
	float MinimumSpawnDistance = 2000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Spawning", meta = (ClampMin = "0.0"))
	float MaximumSpawnDistance = 3500.0f;

private:
	void SpawnInitialEnemies();
	void SpawnWaveEnemies(int32 EnemyCount);
	void StartNextWave();
	void ScheduleNextWave();
	void InitializeCombatRound();
	void FinishCombat(ETPSCombatState Result);
	bool FindEnemySpawnLocation(const FVector& PlayerLocation, FVector& OutLocation) const;

	UFUNCTION()
	void HandlePlayerDeath(class UHealthComponent* HealthComponent, AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION()
	void HandleEnemyDeath(class UHealthComponent* HealthComponent, AController* InstigatedBy, AActor* DamageCauser);

	UPROPERTY(VisibleInstanceOnly, Category = "Combat")
	ETPSCombatState CombatState = ETPSCombatState::Preparing;

	UPROPERTY(VisibleInstanceOnly, Category = "Combat")
	int32 RemainingEnemyCount = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "Combat")
	int32 CurrentWave = 0;

	FTimerHandle NextWaveTimerHandle;
};
