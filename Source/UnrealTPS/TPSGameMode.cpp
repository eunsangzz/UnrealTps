// Copyright Epic Games, Inc. All Rights Reserved.

#include "TPSGameMode.h"

#include "Character/TPSCharacter.h"
#include "UI/TPSHUD.h"

ATPSGameMode::ATPSGameMode()
{
	DefaultPawnClass = ATPSCharacter::StaticClass();
	HUDClass = ATPSHUD::StaticClass();
}
