// Spin To Win - core game rules.

#include "SpinGameMode.h"
#include "SpinPlayerController.h"
#include "SpinHUD.h"
#include "SpinClock.h"
#include "SpinSaveGame.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

ASpinGameMode::ASpinGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	// The player is the camera and the controls: no pawn needed.
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ASpinPlayerController::StaticClass();
	HUDClass = ASpinHUD::StaticClass();

	// Button 0: always available.
	FSpinPrompt First;
	First.KeyboardKey = TEXT("W");
	First.XboxButton = TEXT("A");
	First.PlayStationButton = TEXT("X");
	First.Color = FLinearColor(0.1f, 0.8f, 0.2f); // green
	Prompts.Add(First);

	// Button 1: the second button.
	FSpinPrompt Second;
	Second.KeyboardKey = TEXT("D");
	Second.XboxButton = TEXT("B");
	Second.PlayStationButton = TEXT("O");
	Second.Color = FLinearColor(0.2f, 0.5f, 1.f); // blue
	Prompts.Add(Second);

	// Button 2: the third button. "[]" is the PlayStation Square button.
	FSpinPrompt Third;
	Third.KeyboardKey = TEXT("A");
	Third.XboxButton = TEXT("X");
	Third.PlayStationButton = TEXT("[]");
	Third.Color = FLinearColor(0.85f, 0.25f, 1.f); // purple
	Prompts.Add(Third);

	BuildTutorialLevels();
}

void ASpinGameMode::BuildTutorialLevels()
{
	// All the tutorial numbers live here. Speeds are in degrees per second, zone sizes are half-widths in degrees.

	TutorialLevels.Reset();

	// Level 1: the basics. Slow, big zones, one button.
	{
		FTutorialLevel L;
		L.Title = TEXT("LEVEL 1 - THE BASICS");
		L.IntroLines.Add(TEXT("A hand sweeps around the dial."));
		L.IntroLines.Add(TEXT("Press {0} when it is inside the green zone."));
		L.IntroLines.Add(TEXT("Miss? No problem. The hand keeps going, just try again."));
		L.Instruction = TEXT("Press {0} when the hand is inside the green zone");
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
		L.Instruction = TEXT("Press {0} when the hand is inside the green zone");
		L.Confirm = ETutorialConfirm::FirstButton;
		L.Hits = 10;
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
		L.IntroLines.Add(TEXT("GREEN zone: press {0}"));
		L.IntroLines.Add(TEXT("BLUE zone: press {1}"));
		L.IntroLines.Add(TEXT("The zones are big again while you get used to it."));
		L.Instruction = TEXT("Green zone: {0}     Blue zone: {1}");
		L.Confirm = ETutorialConfirm::SecondButton;
		L.Hits = 10;
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
		L.Instruction = TEXT("Green zone: {0}     Blue zone: {1}");
		L.Confirm = ETutorialConfirm::FirstButton;
		L.Hits = 10;
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
		L.IntroLines.Add(TEXT("HOLD {LIFT} to raise the hand over a post. It drops by itself after one full turn."));
		L.IntroLines.Add(TEXT("In a real run you lose points for as long as the hand is up, so only lift when you need to."));
		L.IntroLines.Add(FString::Printf(TEXT("You get %d lives in a real run, and hopping a post earns one back."), MaxLives));
		L.Instruction = TEXT("Hold {LIFT} to raise the hand over posts");
		L.Confirm = ETutorialConfirm::Lift;
		L.Hits = 10;
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
		L.Instruction = TEXT("Green: {0}   Blue: {1}   Raise: {LIFT}");
		L.Confirm = ETutorialConfirm::SecondButton;
		L.Hits = 10;
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
		L.IntroLines.Add(TEXT("Press {0} again and again to fix it before the time bar runs out."));
		L.IntroLines.Add(TEXT("In a real run you get 3 seconds, or the hand breaks and it is game over."));
		L.Instruction = TEXT("If the hand jams, press {0} again and again");
		L.Confirm = ETutorialConfirm::FirstButton;
		L.Hits = 10;
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

	LoadSaveData();

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

	if (State == ESpinState::WaitingToStart || State == ESpinState::ModeSelect || State == ESpinState::ModifierSelect || State == ESpinState::SettingsMenu
		|| State == ESpinState::Leaderboard || State == ESpinState::ProfileSelect || State == ESpinState::NameEntry)
	{
		// Slow idle spin behind the menus.
		HandAngle = FMath::Fmod(HandAngle + StartSpeed * 0.5f * Dt, 360.f);
		return;
	}

	if (State != ESpinState::Playing)
	{
		return;
	}

	HandAngle = FMath::Fmod(HandAngle + Direction * CurrentSpeed * Dt + 360.f, 360.f);

	// Endless: a lifted hand costs points for as long as it's up. Fractions carry over between
	// frames so the drain stays steady at any frame rate.
	if (!IsTutorial() && Clock && Clock->IsLiftActive())
	{
		LiftPointsDebt += LiftPointsPerSecond * Dt;
		const int32 Whole = FMath::FloorToInt(LiftPointsDebt);
		LiftPointsDebt -= Whole;
		Points = FMath::Max(0, Points - Whole);
	}

	// Invincibility window after losing a life: the hand keeps moving, but
	// nothing can go wrong (and nothing can be hit) until the timer runs out.
	if (bInvincible)
	{
		InvincibleTimeRemaining -= Dt;
		if (InvincibleTimeRemaining <= 0.f)
		{
			bInvincible = false;
			InvincibleTimeRemaining = 0.f;
			bHandWasInZone = false;
			InvincibilityEndTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		}
		else
		{
			return;
		}
	}

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
				ShowNotice(TEXT("You touched a post! Hold {LIFT} to raise the hand over it. Try the lap again."));
			}
		}
		else
		{
			Fail(ESpinMistake::HitObstacle); // ran into an obstacle without lifting the hand over it
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
			OnZoneMissed();
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
		// First button: Play (mode select). Second button: Settings. Mirrors the PLAY / SETTINGS
		// menu cards so keyboard and gamepad reach the same places mouse clicks do.
		if (ButtonIndex == 0)
		{
			OpenModeSelect();
		}
		else if (ButtonIndex == 1)
		{
			OpenSettings();
		}
		break;

	case ESpinState::ModeSelect:
		// Mirrors the three cards: tutorial, Endless (via the modifier screen), daily challenge.
		if (ButtonIndex == 0)
		{
			StartTutorial();
		}
		else if (ButtonIndex == 1)
		{
			OpenModifiers();
		}
		else if (ButtonIndex == 2)
		{
			StartGame(true);
		}
		break;

	case ESpinState::ModifierSelect:
		// First button: start. Second button: back to mode select. Toggling modifiers is mouse-only.
		if (ButtonIndex == 0)
		{
			StartGame();
		}
		else if (ButtonIndex == 1)
		{
			OpenModeSelect();
		}
		break;

	case ESpinState::SettingsMenu:
		// No keyboard shortcuts here - mouse only for now.
		break;

	case ESpinState::Leaderboard:
	case ESpinState::ProfileSelect:
		// The second button backs out (the BACK button does the same). Picking a profile is mouse-only.
		if (ButtonIndex == 1)
		{
			CloseMenus();
		}
		break;

	case ESpinState::NameEntry:
		// Typing goes through TypeNameChar instead - the player controller intercepts the keyboard.
		break;

	case ESpinState::Playing:
		// Invincibility window after losing a life: nothing can be hit and
		// nothing can go wrong, so presses are completely ignored until the timer runs out.
		if (bInvincible)
		{
			break;
		}

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
			Fail(ESpinMistake::WrongButton); // too early, or the wrong button
		}
		break;

	case ESpinState::NewButtonIntro:
		// The player must press the newly unlocked button to confirm and continue.
		if (ButtonIndex == NewButtonIndex)
		{
			ResumeAfterIntro();
		}
		break;

	case ESpinState::ObstacleIntro:
		// Waits for the lift button instead, see SetLiftHeld.
		break;

	case ESpinState::JamIntro:
		// Confirmed with the first button - the same one that repairs a jam.
		if (ButtonIndex == 0)
		{
			ResumeAfterIntro();
		}
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
		else if (ButtonIndex == 1)
		{
			ReturnToTitle();
		}
		break;

	case ESpinState::GameOver:
		// Mirrors the game-over menu: first button retries (the same kind of run), second goes back
		// to the main menu. Quitting is mouse-only so it can't happen by accident.
		if (IsGameOverMenuReady())
		{
			if (ButtonIndex == 0)
			{
				StartGame(bDailyRun);
			}
			else if (ButtonIndex == 1)
			{
				ReturnToTitle();
			}
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

	// The hand can be lifted at any time once obstacles are in play, obstacle or not. It costs
	// points for as long as it's up (see Tick), and drops by itself after a full turn.
	const bool bValidPress = bHeld && State == ESpinState::Playing && JamPhase != EJamPhase::Jammed && ObstaclesEnabled();

	if (Clock)
	{
		Clock->SetLiftHeld(bHeld, bValidPress);
	}
}

