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

	const bool bPlaying = GM->State == ESpinState::Playing;
	const bool bShowZone = GM->State != ESpinState::WaitingToStart;
	const FSpinPrompt* ActivePrompt = GM->Prompts.IsValidIndex(GM->ActivePromptIndex)
		? &GM->Prompts[GM->ActivePromptIndex]
		: nullptr;

	// Target zone: a band of short radial lines, coloured by the button it needs.
	if (bShowZone)
	{
		const FLinearColor BaseColor = ActivePrompt ? ActivePrompt->Color : FLinearColor(0.1f, 0.8f, 0.2f);
		const bool bActive = bPlaying && GM->IsHandInZone();
		const FLinearColor ZoneColor = bActive ? (BaseColor + (FLinearColor::White - BaseColor) * 0.45f) : BaseColor;

		for (float Offset = -GM->ZoneHalfWidth; Offset <= GM->ZoneHalfWidth; Offset += 0.5f)
		{
			const FVector2D A = PointAt(GM->ZoneCenter + Offset, R * 0.72f);
			const FVector2D B = PointAt(GM->ZoneCenter + Offset, R * 0.9f);
			DrawLine(A.X, A.Y, B.X, B.Y, ZoneColor, 4.f);
		}

		// Which key does this zone want? Draw it just inside the zone.
		if (bPlaying && ActivePrompt)
		{
			const FVector2D LabelPos = PointAt(GM->ZoneCenter, R * 0.58f);
			DrawCenteredText(ActivePrompt->Label, LabelPos.X, LabelPos.Y - 14.f, BaseColor, 1.6f);
		}
	}

	// Hand.
	const FLinearColor HandColor = (GM->State == ESpinState::GameOver) ? FLinearColor::Red : FLinearColor::White;
	const FVector2D Tip = PointAt(GM->HandAngle, R * 0.9f);
	DrawLine(CX, CY, Tip.X, Tip.Y, HandColor, 5.f);
	DrawRect(HandColor, CX - 7.f, CY - 7.f, 14.f, 14.f);

	// Top text: level, progress, best.
	const float TopY = CY - R - 130.f;
	DrawCenteredText(FString::Printf(TEXT("LEVEL %d"), GM->GetLevel()), CX, TopY, FLinearColor(0.4f, 0.9f, 1.f), 1.4f);
	DrawCenteredText(FString::Printf(TEXT("%d / %d"), GM->Score, GM->GetLevelTarget()), CX, TopY + 35.f, FLinearColor::White, 2.f);
	DrawCenteredText(FString::Printf(TEXT("Best: %d"), GM->HighScore), CX, TopY + 80.f, FLinearColor(0.8f, 0.8f, 0.8f), 1.1f);

	// Bottom text.
	const float BottomY = CY + R + 30.f;
	if (GM->State == ESpinState::WaitingToStart)
	{
		DrawCenteredText(TEXT("SPIN TO WIN"), CX, CY - 15.f - R * 0.45f, FLinearColor::Yellow, 2.f);
		DrawCenteredText(TEXT("Press SPACE / Click to start"), CX, BottomY, FLinearColor::White, 1.3f);
	}
	else if (GM->State == ESpinState::GameOver)
	{
		DrawCenteredText(TEXT("GAME OVER"), CX, CY - 15.f - R * 0.45f, FLinearColor::Red, 2.f);
		DrawCenteredText(FString::Printf(TEXT("You reached level %d with %d hits"), GM->GetLevel(), GM->Score), CX, CY + R * 0.3f, FLinearColor::White, 1.2f);
		DrawCenteredText(TEXT("Press SPACE / Click to retry"), CX, BottomY, FLinearColor::White, 1.3f);
	}
	else
	{
		// List the buttons that are currently in play.
		FString Hint;
		for (int32 i = 0; i < GM->GetNumActiveButtons(); ++i)
		{
			if (GM->Prompts.IsValidIndex(i))
			{
				if (!Hint.IsEmpty())
				{
					Hint += TEXT("     |     ");
				}
				Hint += GM->Prompts[i].Hint;
			}
		}
		DrawCenteredText(Hint, CX, BottomY, FLinearColor(0.8f, 0.8f, 0.8f), 1.0f);
	}

	// "New button unlocked" pause screen.
	if (GM->State == ESpinState::NewButtonIntro && GM->Prompts.IsValidIndex(1))
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.8f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);

		const FSpinPrompt& First = GM->Prompts[0];
		const FSpinPrompt& Second = GM->Prompts[1];

		DrawCenteredText(TEXT("NEW BUTTON UNLOCKED!"), CX, CY - 150.f, FLinearColor::Yellow, 2.4f);
		DrawCenteredText(TEXT("From now on the zones alternate between two buttons:"), CX, CY - 85.f, FLinearColor::White, 1.2f);
		DrawCenteredText(First.Hint, CX, CY - 35.f, First.Color, 1.6f);
		DrawCenteredText(Second.Hint, CX, CY + 10.f, Second.Color, 1.6f);
		DrawCenteredText(TEXT("The hand has slowed down so you can get used to it."), CX, CY + 75.f, FLinearColor(0.8f, 0.8f, 0.8f), 1.0f);
		DrawCenteredText(FString::Printf(TEXT("Press %s to continue"), *Second.Label), CX, CY + 130.f, Second.Color, 1.8f);
	}

	// Level-up banner.
	if (GM->IsShowingLevelBanner())
	{
		DrawCenteredText(FString::Printf(TEXT("LEVEL %d"), GM->GetLevel()), CX, CY - R * 0.45f - 15.f, FLinearColor::Yellow, 2.4f);

		if (GM->GetLevel() == GM->SecondButtonLevel && GM->Prompts.IsValidIndex(1))
		{
			DrawCenteredText(FString::Printf(TEXT("New button: %s"), *GM->Prompts[1].Label), CX, CY + R * 0.3f, GM->Prompts[1].Color, 1.5f);
		}
	}
}

void ASpinHUD::DrawCenteredText(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float Scale)
{
	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	float W = 0.f, H = 0.f;
	GetTextSize(Text, W, H, Font, Scale);
	DrawText(Text, Color, CenterX - W * 0.5f, Y, Font, Scale);
}
