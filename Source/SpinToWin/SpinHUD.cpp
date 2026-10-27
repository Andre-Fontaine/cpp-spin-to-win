// Spin To Win - 2D overlay: score, level, messages, key prompts, and the clickable menus.
// The dial, hand, target zone and obstacles are 3D meshes (see SpinClock).

#include "SpinHUD.h"
#include "SpinGameMode.h"
#include "SpinClock.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
	FString GetMistakeMessage(ESpinMistake Mistake)
	{
		switch (Mistake)
		{
		case ESpinMistake::MissedZone:  return TEXT("You missed the zone!");
		case ESpinMistake::WrongButton: return TEXT("Wrong button, or too early!");
		case ESpinMistake::HitObstacle: return TEXT("You hit an obstacle!");
		case ESpinMistake::HandBroke:   return TEXT("The hand broke free!");
		default:                        return FString();
		}
	}
}

void ASpinHUD::DrawHUD()
{
	Super::DrawHUD();

	const ASpinGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASpinGameMode>() : nullptr;
	if (!GM || !Canvas)
	{
		return;
	}

	MenuRegions.Reset();
	bHaveMouse = PlayerOwner && PlayerOwner->GetMousePosition(MousePos.X, MousePos.Y);

	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	const float CX = W * 0.5f;
	const float CY = H * 0.5f;

	const FLinearColor LiftColor(0.4f, 0.9f, 1.f);
	const FLinearColor DimColor(0.8f, 0.8f, 0.8f);
	const FLinearColor NoticeColor(1.f, 0.8f, 0.3f);
	const FLinearColor ButtonBg(0.05f, 0.05f, 0.08f, 0.92f);

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

	// Miss lives (Endless only): a separate, non-regenerating pool spent on missed target zones.
	if (!bTitle && !bTutorial && GM->State != ESpinState::TutorialComplete)
	{
		const float Size = 18.f;
		const float Gap = 8.f;
		const float RowWidth = GM->MaxMissLives * Size + (GM->MaxMissLives - 1) * Gap;
		const float RowX = CX - RowWidth * 0.5f;
		const float RowY = TopY + 118.f;

		DrawCenteredText(TEXT("MISSES"), CX, RowY - 20.f, DimColor, 0.85f);
		for (int32 i = 0; i < GM->MaxMissLives; ++i)
		{
			const FLinearColor BoxColor = (i < GM->MissLives) ? FLinearColor(1.f, 0.65f, 0.1f) : FLinearColor(0.2f, 0.2f, 0.24f);
			DrawRect(BoxColor, RowX + i * (Size + Gap), RowY, Size, Size);
		}

		if (GetWorld()->GetTimeSeconds() - GM->MissLifeLostTime < 1.2f)
		{
			DrawCenteredText(TEXT("-1 MISS LIFE  (doesn't come back)"), CX, RowY + 28.f, FLinearColor(1.f, 0.65f, 0.1f), 1.0f);
		}
	}

	// Lives (only once obstacles are in play). In the tutorial they are shown but never go down.
	if (!bTitle && GM->State != ESpinState::TutorialComplete && GM->ObstaclesEnabled())
	{
		const float Size = 22.f;
		const float Gap = 10.f;
		const float RowWidth = GM->MaxLives * Size + (GM->MaxLives - 1) * Gap;
		const float RowX = CX - RowWidth * 0.5f;
		const float RowY = TopY + (bTutorial ? 80.f : 165.f);

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
		// Main menu: PLAY leads to mode select, SETTINGS leads to the settings screen.
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.72f), 0.f, 0.f, W, H);

		DrawCenteredText(TEXT("SPIN TO WIN"), CX, MessageY, FLinearColor::Yellow, 2.8f);
		DrawCenteredText(TEXT("Click to play"), CX, MessageY + 58.f, DimColor, 1.2f);

		const float BtnW = FMath::Min(W * 0.26f, 260.f);
		const float BtnH = 80.f;
		const float Gap = 30.f;
		const float BtnY = CY - BtnH * 0.5f + 20.f;
		const float PlayX = CX - Gap * 0.5f - BtnW;
		const float SettingsX = CX + Gap * 0.5f;

		DrawButton(PlayX, BtnY, BtnW, BtnH, EMenuAction::Play, ButtonBg);
		DrawCenteredText(TEXT("PLAY"), PlayX + BtnW * 0.5f, BtnY + 26.f, FLinearColor::White, 1.8f);

		DrawButton(SettingsX, BtnY, BtnW, BtnH, EMenuAction::OpenSettings, ButtonBg);
		DrawCenteredText(TEXT("SETTINGS"), SettingsX + BtnW * 0.5f, BtnY + 26.f, FLinearColor::White, 1.5f);

		if (GM->Prompts.IsValidIndex(1))
		{
			DrawCenteredText(FString::Printf(TEXT("Or press %s / %s"), *GM->Prompts[0].Hint, *GM->Prompts[1].Hint), CX, BtnY + BtnH + 30.f, DimColor, 0.9f);
		}
	}
	else if (GM->State == ESpinState::ModeSelect)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.72f), 0.f, 0.f, W, H);
		DrawCenteredText(TEXT("CHOOSE A MODE"), CX, MessageY, FLinearColor::Yellow, 2.4f);

		if (GM->Prompts.IsValidIndex(1))
		{
			const FSpinPrompt& First = GM->Prompts[0];
			const FSpinPrompt& Second = GM->Prompts[1];

			const float CardW = FMath::Min(W * 0.32f, 320.f);
			const float CardH = 130.f;
			const float Gap = 50.f;
			const float CardY = CY - CardH * 0.5f + 10.f;
			const float LeftX = CX - Gap * 0.5f - CardW;
			const float RightX = CX + Gap * 0.5f;

			DrawButton(LeftX, CardY, CardW, CardH, EMenuAction::StartTutorial, ButtonBg);
			DrawCenteredText(TEXT("TUTORIAL"), LeftX + CardW * 0.5f, CardY + 20.f, FLinearColor::White, 1.7f);
			DrawCenteredText(TEXT("Learn the ropes - 7 short levels"), LeftX + CardW * 0.5f, CardY + 58.f, DimColor, 1.0f);
			DrawCenteredText(FString::Printf(TEXT("Click, or press %s"), *First.Hint), LeftX + CardW * 0.5f, CardY + 92.f, First.Color, 1.1f);

			DrawButton(RightX, CardY, CardW, CardH, EMenuAction::StartEndless, ButtonBg);
			DrawCenteredText(TEXT("ENDLESS"), RightX + CardW * 0.5f, CardY + 20.f, FLinearColor::White, 1.7f);
			DrawCenteredText(TEXT("Skip straight to the real run"), RightX + CardW * 0.5f, CardY + 58.f, DimColor, 1.0f);
			DrawCenteredText(FString::Printf(TEXT("Click, or press %s"), *Second.Hint), RightX + CardW * 0.5f, CardY + 92.f, Second.Color, 1.1f);

			const float BackW = 140.f, BackH = 44.f;
			const float BackX = CX - BackW * 0.5f;
			const float BackY = CardY + CardH + 40.f;
			DrawButton(BackX, BackY, BackW, BackH, EMenuAction::Back, FLinearColor(0.05f, 0.05f, 0.08f, 0.8f));
			DrawCenteredText(TEXT("BACK"), CX, BackY + 12.f, DimColor, 1.1f);
		}
	}
	else if (GM->State == ESpinState::SettingsMenu)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.72f), 0.f, 0.f, W, H);
		DrawCenteredText(TEXT("SETTINGS"), CX, MessageY, FLinearColor::Yellow, 2.4f);

		const float RowW = FMath::Min(W * 0.5f, 460.f);
		const float RowX = CX - RowW * 0.5f;
		const float ArrowW = 44.f, ArrowH = 44.f;
		const float RowH = 95.f;
		const float FirstRowY = CY - RowH * 1.2f;

		auto DrawSettingRow = [&](float RowY, const FString& Label, const FString& Value, EMenuAction PrevAction, EMenuAction NextAction)
		{
			DrawCenteredText(Label, CX, RowY, DimColor, 1.0f);

			DrawButton(RowX, RowY + 22.f, ArrowW, ArrowH, PrevAction, ButtonBg);
			DrawCenteredText(TEXT("<"), RowX + ArrowW * 0.5f, RowY + 32.f, FLinearColor::White, 1.4f);

			DrawCenteredText(Value, CX, RowY + 32.f, FLinearColor::White, 1.4f);

			DrawButton(RowX + RowW - ArrowW, RowY + 22.f, ArrowW, ArrowH, NextAction, ButtonBg);
			DrawCenteredText(TEXT(">"), RowX + RowW - ArrowW * 0.5f, RowY + 32.f, FLinearColor::White, 1.4f);
		};

		DrawSettingRow(FirstRowY, TEXT("GRAPHICS QUALITY"), GM->GetQualityLabel(), EMenuAction::QualityPrev, EMenuAction::QualityNext);
		DrawSettingRow(FirstRowY + RowH, TEXT("RESOLUTION"), GM->GetResolutionLabel(), EMenuAction::ResolutionPrev, EMenuAction::ResolutionNext);
		DrawSettingRow(FirstRowY + RowH * 2.f, TEXT("VOLUME"), FString::Printf(TEXT("%d%%"), GM->VolumePercent), EMenuAction::VolumeDown, EMenuAction::VolumeUp);

		const float BackW = 140.f, BackH = 44.f;
		const float BackX = CX - BackW * 0.5f;
		const float BackY = FirstRowY + RowH * 2.f + 70.f;
		DrawButton(BackX, BackY, BackW, BackH, EMenuAction::Back, FLinearColor(0.05f, 0.05f, 0.08f, 0.8f));
		DrawCenteredText(TEXT("BACK"), CX, BackY + 12.f, DimColor, 1.1f);
	}
	else if (GM->State == ESpinState::MissPause)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.78f), 0.f, 0.f, W, H);
		DrawCenteredText(TEXT("LIFE LOST!"), CX, CY - 125.f, FLinearColor::Red, 2.6f);
		DrawCenteredText(GetMistakeMessage(GM->LastMistake), CX, CY - 80.f, FLinearColor::White, 1.2f);

		const bool bObstacleLife = (GM->LastMistake == ESpinMistake::HitObstacle);
		const FString LivesLine = bObstacleLife
			? FString::Printf(TEXT("%d / %d lives left"), GM->Lives, GM->MaxLives)
			: FString::Printf(TEXT("%d / %d miss lives left"), GM->MissLives, GM->MaxMissLives);
		DrawCenteredText(LivesLine, CX, CY - 40.f, FLinearColor::White, 1.3f);

		DrawCenteredText(FString::Printf(TEXT("Continuing gives you %.0f seconds of invincibility to get back in rhythm."), GM->InvincibilityDuration), CX, CY - 5.f, DimColor, 1.0f);

		const float BtnW = 260.f, BtnH = 64.f;
		const float BtnX = CX - BtnW * 0.5f;
		const float BtnY = CY + 35.f;
		DrawButton(BtnX, BtnY, BtnW, BtnH, EMenuAction::ContinueAfterMiss, FLinearColor(0.08f, 0.05f, 0.05f, 0.92f));
		DrawCenteredText(TEXT("CONTINUE"), CX, BtnY + 18.f, FLinearColor::White, 1.6f);
		DrawCenteredText(FString::Printf(TEXT("Click, or press %s"), GM->Prompts.IsValidIndex(0) ? *GM->Prompts[0].Hint : TEXT("W")), CX, BtnY + BtnH + 20.f, DimColor, 1.0f);
	}
	else if (GM->State == ESpinState::GameOver)
	{
		DrawCenteredText(TEXT("GAME OVER"), CX, MessageY, FLinearColor::Red, 2.4f);
		DrawCenteredText(FString::Printf(TEXT("You reached level %d with %d hits"), GM->GetLevel(), GM->Score), CX, MessageY + 55.f, FLinearColor::White, 1.2f);
		if (GM->bOutOfLives)
		{
			DrawCenteredText(TEXT("Out of lives!"), CX, MessageY + 90.f, FLinearColor::Red, 1.2f);
		}
		else if (GM->bOutOfMissLives)
		{
			DrawCenteredText(TEXT("Missed too many zones!"), CX, MessageY + 90.f, FLinearColor::Red, 1.2f);
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
		// What to do right now. The invincibility countdown itself is drawn big and centered,
		// below, so it needs no line here.
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
		if (GM->ObstaclesEnabled() && !bJammed && !GM->bInvincible)
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

		// Notices: "Try again!" in the tutorial, "Missed it!" in Endless.
		if (GM->IsShowingNotice())
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

	// Invincibility countdown: a big "in your face" number, dead center, that punches in huge
	// and quickly shrinks and fades away - once per second, for both the post-mistake recovery
	// window and the few seconds at the start of a fresh run. Skipped for an obstacle hit - the
	// obstacle's own feedback is enough, and the giant number just gets in the way there.
	if (GM->State == ESpinState::Playing && GM->bInvincible && GM->LastMistake != ESpinMistake::HitObstacle)
	{
		DrawInvincibilityCountdown(GM->InvincibleTimeRemaining, CX, CY);
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

bool ASpinHUD::IsHovered(float X, float Y, float W, float H) const
{
	return bHaveMouse && MousePos.X >= X && MousePos.X <= X + W && MousePos.Y >= Y && MousePos.Y <= Y + H;
}

void ASpinHUD::DrawButton(float X, float Y, float W, float H, EMenuAction Action, const FLinearColor& BgColor)
{
	const bool bHover = IsHovered(X, Y, W, H);
	const FLinearColor Final = bHover ? (BgColor + FLinearColor(0.1f, 0.1f, 0.12f, 0.f)) : BgColor;
	DrawRect(Final, X, Y, W, H);

	FSpinMenuRegion Region;
	Region.Rect = FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H));
	Region.Action = Action;
	MenuRegions.Add(Region);
}

void ASpinHUD::DrawInvincibilityCountdown(float TimeRemaining, float CenterX, float CenterY)
{
	// Which whole second are we in ("5", "4", ... "1"), and how far into that second's display.
	const int32 Number = FMath::Max(1, FMath::CeilToInt(TimeRemaining - KINDA_SMALL_NUMBER));
	const float TimeIntoNumber = Number - TimeRemaining;

	// The punch animation itself is much shorter than the full second, so the number appears,
	// slams in huge, and is gone well before the next one - rather than lingering.
	const float PunchDuration = 0.6f;
	if (TimeIntoNumber < 0.f || TimeIntoNumber >= PunchDuration)
	{
		return;
	}

	const float T = TimeIntoNumber / PunchDuration;
	const float Scale = FMath::Lerp(16.f, 6.f, T * T);
	const float Alpha = FMath::Square(1.f - T);

	const FString Text = FString::FromInt(Number);
	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	float TextW = 0.f, TextH = 0.f;
	GetTextSize(Text, TextW, TextH, Font, Scale);
	DrawText(Text, FLinearColor(1.f, 0.85f, 0.15f, Alpha), CenterX - TextW * 0.5f, CenterY - TextH * 0.5f, Font, Scale);
}