void ASpinGameMode::OnObstacleCleared()
{
	// The hand got over an obstacle: earn a life back.
	if (State == ESpinState::Playing && Lives < GetRunMaxLives())
	{
		++Lives;
		if (GetWorld())
		{
			LifeGainedTime = GetWorld()->GetTimeSeconds();
		}
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

int32 ASpinGameMode::GetHitsForLevel(int32 Level) const
{
	const bool bShortened = bFastStart && Level <= FastStartLevels;
	return FMath::Max(1, bShortened ? FastStartHitsPerLevel : HitsPerLevel);
}

int32 ASpinGameMode::GetLevel() const
{
	// Walk the levels, since a fast start makes the opening ones shorter than the rest.
	int32 Level = 1;
	int32 LevelEnd = GetHitsForLevel(1);
	while (Hits >= LevelEnd)
	{
		++Level;
		LevelEnd += GetHitsForLevel(Level);
	}
	return Level;
}

int32 ASpinGameMode::GetLevelTarget() const
{
	int32 LevelEnd = 0;
	for (int32 Level = 1, Current = GetLevel(); Level <= Current; ++Level)
	{
		LevelEnd += GetHitsForLevel(Level);
	}
	return LevelEnd;
}

const USpinSaveGame* ASpinGameMode::GetSaveData() const
{
	return SaveData;
}

int32 ASpinGameMode::GetNumActiveButtons() const
{
	if (IsTutorial())
	{
		const FTutorialLevel* Level = GetTutorialLevel();
		return FMath::Clamp(Level ? Level->NumButtons : 1, 1, FMath::Max(1, Prompts.Num()));
	}

	int32 Wanted = 1;
	while (Wanted < Prompts.Num() && GetLevel() >= GetButtonUnlockLevel(Wanted))
	{
		++Wanted;
	}
	return Wanted;
}

int32 ASpinGameMode::GetButtonUnlockLevel(int32 ButtonIndex) const
{
	switch (ButtonIndex)
	{
	case 0:  return 1;
	case 1:  return SecondButtonLevel;
	case 2:  return ThirdButtonLevel;
	default: return MAX_int32; // no unlock level set: never in play
	}
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

float ASpinGameMode::GetHitPulse() const
{
	if (!GetWorld())
	{
		return 0.f;
	}

	// Snaps to full on the hit, then falls off quickly (squared) so it reads as a punchy flash.
	const float T = (GetWorld()->GetTimeSeconds() - LastHitTime) / FMath::Max(0.01f, HitPulseDuration);
	return (T >= 0.f && T < 1.f) ? FMath::Square(1.f - T) : 0.f;
}

bool ASpinGameMode::IsGameOverMenuReady() const
{
	return State == ESpinState::GameOver && GetWorld() && GetWorld()->GetTimeSeconds() - GameOverTime >= RestartDelay;
}

void ASpinGameMode::RegisterHit()
{
	if (GetWorld())
	{
		LastHitTime = GetWorld()->GetTimeSeconds();
	}
}

// ---------------------------------------------------------------------------------------------
// Scoring and the saved leaderboard
// ---------------------------------------------------------------------------------------------

ESpinHitGrade ASpinGameMode::GradeHit() const
{
	// 0 = dead centre of the zone, 1 = its very edge.
	const float Offset = FMath::Abs(FMath::FindDeltaAngleDegrees(HandAngle, ZoneCenter)) / FMath::Max(0.01f, ZoneHalfWidth);

	if (Offset <= PerfectFraction)
	{
		return ESpinHitGrade::Perfect;
	}
	return Offset <= GoodFraction ? ESpinHitGrade::Good : ESpinHitGrade::Ok;
}

void ASpinGameMode::ScoreHit(ESpinHitGrade Grade, float PointsScale)
{
	LastHitGrade = Grade;

	// PERFECT builds the streak, GOOD holds it, OK breaks it.
	if (Grade == ESpinHitGrade::Perfect)
	{
		++PerfectStreak;
		ComboMultiplier = FMath::Min(MaxMultiplier, 1 + PerfectStreak / FMath::Max(1, PerfectsPerMultiplier));
	}
	else if (Grade == ESpinHitGrade::Ok)
	{
		BreakCombo();
	}

	const int32 BasePoints = (Grade == ESpinHitGrade::Perfect) ? PerfectPoints
		: (Grade == ESpinHitGrade::Good) ? GoodPoints
		: OkPoints;

	LastHitPoints = FMath::RoundToInt(BasePoints * PointsScale * RunPointsMultiplier) * ComboMultiplier;
	const int32 OldPoints = Points;
	Points += LastHitPoints;

	// Flash the moment this run overtakes the saved best (only once, and only if there is one).
	if (BestPoints > 0 && OldPoints <= BestPoints && Points > BestPoints && PassedBestPointsTime < 0.f && GetWorld())
	{
		PassedBestPointsTime = GetWorld()->GetTimeSeconds();
	}
}

void ASpinGameMode::BreakCombo()
{
	if (ComboMultiplier > 1 && GetWorld())
	{
		ComboLostTime = GetWorld()->GetTimeSeconds();
	}
	PerfectStreak = 0;
	ComboMultiplier = 1;
}

void ASpinGameMode::LoadSaveData()
{
	SaveData = Cast<USpinSaveGame>(UGameplayStatics::LoadGameFromSlot(USpinSaveGame::SlotName, 0));
	if (!SaveData)
	{
		SaveData = Cast<USpinSaveGame>(UGameplayStatics::CreateSaveGameObject(USpinSaveGame::StaticClass()));
	}
	bPlayStationIcons = SaveData && SaveData->bPlayStationIcons;
	SelectedModifiers = SaveData ? SaveData->SelectedModifiers : 0;
}

void ASpinGameMode::RecordRun()
{
	LastRunLevelRank = 0;
	LastRunScoreRank = 0;
	LastRunDailyRank = 0;

	// A run that never landed a hit isn't worth a leaderboard slot.
	if (!SaveData || Hits <= 0)
	{
		return;
	}

	FSpinRunRecord Run;
	Run.PlayerName = GetPlayerName();
	Run.Level = GetLevel();
	Run.Points = Points;
	Run.Hits = Hits;
	Run.Date = FDateTime::Now();
	Run.Modifiers = RunModifiers;

	// Daily runs only compete with that day's daily runs. One that ran past midnight is
	// yesterday's challenge, and yesterday's board is already gone - so it isn't recorded.
	if (bDailyRun)
	{
		if (RunDayKey == GetTodayKey())
		{
			LastRunDailyRank = SaveData->AddDailyRun(Run, RunDayKey, LeaderboardSize);
		}
	}
	else
	{
		SaveData->AddRun(Run, LeaderboardSize, LastRunLevelRank, LastRunScoreRank);
	}
	WriteSaveData();
}

// ---------------------------------------------------------------------------------------------
// Modifiers and the daily challenge
// ---------------------------------------------------------------------------------------------

const FSpinModifierInfo& ASpinGameMode::GetModifierInfo(ESpinModifier Modifier)
{
	// The multipliers roughly track how much harder each one makes a run.
	static const FSpinModifierInfo Infos[] =
	{
		{ TEXT("FAST HAND"),  TEXT("The hand starts 35% faster."),                           1.3f },
		{ TEXT("TINY ZONES"), TEXT("Target zones are 30% narrower."),                       1.5f },
		{ TEXT("WILD SPIN"),  TEXT("The hand only sometimes reverses after a hit."),         1.25f },
		{ TEXT("EARLY JAMS"), TEXT("Jams from level 1, and half again as often."),           1.3f },
		{ TEXT("ONE LIFE"),   TEXT("One life and one miss: any mistake ends the run."),      2.f },
	};
	static_assert(UE_ARRAY_COUNT(Infos) == static_cast<int32>(ESpinModifier::Count), "One entry per modifier");
	return Infos[FMath::Clamp(static_cast<int32>(Modifier), 0, static_cast<int32>(ESpinModifier::Count) - 1)];
}

float ASpinGameMode::GetModifiersMultiplier(int32 Mask)
{
	float Multiplier = 1.f;
	for (int32 i = 0; i < static_cast<int32>(ESpinModifier::Count); ++i)
	{
		if (Mask & (1 << i))
		{
			Multiplier *= GetModifierInfo(static_cast<ESpinModifier>(i)).PointsMultiplier;
		}
	}
	return Multiplier;
}

FString ASpinGameMode::GetModifierNames(int32 Mask)
{
	TArray<FString> Names;
	for (int32 i = 0; i < static_cast<int32>(ESpinModifier::Count); ++i)
	{
		if (Mask & (1 << i))
		{
			Names.Add(GetModifierInfo(static_cast<ESpinModifier>(i)).Name);
		}
	}
	return FString::Join(Names, TEXT(" + "));
}

bool ASpinGameMode::HasModifier(ESpinModifier Modifier) const
{
	return (RunModifiers & (1 << static_cast<int32>(Modifier))) != 0;
}

int32 ASpinGameMode::GetTodayKey()
{
	const FDateTime Today = FDateTime::Now();
	return Today.GetYear() * 10000 + Today.GetMonth() * 100 + Today.GetDay();
}

int32 ASpinGameMode::GetDailyModifiers()
{
	const FRandomStream DayRandom(static_cast<int32>(HashCombine(static_cast<uint32>(GetTodayKey()), 0xD411u)));
	return 1 << DayRandom.RandRange(0, static_cast<int32>(ESpinModifier::Count) - 1);
}

int32 ASpinGameMode::GetDailyBest() const
{
	const TArray<FSpinRunRecord>* Board = SaveData ? SaveData->GetDailyBoard(GetTodayKey()) : nullptr;
	const FString Me = GetPlayerName();
	if (!Board || Me.IsEmpty())
	{
		return 0;
	}

	// The board is sorted by score, so the first run of theirs is their best.
	const FSpinRunRecord* Best = Board->FindByPredicate([&Me](const FSpinRunRecord& Run)
	{
		return Run.PlayerName.Equals(Me, ESearchCase::IgnoreCase);
	});
	return Best ? Best->Points : 0;
}

int32 ASpinGameMode::GetRunMaxLives() const
{
	return HasModifier(ESpinModifier::OneLife) ? 1 : MaxLives;
}

int32 ASpinGameMode::GetRunMaxMissLives() const
{
	return HasModifier(ESpinModifier::OneLife) ? 1 : MaxMissLives;
}

float ASpinGameMode::GetRunStartSpeed() const
{
	return StartSpeed * (HasModifier(ESpinModifier::FastHand) ? 1.35f : 1.f);
}

float ASpinGameMode::GetRunStartZoneHalfWidth() const
{
	return StartZoneHalfWidth * (HasModifier(ESpinModifier::TinyZones) ? 0.7f : 1.f);
}

float ASpinGameMode::GetRunMinZoneHalfWidth() const
{
	return MinZoneHalfWidth * (HasModifier(ESpinModifier::TinyZones) ? 0.7f : 1.f);
}

int32 ASpinGameMode::GetRunJamStartLevel() const
{
	return HasModifier(ESpinModifier::EarlyJams) ? 1 : JamStartLevel;
}

float ASpinGameMode::GetRunJamChance() const
{
	return FMath::Min(1.f, JamChance * (HasModifier(ESpinModifier::EarlyJams) ? 1.5f : 1.f));
}

void ASpinGameMode::WriteSaveData()
{
	if (SaveData)
	{
		UGameplayStatics::SaveGameToSlot(SaveData, USpinSaveGame::SlotName, 0);
	}
}

// ---------------------------------------------------------------------------------------------
// Player profiles
// ---------------------------------------------------------------------------------------------

FString ASpinGameMode::GetPlayerName() const
{
	const FSpinProfile* Profile = SaveData ? SaveData->GetActiveProfile() : nullptr;
	return Profile ? Profile->Name : FString();
}

void ASpinGameMode::OpenProfiles()
{
	PendingDeleteProfile = INDEX_NONE;
	State = ESpinState::ProfileSelect;
}

void ASpinGameMode::BeginNameEntry(bool bThenPlay)
{
	NameEntryText.Reset();
	NameEntryError.Reset();
	bNameEntryThenPlay = bThenPlay;
	State = ESpinState::NameEntry;
}

void ASpinGameMode::TypeNameChar(TCHAR Char)
{
	if (State == ESpinState::NameEntry && NameEntryText.Len() < USpinSaveGame::MaxNameLength)
	{
		// No leading or double spaces - they'd just make names that look identical.
		if (Char == TEXT(' ') && (NameEntryText.IsEmpty() || NameEntryText.EndsWith(TEXT(" "))))
		{
			return;
		}
		NameEntryText.AppendChar(Char);
		NameEntryError.Reset();
	}
}

void ASpinGameMode::EraseNameChar()
{
	if (State == ESpinState::NameEntry && !NameEntryText.IsEmpty())
	{
		NameEntryText.LeftChopInline(1);
		NameEntryError.Reset();
	}
}

void ASpinGameMode::ConfirmName()
{
	if (State != ESpinState::NameEntry || !SaveData)
	{
		return;
	}

	const FString Name = NameEntryText.TrimStartAndEnd();
	if (Name.IsEmpty())
	{
		NameEntryError = TEXT("Type a name first");
		return;
	}
	if (SaveData->FindProfile(Name) != INDEX_NONE)
	{
		NameEntryError = FString::Printf(TEXT("%s is already taken - pick it from the player list instead"), *Name);
		return;
	}

	if (SaveData->AddProfile(Name) == INDEX_NONE)
	{
		NameEntryError = FString::Printf(TEXT("%d players max - delete one first"), USpinSaveGame::MaxProfiles);
		return;
	}
	WriteSaveData();

	if (bNameEntryThenPlay)
	{
		bNameEntryThenPlay = false;
		OpenModeSelect();
	}
	else
	{
		CloseMenus();
	}
}

void ASpinGameMode::CancelName()
{
	if (State != ESpinState::NameEntry)
	{
		return;
	}

	// Back to wherever it was opened from: the profile list, or the main menu if it came from PLAY.
	if (bNameEntryThenPlay || !SaveData || SaveData->Profiles.Num() == 0)
	{
		bNameEntryThenPlay = false;
		CloseMenus();
	}
	else
	{
		OpenProfiles();
	}
}

// ---------------------------------------------------------------------------------------------
// Endless mode
// ---------------------------------------------------------------------------------------------

void ASpinGameMode::OpenModifiers()
{
	State = ESpinState::ModifierSelect;
}

void ASpinGameMode::StartGame(bool bDaily)
{
	Mode = ESpinMode::Endless;
	State = ESpinState::Playing;

	// The daily challenge is the same run for everyone today: a seed and a modifier picked from
	// the date. A normal run plays whatever was picked on the modifier screen, on a fresh seed.
	bDailyRun = bDaily;
	RunDayKey = GetTodayKey();
	RunModifiers = bDaily ? GetDailyModifiers() : SelectedModifiers;
	RunPointsMultiplier = GetModifiersMultiplier(RunModifiers);
	RunSeed = bDaily ? static_cast<int32>(HashCombine(static_cast<uint32>(RunDayKey), 0x5A17u)) : FMath::Rand();
	RunRandom.Initialize(RunSeed);

	Hits = 0;
	Points = 0;
	ComboMultiplier = 1;
	PerfectStreak = 0;
	LastHitGrade = ESpinHitGrade::None;
	LastHitPoints = 0;
	ComboLostTime = -1000.f;
	PassedBestPointsTime = -1000.f;

	// What this run is chasing (the player's own bests), and whether they've earned a quicker opening.
	const FSpinProfile* Profile = SaveData ? SaveData->GetActiveProfile() : nullptr;
	// A daily run chases today's daily best rather than the all-time one. BestLevel still decides
	// which explanation screens the player has already seen.
	BestLevel = Profile ? Profile->BestLevel : 0;
	BestPoints = bDaily ? GetDailyBest() : (Profile ? Profile->BestPoints : 0);

	// No fast start on the daily, so everyone plays exactly the same opening.
	bFastStart = !bDaily && FastStartLevels > 0 && BestLevel >= FastStartUnlockLevel;
	if (bDaily)
	{
		ShowNotice(FString::Printf(TEXT("DAILY CHALLENGE - today's twist: %s"), *GetModifierNames(RunModifiers)));
	}
	else if (bFastStart)
	{
		ShowNotice(FString::Printf(TEXT("FAST START: you've reached level %d before, so the first %d levels fly by"), BestLevel, FastStartLevels));
	}

	HandAngle = 0.f;
	Direction = 1.f;
	CurrentSpeed = GetRunStartSpeed();
	ZoneHalfWidth = GetRunStartZoneHalfWidth();
	ActivePromptIndex = 0;
	PromptStreak = 1;
	LevelUpTime = -1000.f;
	Lives = GetRunMaxLives();
	LifeLostTime = -1000.f;
	LifeGainedTime = -1000.f;
	bOutOfLives = false;
	MissLives = GetRunMaxMissLives();
	MissLifeLostTime = -1000.f;
	bOutOfMissLives = false;
	// A few seconds of invincibility (with the big countdown) to get oriented before anything can
	// go wrong, same as resuming after a mistake.
	bInvincible = true;
	InvincibleTimeRemaining = InvincibilityDuration;
	InvincibilityEndTime = -1000.f;
	LastMistake = ESpinMistake::None;
	bHandBroke = false;
	bHandWasInZone = false;
	bZoneTainted = false;
	bTutorialObstacleHit = false;
	LiftPointsDebt = 0.f;
	ResetJam();
	PlaceNextZone();
}

void ASpinGameMode::OnHit()
{
	const int32 OldLevel = GetLevel();

	// A shortened (fast start) level packs HitsPerLevel hits' worth of ramp into fewer hits, so
	// each of its hits counts for that many normal ones - in speed, zone shrink, and points.
	const float RampSteps = static_cast<float>(FMath::Max(1, HitsPerLevel)) / GetHitsForLevel(OldLevel);

	// Grade the hit before anything moves.
	ScoreHit(GradeHit(), RampSteps);

	++Hits;
	RegisterHit();

	// Every roll after this hit comes from the seed and the hit count alone, so the same seed
	// always plays out the same - however the run went up to here.
	RunRandom.Initialize(static_cast<int32>(HashCombine(static_cast<uint32>(RunSeed), static_cast<uint32>(Hits))));

	const int32 NewLevel = GetLevel();

	// The newest button this hit brings into play, if any.
	int32 UnlockedButton = INDEX_NONE;
	for (int32 i = 1; i < Prompts.Num(); ++i)
	{
		const int32 UnlockLevel = GetButtonUnlockLevel(i);
		if (OldLevel < UnlockLevel && NewLevel >= UnlockLevel)
		{
			UnlockedButton = i;
		}
	}
	const bool bUnlocksButton = UnlockedButton != INDEX_NONE;
	const bool bUnlocksObstacles = OldLevel < ObstacleStartLevel && NewLevel >= ObstacleStartLevel;
	const bool bUnlocksJams = OldLevel < GetRunJamStartLevel() && NewLevel >= GetRunJamStartLevel();

	// The explanation (pause) screens only show until the saved best has reached that level - after
	// that the player has seen them, and the "next level" heads-up plus the level banner's unlock line
	// are enough. Only one screen at a time; if several land on the same level, the new-button
	// screen wins, then obstacles, then jams.
	const bool bButtonIntro = bUnlocksButton && BestLevel < GetButtonUnlockLevel(UnlockedButton);
	const bool bObstacleIntro = !bButtonIntro && bUnlocksObstacles && BestLevel < ObstacleStartLevel;
	const bool bJamIntro = !bButtonIntro && !bObstacleIntro && bUnlocksJams && BestLevel < GetRunJamStartLevel();
	const bool bPauseScreen = bButtonIntro || bObstacleIntro || bJamIntro;

	// The normal "LEVEL N" banner is skipped when a pause screen takes over instead.
	if (NewLevel > OldLevel && GetWorld() && !bPauseScreen)
	{
		LevelUpTime = GetWorld()->GetTimeSeconds();
	}

	Shake(HitShake);

	// Flip direction (only sometimes, with Wild Spin), speed up, shrink the zone, and pick the next target.
	if (!HasModifier(ESpinModifier::WildSpin) || RunRandom.FRand() < 0.5f)
	{
		Direction = -Direction;
	}
	CurrentSpeed = FMath::Min(CurrentSpeed * FMath::Pow(SpeedMultiplier, RampSteps), MaxSpeed);
	ZoneHalfWidth = FMath::Max(GetRunMinZoneHalfWidth(), ZoneHalfWidth - ZoneShrinkPerHit * RampSteps);
	bHandWasInZone = false;

	// Pick which button the next zone needs (random once several buttons are in play).
	PickNextPrompt();

	// No new obstacle while a pause screen is about to take over.
	PlaceNextZone(!bPauseScreen);

	// Sometimes the hand starts to overheat.
	if (!bPauseScreen && JamPhase == EJamPhase::None
		&& GetLevel() >= GetRunJamStartLevel() && RunRandom.FRand() < GetRunJamChance())
	{
		StartJamSparks();
	}

	if (bUnlocksButton)
	{
		// A button just unlocked: reset the hand to its slowest speed and the zone to its
		// widest, and make the very first zone need the new button - with or without the screen.
		CurrentSpeed = GetRunStartSpeed();
		ZoneHalfWidth = GetRunStartZoneHalfWidth();
		ActivePromptIndex = UnlockedButton;
		PromptStreak = 1;
	}

	if (bButtonIntro)
	{
		// Pause and wait for the player to press the new button.
		NewButtonIndex = UnlockedButton;
		State = ESpinState::NewButtonIntro;
	}
	else if (bObstacleIntro)
	{
		// Obstacles just unlocked: pause until the player holds the lift button.
		State = ESpinState::ObstacleIntro;
	}
	else if (bJamIntro)
	{
		// Jams can happen from here on: explain the warning signs and the fix before the first one.
		State = ESpinState::JamIntro;
	}
}

bool ASpinGameMode::GetUnlockAtLevel(int32 Level, FString& OutTitle, FString& OutHint) const
{
	TArray<FString> Titles;
	TArray<FString> Hints;

	if (Level == ObstacleStartLevel)
	{
		Titles.Add(TEXT("OBSTACLES"));
		Hints.Add(TEXT("Hold {LIFT} to raise the hand over them"));
	}
	static const TCHAR* ButtonTitles[] = { TEXT("FIRST BUTTON"), TEXT("SECOND BUTTON"), TEXT("THIRD BUTTON") };
	for (int32 i = 1; i < Prompts.Num() && i < static_cast<int32>(UE_ARRAY_COUNT(ButtonTitles)); ++i)
	{
		if (Level == GetButtonUnlockLevel(i))
		{
			Titles.Add(ButtonTitles[i]);
			Hints.Add(FString::Printf(TEXT("Zones can ask for {%d} too - match the colour"), i));
		}
	}
	if (Level == GetRunJamStartLevel() && Prompts.IsValidIndex(0))
	{
		Titles.Add(TEXT("JAMS"));
		Hints.Add(TEXT("Sparks mean a jam is coming - mash {0} to repair it"));
	}

	OutTitle = FString::Join(Titles, TEXT("  +  "));
	OutHint = ExpandKeys(FString::Join(Hints, TEXT("   |   ")));
	return Titles.Num() > 0;
}

void ASpinGameMode::OnZoneMissed()
{
	Fail(ESpinMistake::MissedZone);
}

void ASpinGameMode::Fail(ESpinMistake Mistake)
{
	LastMistake = Mistake;
	Shake(LifeLostShake);
	bHandWasInZone = false;
	BreakCombo();

	// Obstacle hits spend the regenerating "Lives" pool; everything else spends the
	// non-regenerating "MissLives" pool. Either way, running out ends the run - otherwise play
	// carries straight on at the current level/score.
	if (Mistake == ESpinMistake::HitObstacle)
	{
		Lives = FMath::Max(0, Lives - 1);
		if (GetWorld())
		{
			LifeLostTime = GetWorld()->GetTimeSeconds();
		}

		if (Lives <= 0)
		{
			bOutOfLives = true;
			EndGame();
			return;
		}
	}
	else
	{
		MissLives = FMath::Max(0, MissLives - 1);
		if (GetWorld())
		{
			MissLifeLostTime = GetWorld()->GetTimeSeconds();
		}

		if (MissLives <= 0)
		{
			bOutOfMissLives = true;
			if (Mistake == ESpinMistake::HandBroke)
			{
				bHandBroke = true;
			}
			EndGame();
			return;
		}

		// The jam that broke the hand is fixed on the spot - the player is not left looking at a
		// still-jammed hand.
		if (Mistake == ESpinMistake::HandBroke)
		{
			ResetJam();
		}

		// No pause: straight into the invincibility countdown to get back in rhythm. Obstacle hits
		// skip this - the obstacle is cleared below and play simply carries on.
		bInvincible = true;
		InvincibleTimeRemaining = InvincibilityDuration;
		InvincibilityEndTime = -1000.f;
	}

	switch (Mistake)
	{
	case ESpinMistake::MissedZone:  ShowNotice(TEXT("You missed the zone!")); break;
	case ESpinMistake::WrongButton: ShowNotice(TEXT("Wrong button, or too early!")); break;
	case ESpinMistake::HitObstacle: ShowNotice(TEXT("You hit an obstacle!")); break;
	case ESpinMistake::HandBroke:   ShowNotice(TEXT("The hand broke free!")); break;
	default: break;
	}

	// A clean slate: nothing left over to dodge (and the obstacle just hit can't hit again).
	if (Clock)
	{
		Clock->RetireObstacles();
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
	RecordRun();

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
	bDailyRun = false;
	RunModifiers = 0;
	RunPointsMultiplier = 1.f;
	RunRandom.Initialize(FMath::Rand());
	Hits = 0;
	Points = 0;
	LastHitGrade = ESpinHitGrade::None;
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
		ShowNotice(FString::Printf(TEXT("Wrong button! This zone needs {%d}."), ActivePromptIndex));
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

	// Grade the hit so the tutorial teaches aiming for the centre, but no points are scored.
	LastHitGrade = GradeHit();
	LastHitPoints = 0;

	++TutorialHits;
	Shake(HitShake);
	RegisterHit();

	// Level finished.
	if (TutorialHits >= Level->Hits)
	{
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
	if (!Level || !Level->bObstacles || RunRandom.FRand() >= Level->ObstacleChance)
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

	const float Dist = FMath::Min(MinDist + RunRandom.FRandRange(0.f, 15.f), MaxDist);
	Clock->SpawnObstacle(
		HandAngle + Direction * Dist,
		RunRandom.FRandRange(ObstacleMinRadiusFraction, ObstacleMaxRadiusFraction),
		Dist);
}

void ASpinGameMode::ReturnToTitle()
{
	State = ESpinState::WaitingToStart;
	bInvincible = false;
	InvincibilityEndTime = -1000.f;
	bHandBroke = false;
	ResetJam();
	if (Clock)
	{
		Clock->RetireObstacles();
		Clock->SetLiftHeld(false, false);
	}
}

// ---------------------------------------------------------------------------------------------
// Menus
// ---------------------------------------------------------------------------------------------

void ASpinGameMode::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr, EQuitPreference::Quit, false);
}

void ASpinGameMode::OpenModeSelect()
{
	// Runs are saved under a name, so the first PLAY on a fresh save asks for one.
	if (GetPlayerName().IsEmpty())
	{
		BeginNameEntry(true);
		return;
	}
	State = ESpinState::ModeSelect;
}

void ASpinGameMode::OpenSettings()
{
	InitSettingsFromCurrent();
	State = ESpinState::SettingsMenu;
}

void ASpinGameMode::OpenLeaderboard()
{
	State = ESpinState::Leaderboard;
}

void ASpinGameMode::CloseMenus()
{
	State = ESpinState::WaitingToStart;
}

void ASpinGameMode::HandleMenuAction(EMenuAction Action, int32 Index)
{
	switch (Action)
	{
	case EMenuAction::OpenProfiles:
		if (State == ESpinState::WaitingToStart)
		{
			OpenProfiles();
		}
		break;

	case EMenuAction::SelectProfile:
		if (State == ESpinState::ProfileSelect && SaveData && SaveData->Profiles.IsValidIndex(Index))
		{
			SaveData->ActiveProfile = Index;
			WriteSaveData();
			CloseMenus();
		}
		break;

	case EMenuAction::NewProfile:
		if (State == ESpinState::ProfileSelect)
		{
			BeginNameEntry(false);
		}
		break;

	case EMenuAction::DeleteProfile:
		if (State == ESpinState::ProfileSelect && SaveData && SaveData->Profiles.IsValidIndex(Index))
		{
			PendingDeleteProfile = Index;
		}
		break;

	case EMenuAction::ConfirmDeleteProfile:
		if (State == ESpinState::ProfileSelect && SaveData && SaveData->Profiles.IsValidIndex(PendingDeleteProfile))
		{
			SaveData->DeleteProfile(PendingDeleteProfile);
			WriteSaveData();
			PendingDeleteProfile = INDEX_NONE;
			LastRunLevelRank = 0; // board positions have shifted
			LastRunScoreRank = 0;
		}
		break;

	case EMenuAction::CancelDeleteProfile:
		PendingDeleteProfile = INDEX_NONE;
		break;

	case EMenuAction::ConfirmName:
		ConfirmName();
		break;

	case EMenuAction::CancelName:
		CancelName();
		break;

	case EMenuAction::Play:
		if (State == ESpinState::WaitingToStart)
		{
			OpenModeSelect();
		}
		break;

	case EMenuAction::OpenSettings:
		if (State == ESpinState::WaitingToStart)
		{
			OpenSettings();
		}
		break;

	case EMenuAction::OpenLeaderboard:
		if (State == ESpinState::WaitingToStart)
		{
			OpenLeaderboard();
		}
		break;

	case EMenuAction::Back:
		if (State == ESpinState::ModifierSelect)
		{
			OpenModeSelect();
		}
		else if (State == ESpinState::ModeSelect || State == ESpinState::SettingsMenu || State == ESpinState::Leaderboard
			|| State == ESpinState::ProfileSelect)
		{
			CloseMenus();
		}
		break;

	case EMenuAction::StartTutorial:
		if (State == ESpinState::ModeSelect)
		{
			StartTutorial();
		}
		break;

	case EMenuAction::StartEndless:
		if (State == ESpinState::ModifierSelect || State == ESpinState::TutorialComplete)
		{
			StartGame();
		}
		break;

	case EMenuAction::OpenModifiers:
		if (State == ESpinState::ModeSelect)
		{
			OpenModifiers();
		}
		break;

	case EMenuAction::ToggleModifier:
		if (State == ESpinState::ModifierSelect && Index >= 0 && Index < static_cast<int32>(ESpinModifier::Count))
		{
			SelectedModifiers ^= 1 << Index;
			if (SaveData)
			{
				SaveData->SelectedModifiers = SelectedModifiers;
				WriteSaveData();
			}
		}
		break;

	case EMenuAction::StartDaily:
		if (State == ESpinState::ModeSelect)
		{
			StartGame(true);
		}
		break;

	case EMenuAction::QualityPrev:
		QualityIndex = (QualityIndex + 3) % 4;
		ApplyQuality();
		break;

	case EMenuAction::QualityNext:
		QualityIndex = (QualityIndex + 1) % 4;
		ApplyQuality();
		break;

	case EMenuAction::ResolutionPrev:
		ResolutionIndex = (ResolutionIndex + 3) % 4;
		ApplyResolution();
		break;

	case EMenuAction::ResolutionNext:
		ResolutionIndex = (ResolutionIndex + 1) % 4;
		ApplyResolution();
		break;

	case EMenuAction::VolumeDown:
		VolumePercent = FMath::Clamp(VolumePercent - 10, 0, 100);
		ApplyVolume();
		break;

	case EMenuAction::VolumeUp:
		VolumePercent = FMath::Clamp(VolumePercent + 10, 0, 100);
		ApplyVolume();
		break;

	case EMenuAction::ToggleControllerIcons:
		bPlayStationIcons = !bPlayStationIcons;
		SetUsingGamepad(bUsingGamepad); // refresh InputStyle if a pad is in use
		if (SaveData)
		{
			SaveData->bPlayStationIcons = bPlayStationIcons;
			WriteSaveData();
		}
		break;

	case EMenuAction::RetryGame:
		if (IsGameOverMenuReady())
		{
			StartGame(bDailyRun);
		}
		break;

	case EMenuAction::ReturnToMainMenu:
		if (IsGameOverMenuReady() || State == ESpinState::TutorialComplete)
		{
			ReturnToTitle();
		}
		break;

	case EMenuAction::QuitGame:
		if (IsGameOverMenuReady())
		{
			QuitGame();
		}
		break;

	default:
		break;
	}
}

namespace
{
	// Shared by ApplyResolution / InitSettingsFromCurrent / GetResolutionLabel so they can't drift apart.
	const FIntPoint ResolutionPresets[] = { {1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440} };
	const TCHAR* QualityLabels[] = { TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("EPIC") };
}

void ASpinGameMode::InitSettingsFromCurrent()
{
	const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Settings)
	{
		return;
	}

	QualityIndex = FMath::Clamp(Settings->GetOverallScalabilityLevel(), 0, 3);

	// Match the current resolution to whichever preset is closest, so the label isn't a lie.
	const FIntPoint CurrentRes = Settings->GetScreenResolution();
	int32 BestIndex = ResolutionIndex;
	int32 BestDist = TNumericLimits<int32>::Max();
	for (int32 i = 0; i < UE_ARRAY_COUNT(ResolutionPresets); ++i)
	{
		const int32 Dist = FMath::Abs(ResolutionPresets[i].X - CurrentRes.X) + FMath::Abs(ResolutionPresets[i].Y - CurrentRes.Y);
		if (Dist < BestDist)
		{
			BestDist = Dist;
			BestIndex = i;
		}
	}
	ResolutionIndex = BestIndex;
}

void ASpinGameMode::ApplyQuality()
{
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Settings->SetOverallScalabilityLevel(QualityIndex);
		Settings->ApplySettings(false);
		Settings->SaveSettings();
	}
}

void ASpinGameMode::ApplyResolution()
{
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Settings->SetScreenResolution(ResolutionPresets[FMath::Clamp(ResolutionIndex, 0, (int32)UE_ARRAY_COUNT(ResolutionPresets) - 1)]);
		Settings->ApplySettings(false);
		Settings->SaveSettings();
	}
}

