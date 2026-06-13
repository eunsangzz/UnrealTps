// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TPSGameMode.generated.h"

UCLASS()
class UNREALTPS_API ATPSGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATPSGameMode();

	virtual void StartPlay() override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Spawning", meta = (ClampMin = "0"))
	int32 InitialEnemyCount = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Spawning", meta = (ClampMin = "0.0"))
	float MinimumSpawnDistance = 2000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Spawning", meta = (ClampMin = "0.0"))
	float MaximumSpawnDistance = 3500.0f;

private:
	void SpawnInitialEnemies();
	bool FindEnemySpawnLocation(const FVector& PlayerLocation, FVector& OutLocation) const;
};
