// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TPSHUD.generated.h"

UCLASS()
class UNREALTPS_API ATPSHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair")
	float CrosshairHalfSize = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair")
	float CrosshairGap = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair")
	float CrosshairThickness = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair")
	FLinearColor CrosshairColor = FLinearColor::White;
};