void ASpinGameMode::ApplyVolume()
{
	if (UWorld* World = GetWorld())
	{
		if (FAudioDeviceHandle AudioDevice = World->GetAudioDevice())
		{
			AudioDevice->SetTransientPrimaryVolume(VolumePercent / 100.f);
		}
	}
}

FString ASpinGameMode::GetQualityLabel() const
{
	return QualityLabels[FMath::Clamp(QualityIndex, 0, (int32)UE_ARRAY_COUNT(QualityLabels) - 1)];
}

FString ASpinGameMode::GetResolutionLabel() const
{
	const FIntPoint& Res = ResolutionPresets[FMath::Clamp(ResolutionIndex, 0, (int32)UE_ARRAY_COUNT(ResolutionPresets) - 1)];
	return FString::Printf(TEXT("%d x %d"), Res.X, Res.Y);
}

FString ASpinGameMode::GetControllerIconsLabel() const
{
	return bPlayStationIcons ? TEXT("PLAYSTATION") : TEXT("XBOX");
}

// ---------------------------------------------------------------------------------------------
// Button names
// ---------------------------------------------------------------------------------------------

void ASpinGameMode::SetUsingGamepad(bool bGamepad)
{
	bUsingGamepad = bGamepad;
	InputStyle = !bGamepad ? ESpinInputStyle::Keyboard
		: bPlayStationIcons ? ESpinInputStyle::PlayStation
		: ESpinInputStyle::Xbox;
}

