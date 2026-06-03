// Copyright Epic Games, Inc. All Rights Reserved.

#include "TPSHUD.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "../Character/TPSCharacter.h"

void ATPSHUD::DrawHUD()
{
	Super::DrawHUD();

	const ATPSCharacter* TPSCharacter = Cast<ATPSCharacter>(GetOwningPawn());
	if (!Canvas || !TPSCharacter || !TPSCharacter->IsAiming())
	{
		return;
	}

	const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
	const FLinearColor DrawColor = CrosshairColor;

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
}
