// Copyright Epic Games, Inc. All Rights Reserved.

#include "TPSGameMode.h"

#include "Character/TPSCharacter.h"

ATPSGameMode::ATPSGameMode()
{
	DefaultPawnClass = ATPSCharacter::StaticClass();
}
