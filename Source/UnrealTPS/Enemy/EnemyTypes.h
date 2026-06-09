// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnemyTypes.generated.h"

UENUM(BlueprintType)
enum class EEnemyState : uint8
{
	Idle,
	Patrol,
	Chase,
	Attack,
	Dead
};

UENUM(BlueprintType)
enum class EEnemyType : uint8
{
	Melee,
	Ranged
};
