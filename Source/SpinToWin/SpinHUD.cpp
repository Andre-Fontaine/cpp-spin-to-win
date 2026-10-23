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
	const FLinearColor DimColor(0.8f, 0.8f, 0.8f);
	const FLinearColor NoticeColor(1.f, 0.8f, 0.3f);

	const bool bTutorial = GM->IsTutorial();
	const bool bTitle = GM->State == ESpinState::WaitingToStart;
	const bool bJammed = GM->JamPhase == EJamPhase::Jammed;

	const FSpinPrompt* ActivePrompt = GM->Prompts.IsValidIndex(GM->ActivePromptIndex)
		? &GM->Prompts[GM->ActivePromptIndex]
		: nullptr;

	// Which key does the current zone want? Float the label over the zone in the 3D scene.
	if (GM->State == ESpinState::Playing && !bJammed && ActivePrompt)
	{
		if (const ASpinClock* Clock = GM->GetClock())
		{
			const FVector Screen = Project(Clock->GetZoneWorldLocation(0.5f));
			DrawCenteredText(ActivePrompt->Label, Screen.X, Screen.Y - 14.f, ActivePrompt->Color, 1.8f);
		}
	}

	// Top text: level, progress, best.
	const float TopY = H * 0.025f;
	if (!bTitle)
	{
		if (bTutorial)
		{
			const FTutorialLevel* Level = GM->GetTutorialLevel();
			if (GM->State != ESpinState::TutorialComplete && Level)
			{
				DrawCenteredText(FString::Printf(TEXT("TUTORIAL  -  LEVEL %d / %d"), GM->TutorialIndex + 1, GM->TutorialLevels.Num()), CX, TopY, LiftColor, 1.4f);
				DrawCenteredText(FString::Printf(TEXT("%d / %d"), GM->TutorialHits, Level->Hits), CX, TopY + 35.f, FLinearColor::White, 2.f);
			}
		}
		else
		{
			DrawCenteredText(FString::Printf(TEXT("LEVEL %d"), GM->GetLevel()), CX, TopY, LiftColor, 1.4f);
			DrawCenteredText(FString::Printf(TEXT("%d / %d"), GM->Score, GM->GetLevelTarget()), CX, TopY + 35.f, FLinearColor::White, 2.f);
			DrawCenteredText(FString::Printf(TEXT("Best: %d"), GM->HighScore), CX, TopY + 80.f, DimColor, 1.1f);
		}
	}

	// Lives (only once obstacles are in play). In the tutorial they are shown but never go down.
	if (!bTitle && GM->State != ESpinState::TutorialComplete && GM->ObstaclesEnabled())
	{
		const float Size = 22.f;
		const float Gap = 10.f;
		const float RowWidth = GM->MaxLives * Size + (GM->MaxLives - 1) * Gap;
		const float RowX = CX - RowWidth * 0.5f;
		const float RowY = TopY + (bTutorial ? 80.f : 118.f);

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

	if (bTitle)
	{
		DrawCenteredText(TEXT("SPIN TO WIN"), CX, MessageY, FLinearColor::Yellow, 2.4f);
		if (GM->Prompts.IsValidIndex(1))
		{
			DrawCenteredText(FString::Printf(TEXT("%s  -  TUTORIAL"), *GM->Prompts[0].Hint), CX, BottomY - 40.f, GM->Prompts[0].Color, 1.5f);
			DrawCenteredText(FString::Printf(TEXT("%s  -  ENDLESS (skip the tutorial)"), *GM->Prompts[1].Hint), CX, BottomY, GM->Prompts[1].Color, 1.5f);
		}
	}
	else if (GM->State == ESpinState::GameOver)
	{
		DrawCenteredText(TEXT("GAME OVER"), CX, MessageY, FLinearColor::Red, 2.4f);
		DrawCenteredText(FString::Printf(TEXT("You reached level %d with %d hits"), GM->GetLevel(), GM->Score), CX, MessageY + 55.f, FLinearColor::White, 1.2f);
		if (GM->bOutOfLives)
		{
			DrawCenteredText(TEXT("Out of lives!"), CX, MessageY + 90.f, FLinearColor::Red, 1.2f);
		}
		else if (GM->bHandBroke)
		{
			DrawCenteredText(TEXT("The hand broke! You didn't fix the jam in time."), CX, MessageY + 90.f, FLinearColor::Red, 1.2f);
		}
		DrawCenteredText(FString::Printf(TEXT("Press %s to retry"), GM->Prompts.IsValidIndex(0) ? *GM->Prompts[0].Hint : TEXT("W")), CX, BottomY, FLinearColor::White, 1.3f);
	}
	else if (GM->State == ESpinState::TutorialComplete)
	{
		DrawCenteredText(TEXT("TUTORIAL COMPLETE!"), CX, MessageY, FLinearColor::Yellow, 2.4f);
		DrawCenteredText(TEXT("From here on mistakes cost lives, and a jam you can't fix ends the run."), CX, MessageY + 55.f, FLinearColor::White, 1.2f);
		if (GM->Prompts.IsValidIndex(1))
		{
			DrawCenteredText(FString::Printf(TEXT("%s  -  START ENDLESS"), *GM->Prompts[0].Hint), CX, BottomY - 40.f, GM->Prompts[0].Color, 1.5f);
			DrawCenteredText(FString::Printf(TEXT("%s  -  BACK TO TITLE"), *GM->Prompts[1].Hint), CX, BottomY, GM->Prompts[1].Color, 1.5f);
		}
	}
	else if (GM->State == ESpinState::Playing)
	{
		// What to do right now.
		if (bTutorial)
		{
			if (const FTutorialLevel* Level = GM->GetTutorialLevel())
			{
				DrawCenteredText(Level->Instruction, CX, BottomY, DimColor, 1.1f);
			}
		}
		else
		{
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
			DrawCenteredText(Hint, CX, BottomY, DimColor, 1.0f);
		}

		// Lift hint.
		if (GM->ObstaclesEnabled() && !bJammed)
		{
			const ASpinClock* LiftClock = GM->GetClock();
			if (LiftClock && LiftClock->IsLiftAvailable())
			{
				DrawCenteredText(TEXT("HOLD LEFT CLICK / R2 TO RAISE THE HAND!"), CX, BottomY + 32.f, LiftColor, 1.4f);
			}
			else if (!bTutorial)
			{
				DrawCenteredText(TEXT("Raise the hand (LEFT CLICK / R2) only when an obstacle is out, or lose a life"), CX, BottomY + 32.f, FLinearColor(0.45f, 0.45f, 0.5f), 1.0f);
			}
		}

		// Tutorial messages ("Try again!").
		if (bTutorial && GM->IsShowingNotice())
		{
			DrawCenteredText(GM->NoticeText, CX, MessageY, NoticeColor, 1.3f);
		}

		// Jam: warning, then the repair bars.
		if (GM->JamPhase == EJamPhase::Sparking)
		{
			DrawCenteredText(TEXT("SPARKS!  The hand is about to jam..."), CX, MessageY + 45.f, FLinearColor(1.f, 0.7f, 0.1f), 1.5f);
		}
		else if (bJammed)
		{
			const FString Key = GM->Prompts.IsValidIndex(0) ? GM->Prompts[0].Hint : FString(TEXT("W"));
			DrawCenteredText(FString::Printf(TEXT("JAMMED!  Press %s again and again!"), *Key), CX, MessageY + 45.f, FLinearColor(1.f, 0.4f, 0.1f), 1.8f);

			const float BarW = W * 0.34f;
			const float BarH = 22.f;
			const float BarX = CX - BarW * 0.5f;
			const float BarY = BottomY - 95.f;

			// Repair progress.
			DrawCenteredText(TEXT("REPAIR"), CX, BarY - 28.f, FLinearColor::White, 1.0f);
			DrawRect(FLinearColor(0.15f, 0.15f, 0.18f), BarX, BarY, BarW, BarH);
			DrawRect(FLinearColor(0.2f, 0.9f, 0.3f), BarX, BarY, BarW * GM->GetJamProgress(), BarH);

			// Time left.
			const float TimeY = BarY + BarH + 22.f;
			DrawRect(FLinearColor(0.15f, 0.15f, 0.18f), BarX, TimeY, BarW, 10.f);
			DrawRect(FLinearColor(0.95f, 0.3f, 0.2f), BarX, TimeY, BarW * GM->GetJamTimeFraction(), 10.f);
		}
	}

	// Endless level-up banner.
	if (!bTutorial && GM->IsShowingLevelBanner())
	{
		DrawCenteredText(FString::Printf(TEXT("LEVEL %d"), GM->GetLevel()), CX, MessageY, FLinearColor::Yellow, 2.4f);
	}

	// "New button unlocked" pause screen (Endless).
	if (GM->State == ESpinState::NewButtonIntro && GM->Prompts.IsValidIndex(1))
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.8f), 0.f, 0.f, W, H);

		const FSpinPrompt& First = GM->Prompts[0];
		const FSpinPrompt& Second = GM->Prompts[1];

		DrawCenteredText(TEXT("NEW BUTTON UNLOCKED!"), CX, CY - 150.f, FLinearColor::Yellow, 2.4f);
		DrawCenteredText(TEXT("From now on the zones will ask for either button, at random:"), CX, CY - 85.f, FLinearColor::White, 1.2f);
		DrawCenteredText(First.Hint, CX, CY - 35.f, First.Color, 1.6f);
		DrawCenteredText(Second.Hint, CX, CY + 10.f, Second.Color, 1.6f);
		DrawCenteredText(TEXT("The hand has slowed down and the zones are wide again so you can get used to it."), CX, CY + 75.f, DimColor, 1.0f);
		DrawCenteredText(FString::Printf(TEXT("Press %s to continue"), *Second.Hint), CX, CY + 130.f, Second.Color, 1.8f);
	}

	// "Obstacles" pause screen (Endless).
	if (GM->State == ESpinState::ObstacleIntro)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.8f), 0.f, 0.f, W, H);

		DrawCenteredText(TEXT("OBSTACLES!"), CX, CY - 150.f, FLinearColor::Yellow, 2.4f);
		DrawCenteredText(TEXT("Objects will now slide onto the dial. Don't let the hand touch them."), CX, CY - 85.f, FLinearColor::White, 1.2f);
		DrawCenteredText(TEXT("HOLD  LEFT CLICK / R2  to raise the hand over them"), CX, CY - 35.f, LiftColor, 1.6f);
		DrawCenteredText(TEXT("It only works while an obstacle is out, and drops by itself once you're past it."), CX, CY + 20.f, DimColor, 1.0f);
		DrawCenteredText(FString::Printf(TEXT("You have %d lives. Raising the hand with no obstacle out costs one; hopping an obstacle earns one back."), GM->MaxLives), CX, CY + 55.f, FLinearColor(1.f, 0.6f, 0.6f), 1.0f);
		DrawCenteredText(TEXT("Hold LEFT CLICK / R2 to continue"), CX, CY + 110.f, LiftColor, 1.8f);
	}

	// Tutorial level explanation screen.
	if (GM->State == ESpinState::TutorialIntro)
	{
		if (const FTutorialLevel* Level = GM->GetTutorialLevel())
		{
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.8f), 0.f, 0.f, W, H);

			const int32 NumLines = Level->IntroLines.Num();
			const float LineGap = 38.f;
			const float TopOfBlock = CY - 120.f - (NumLines * LineGap) * 0.5f;

			DrawCenteredText(Level->Title, CX, TopOfBlock, FLinearColor::Yellow, 2.2f);
			for (int32 i = 0; i < NumLines; ++i)
			{
				DrawCenteredText(Level->IntroLines[i], CX, TopOfBlock + 70.f + i * LineGap, FLinearColor::White, 1.2f);
			}

			// What confirms the screen.
			FString ConfirmText;
			FLinearColor ConfirmColor = LiftColor;
			switch (Level->Confirm)
			{
			case ETutorialConfirm::FirstButton:
				ConfirmText = FString::Printf(TEXT("Press %s to start"), *GM->Prompts[0].Hint);
				ConfirmColor = GM->Prompts[0].Color;
				break;
			case ETutorialConfirm::SecondButton:
				ConfirmText = FString::Printf(TEXT("Press %s to start"), *GM->Prompts[1].Hint);
				ConfirmColor = GM->Prompts[1].Color;
				break;
			case ETutorialConfirm::Lift:
				ConfirmText = TEXT("Hold LEFT CLICK / R2 to start");
				break;
			}
			DrawCenteredText(ConfirmText, CX, TopOfBlock + 70.f + NumLines * LineGap + 45.f, ConfirmColor, 1.8f);
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
