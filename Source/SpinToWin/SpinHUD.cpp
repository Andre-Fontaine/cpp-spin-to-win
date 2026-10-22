// Spin To Win - draws the clock face, hand, target zone and text.

#include "SpinHUD.h"
#include "SpinGameMode.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

void ASpinHUD::DrawHUD()
{
	Super::DrawHUD();

	const ASpinGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASpinGameMode>() : nullptr;
	if (!GM || !Canvas)
	{
		return;
	}

	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;
	const float R = FMath::Min(Canvas->SizeX, Canvas->SizeY) * 0.35f;

	// 0 deg = 12 o'clock, clockwise positive.
	auto PointAt = [&](float AngleDeg, float Radius) -> FVector2D
	{
		const float Rad = FMath::DegreesToRadians(AngleDeg);
		return FVector2D(CX + Radius * FMath::Sin(Rad), CY - Radius * FMath::Cos(Rad));
	};

	// Dial outline.
	const int32 Segments = 72;
	for (int32 i = 0; i < Segments; ++i)
	{
		const FVector2D A = PointAt(i * 360.f / Segments, R);
		const FVector2D B = PointAt((i + 1) * 360.f / Segments, R);
		DrawLine(A.X, A.Y, B.X, B.Y, FLinearColor(0.6f, 0.6f, 0.6f), 3.f);
	}

	// Hour ticks.
	for (int32 i = 0; i < 12; ++i)
	{
		const FVector2D A = PointAt(i * 30.f, R * 0.92f);
		const FVector2D B = PointAt(i * 30.f, R);
		DrawLine(A.X, A.Y, B.X, B.Y, FLinearColor(0.6f, 0.6f, 0.6f), 3.f);
	}

	const bool bShowZone = GM->State != ESpinState::WaitingToStart;

	// Target zone: a band of short radial lines.
	if (bShowZone)
	{
		const bool bActive = GM->State == ESpinState::Playing && GM->IsHandInZone();
		const FLinearColor ZoneColor = bActive ? FLinearColor(0.5f, 1.f, 0.5f) : FLinearColor(0.1f, 0.8f, 0.2f);

		for (float Offset = -GM->ZoneHalfWidth; Offset <= GM->ZoneHalfWidth; Offset += 0.5f)
		{
			const FVector2D A = PointAt(GM->ZoneCenter + Offset, R * 0.72f);
			const FVector2D B = PointAt(GM->ZoneCenter + Offset, R * 0.9f);
			DrawLine(A.X, A.Y, B.X, B.Y, ZoneColor, 4.f);
		}
	}

	// Hand.
	const FLinearColor HandColor = (GM->State == ESpinState::GameOver) ? FLinearColor::Red : FLinearColor::White;
	const FVector2D Tip = PointAt(GM->HandAngle, R * 0.9f);
	DrawLine(CX, CY, Tip.X, Tip.Y, HandColor, 5.f);
	DrawRect(HandColor, CX - 7.f, CY - 7.f, 14.f, 14.f);

	// Text.
	const float TopY = CY - R - 90.f;
	DrawCenteredText(FString::Printf(TEXT("Score: %d"), GM->Score), CX, TopY, FLinearColor::White, 2.f);
	DrawCenteredText(FString::Printf(TEXT("Best: %d"), GM->HighScore), CX, TopY + 40.f, FLinearColor(0.8f, 0.8f, 0.8f), 1.2f);

	const float BottomY = CY + R + 30.f;
	if (GM->State == ESpinState::WaitingToStart)
	{
		DrawCenteredText(TEXT("SPIN TO WIN"), CX, CY - 15.f - R * 0.45f, FLinearColor::Yellow, 2.f);
		DrawCenteredText(TEXT("Press SPACE / Click to start"), CX, BottomY, FLinearColor::White, 1.3f);
	}
	else if (GM->State == ESpinState::GameOver)
	{
		DrawCenteredText(TEXT("GAME OVER"), CX, CY - 15.f - R * 0.45f, FLinearColor::Red, 2.f);
		DrawCenteredText(TEXT("Press SPACE / Click to retry"), CX, BottomY, FLinearColor::White, 1.3f);
	}
	else
	{
		DrawCenteredText(TEXT("Press SPACE when the hand is in the green!"), CX, BottomY, FLinearColor(0.8f, 0.8f, 0.8f), 1.0f);
	}
}

void ASpinHUD::DrawCenteredText(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float Scale)
{
	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	float W = 0.f, H = 0.f;
	GetTextSize(Text, W, H, Font, Scale);
	DrawText(Text, Color, CenterX - W * 0.5f, Y, Font, Scale);
}
