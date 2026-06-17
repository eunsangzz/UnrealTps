// Copyright Epic Games, Inc. All Rights Reserved.

#include "TPSHUD.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "../Character/TPSCharacter.h"
#include "../Components/HealthComponent.h"
#include "../Components/WeaponComponent.h"
#include "../TPSGameMode.h"
#include "../Weapon/WeaponBase.h"

void ATPSHUD::DrawHUD()
{
	Super::DrawHUD();

	const ATPSCharacter* TPSCharacter = Cast<ATPSCharacter>(GetOwningPawn());
	if (!Canvas || !TPSCharacter)
	{
		return;
	}

	const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
	const FLinearColor DrawColor = CrosshairColor;
	const float UIScale = FMath::Clamp(Canvas->ClipY / 1080.0f, 0.7f, 1.5f);

	const float DamageFlashAlpha = TPSCharacter->GetDamageFlashAlpha();
	if (DamageFlashAlpha > 0.0f)
	{
		FCanvasTileItem DamageOverlay(
			FVector2D::ZeroVector,
			FVector2D(Canvas->ClipX, Canvas->ClipY),
			FLinearColor(1.0f, 0.0f, 0.0f, DamageFlashAlpha));
		DamageOverlay.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(DamageOverlay);
	}

	FCanvasLineItem LeftLine(
		FVector2D(Center.X - CrosshairGap - CrosshairHalfSize, Center.Y),
		FVector2D(Center.X - CrosshairGap, Center.Y));
	LeftLine.SetColor(DrawColor);
	LeftLine.LineThickness = CrosshairThickness;

	FCanvasLineItem RightLine(
		FVector2D(Center.X + CrosshairGap, Center.Y),
		FVector2D(Center.X + CrosshairGap + CrosshairHalfSize, Center.Y));
	RightLine.SetColor(DrawColor);
	RightLine.LineThickness = CrosshairThickness;

	FCanvasLineItem TopLine(
		FVector2D(Center.X, Center.Y - CrosshairGap - CrosshairHalfSize),
		FVector2D(Center.X, Center.Y - CrosshairGap));
	TopLine.SetColor(DrawColor);
	TopLine.LineThickness = CrosshairThickness;

	FCanvasLineItem BottomLine(
		FVector2D(Center.X, Center.Y + CrosshairGap),
		FVector2D(Center.X, Center.Y + CrosshairGap + CrosshairHalfSize));
	BottomLine.SetColor(DrawColor);
	BottomLine.LineThickness = CrosshairThickness;

	Canvas->DrawItem(LeftLine);
	Canvas->DrawItem(RightLine);
	Canvas->DrawItem(TopLine);
	Canvas->DrawItem(BottomLine);

	DrawPlayerStatus(TPSCharacter, UIScale);
	DrawWeaponStatus(TPSCharacter, UIScale);
	DrawRemainingEnemies(UIScale);

	const ATPSGameMode* GameMode = GetWorld()->GetAuthGameMode<ATPSGameMode>();
	if (GameMode && GameMode->GetCombatState() == ETPSCombatState::Preparing && GameMode->GetCurrentWave() > 0)
	{
		DrawWaveClear(UIScale, GameMode->GetCurrentWave());
	}
	else if (GameMode && GameMode->GetCombatState() == ETPSCombatState::Victory)
	{
		DrawCombatResult(UIScale, true);
	}
	else if (GameMode && GameMode->GetCombatState() == ETPSCombatState::Defeat)
	{
		DrawCombatResult(UIScale, false);
	}
}

void ATPSHUD::DrawPlayerStatus(const ATPSCharacter* Character, float Scale)
{
	const UHealthComponent* Health = Character->GetHealthComponent();
	if (!Health || !GEngine)
	{
		return;
	}

	const float Width = 430.0f * Scale;
	const float Height = 58.0f * Scale;
	const FVector2D Position((Canvas->ClipX - Width) * 0.5f, Canvas->ClipY - 92.0f * Scale);
	DrawPanel(Position, FVector2D(Width, Height), FLinearColor(0.015f, 0.02f, 0.025f, 0.82f));

	const float HealthPercent = Health->GetHealthPercent();
	const FVector2D BarPosition = Position + FVector2D(18.0f, 27.0f) * Scale;
	const FVector2D BarSize(Width - 36.0f * Scale, 14.0f * Scale);
	DrawPanel(BarPosition, BarSize, FLinearColor(0.12f, 0.12f, 0.12f, 0.95f));

	const FLinearColor HealthColor = HealthPercent > 0.3f
		? FLinearColor(0.92f, 0.92f, 0.86f, 1.0f)
		: FLinearColor(0.9f, 0.12f, 0.08f, 1.0f);
	DrawPanel(BarPosition, FVector2D(BarSize.X * HealthPercent, BarSize.Y), HealthColor);

	DrawLabel(TEXT("HP"), Position + FVector2D(18.0f, 4.0f) * Scale, GEngine->GetSmallFont(), AccentColor, Scale);
	DrawLabel(
		FString::Printf(TEXT("%.0f"), Health->GetCurrentHealth()),
		Position + FVector2D(52.0f, 1.0f) * Scale,
		GEngine->GetMediumFont(),
		FLinearColor::White,
		Scale);
}

