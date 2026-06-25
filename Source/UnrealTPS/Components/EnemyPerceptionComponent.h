// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../Enemy/EnemyTypes.h"
#include "EnemyPerceptionComponent.generated.h"

class AEnemyCharacter;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UNREALTPS_API UEnemyPerceptionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyPerceptionComponent();

	void ConfigureForEnemyType(EEnemyType EnemyType);
	void DrawDetectionRange(const AEnemyCharacter* Enemy) const;
	bool TryAcquireTarget(AEnemyCharacter* Enemy);
	bool CanSeeTarget(const AEnemyCharacter* Enemy, const AActor* Target) const;
	bool HasLineOfSightToTarget(const AEnemyCharacter* Enemy, const AActor* Target) const;
	bool ShouldLoseTarget(const AEnemyCharacter* Enemy) const;
	void SetTarget(AActor* NewTarget, const UWorld* World);
	void ClearTarget();

	float GetSightDistance() const { return SightDistance; }
	AActor* GetTargetActor() const { return TargetActor; }
	const FVector& GetLastKnownTargetLocation() const { return LastKnownTargetLocation; }

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

private:
	UPROPERTY()
	TObjectPtr<AActor> TargetActor;

	FVector LastKnownTargetLocation = FVector::ZeroVector;
	double LastTargetSeenTime = -1000.0;
};
