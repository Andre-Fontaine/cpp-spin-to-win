// Spin To Win - 2D overlay: score, level, messages and key prompts.
// The dial, hand, target zone and obstacles are 3D meshes (see SpinClock).

#include "SpinHUD.h"
#include "SpinGameMode.h"
#include "SpinClock.h"
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

	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	const float CX = W * 0.5f;
	const float CY = H * 0.5f;

	const FLinearColor LiftColor(0.4f, 0.9f, 1.f);

	const FSpinPrompt* ActivePrompt = GM->Prompts.IsValidIndex(GM->ActivePromptIndex)
		? &GM->Prompts[GM->ActivePromptIndex]
		: nullptr;

	// Which key does the current zone want? Float the label over the zone in the 3D scene.
	if (GM->State == ESpinState::Playing && ActivePrompt)
	{
		if (const ASpinClock* Clock = GM->GetClock())
		{
			const FVector Screen = Project(Clock->GetZoneWorldLocation(0.5f));
			DrawCenteredText(ActivePrompt->Label, Screen.X, Screen.Y - 14.f, ActivePrompt->Color, 1.8f);
		}
	}

	// Top text: level, progress, best.
	const float TopY = H * 0.025f;
	DrawCenteredText(FString::Printf(TEXT("LEVEL %d"), GM->GetLevel()), CX, TopY, FLinearColor(0.4f, 0.9f, 1.f), 1.4f);
	DrawCenteredText(FString::Printf(TEXT("%d / %d"), GM->Score, GM->GetLevelTarget()), CX, TopY + 35.f, FLinearColor::White, 2.f);
	DrawCenteredText(FString::Printf(TEXT("Best: %d"), GM->HighScore), CX, TopY + 80.f, FLinearColor(0.8f, 0.8f, 0.8f), 1.1f);

	// Lives (only once obstacles are in play).
	if (GM->State != ESpinState::WaitingToStart && GM->GetLevel() >= GM->ObstacleStartLevel)
	{
		const float Size = 22.f;
		const float Gap = 10.f;
		const float RowWidth = GM->MaxLives * Size + (GM->MaxLives - 1) * Gap;
		const float RowX = CX - RowWidth * 0.5f;
		const float RowY = TopY + 118.f;

		for (int32 i = 0; i < GM->MaxLives; ++i)
		{
			const FLinearColor BoxColor = (i < GM->Lives) ? FLinearColor(0.95f, 0.25f, 0.3f) : FLinearColor(0.2f, 0.2f, 0.24f);
			DrawRect(BoxColor, RowX + i * (Size + Gap), RowY, Size, Size);
		}

		const float Now = GetWorld()->GetTimeSeconds();
		if (Now - GM->LifeLostTime < 1.2f)
		{
			DrawCenteredText(TEXT("-1 LIFE  (nothing to raise the hand over)"), CX, RowY + 32.f, FLinearColor::Red, 1.1f);
		}
		else if (Now - GM->LifeGainedTime < 1.2f)
		{
			DrawCenteredText(TEXT("+1 LIFE"), CX, RowY + 32.f, FLinearColor::Green, 1.1f);
		}
	}

	// Messages sit above the dial, hints below it.
	const float MessageY = H * 0.21f;
	const float BottomY = H * 0.9f;

	if (GM->State == ESpinState::WaitingToStart)
	{
		DrawCenteredText(TEXT("SPIN TO WIN"), CX, MessageY, FLinearColor::Yellow, 2.4f);
		DrawCenteredText(TEXT("Press SPACE / A to start"), CX, BottomY, FLinearColor::White, 1.3f);
	}
	else if (GM->State == ESpinState::GameOver)
	{
		DrawCenteredText(TEXT("GAME OVER"), CX, MessageY, FLinearColor::Red, 2.4f);
		DrawCenteredText(FString::Printf(TEXT("You reached level %d with %d hits"), GM->GetLevel(), GM->Score), CX, MessageY + 55.f, FLinearColor::White, 1.2f);
		if (GM->bOutOfLives)
		{
			DrawCenteredText(TEXT("Out of lives!"), CX, MessageY + 90.f, FLinearColor::Red, 1.2f);
		}
		DrawCenteredText(TEXT("Press SPACE / A to retry"), CX, BottomY, FLinearColor::White, 1.3f);
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

		if (GM->GetLevel() >= GM->ObstacleStartLevel)
		{
			const ASpinClock* LiftClock = GM->GetClock();
			if (LiftClock && LiftClock->IsLiftAvailable())
			{
				DrawCenteredText(TEXT("HOLD LEFT CLICK / R2 TO RAISE THE HAND!"), CX, BottomY + 32.f, LiftColor, 1.4f);
			}
			else
			{
				DrawCenteredText(TEXT("Raise the hand (LEFT CLICK / R2) only when an obstacle is out, or lose a life"), CX, BottomY + 32.f, FLinearColor(0.45f, 0.45f, 0.5f), 1.0f);
			}
		}
	}

	// Level-up banner.
	if (GM->IsShowingLevelBanner())
	{
		DrawCenteredText(FString::Printf(TEXT("LEVEL %d"), GM->GetLevel()), CX, MessageY, FLinearColor::Yellow, 2.4f);
	}

	// "New button unlocked" pause screen.
	if (GM->State == ESpinState::NewButtonIntro && GM->Prompts.IsValidIndex(1))
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.8f), 0.f, 0.f, W, H);

		const FSpinPrompt& First = GM->Prompts[0];
		const FSpinPrompt& Second = GM->Prompts[1];

		DrawCenteredText(TEXT("NEW BUTTON UNLOCKED!"), CX, CY - 150.f, FLinearColor::Yellow, 2.4f);
		DrawCenteredText(TEXT("From now on the zones will ask for either button, at random:"), CX, CY - 85.f, FLinearColor::White, 1.2f);
		DrawCenteredText(First.Hint, CX, CY - 35.f, First.Color, 1.6f);
		DrawCenteredText(Second.Hint, CX, CY + 10.f, Second.Color, 1.6f);
		DrawCenteredText(TEXT("The hand has slowed down and the zones are wide again so you can get used to it."), CX, CY + 75.f, FLinearColor(0.8f, 0.8f, 0.8f), 1.0f);
		DrawCenteredText(FString::Printf(TEXT("Press %s to continue"), *Second.Hint), CX, CY + 130.f, Second.Color, 1.8f);
	}

	// "Obstacles" tutorial pause screen.
	if (GM->State == ESpinState::ObstacleIntro)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.8f), 0.f, 0.f, W, H);

		DrawCenteredText(TEXT("OBSTACLES!"), CX, CY - 150.f, FLinearColor::Yellow, 2.4f);
		DrawCenteredText(TEXT("Objects will now slide onto the dial. Don't let the hand touch them."), CX, CY - 85.f, FLinearColor::White, 1.2f);
		DrawCenteredText(TEXT("HOLD  LEFT CLICK / R2  to raise the hand over them"), CX, CY - 35.f, LiftColor, 1.6f);
		DrawCenteredText(TEXT("It only works while an obstacle is out, and drops by itself once you're past it."), CX, CY + 20.f, FLinearColor(0.8f, 0.8f, 0.8f), 1.0f);
		DrawCenteredText(FString::Printf(TEXT("You have %d lives. Raising the hand with no obstacle out costs one; hopping an obstacle earns one back."), GM->MaxLives), CX, CY + 55.f, FLinearColor(1.f, 0.6f, 0.6f), 1.0f);
		DrawCenteredText(TEXT("Hold LEFT CLICK / R2 to continue"), CX, CY + 110.f, LiftColor, 1.8f);
	}
}

void ASpinHUD::DrawCenteredText(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float Scale)
{
	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	float W = 0.f, H = 0.f;
	GetTextSize(Text, W, H, Font, Scale);
	DrawText(Text, Color, CenterX - W * 0.5f, Y, Font, Scale);
}
