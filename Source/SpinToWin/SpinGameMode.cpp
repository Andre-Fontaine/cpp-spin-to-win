// Spin To Win - core game rules.

#include "SpinGameMode.h"
#include "SpinPlayerController.h"
#include "SpinHUD.h"
#include "Engine/World.h"

ASpinGameMode::ASpinGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	// 2D game on a HUD: no pawn needed.
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ASpinPlayerController::StaticClass();
	HUDClass = ASpinHUD::StaticClass();

	// Button 0: always available.
	FSpinPrompt First;
	First.Label = TEXT("SPACE");
	First.Hint = TEXT("SPACE / Left Click");
	First.Color = FLinearColor(0.1f, 0.8f, 0.2f); // green
	Prompts.Add(First);

	// Button 1: unlocks at SecondButtonLevel.
	FSpinPrompt Second;
	Second.Label = TEXT("F");
	Second.Hint = TEXT("F / Right Click");
	Second.Color = FLinearColor(0.2f, 0.5f, 1.f); // blue
	Prompts.Add(Second);
}

void ASpinGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Clamp so a frame hitch can't make the hand skip over the zone.
	const float Dt = FMath::Min(DeltaSeconds, 0.05f);

	if (State == ESpinState::WaitingToStart)
	{
		// Slow idle spin on the title screen.
		HandAngle = FMath::Fmod(HandAngle + StartSpeed * 0.5f * Dt, 360.f);
		return;
	}

	if (State != ESpinState::Playing)
	{
		return;
	}

	HandAngle = FMath::Fmod(HandAngle + Direction * CurrentSpeed * Dt + 360.f, 360.f);

	// If the hand entered the zone and has now left it without a press, the player missed.
	const bool bInZone = IsHandInZone();
	if (bHandWasInZone && !bInZone)
	{
		EndGame();
		return;
	}
	bHandWasInZone = bInZone;
}

void ASpinGameMode::HandleButtonPressed(int32 ButtonIndex)
{
	switch (State)
	{
	case ESpinState::WaitingToStart:
		StartGame();
		break;

	case ESpinState::Playing:
		// Buttons that haven't unlocked yet are simply ignored.
		if (ButtonIndex >= GetNumActiveButtons())
		{
			break;
		}

		if (IsHandInZone() && ButtonIndex == ActivePromptIndex)
		{
			OnHit();
		}
		else
		{
			EndGame(); // too early, or the wrong button
		}
		break;

	case ESpinState::NewButtonIntro:
		// The player must press the newly unlocked button (index 1) to confirm and continue.
		if (ButtonIndex == 1)
		{
			ResumeAfterIntro();
		}
		break;

	case ESpinState::GameOver:
		if (GetWorld() && GetWorld()->GetTimeSeconds() - GameOverTime >= RestartDelay)
		{
			StartGame();
		}
		break;
	}
}

bool ASpinGameMode::IsHandInZone() const
{
	return FMath::Abs(FMath::FindDeltaAngleDegrees(HandAngle, ZoneCenter)) <= ZoneHalfWidth;
}

int32 ASpinGameMode::GetLevel() const
{
	return 1 + Score / FMath::Max(1, HitsPerLevel);
}

int32 ASpinGameMode::GetLevelTarget() const
{
	return GetLevel() * FMath::Max(1, HitsPerLevel);
}

int32 ASpinGameMode::GetNumActiveButtons() const
{
	const int32 Wanted = (GetLevel() >= SecondButtonLevel) ? 2 : 1;
	return FMath::Clamp(Wanted, 1, FMath::Max(1, Prompts.Num()));
}

bool ASpinGameMode::IsShowingLevelBanner() const
{
	return State == ESpinState::Playing
		&& GetWorld()
		&& (GetWorld()->GetTimeSeconds() - LevelUpTime) < LevelBannerDuration;
}

void ASpinGameMode::StartGame()
{
	State = ESpinState::Playing;
	Score = 0;
	HandAngle = 0.f;
	Direction = 1.f;
	CurrentSpeed = StartSpeed;
	ZoneHalfWidth = StartZoneHalfWidth;
	ActivePromptIndex = 0;
	LevelUpTime = -1000.f;
	bHandWasInZone = false;
	PlaceNextZone();
}

void ASpinGameMode::OnHit()
{
	const int32 OldLevel = GetLevel();

	++Score;
	HighScore = FMath::Max(HighScore, Score);

	const int32 NewLevel = GetLevel();
	const bool bUnlocksSecondButton = OldLevel < SecondButtonLevel && NewLevel >= SecondButtonLevel && Prompts.Num() > 1;

	// The normal "LEVEL N" banner is skipped when the pause screen takes over instead.
	if (NewLevel > OldLevel && GetWorld() && !bUnlocksSecondButton)
	{
		LevelUpTime = GetWorld()->GetTimeSeconds();
	}

	// Flip direction, speed up, shrink the zone, and pick the next target.
	Direction = -Direction;
	CurrentSpeed = FMath::Min(CurrentSpeed * SpeedMultiplier, MaxSpeed);
	ZoneHalfWidth = FMath::Max(MinZoneHalfWidth, ZoneHalfWidth - ZoneShrinkPerHit);
	bHandWasInZone = false;

	// Alternate between the buttons that are unlocked at the new level.
	ActivePromptIndex = (ActivePromptIndex + 1) % GetNumActiveButtons();

	PlaceNextZone();

	// Second button just unlocked: pause, reset the hand to its slowest speed, and wait for confirmation.
	if (bUnlocksSecondButton)
	{
		CurrentSpeed = StartSpeed;
		State = ESpinState::NewButtonIntro;
	}
}

void ASpinGameMode::ResumeAfterIntro()
{
	State = ESpinState::Playing;
	bHandWasInZone = false;
}

void ASpinGameMode::EndGame()
{
	State = ESpinState::GameOver;
	HighScore = FMath::Max(HighScore, Score);
	GameOverTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

void ASpinGameMode::PlaceNextZone()
{
	// Place the zone ahead of the hand in its current direction of travel.
	const float Offset = FMath::FRandRange(MinZoneOffset, MaxZoneOffset);
	ZoneCenter = FMath::Fmod(HandAngle + Direction * Offset + 360.f, 360.f);
}
