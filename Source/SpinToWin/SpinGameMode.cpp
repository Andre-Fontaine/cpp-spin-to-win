// Spin To Win - core game rules.

#include "SpinGameMode.h"
#include "SpinPlayerController.h"
#include "SpinHUD.h"
#include "SpinClock.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

ASpinGameMode::ASpinGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	// The player is the camera and the controls: no pawn needed.
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ASpinPlayerController::StaticClass();
	HUDClass = ASpinHUD::StaticClass();

	// Button 0: always available.
	FSpinPrompt First;
	First.Label = TEXT("SPACE");
	First.Hint = TEXT("SPACE / A / X");
	First.Color = FLinearColor(0.1f, 0.8f, 0.2f); // green
	Prompts.Add(First);

	// Button 1: unlocks at SecondButtonLevel.
	FSpinPrompt Second;
	Second.Label = TEXT("F");
	Second.Hint = TEXT("F / B / O");
	Second.Color = FLinearColor(0.2f, 0.5f, 1.f); // blue
	Prompts.Add(Second);
}

void ASpinGameMode::StartPlay()
{
	Super::StartPlay();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Spawn the physical dial a little above the origin (keeps it clear of any floor at Z=0)
	// and make its camera the player's view.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Clock = World->SpawnActor<ASpinClock>(ASpinClock::StaticClass(), FTransform(FVector(0.f, 0.f, 100.f)), Params);

	if (Clock)
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->SetViewTarget(Clock);
		}
	}
}

ASpinClock* ASpinGameMode::GetClock() const
{
	return Clock;
}

void ASpinGameMode::Shake(float Intensity)
{
	if (Clock)
	{
		Clock->PlayShake(Intensity);
	}
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

	// Ran into an obstacle without lifting the hand over it.
	if (Clock && Clock->IsHandHittingObstacle(HandAngle))
	{
		EndGame();
		return;
	}

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

	case ESpinState::ObstacleIntro:
		// Waits for the lift button instead, see SetLiftHeld.
		break;

	case ESpinState::GameOver:
		if (GetWorld() && GetWorld()->GetTimeSeconds() - GameOverTime >= RestartDelay)
		{
			StartGame();
		}
		break;
	}
}

void ASpinGameMode::SetLiftHeld(bool bHeld)
{
	// Holding the lift button is how the player confirms the obstacle tutorial.
	// That press is not a lift attempt, so it never costs a life.
	if (State == ESpinState::ObstacleIntro)
	{
		if (bHeld)
		{
			ResumeAfterIntro();
		}
		if (Clock)
		{
			Clock->SetLiftHeld(false, false);
		}
		return;
	}

	// A press only counts as a lift if an obstacle is out. Reaching for the lift with nothing to
	// hop over costs a life (once obstacles exist at all).
	bool bValidPress = false;
	if (bHeld && State == ESpinState::Playing && GetLevel() >= ObstacleStartLevel)
	{
		bValidPress = Clock && Clock->IsLiftAvailable();
		if (!bValidPress)
		{
			LoseLife();
		}
	}

	if (Clock)
	{
		Clock->SetLiftHeld(bHeld, bValidPress);
	}
}

void ASpinGameMode::OnObstacleCleared()
{
	// The hand got over an obstacle: earn a life back.
	if (State == ESpinState::Playing && Lives < MaxLives)
	{
		++Lives;
		if (GetWorld())
		{
			LifeGainedTime = GetWorld()->GetTimeSeconds();
		}
	}
}