FString ASpinGameMode::GetButtonKey(int32 ButtonIndex) const
{
	if (!Prompts.IsValidIndex(ButtonIndex))
	{
		return FString();
	}

	const FSpinPrompt& Prompt = Prompts[ButtonIndex];
	switch (InputStyle)
	{
	case ESpinInputStyle::Xbox:        return Prompt.XboxButton;
	case ESpinInputStyle::PlayStation: return Prompt.PlayStationButton;
	default:                           return Prompt.KeyboardKey;
	}
}

FString ASpinGameMode::GetLiftKey(bool bShort) const
{
	switch (InputStyle)
	{
	case ESpinInputStyle::Xbox:        return TEXT("RT");
	case ESpinInputStyle::PlayStation: return TEXT("R2");
	default:                           return bShort ? TEXT("LMB") : TEXT("LEFT CLICK");
	}
}

FString ASpinGameMode::ExpandKeys(const FString& Text) const
{
	FString Result = Text.Replace(TEXT("{LIFT}"), *GetLiftKey(false));
	for (int32 i = 0; i < Prompts.Num(); ++i)
	{
		Result.ReplaceInline(*FString::Printf(TEXT("{%d}"), i), *GetButtonKey(i));
	}
	return Result;
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
				ShowNotice(TEXT("Jammed! Press {0} again and again to fix it."));
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
			Fail(ESpinMistake::HandBroke);
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
		Next = RunRandom.RandRange(0, NumButtons - 1);

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
	float Offset = RunRandom.FRandRange(MinZoneOffset, MaxZoneOffset);

	float Chance = ObstacleChance;
	if (IsTutorial())
	{
		const FTutorialLevel* Level = GetTutorialLevel();
		Chance = Level ? Level->ObstacleChance : 0.f;
	}

	if (bAllowObstacle && Clock && ObstaclesEnabled() && RunRandom.FRand() < Chance)
	{
		// The zone must never start on top of the hand, so it can't wrap too close to where the hand is.
		const float MaxOffset = 360.f - ZoneHalfWidth - 12.f;

		// The obstacle sits far enough ahead that there is time to see it and lift the hand...
		float ObstacleDist = FMath::Max(CurrentSpeed * ObstacleWarningTime, MinObstacleDistance) + RunRandom.FRandRange(0.f, 15.f);
		ObstacleDist = FMath::Min(ObstacleDist, MaxOffset - ObstacleClearGap - ZoneHalfWidth);

		// ...and the zone sits beyond it, with a gap after it.
		Offset = FMath::Clamp(FMath::Max(Offset, ObstacleDist + ObstacleClearGap + ZoneHalfWidth), MinZoneOffset, MaxOffset);

		Clock->SpawnObstacle(
			HandAngle + Direction * ObstacleDist,
			RunRandom.FRandRange(ObstacleMinRadiusFraction, ObstacleMaxRadiusFraction),
			ObstacleDist);
	}

	ZoneCenter = FMath::Fmod(HandAngle + Direction * Offset + 360.f, 360.f);
}