void ATPSHUD::DrawWeaponStatus(const ATPSCharacter* Character, float Scale)
{
	const UWeaponComponent* WeaponComponent = Character->GetWeaponComponent();
	const AWeaponBase* Weapon = WeaponComponent ? WeaponComponent->CurrentWeapon : nullptr;
	if (!Weapon || !GEngine)
	{
		return;
	}

	const FVector2D Size(285.0f, 112.0f);
	const FVector2D ScaledSize = Size * Scale;
	const FVector2D Position(Canvas->ClipX - ScaledSize.X - 42.0f * Scale, Canvas->ClipY - ScaledSize.Y - 42.0f * Scale);
	DrawPanel(Position, ScaledSize, FLinearColor(0.015f, 0.02f, 0.025f, 0.84f));

	const FString FireMode = WeaponComponent->IsAutomaticFireMode() ? TEXT("AUTO") : TEXT("SINGLE");
	DrawLabel(FireMode, Position + FVector2D(18.0f, 14.0f) * Scale, GEngine->GetSmallFont(), AccentColor, Scale);
	DrawLabel(
		FString::Printf(TEXT("%02d"), Weapon->CurrentAmmo),
		Position + FVector2D(18.0f, 39.0f) * Scale,
		GEngine->GetLargeFont(),
		FLinearColor::White,
		Scale * 1.25f);
	DrawLabel(
		FString::Printf(TEXT("/ %03d"), Weapon->ReserveAmmo),
		Position + FVector2D(112.0f, 65.0f) * Scale,
		GEngine->GetMediumFont(),
		FLinearColor(0.7f, 0.72f, 0.72f, 1.0f),
		Scale);
	DrawLabel(TEXT("B  FIRE MODE"), Position + FVector2D(174.0f, 17.0f) * Scale, GEngine->GetTinyFont(), FLinearColor::Gray, Scale);
}

void ATPSHUD::DrawRemainingEnemies(float Scale)
{
	if (!GEngine || !GetWorld())
	{
		return;
	}

	const ATPSGameMode* GameMode = GetWorld()->GetAuthGameMode<ATPSGameMode>();
	const int32 RemainingEnemies = GameMode ? GameMode->GetRemainingEnemyCount() : 0;
	const int32 CurrentWave = GameMode ? GameMode->GetCurrentWave() : 0;

	const FVector2D Size(200.0f, 72.0f);
	const FVector2D Position(Canvas->ClipX - Size.X * Scale - 42.0f * Scale, 38.0f * Scale);
	DrawPanel(Position, Size * Scale, FLinearColor(0.015f, 0.02f, 0.025f, 0.8f));
	DrawLabel(
		FString::Printf(TEXT("WAVE %d  ENEMIES ALIVE"), CurrentWave),
		Position + FVector2D(16.0f, 10.0f) * Scale,
		GEngine->GetTinyFont(),
		AccentColor,
		Scale);
	DrawLabel(
		FString::Printf(TEXT("%d"), RemainingEnemies),
		Position + FVector2D(16.0f, 27.0f) * Scale,
		GEngine->GetLargeFont(),
		FLinearColor::White,
		Scale);
}

void ATPSHUD::DrawWaveClear(float Scale, int32 ClearedWave)
{
	if (!GEngine)
	{
		return;
	}

	const FString WaveClearText = FString::Printf(TEXT("WAVE %d CLEAR"), ClearedWave);
	float TextWidth = 0.0f;
	float TextHeight = 0.0f;
	Canvas->StrLen(GEngine->GetLargeFont(), WaveClearText, TextWidth, TextHeight);

	DrawLabel(
		WaveClearText,
		FVector2D(
			(Canvas->ClipX - TextWidth * Scale * 1.25f) * 0.5f,
			Canvas->ClipY * 0.3f),
		GEngine->GetLargeFont(),
		AccentColor,
		Scale * 1.25f);
}

void ATPSHUD::DrawCombatResult(float Scale, bool bVictory)
{
	if (!GEngine)
	{
		return;
	}

	const FLinearColor ResultColor = bVictory
		? FLinearColor(0.12f, 0.9f, 0.42f, 1.0f)
		: FLinearColor(0.95f, 0.08f, 0.04f, 1.0f);
	FCanvasTileItem Overlay(
		FVector2D::ZeroVector,
		FVector2D(Canvas->ClipX, Canvas->ClipY),
		bVictory
			? FLinearColor(0.0f, 0.12f, 0.04f, 0.52f)
			: FLinearColor(0.12f, 0.0f, 0.0f, 0.52f));
	Overlay.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Overlay);

	const FString ResultText = bVictory ? TEXT("AREA CLEAR") : TEXT("YOU DIED");
	float TextWidth = 0.0f;
	float TextHeight = 0.0f;
	Canvas->StrLen(GEngine->GetLargeFont(), ResultText, TextWidth, TextHeight);

	DrawLabel(
		ResultText,
		FVector2D(
			(Canvas->ClipX - TextWidth * Scale * 1.6f) * 0.5f,
			Canvas->ClipY * 0.38f),
		GEngine->GetLargeFont(),
		ResultColor,
		Scale * 1.6f);

	const FString RestartText = TEXT("PRESS ENTER TO RESTART");
	Canvas->StrLen(GEngine->GetMediumFont(), RestartText, TextWidth, TextHeight);
	DrawLabel(
		RestartText,
		FVector2D(
			(Canvas->ClipX - TextWidth * Scale) * 0.5f,
			Canvas->ClipY * 0.52f),
		GEngine->GetMediumFont(),
		FLinearColor::White,
		Scale);
}

void ATPSHUD::DrawPanel(const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color)
{
	FCanvasTileItem Panel(Position, Size, Color);
	Panel.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Panel);
}

void ATPSHUD::DrawLabel(
	const FString& Text,
	const FVector2D& Position,
	UFont* Font,
	const FLinearColor& Color,
	float Scale)
{
	if (!Font)
	{
		return;
	}

	FCanvasTextItem TextItem(Position, FText::FromString(Text), Font, Color);
	TextItem.Scale = FVector2D(Scale);
	TextItem.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(TextItem);
}