void ASpinGameMode::LoseLife()
{
	Lives = FMath::Max(0, Lives - 1);

	if (GetWorld())
	{
		LifeLostTime = GetWorld()->GetTimeSeconds();
	}
	Shake(LifeLostShake);

	if (Lives <= 0)
	{
		bOutOfLives = true;
		EndGame();
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
	PromptStreak = 1;
	LevelUpTime = -1000.f;
	Lives = MaxLives;
	LifeLostTime = -1000.f;
	LifeGainedTime = -1000.f;
	bOutOfLives = false;
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
	// If both unlock on the same level, the new-button screen wins.
	const bool bObstacleIntro = !bUnlocksSecondButton && OldLevel < ObstacleStartLevel && NewLevel >= ObstacleStartLevel;

	// The normal "LEVEL N" banner is skipped when a pause screen takes over instead.
	if (NewLevel > OldLevel && GetWorld() && !bUnlocksSecondButton && !bObstacleIntro)
	{
		LevelUpTime = GetWorld()->GetTimeSeconds();
		Shake(LevelUpShake);
	}

	Shake(HitShake);

	// Flip direction, speed up, shrink the zone, and pick the next target.
	Direction = -Direction;
	CurrentSpeed = FMath::Min(CurrentSpeed * SpeedMultiplier, MaxSpeed);
	ZoneHalfWidth = FMath::Max(MinZoneHalfWidth, ZoneHalfWidth - ZoneShrinkPerHit);
	bHandWasInZone = false;

	// Pick which button the next zone needs (random once two buttons are in play).
	PickNextPrompt();

	// No new obstacle while a tutorial screen is about to pause the game.
	PlaceNextZone(!bUnlocksSecondButton && !bObstacleIntro);

	if (bUnlocksSecondButton)
	{
		// Second button just unlocked: pause, reset the hand to its slowest speed and the zone to
		// its widest, make the very first zone need the new button, and wait for confirmation.
		CurrentSpeed = StartSpeed;
		ZoneHalfWidth = StartZoneHalfWidth;
		ActivePromptIndex = 1;
		PromptStreak = 1;
		State = ESpinState::NewButtonIntro;
		Shake(LevelUpShake);
	}
	else if (bObstacleIntro)
	{
		// Obstacles just unlocked: pause on the tutorial until the player holds the lift button.
		State = ESpinState::ObstacleIntro;
		Shake(LevelUpShake);
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
	Shake(GameOverShake);
}

void ASpinGameMode::PickNextPrompt()
{
	const int32 NumButtons = GetNumActiveButtons();

	int32 Next = 0;
	if (NumButtons > 1)
	{
		Next = FMath::RandRange(0, NumButtons - 1);

		// Keep it random, but never let one button repeat more than MaxSameButtonInRow times.
		if (Next == ActivePromptIndex && PromptStreak >= MaxSameButtonInRow)
		{
			Next = (Next + 1) % NumButtons;
		}
	}

	PromptStreak = (Next == ActivePromptIndex) ? PromptStreak + 1 : 1;
	ActivePromptIndex = Next;
}

void ASpinGameMode::PlaceNextZone(bool bAllowObstacle)
{
	// Old obstacles slide away whenever a new target is placed.
	if (Clock)
	{
		Clock->RetireObstacles();
	}

	// Place the zone ahead of the hand in its current direction of travel.
	float Offset = FMath::FRandRange(MinZoneOffset, MaxZoneOffset);

	if (bAllowObstacle && Clock && GetLevel() >= ObstacleStartLevel && FMath::FRand() < ObstacleChance)
	{
		// The zone must never start on top of the hand, so it can't wrap too close to where the hand is.
		const float MaxOffset = 360.f - ZoneHalfWidth - 12.f;

		// The obstacle sits far enough ahead that there is time to see it and lift the hand...
		float ObstacleDist = FMath::Max(CurrentSpeed * ObstacleWarningTime, MinObstacleDistance) + FMath::FRandRange(0.f, 15.f);
		ObstacleDist = FMath::Min(ObstacleDist, MaxOffset - ObstacleClearGap - ZoneHalfWidth);

		// ...and the zone sits beyond it, with room to lower the hand again.
		Offset = FMath::Clamp(FMath::Max(Offset, ObstacleDist + ObstacleClearGap + ZoneHalfWidth), MinZoneOffset, MaxOffset);

		Clock->SpawnObstacle(
			HandAngle + Direction * ObstacleDist,
			FMath::FRandRange(ObstacleMinRadiusFraction, ObstacleMaxRadiusFraction),
			ObstacleDist);
	}

	ZoneCenter = FMath::Fmod(HandAngle + Direction * Offset + 360.f, 360.f);
}
