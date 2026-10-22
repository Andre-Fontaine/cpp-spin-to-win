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

void ASpinGameMode::HandleActionPressed()
{
	switch (State)
	{
	case ESpinState::WaitingToStart:
		StartGame();
		break;

	case ESpinState::Playing:
		if (IsHandInZone())
		{
			OnHit();
		}
		else
		{
			EndGame(); // pressed too early
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

void ASpinGameMode::StartGame()
{
	State = ESpinState::Playing;
	Score = 0;
	HandAngle = 0.f;
	Direction = 1.f;
	CurrentSpeed = StartSpeed;
	ZoneHalfWidth = StartZoneHalfWidth;
	bHandWasInZone = false;
	PlaceNextZone();
}

void ASpinGameMode::OnHit()
{
	++Score;
	HighScore = FMath::Max(HighScore, Score);

	// Flip direction, speed up, shrink the zone, and pick the next target.
	Direction = -Direction;
	CurrentSpeed = FMath::Min(CurrentSpeed * SpeedMultiplier, MaxSpeed);
	ZoneHalfWidth = FMath::Max(MinZoneHalfWidth, ZoneHalfWidth - ZoneShrinkPerHit);
	bHandWasInZone = false;
	PlaceNextZone();
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
