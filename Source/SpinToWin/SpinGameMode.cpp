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
	First.Label = TEXT("W");
	First.Hint = TEXT("W / A / X");
	First.Color = FLinearColor(0.1f, 0.8f, 0.2f); // green
	Prompts.Add(First);

	// Button 1: the second button.
	FSpinPrompt Second;
	Second.Label = TEXT("D");
	Second.Hint = TEXT("D / B / O");
	Second.Color = FLinearColor(0.2f, 0.5f, 1.f); // blue
	Prompts.Add(Second);

	BuildTutorialLevels();
}

void ASpinGameMode::BuildTutorialLevels()
{
	// All the tutorial numbers live here. Speeds are in degrees per second, zone sizes are half-widths in degrees.
	const FString First = Prompts[0].Hint;
	const FString Second = Prompts[1].Hint;

	TutorialLevels.Reset();

	// Level 1: the basics. Slow, big zones, one button.
	{
		FTutorialLevel L;
		L.Title = TEXT("LEVEL 1 - THE BASICS");
		L.IntroLines.Add(TEXT("A hand sweeps around the dial."));
		L.IntroLines.Add(FString::Printf(TEXT("Press %s when it is inside the green zone."), *First));
		L.IntroLines.Add(TEXT("Miss? No problem. The hand keeps going, just try again."));
		L.Instruction = FString::Printf(TEXT("Press %s when the hand is inside the green zone"), *First);
		L.Confirm = ETutorialConfirm::FirstButton;
		L.Hits = 10;
		L.Speed = 70.f;
		L.ZoneHalfWidth = 45.f;
		L.NumButtons = 1;
		TutorialLevels.Add(L);
	}

	// Level 2: a little faster, slightly smaller zones.
	{
		FTutorialLevel L;
		L.Title = TEXT("LEVEL 2 - A LITTLE FASTER");
		L.IntroLines.Add(TEXT("Same thing, a little quicker, with smaller zones."));
		L.Instruction = FString::Printf(TEXT("Press %s when the hand is inside the green zone"), *First);
		L.Confirm = ETutorialConfirm::FirstButton;
		L.Hits = 15;
		L.Speed = 85.f;
		L.ZoneHalfWidth = 38.f;
		L.NumButtons = 1;
		TutorialLevels.Add(L);
	}

	// Level 3: the second button. Same speed as level 2, zones as big as level 1.
	{
		FTutorialLevel L;
		L.Title = TEXT("LEVEL 3 - A SECOND BUTTON");
		L.IntroLines.Add(TEXT("Zones now come in two colors."));
		L.IntroLines.Add(FString::Printf(TEXT("GREEN zone: press %s"), *First));
		L.IntroLines.Add(FString::Printf(TEXT("BLUE zone: press %s"), *Second));
		L.IntroLines.Add(TEXT("The zones are big again while you get used to it."));
		L.Instruction = FString::Printf(TEXT("Green zone: %s     Blue zone: %s"), *First, *Second);
		L.Confirm = ETutorialConfirm::SecondButton;
		L.Hits = 15;
		L.Speed = 85.f;
		L.ZoneHalfWidth = 45.f;
		L.NumButtons = 2;
		L.FirstPrompt = 1;
		TutorialLevels.Add(L);
	}

	// Level 4: both buttons, a little faster, smaller zones.
	{
		FTutorialLevel L;
		L.Title = TEXT("LEVEL 4 - SPEEDING UP");
		L.IntroLines.Add(TEXT("Both buttons again, a little faster, with smaller zones."));
		L.Instruction = FString::Printf(TEXT("Green zone: %s     Blue zone: %s"), *First, *Second);
		L.Confirm = ETutorialConfirm::FirstButton;
		L.Hits = 15;
		L.Speed = 100.f;
		L.ZoneHalfWidth = 38.f;
		L.NumButtons = 2;
		TutorialLevels.Add(L);
	}

	// Level 5: obstacles and the lift. One button, so the lift can be practiced.
	{
		FTutorialLevel L;
		L.Title = TEXT("LEVEL 5 - OBSTACLES");
		L.IntroLines.Add(TEXT("Posts will slide onto the dial. Don't let the hand touch them."));
		L.IntroLines.Add(TEXT("HOLD LEFT CLICK / R2 to raise the hand over a post. It drops by itself once you are past."));
		L.IntroLines.Add(TEXT("Raise the hand with no post out and a real run takes a life. Not here, but remember it."));
		L.IntroLines.Add(FString::Printf(TEXT("You get %d lives in a real run, and hopping a post earns one back."), MaxLives));
		L.Instruction = TEXT("Hold LEFT CLICK / R2 to raise the hand over posts");
		L.Confirm = ETutorialConfirm::Lift;
		L.Hits = 15;
		L.Speed = 100.f;
		L.ZoneHalfWidth = 38.f;
		L.NumButtons = 1;
		L.bObstacles = true;
		L.ObstacleChance = 0.7f;
		TutorialLevels.Add(L);
	}

	// Level 6: the second button comes back, obstacles stay.
	{
		FTutorialLevel L;
		L.Title = TEXT("LEVEL 6 - EVERYTHING TOGETHER");
		L.IntroLines.Add(TEXT("The second button is back."));
		L.IntroLines.Add(TEXT("Use both buttons and hop the posts at the same time."));
		L.Instruction = FString::Printf(TEXT("Green: %s   Blue: %s   Raise: LEFT CLICK / R2"), *First, *Second);
		L.Confirm = ETutorialConfirm::SecondButton;
		L.Hits = 15;
		L.Speed = 100.f;
		L.ZoneHalfWidth = 38.f;
		L.NumButtons = 2;
		L.FirstPrompt = 1;
		L.bObstacles = true;
		L.ObstacleChance = 0.7f;
		TutorialLevels.Add(L);
	}

	// Level 7: the jam.
	{
		FTutorialLevel L;
		L.Title = TEXT("LEVEL 7 - JAMS");
		L.IntroLines.Add(TEXT("Sparks at the center mean the hand is about to jam."));
		L.IntroLines.Add(TEXT("A jammed hand keeps spinning and ignores everything in its way."));
		L.IntroLines.Add(FString::Printf(TEXT("Press %s again and again to fix it before the time bar runs out."), *First));
		L.IntroLines.Add(TEXT("In a real run you get 3 seconds, or the hand breaks and it is game over."));
		L.Instruction = FString::Printf(TEXT("If the hand jams, press %s again and again"), *First);
		L.Confirm = ETutorialConfirm::FirstButton;
		L.Hits = 15;
		L.Speed = 100.f;
		L.ZoneHalfWidth = 38.f;
		L.NumButtons = 2;
		L.bObstacles = true;
		L.ObstacleChance = 0.7f;
		L.JamAtHits.Add(3);
		L.JamAtHits.Add(9);
		TutorialLevels.Add(L);
	}
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

void ASpinGameMode::ShowNotice(const FString& Text)
{
	NoticeText = Text;
	NoticeTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

bool ASpinGameMode::IsShowingNotice() const
{
	return GetWorld() && (GetWorld()->GetTimeSeconds() - NoticeTime) < NoticeDuration;
}

// ---------------------------------------------------------------------------------------------
// Per-frame update
// ---------------------------------------------------------------------------------------------

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

	UpdateJam(Dt);
	if (State != ESpinState::Playing)
	{
		return; // the jam ended the run
	}

	// A jammed hand keeps spinning but ignores zones and obstacles completely.
	if (JamPhase == EJamPhase::Jammed)
	{
		return;
	}

	// Obstacles.
	if (Clock && Clock->IsHandHittingObstacle(HandAngle))
	{
		if (IsTutorial())
		{
			if (!bTutorialObstacleHit)
			{
				bTutorialObstacleHit = true;
				bZoneTainted = true;
				Shake(LifeLostShake);
				ShowNotice(TEXT("You touched a post! Hold LEFT CLICK / R2 to raise the hand over it. Try the lap again."));
			}
		}
		else
		{
			EndGame(); // ran into an obstacle without lifting the hand over it
			return;
		}
	}
	else
	{
		bTutorialObstacleHit = false;
	}

	// If the hand entered the zone and has now left it without a press, the player missed.
	const bool bInZone = IsHandInZone();
	if (bHandWasInZone && !bInZone)
	{
		if (IsTutorial())
		{
			OnTutorialMiss();
		}
		else
		{
			EndGame();
			return;
		}
	}
	bHandWasInZone = bInZone;
}

// ---------------------------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------------------------

void ASpinGameMode::HandleButtonPressed(int32 ButtonIndex)
{
	switch (State)
	{
	case ESpinState::WaitingToStart:
		// First button: tutorial. Second button: straight into Endless.
		if (ButtonIndex == 0)
		{
			StartTutorial();
		}
		else
		{
			StartGame();
		}
		break;

	case ESpinState::Playing:
		// A jammed hand only listens to the first button, which mashes it free.
		if (JamPhase == EJamPhase::Jammed)
		{
			if (ButtonIndex == 0)
			{
				RegisterJamPress();
			}
			break;
		}

		// Buttons that aren't in play are simply ignored.
		if (ButtonIndex >= GetNumActiveButtons())
		{
			break;
		}

		if (IsTutorial())
		{
			HandleTutorialPress(ButtonIndex);
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

	case ESpinState::TutorialIntro:
	{
		const FTutorialLevel* Level = GetTutorialLevel();
		const ETutorialConfirm Confirm = Level ? Level->Confirm : ETutorialConfirm::FirstButton;
		if ((Confirm == ETutorialConfirm::FirstButton && ButtonIndex == 0)
			|| (Confirm == ETutorialConfirm::SecondButton && ButtonIndex == 1))
		{
			BeginTutorialPlay();
		}
		break;
	}

	case ESpinState::TutorialComplete:
		// First button: on to Endless. Second button: back to the title screen.
		if (ButtonIndex == 0)
		{
			StartGame();
		}
		else
		{
			ReturnToTitle();
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

void ASpinGameMode::SetLiftHeld(bool bHeld)
{
	// Holding the lift button confirms the Endless obstacle screen. That press is not a lift attempt.
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

	// The same goes for a tutorial screen that asks for the lift.
	if (State == ESpinState::TutorialIntro)
	{
		const FTutorialLevel* Level = GetTutorialLevel();
		if (bHeld && Level && Level->Confirm == ETutorialConfirm::Lift)
		{
			BeginTutorialPlay();
		}
		if (Clock)
		{
			Clock->SetLiftHeld(false, false);
		}
		return;
	}

	// A press only counts as a lift if an obstacle is out. Reaching for the lift with nothing to
	// hop over costs a life in Endless, and shows a message in the tutorial.
	bool bValidPress = false;
	if (bHeld && State == ESpinState::Playing && JamPhase != EJamPhase::Jammed && ObstaclesEnabled())
	{
		bValidPress = Clock && Clock->IsLiftAvailable();
		if (!bValidPress)
		{
			if (IsTutorial())
			{
				ShowNotice(TEXT("Nothing to raise the hand over! In a real run that costs a life."));
			}
			else
			{
				LoseLife();
			}
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

// ---------------------------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------------------------

bool ASpinGameMode::IsTutorial() const
{
	return Mode == ESpinMode::Tutorial;
}

const FTutorialLevel* ASpinGameMode::GetTutorialLevel() const
{
	return TutorialLevels.IsValidIndex(TutorialIndex) ? &TutorialLevels[TutorialIndex] : nullptr;
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
	if (IsTutorial())
	{
		const FTutorialLevel* Level = GetTutorialLevel();
		return FMath::Clamp(Level ? Level->NumButtons : 1, 1, FMath::Max(1, Prompts.Num()));
	}

	const int32 Wanted = (GetLevel() >= SecondButtonLevel) ? 2 : 1;
	return FMath::Clamp(Wanted, 1, FMath::Max(1, Prompts.Num()));
}

bool ASpinGameMode::ObstaclesEnabled() const
{
	if (IsTutorial())
	{
		const FTutorialLevel* Level = GetTutorialLevel();
		return Level && Level->bObstacles;
	}
	return GetLevel() >= ObstacleStartLevel;
}

bool ASpinGameMode::IsShowingLevelBanner() const
{
	return State == ESpinState::Playing
		&& GetWorld()
		&& (GetWorld()->GetTimeSeconds() - LevelUpTime) < LevelBannerDuration;
}

float ASpinGameMode::GetJamProgress() const
{
	return FMath::Clamp(static_cast<float>(JamPresses) / FMath::Max(1, JamPressesRequired), 0.f, 1.f);
}

float ASpinGameMode::GetJamTimeFraction() const
{
	return FMath::Clamp(1.f - JamTimer / FMath::Max(0.01f, JamTimeLimit), 0.f, 1.f);
}

// ---------------------------------------------------------------------------------------------
// Endless mode
// ---------------------------------------------------------------------------------------------

void ASpinGameMode::StartGame()
{
	Mode = ESpinMode::Endless;
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
	bHandBroke = false;
	bHandWasInZone = false;
	bZoneTainted = false;
	bTutorialObstacleHit = false;
	ResetJam();
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

	// No new obstacle while a pause screen is about to take over.
	PlaceNextZone(!bUnlocksSecondButton && !bObstacleIntro);

	// Sometimes the hand starts to overheat.
	if (!bUnlocksSecondButton && !bObstacleIntro && JamPhase == EJamPhase::None
		&& GetLevel() >= JamStartLevel && FMath::FRand() < JamChance)
	{
		StartJamSparks();
	}

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
		// Obstacles just unlocked: pause until the player holds the lift button.
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
	// The tutorial can never be failed.
	if (IsTutorial())
	{
		return;
	}

	State = ESpinState::GameOver;
	HighScore = FMath::Max(HighScore, Score);

	// Stop the sparks, but a hand that broke stays visibly broken.
	if (!bHandBroke)
	{
		ResetJam();
	}
	GameOverTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	Shake(GameOverShake);
}

// ---------------------------------------------------------------------------------------------
// Tutorial mode
// ---------------------------------------------------------------------------------------------

void ASpinGameMode::StartTutorial()
{
	Mode = ESpinMode::Tutorial;
	Score = 0;
	Lives = MaxLives;
	LifeLostTime = -1000.f;
	LifeGainedTime = -1000.f;
	bOutOfLives = false;
	bHandBroke = false;
	NoticeTime = -1000.f;
	Direction = 1.f;
	ResetJam();
	BeginTutorialLevel(0);
}

void ASpinGameMode::BeginTutorialLevel(int32 Index)
{
	TutorialIndex = Index;
	TutorialHits = 0;

	const FTutorialLevel* Level = GetTutorialLevel();
	if (!Level)
	{
		ReturnToTitle();
		return;
	}

	CurrentSpeed = Level->Speed;
	ZoneHalfWidth = Level->ZoneHalfWidth;
	ActivePromptIndex = FMath::Clamp(Level->FirstPrompt, 0, FMath::Max(0, Prompts.Num() - 1));
	PromptStreak = 1;
	Direction = 1.f;
	bHandWasInZone = false;
	bZoneTainted = false;
	bTutorialObstacleHit = false;
	ResetJam();

	if (Clock)
	{
		Clock->RetireObstacles();
		Clock->SetLiftHeld(false, false);
	}

	// The explanation screen comes first. The zone is placed once the player confirms it.
	State = ESpinState::TutorialIntro;
}

void ASpinGameMode::BeginTutorialPlay()
{
	const FTutorialLevel* Level = GetTutorialLevel();

	State = ESpinState::Playing;
	bHandWasInZone = false;
	PlaceNextZone(Level && Level->bObstacles);
}

void ASpinGameMode::HandleTutorialPress(int32 ButtonIndex)
{
	if (!IsHandInZone())
	{
		ShowNotice(TEXT("Too early! Wait until the hand is inside the zone."));
		return;
	}

	if (ButtonIndex != ActivePromptIndex)
	{
		const FString Needed = Prompts.IsValidIndex(ActivePromptIndex) ? Prompts[ActivePromptIndex].Hint : FString();
		ShowNotice(FString::Printf(TEXT("Wrong button! This zone needs %s."), *Needed));
		return;
	}

	if (bZoneTainted)
	{
		ShowNotice(TEXT("You touched a post on the way. Go around again and raise the hand over it."));
		return;
	}

	OnTutorialHit();
}

void ASpinGameMode::OnTutorialHit()
{
	const FTutorialLevel* Level = GetTutorialLevel();
	if (!Level)
	{
		return;
	}

	++TutorialHits;
	Shake(HitShake);

	// Level finished.
	if (TutorialHits >= Level->Hits)
	{
		Shake(LevelUpShake);

		if (TutorialIndex + 1 < TutorialLevels.Num())
		{
			BeginTutorialLevel(TutorialIndex + 1);
		}
		else
		{
			ResetJam();
			if (Clock)
			{
				Clock->RetireObstacles();
				Clock->SetLiftHeld(false, false);
			}
			State = ESpinState::TutorialComplete;
		}
		return;
	}

	// Next target: the hand reverses and a new zone appears. The zone never changes until a hit succeeds.
	Direction = -Direction;
	bHandWasInZone = false;
	bZoneTainted = false;
	PickNextPrompt();

	if (JamPhase == EJamPhase::None && Level->JamAtHits.Contains(TutorialHits))
	{
		StartJamSparks();
	}

	PlaceNextZone(Level->bObstacles);
}

void ASpinGameMode::OnTutorialMiss()
{
	// The hand keeps moving and the zone stays exactly where it is. It will come around again.
	ShowNotice(TEXT("Missed it! The hand keeps going. Try again."));
	bZoneTainted = false;
	RespawnObstacleForCurrentZone();
}

void ASpinGameMode::RespawnObstacleForCurrentZone()
{
	if (!Clock)
	{
		return;
	}

	Clock->RetireObstacles();

	const FTutorialLevel* Level = GetTutorialLevel();
	if (!Level || !Level->bObstacles || FMath::FRand() >= Level->ObstacleChance)
	{
		return;
	}

	// Degrees from the hand to the zone centre, in the direction of travel.
	const float ToZone = FMath::Fmod(Direction * (ZoneCenter - HandAngle) + 720.f, 360.f);
	const float MinDist = FMath::Max(CurrentSpeed * ObstacleWarningTime, MinObstacleDistance);
	const float MaxDist = ToZone - ObstacleClearGap - ZoneHalfWidth;
	if (MaxDist < MinDist)
	{
		return; // no room to give the player a fair warning
	}

	const float Dist = FMath::Min(MinDist + FMath::FRandRange(0.f, 15.f), MaxDist);
	Clock->SpawnObstacle(
		HandAngle + Direction * Dist,
		FMath::FRandRange(ObstacleMinRadiusFraction, ObstacleMaxRadiusFraction),
		Dist);
}

void ASpinGameMode::ReturnToTitle()
{
	State = ESpinState::WaitingToStart;
	ResetJam();
	if (Clock)
	{
		Clock->RetireObstacles();
		Clock->SetLiftHeld(false, false);
	}
}

// ---------------------------------------------------------------------------------------------
// Jam
// ---------------------------------------------------------------------------------------------

void ASpinGameMode::StartJamSparks()
{
	JamPhase = EJamPhase::Sparking;
	JamTimer = 0.f;
	JamPresses = 0;

	if (Clock)
	{
		Clock->SetSparking(true);
	}
	if (IsTutorial())
	{
		ShowNotice(TEXT("Sparks! The hand is about to jam."));
	}
}

void ASpinGameMode::UpdateJam(float DeltaSeconds)
{
	if (JamPhase == EJamPhase::None)
	{
		return;
	}

	JamTimer += DeltaSeconds;

	if (JamPhase == EJamPhase::Sparking)
	{
		if (JamTimer >= JamSparkTime)
		{
			// The hand jams.
			JamPhase = EJamPhase::Jammed;
			JamTimer = 0.f;
			JamPresses = 0;
			Shake(LifeLostShake);

			if (Clock)
			{
				Clock->SetSparking(false);
				Clock->SetJammed(true);
				Clock->SetLiftHeld(false, false);
			}
			if (IsTutorial())
			{
				ShowNotice(FString::Printf(TEXT("Jammed! Press %s again and again to fix it."), *Prompts[0].Hint));
			}
		}
		return;
	}

	// Jammed: out of time?
	if (JamTimer >= JamTimeLimit)
	{
		if (IsTutorial())
		{
			// No penalty in the tutorial: start the attempt over.
			JamTimer = 0.f;
			JamPresses = 0;
			ShowNotice(TEXT("Too slow! In a real run the hand would break. Try again."));
		}
		else
		{
			bHandBroke = true;
			EndGame();
		}
	}
}

void ASpinGameMode::RegisterJamPress()
{
	++JamPresses;
	Shake(0.06f);

	if (JamPresses >= JamPressesRequired)
	{
		FixJam();
	}
}

void ASpinGameMode::FixJam()
{
	JamPhase = EJamPhase::None;
	JamTimer = 0.f;
	JamPresses = 0;
	bHandWasInZone = false;
	bZoneTainted = false;

	if (Clock)
	{
		Clock->SetJammed(false);
	}
	Shake(HitShake);

	// The hand spun freely while it was broken, so give it a fresh zone ahead.
	PlaceNextZone(true);

	if (IsTutorial())
	{
		ShowNotice(TEXT("Fixed! Back to it."));
	}
}

void ASpinGameMode::ResetJam()
{
	JamPhase = EJamPhase::None;
	JamTimer = 0.f;
	JamPresses = 0;

	if (Clock)
	{
		Clock->SetSparking(false);
		Clock->SetJammed(false);
	}
}

// ---------------------------------------------------------------------------------------------
// Shared zone logic
// ---------------------------------------------------------------------------------------------

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

	float Chance = ObstacleChance;
	if (IsTutorial())
	{
		const FTutorialLevel* Level = GetTutorialLevel();
		Chance = Level ? Level->ObstacleChance : 0.f;
	}

	if (bAllowObstacle && Clock && ObstaclesEnabled() && FMath::FRand() < Chance)
	{
		// The zone must never start on top of the hand, so it can't wrap too close to where the hand is.
		const float MaxOffset = 360.f - ZoneHalfWidth - 12.f;

		// The obstacle sits far enough ahead that there is time to see it and lift the hand...
		float ObstacleDist = FMath::Max(CurrentSpeed * ObstacleWarningTime, MinObstacleDistance) + FMath::FRandRange(0.f, 15.f);
		ObstacleDist = FMath::Min(ObstacleDist, MaxOffset - ObstacleClearGap - ZoneHalfWidth);

		// ...and the zone sits beyond it, with a gap after it.
		Offset = FMath::Clamp(FMath::Max(Offset, ObstacleDist + ObstacleClearGap + ZoneHalfWidth), MinZoneOffset, MaxOffset);

		Clock->SpawnObstacle(
			HandAngle + Direction * ObstacleDist,
			FMath::FRandRange(ObstacleMinRadiusFraction, ObstacleMaxRadiusFraction),
			ObstacleDist);
	}

	ZoneCenter = FMath::Fmod(HandAngle + Direction * Offset + 360.f, 360.f);
}
