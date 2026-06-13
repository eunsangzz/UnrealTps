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
	void DrawPlayerStatus(const class ATPSCharacter* Character, float Scale);
	void DrawWeaponStatus(const class ATPSCharacter* Character, float Scale);
	void DrawRemainingEnemies(float Scale);
	void DrawPanel(const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color);
	void DrawLabel(const FString& Text, const FVector2D& Position, class UFont* Font, const FLinearColor& Color, float Scale);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair")
	float CrosshairHalfSize = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair")
	float CrosshairGap = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair")
	float CrosshairThickness = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crosshair")
	FLinearColor CrosshairColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD")
	FLinearColor AccentColor = FLinearColor(0.95f, 0.72f, 0.12f, 1.0f);
};
