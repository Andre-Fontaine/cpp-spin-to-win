// Spin To Win - core game rules.

#include "SpinGameMode.h"
#include "SpinPlayerController.h"
#include "SpinHUD.h"
#include "SpinClock.h"
#include "SpinSaveGame.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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
		L.IntroLines.Add(TEXT("Press {0} when the hand is in the green zone."));
		L.IntroLines.Add(TEXT("Missed? It comes back around."));
		L.Instruction = TEXT("Press {0} in the green zone");
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
		L.Title = TEXT("LEVEL 2 - FASTER");
		L.IntroLines.Add(TEXT("Faster hand, smaller zones."));
		L.Instruction = TEXT("Press {0} in the green zone");
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
		L.Title = TEXT("LEVEL 3 - SECOND BUTTON");
		L.IntroLines.Add(TEXT("Green zone: {0}"));
		L.IntroLines.Add(TEXT("Blue zone: {1}"));
		L.Instruction = TEXT("Green {0}     Blue {1}");
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
		L.IntroLines.Add(TEXT("Both buttons, faster, smaller zones."));
		L.Instruction = TEXT("Green {0}     Blue {1}");
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
		L.IntroLines.Add(TEXT("Hold {LIFT} to lift the hand over posts."));
		L.IntroLines.Add(TEXT("Lifting costs points - only lift when needed."));
		L.IntroLines.Add(FString::Printf(TEXT("Hit a post: -1 life. Clear %d: +1 life."), ObstaclesPerLife));
		L.Instruction = TEXT("Hold {LIFT} to lift over posts");
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
		L.Title = TEXT("LEVEL 6 - ALL TOGETHER");
		L.IntroLines.Add(TEXT("Both buttons, plus posts."));
		L.Instruction = TEXT("Green {0}   Blue {1}   Lift {LIFT}");
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
		L.IntroLines.Add(TEXT("Sparks mean a jam is coming."));
		L.IntroLines.Add(TEXT("When it jams, mash {REPAIR} to fix it."));
		L.IntroLines.Add(TEXT("Too slow and the hand breaks."));
		L.IntroLines.Add(TEXT("Clear this stage and Endless begins."));
		L.Instruction = TEXT("Jammed? Mash {REPAIR}");
		L.Confirm = ETutorialConfirm::Repair;
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

	// Use the dial placed in the level (so its settings - camera, colours, sizes - can be tuned in the
	// editor). If there isn't one, spawn a default dial a little above the origin (clear of any floor
	// at Z=0). Either way its camera becomes the player's view.
	for (TActorIterator<ASpinClock> It(World); It; ++It)
	{
		Clock = *It;
		break;
	}
	if (!Clock)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Clock = World->SpawnActor<ASpinClock>(ASpinClock::StaticClass(), FTransform(FVector(0.f, 0.f, 100.f)), Params);
	}

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

	UpdateLights(Dt);

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

	// Just unpaused: everything holds still until the countdown runs out, then picks up exactly
	// where it stopped - so pausing never gets anyone out of trouble.
	if (IsResumeCountingDown())
	{
		ResumeTimeRemaining -= DeltaSeconds;
		if (ResumeTimeRemaining <= 0.f)
		{
			ResumeTimeRemaining = 0.f;
			ZoneShownTime = EventTime; // in a blackout, another look at the zone after the break
			if (!bInvincible && GetWorld())
			{
				InvincibilityEndTime = GetWorld()->GetTimeSeconds(); // the HUD's "GO!"
			}
		}
		return;
	}

	EventTime += Dt;

	// A jammed hand grinds along slowly, easing down when it jams and back up once it's fixed.
	JamSpeedFactor = FMath::FInterpTo(JamSpeedFactor, JamPhase == EJamPhase::Jammed ? JamSlowdown : 1.f, Dt, 5.f);
	HandAngle = FMath::Fmod(HandAngle + Direction * GetHandSpeed() * JamSpeedFactor * Dt + 360.f, 360.f);

	// Endless: a lifted hand costs points for as long as it's up. Fractions carry over between
	// frames so the drain stays steady at any frame rate.
	if (!IsTutorial() && Clock && Clock->IsLiftActive())
	{
		// Each Spring Hand still running cuts the drain again.
		const float Drain = FMath::Pow(SpringHandDrain, static_cast<float>(GetActiveUpgradeWindows(ESpinUpgrade::SpringHand)));
		LiftPointsDebt += LiftPointsPerSecond * Drain * Dt;
		const int32 Whole = FMath::FloorToInt(LiftPointsDebt);
		LiftPointsDebt -= Whole;
		const int32 OldPoints = Points;
		Points = FMath::Max(0, Points - Whole);
		PointsLostToLift += OldPoints - Points;
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
			// Ending with the hand already inside a zone gives no fair chance to hit it (presses were
			// ignored a moment ago), so that one zone can't count as a miss - it can still be hit.
			bZoneMissForgiven = IsHandInZone();
			ZoneShownTime = EventTime; // in a blackout, another look at the zone before play resumes
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
	if (Clock && Clock->IsHandHittingObstacle(HandAngle, &bLastObstacleHigh))
	{
		if (IsTutorial())
		{
			if (!bTutorialObstacleHit)
			{
				bTutorialObstacleHit = true;
				bZoneTainted = true;
				Shake(LifeLostShake);
				ShowNotice(TEXT("Hit a post! Hold {LIFT} to lift over it"));
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
	// (Except the zone the hand was already in when invincibility ended, see bZoneMissForgiven.)
	const bool bInZone = IsHandInZone();
	if (!bInZone && bZoneMissForgiven)
	{
		bZoneMissForgiven = false; // left the forgiven zone: from here on, misses count as usual
		bHandWasInZone = false;
	}
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
	// The menus (and the pause menu) never get here: on them the player controller turns the first
	// and second buttons into select / back for the highlighted item (see ASpinPlayerController).
	if (IsMenuScreen())
	{
		return;
	}

	switch (State)
	{
	case ESpinState::Playing:
		// Invincibility window after losing a life: nothing can be hit and
		// nothing can go wrong, so presses are completely ignored until the timer runs out.
		// The same goes for the countdown after unpausing.
		if (bInvincible || IsResumeCountingDown())
		{
			break;
		}

		// A jammed hand ignores the hit buttons - only the repair button works (HandleRepairPressed).
		if (JamPhase == EJamPhase::Jammed)
		{
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
		// Waits for the repair button instead, see HandleRepairPressed.
		break;

	case ESpinState::HighObstacleIntro:
		// Not the lift: the whole point is leaving it alone.
		if (ButtonIndex == 0)
		{
			ResumeAfterIntro();
		}
		break;

	case ESpinState::EndlessIntro:
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

	default:
		break;
	}
}

void ASpinGameMode::HandleRepairPressed()
{
	switch (State)
	{
	case ESpinState::Playing:
		// Only does anything while the hand is jammed; any other time it's harmlessly ignored.
		if (!bInvincible && !IsResumeCountingDown() && JamPhase == EJamPhase::Jammed)
		{
			RegisterJamPress();
		}
		break;

	case ESpinState::JamIntro:
		// Confirming the jam screen with the button that repairs a jam.
		ResumeAfterIntro();
		break;

	case ESpinState::TutorialIntro:
		if (const FTutorialLevel* Level = GetTutorialLevel(); Level && Level->Confirm == ETutorialConfirm::Repair)
		{
			BeginTutorialPlay();
		}
		break;

	default:
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
	if (State != ESpinState::Playing)
	{
		return;
	}
	++ObstaclesCleared;

	// Drought: posts no longer earn lives back.
	if (GetUpgradeStacks(ESpinUpgrade::Drought) > 0)
	{
		return;
	}

	// Every few posts cleared earn a life back. Nothing builds up while lives are full.
	if (Lives >= GetRunMaxLives())
	{
		return;
	}
	if (++ObstacleLifeProgress >= ObstaclesPerLife)
	{
		ObstacleLifeProgress = 0;
		++Lives;
		if (GetWorld())
		{
			LifeGainedTime = GetWorld()->GetTimeSeconds();
		}
	}
}

bool ASpinGameMode::HasSeenHighPosts() const
{
	const FSpinProfile* Profile = SaveData ? SaveData->GetActiveProfile() : nullptr;
	return Profile && Profile->bSeenHighPosts;
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
	return FMath::Abs(FMath::FindDeltaAngleDegrees(HandAngle, ZoneCenter)) <= GetZoneHalfWidth();
}

float ASpinGameMode::GetZoneHalfWidth() const
{
	if (IsTutorial())
	{
		return ZoneHalfWidth;
	}
	// Each Steady Grip still running widens the zones a little more.
	return ZoneHalfWidth * (1.f + SteadyGripWidthBonus * GetActiveUpgradeWindows(ESpinUpgrade::SteadyGrip));
}

float ASpinGameMode::GetHandSpeed() const
{
	// Each Brake still running slows the hand a little more (never below half speed).
	const float Slow = BrakeSlowdown * GetActiveUpgradeWindows(ESpinUpgrade::Brake);
	return CurrentSpeed * FMath::Max(0.5f, 1.f - Slow);
}

float ASpinGameMode::GetRunPerfectFraction() const
{
	// Sweet Spot widens PERFECT, but it never takes over most of GOOD.
	const float Wider = PerfectFraction * (1.f + SweetSpotBonus * GetUpgradeStacks(ESpinUpgrade::SweetSpot));
	return FMath::Min(Wider, GoodFraction * 0.9f);
}

int32 ASpinGameMode::GetUpgradeWindowLevels(ESpinUpgrade Upgrade) const
{
	switch (Upgrade)
	{
	case ESpinUpgrade::SteadyGrip:  return SteadyGripLevels;
	case ESpinUpgrade::Brake:       return BrakeLevels;
	case ESpinUpgrade::SpringHand:  return SpringHandLevels;
	case ESpinUpgrade::CoolRunning: return CoolRunningLevels;
	case ESpinUpgrade::ClearSkies:  return 1;
	default:                        return 0;
	}
}

int32 ASpinGameMode::GetActiveUpgradeWindows(ESpinUpgrade Upgrade) const
{
	const int32 Index = static_cast<int32>(Upgrade);
	if (IsTutorial() || !UpgradeWindows.IsValidIndex(Index))
	{
		return 0;
	}
	const int32 Level = GetLevel();
	int32 Active = 0;
	for (const int32 Until : UpgradeWindows[Index])
	{
		Active += Level <= Until ? 1 : 0;
	}
	return Active;
}

float ASpinGameMode::GetZoneBrightness() const
{
	return IsTutorial() ? 1.f : FMath::Pow(BlindFaithDim, static_cast<float>(GetUpgradeStacks(ESpinUpgrade::BlindFaith)));
}


bool ASpinGameMode::IsBlackoutDark() const
{
	return !IsTutorial() && LevelEvent == ESpinLevelEvent::Blackout && !bBlackoutBroken && EventTime >= BlackoutFlickerTime;
}

float ASpinGameMode::GetZoneVisibility() const
{
	if (!IsBlackoutDark())
	{
		return 1.f;
	}
	// A zone placed while the lights were still flickering gets its look once they're out.
	const float Since = EventTime - FMath::Max(ZoneShownTime, BlackoutFlickerTime);
	const float ShowTime = BlackoutZoneShowTime + AfterglowTime * GetUpgradeStacks(ESpinUpgrade::Afterglow);
	if (Since <= ShowTime)
	{
		return 1.f;
	}
	return FMath::Clamp(1.f - (Since - ShowTime) / BlackoutZoneFadeTime, 0.f, 1.f);
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
	return FMath::Clamp(static_cast<float>(JamPresses) / GetRunJamPressesRequired(), 0.f, 1.f);
}

float ASpinGameMode::GetJamTimeFraction() const
{
	return FMath::Clamp(1.f - JamTimer / FMath::Max(0.01f, GetRunJamTimeLimit()), 0.f, 1.f);
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
	return State == ESpinState::GameOver && GetWorld() && GetWorld()->GetTimeSeconds() - GameOverTime >= GameOverScreenDelay + RestartDelay;
}

float ASpinGameMode::GetGameOverScreenAlpha() const
{
	if (State != ESpinState::GameOver || !GetWorld())
	{
		return 1.f;
	}
	const float SinceShown = GetWorld()->GetTimeSeconds() - GameOverTime - GameOverScreenDelay;
	if (SinceShown <= 0.f)
	{
		return 0.f;
	}
	return GameOverFadeTime > 0.f ? FMath::SmoothStep(0.f, 1.f, SinceShown / GameOverFadeTime) : 1.f;
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
	const float Offset = FMath::Abs(FMath::FindDeltaAngleDegrees(HandAngle, ZoneCenter)) / FMath::Max(0.01f, GetZoneHalfWidth());

	if (Offset <= GetRunPerfectFraction())
	{
		return ESpinHitGrade::Perfect;
	}
	return Offset <= GoodFraction ? ESpinHitGrade::Good : ESpinHitGrade::Ok;
}

void ASpinGameMode::ScoreHit(ESpinHitGrade Grade, float PointsScale)
{
	// Tight Lines: only PERFECT counts - a GOOD is graded OK (and breaks the combo), but a PERFECT
	// pays far more.
	if (const int32 Tight = GetUpgradeStacks(ESpinUpgrade::TightLines))
	{
		if (Grade == ESpinHitGrade::Good)
		{
			Grade = ESpinHitGrade::Ok;
		}
		else if (Grade == ESpinHitGrade::Perfect)
		{
			PointsScale *= 1.f + TightLinesPerfectBonus * Tight;
		}
	}

	LastHitGrade = Grade;

	// PERFECT builds the streak, GOOD holds it, OK breaks it. All In builds it faster, but a GOOD
	// breaks it too.
	const int32 AllIn = GetUpgradeStacks(ESpinUpgrade::AllIn);
	if (Grade == ESpinHitGrade::Perfect)
	{
		++PerfectStreak;
		// Glass Ceiling caps it low (lower still with every stack, never below x2).
		int32 Cap = MaxMultiplier;
		if (const int32 Ceiling = GetUpgradeStacks(ESpinUpgrade::GlassCeiling))
		{
			Cap = FMath::Min(Cap, FMath::Max(2, GlassCeilingCap - (Ceiling - 1)));
		}
		ComboMultiplier = FMath::Min(Cap, 1 + PerfectStreak * (1 + AllIn) / FMath::Max(1, PerfectsPerMultiplier));
		LongestPerfectStreak = FMath::Max(LongestPerfectStreak, PerfectStreak);
		PeakMultiplier = FMath::Max(PeakMultiplier, ComboMultiplier);
	}
	else if (Grade == ESpinHitGrade::Ok || (Grade == ESpinHitGrade::Good && AllIn > 0))
	{
		BreakCombo();
	}

	const int32 BasePoints = (Grade == ESpinHitGrade::Perfect) ? PerfectPoints
		: (Grade == ESpinHitGrade::Good) ? GoodPoints
		: OkPoints;

	// Upgrades that add to the run's multiplier: the curses always, Glass Hand while it lasts, and
	// Momentum for every clean level. Double or Nothing doubles its level on top.
	const float CurseBonus = GetCursePointsBonus();
	float UpgradeMultiplier = 1.f + GetMomentumBonus() + CurseBonus;
	if (GlassHandHitsLeft > 0)
	{
		UpgradeMultiplier += GlassHandPointsBonus * GetUpgradeStacks(ESpinUpgrade::GlassHand);
	}
	// The curses' share of this hit, for the game-over screen (worked out before any doubling).
	const float CurseShare = CurseBonus / UpgradeMultiplier;
	const bool bDoubleOrNothing = DoubleOrNothingLevel > 0 && GetLevel() == DoubleOrNothingLevel && !bDoubleOrNothingBust;
	if (bDoubleOrNothing)
	{
		UpgradeMultiplier *= 2.f;
	}
	LastHitPoints = FMath::RoundToInt(BasePoints * PointsScale * RunPointsMultiplier * UpgradeMultiplier) * ComboMultiplier;
	if (DebtHitsLeft > 0)
	{
		--DebtHitsLeft;
		LastHitPoints /= 2;
	}
	if (IsBlackoutDark())
	{
		BlackoutPointsEarned += LastHitPoints;
	}
	if (bDoubleOrNothing)
	{
		DoubleOrNothingEarned += LastHitPoints;
	}
	CursePointsEarned += FMath::RoundToInt(LastHitPoints * CurseShare);

	// Glass Hand counts down its hits, then gives the miss lives back.
	if (GlassHandHitsLeft > 0 && --GlassHandHitsLeft == 0)
	{
		MissLives = FMath::Max(MissLives, FMath::Min(GlassHandSavedMissLives, GetRunMaxMissLives()));
		MissLifeGainedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		ShowNotice(TEXT("GLASS HAND over - your misses are back"));
	}
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
	// Combo Saver catches the break - as long as there's a streak to lose.
	if (ComboSaves > 0 && PerfectStreak > 0)
	{
		--ComboSaves;
		ComboSavedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		return;
	}

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
	if (SaveData && SaveData->SaveVersion < 1)
	{
		// Unlocks used to follow straight from the stats. Keep everything each player already had,
		// and limit them to one new unlock per run from here on.
		for (FSpinProfile& Profile : SaveData->Profiles)
		{
			while (!ClaimNextUnlock(Profile).IsEmpty())
			{
			}
		}
		SaveData->SaveVersion = USpinSaveGame::CurrentSaveVersion;
		WriteSaveData();
	}
	bPlayStationIcons = SaveData && SaveData->bPlayStationIcons;
	SelectedModifiers = SaveData ? SaveData->SelectedModifiers : 0;
	VolumePercent = SaveData ? SaveData->VolumePercent : 100;
	ApplyVolume();
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
	Run.Bonuses = RunBonuses;

	// Bonus slots before this run, to tell whether it opened one.
	const int32 OldSlots = GetBonusSlots();

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

	// Lifetime totals count every Endless run, daily ones included (AddRun already counted a
	// normal run as played).
	const int32 ProfileIndex = SaveData->FindProfile(Run.PlayerName);
	if (SaveData->Profiles.IsValidIndex(ProfileIndex))
	{
		FSpinProfile& Profile = SaveData->Profiles[ProfileIndex];
		Profile.ObstaclesCleared += ObstaclesCleared;
		Profile.JamsFixed += JamsFixed;

		// Curse stats, which unlock some modifiers and bonuses.
		int32 Curses = 0;
		for (int32 i = 0; i < UpgradeStacks.Num(); ++i)
		{
			Curses += GetUpgradeInfo(static_cast<ESpinUpgrade>(i)).Kind == ESpinUpgradeKind::Curse ? UpgradeStacks[i] : 0;
		}
		Profile.CursesTaken += Curses;
		Profile.CursePoints += CursePointsEarned;
		if (Curses >= 2)
		{
			Profile.BestCursedLevel = FMath::Max(Profile.BestCursedLevel, Run.Level);
		}
		if (bDailyRun)
		{
			++Profile.RunsPlayed;
		}

		// Only one modifier or bonus unlocks per run; anything else earned waits for later runs.
		const FString Claimed = ClaimNextUnlock(Profile);
		if (!Claimed.IsEmpty())
		{
			NewUnlocks.Add(Claimed);
		}
	}

	for (int32 Slot = OldSlots + 1; Slot <= GetBonusSlots(); ++Slot)
	{
		NewUnlocks.Add(FString::Printf(TEXT("BONUS SLOT %d"), Slot));
	}
	if (const int32 Waiting = GetPendingUnlockCount())
	{
		NewUnlocks.Add(FString::Printf(TEXT("%d MORE NEXT RUN%s"), Waiting, Waiting == 1 ? TEXT("") : TEXT("S")));
	}

	WriteSaveData();
}

// ---------------------------------------------------------------------------------------------
// Modifiers and the daily challenge
// ---------------------------------------------------------------------------------------------

const FSpinModifierInfo& ASpinGameMode::GetModifierInfo(ESpinModifier Modifier)
{
	// The multipliers roughly track how much harder each one makes a run. Each one unlocks once the
	// player has shown they can handle the plain game that far.
	static const FSpinModifierInfo Infos[] =
	{
		{ TEXT("FAST HAND"),  TEXT("The hand starts 35% faster."),                      1.3f,  TEXT("Reach level 5"),  ESpinUnlockStat::BestLevel, 5 },
		{ TEXT("TINY ZONES"), TEXT("Target zones are 30% narrower."),                  1.5f,  TEXT("Take 10 curses"), ESpinUnlockStat::CursesTaken, 10 },
		{ TEXT("WILD SPIN"),  TEXT("The hand only sometimes reverses after a hit."),    1.25f, TEXT("Reach level 8"),  ESpinUnlockStat::BestLevel, 8 },
		{ TEXT("EARLY JAMS"), TEXT("Jams from level 1, and half again as often."),      1.3f,  TEXT("Fix 15 jams"),    ESpinUnlockStat::JamsFixed, 15 },
		{ TEXT("ONE LIFE"),   TEXT("One life and one miss: any mistake ends the run."), 2.f,   TEXT("Reach level 15 with 2 curses"), ESpinUnlockStat::BestCursedLevel, 15 },
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

static int32 GetProfileStat(const FSpinProfile& Profile, ESpinUnlockStat Stat)
{
	switch (Stat)
	{
	case ESpinUnlockStat::BestLevel:        return Profile.BestLevel;
	case ESpinUnlockStat::ObstaclesCleared: return Profile.ObstaclesCleared;
	case ESpinUnlockStat::JamsFixed:        return Profile.JamsFixed;
	case ESpinUnlockStat::RunsPlayed:       return Profile.RunsPlayed;
	case ESpinUnlockStat::CursesTaken:      return Profile.CursesTaken;
	case ESpinUnlockStat::CursePoints:      return Profile.CursePoints;
	case ESpinUnlockStat::BestCursedLevel:  return Profile.BestCursedLevel;
	default:                                return 0;
	}
}

int32 ASpinGameMode::GetUnlockStat(ESpinUnlockStat Stat) const
{
	const FSpinProfile* Profile = SaveData ? SaveData->GetActiveProfile() : nullptr;
	return Profile ? GetProfileStat(*Profile, Stat) : 0;
}

FString ASpinGameMode::ClaimNextUnlock(FSpinProfile& Profile) const
{
	// Modifiers first, then bonuses, each in list order.
	for (int32 i = 0; i < static_cast<int32>(ESpinModifier::Count); ++i)
	{
		const FSpinModifierInfo& Info = GetModifierInfo(static_cast<ESpinModifier>(i));
		if (!(Profile.UnlockedModifiers & (1 << i)) && GetProfileStat(Profile, Info.Stat) >= Info.Target)
		{
			Profile.UnlockedModifiers |= 1 << i;
			return Info.Name;
		}
	}
	for (int32 i = 0; i < static_cast<int32>(ESpinBonus::Count); ++i)
	{
		const FSpinBonusInfo& Info = GetBonusInfo(static_cast<ESpinBonus>(i));
		if (!(Profile.UnlockedBonuses & (1 << i)) && GetProfileStat(Profile, Info.Stat) >= Info.Target)
		{
			Profile.UnlockedBonuses |= 1 << i;
			return Info.Name;
		}
	}
	return FString();
}

int32 ASpinGameMode::GetPendingUnlockCount() const
{
	return FMath::CountBits(static_cast<uint32>(GetEarnedModifiers() & ~GetUnlockedModifiers()))
		+ FMath::CountBits(static_cast<uint32>(GetEarnedBonuses() & ~GetUnlockedBonuses()));
}

void ASpinGameMode::GetModifierProgress(ESpinModifier Modifier, int32& OutCurrent, int32& OutTarget) const
{
	const FSpinModifierInfo& Info = GetModifierInfo(Modifier);
	OutTarget = Info.Target;
	OutCurrent = GetUnlockStat(Info.Stat);
}

bool ASpinGameMode::IsModifierEarned(ESpinModifier Modifier) const
{
	int32 Current = 0, Target = 0;
	GetModifierProgress(Modifier, Current, Target);
	return Current >= Target;
}

bool ASpinGameMode::IsModifierUnlocked(ESpinModifier Modifier) const
{
	return (GetUnlockedModifiers() & (1 << static_cast<int32>(Modifier))) != 0;
}

int32 ASpinGameMode::GetUnlockedModifiers() const
{
	const FSpinProfile* Profile = SaveData ? SaveData->GetActiveProfile() : nullptr;
	return Profile ? Profile->UnlockedModifiers : 0;
}

int32 ASpinGameMode::GetEarnedModifiers() const
{
	int32 Mask = 0;
	for (int32 i = 0; i < static_cast<int32>(ESpinModifier::Count); ++i)
	{
		if (IsModifierEarned(static_cast<ESpinModifier>(i)))
		{
			Mask |= 1 << i;
		}
	}
	return Mask;
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
	// Plating adds to the most there can be (and ONE LIFE still means one - plus any plating).
	return (HasModifier(ESpinModifier::OneLife) ? 1 : MaxLives) + GetUpgradeStacks(ESpinUpgrade::Plating);
}

int32 ASpinGameMode::GetRunMaxMissLives() const
{
	// ONE LIFE means one: it wins over EXTRA MISSES.
	if (HasModifier(ESpinModifier::OneLife))
	{
		return 1;
	}
	// Cursed Gold takes one off the most there can be (never below one).
	return FMath::Max(1, MaxMissLives + (HasBonus(ESpinBonus::ExtraMisses) ? 2 : 0) - GetUpgradeStacks(ESpinUpgrade::CursedGold));
}

float ASpinGameMode::GetRunStartSpeed() const
{
	return StartSpeed * (HasModifier(ESpinModifier::FastHand) ? 1.35f : 1.f);
}

float ASpinGameMode::GetRunStartZoneHalfWidth() const
{
	return StartZoneHalfWidth * (HasModifier(ESpinModifier::TinyZones) ? 0.7f : 1.f) * (HasBonus(ESpinBonus::WideOpen) ? 1.15f : 1.f);
}

float ASpinGameMode::GetRunMinZoneHalfWidth() const
{
	return MinZoneHalfWidth * (HasModifier(ESpinModifier::TinyZones) ? 0.7f : 1.f);
}

int32 ASpinGameMode::GetRunJamStartLevel() const
{
	if (HasModifier(ESpinModifier::EarlyJams))
	{
		return 1;
	}
	// The tutorial already taught jams, so a run carried on from it has them straight away.
	return bRunFromTutorial ? FMath::Min(JamStartLevel, TutorialHandoffLevel) : JamStartLevel;
}

float ASpinGameMode::GetRunJamChance() const
{
	return FMath::Min(1.f, JamChance * (HasModifier(ESpinModifier::EarlyJams) ? 1.5f : 1.f));
}

int32 ASpinGameMode::GetRunJamPressesRequired() const
{
	// Jam Session makes every fix a longer mash.
	return FMath::Max(1, JamPressesRequired - (HasBonus(ESpinBonus::SteadyHands) ? 2 : 0)
		+ JamSessionExtraPresses * GetUpgradeStacks(ESpinUpgrade::JamSession));
}

float ASpinGameMode::GetRunJamTimeLimit() const
{
	return JamTimeLimit * (HasBonus(ESpinBonus::SteadyHands) ? 1.5f : 1.f);
}

float ASpinGameMode::GetRunJamSparkTime() const
{
	return JamSparkTime * (HasBonus(ESpinBonus::EarlyWarning) ? 1.5f : 1.f);
}

float ASpinGameMode::GetRunInvincibilityDuration() const
{
	return InvincibilityDuration + (HasBonus(ESpinBonus::Breather) ? 2.f : 0.f);
}

// ---------------------------------------------------------------------------------------------
// Bonuses
// ---------------------------------------------------------------------------------------------

const FSpinBonusInfo& ASpinGameMode::GetBonusInfo(ESpinBonus Bonus)
{
	// Effects live in the GetRun* functions; keep the descriptions in step with them.
	static const FSpinBonusInfo Infos[] =
	{
		{ TEXT("WIDE OPEN"),    TEXT("Target zones start 15% wider."),                         TEXT("Reach level 10"),     ESpinUnlockStat::BestLevel,        10 },
		{ TEXT("EXTRA MISSES"), TEXT("Two more miss lives: five in all."),                     TEXT("Reach level 15"),     ESpinUnlockStat::BestLevel,        15 },
		{ TEXT("SECOND WIND"),  TEXT("Your first obstacle hit each run doesn't cost a life."), TEXT("Clear 50 obstacles"), ESpinUnlockStat::ObstaclesCleared, 50 },
		{ TEXT("STEADY HANDS"), TEXT("Jams take 2 fewer presses, with half again the time."),  TEXT("Fix 25 jams"),        ESpinUnlockStat::JamsFixed,        25 },
		{ TEXT("BREATHER"),     TEXT("Invincibility after a miss lasts 2 seconds longer."),    TEXT("Earn 25,000 points from curses"), ESpinUnlockStat::CursePoints, 25000 },
		{ TEXT("EARLY WARNING"), TEXT("Jam sparks last 50% longer before the hand jams."),     TEXT("Fix 10 jams"),        ESpinUnlockStat::JamsFixed,        10 },
	};
	static_assert(UE_ARRAY_COUNT(Infos) == static_cast<int32>(ESpinBonus::Count), "One entry per bonus");
	return Infos[FMath::Clamp(static_cast<int32>(Bonus), 0, static_cast<int32>(ESpinBonus::Count) - 1)];
}

FString ASpinGameMode::GetBonusNames(int32 Mask)
{
	TArray<FString> Names;
	for (int32 i = 0; i < static_cast<int32>(ESpinBonus::Count); ++i)
	{
		if (Mask & (1 << i))
		{
			Names.Add(GetBonusInfo(static_cast<ESpinBonus>(i)).Name);
		}
	}
	return FString::Join(Names, TEXT(" + "));
}

void ASpinGameMode::GetBonusProgress(ESpinBonus Bonus, int32& OutCurrent, int32& OutTarget) const
{
	const FSpinBonusInfo& Info = GetBonusInfo(Bonus);
	OutTarget = Info.Target;
	OutCurrent = GetUnlockStat(Info.Stat);
}

bool ASpinGameMode::IsBonusEarned(ESpinBonus Bonus) const
{
	int32 Current = 0, Target = 0;
	GetBonusProgress(Bonus, Current, Target);
	return Current >= Target;
}

bool ASpinGameMode::IsBonusUnlocked(ESpinBonus Bonus) const
{
	return (GetUnlockedBonuses() & (1 << static_cast<int32>(Bonus))) != 0;
}

int32 ASpinGameMode::GetUnlockedBonuses() const
{
	const FSpinProfile* Profile = SaveData ? SaveData->GetActiveProfile() : nullptr;
	return Profile ? Profile->UnlockedBonuses : 0;
}

int32 ASpinGameMode::GetEarnedBonuses() const
{
	int32 Mask = 0;
	for (int32 i = 0; i < static_cast<int32>(ESpinBonus::Count); ++i)
	{
		if (IsBonusEarned(static_cast<ESpinBonus>(i)))
		{
			Mask |= 1 << i;
		}
	}
	return Mask;
}

int32 ASpinGameMode::GetBonusSlots() const
{
	const FSpinProfile* Profile = SaveData ? SaveData->GetActiveProfile() : nullptr;
	const int32 Best = Profile ? Profile->BestLevel : 0;
	return 1 + (Best >= SecondSlotLevel ? 1 : 0) + (Best >= ThirdSlotLevel ? 1 : 0);
}

int32 ASpinGameMode::GetNextBonusSlotLevel() const
{
	const int32 Slots = GetBonusSlots();
	return Slots == 1 ? SecondSlotLevel : Slots == 2 ? ThirdSlotLevel : 0;
}

int32 ASpinGameMode::GetEquippedBonuses() const
{
	const FSpinProfile* Profile = SaveData ? SaveData->GetActiveProfile() : nullptr;
	return Profile ? Profile->EquippedBonuses & GetUnlockedBonuses() : 0;
}

bool ASpinGameMode::HasBonus(ESpinBonus Bonus) const
{
	return (RunBonuses & (1 << static_cast<int32>(Bonus))) != 0;
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

bool ASpinGameMode::IsMenuScreen() const
{
	if (IsPauseMenuOpen())
	{
		return true;
	}

	switch (State)
	{
	case ESpinState::WaitingToStart:
	case ESpinState::ModeSelect:
	case ESpinState::ModifierSelect:
	case ESpinState::SettingsMenu:
	case ESpinState::Leaderboard:
	case ESpinState::ProfileSelect:
	case ESpinState::NameEntry:
	case ESpinState::GameOver:
	case ESpinState::UpgradeSelect:
		return true;
	default:
		return false;
	}
}

void ASpinGameMode::HandleMenuBack()
{
	// From the pause menu, back is "resume" - pressing B / O out of habit mustn't throw the run away.
	if (IsPauseMenuOpen())
	{
		HandleMenuAction(EMenuAction::ResumeGame);
		return;
	}

	switch (State)
	{
	case ESpinState::ProfileSelect:
		// Backing out of "delete this player?" just cancels it.
		HandleMenuAction(PendingDeleteProfile != INDEX_NONE ? EMenuAction::CancelDeleteProfile : EMenuAction::Back);
		break;

	case ESpinState::ModeSelect:
	case ESpinState::ModifierSelect:
	case ESpinState::SettingsMenu:
	case ESpinState::Leaderboard:
		HandleMenuAction(EMenuAction::Back);
		break;

	case ESpinState::NameEntry:
		// Back erases what's been typed, one character at a time, then leaves.
		if (NameEntryText.IsEmpty())
		{
			CancelName();
		}
		else
		{
			EraseNameChar();
		}
		break;

	case ESpinState::GameOver:
		HandleMenuAction(EMenuAction::ReturnToMainMenu); // waits for the menu to be ready, like the button
		break;

	default:
		// The main menu is the top level - nothing to back out to.
		break;
	}
}

void ASpinGameMode::HandleStartButton()
{
	// On the screens right before a run, Start starts it; on name entry it's "done"; during a run it pauses.
	if (State == ESpinState::ModifierSelect)
	{
		StartGame();
		return;
	}
	if (State == ESpinState::NameEntry)
	{
		ConfirmName();
		return;
	}
	TogglePauseMenu();
}

void ASpinGameMode::StartGame(bool bDaily)
{
	Mode = ESpinMode::Endless;
	State = ESpinState::Playing;

	// The daily challenge is the same run for everyone today: a seed and a modifier picked from
	// the date. A normal run plays whatever was picked on the modifier screen, on a fresh seed.
	bDailyRun = bDaily;
	bRunFromTutorial = false;
	RunDayKey = GetTodayKey();
	RunModifiers = bDaily ? GetDailyModifiers() : SelectedModifiers & GetUnlockedModifiers(); // a locked one can't be played, even if saved as picked
	RunPointsMultiplier = GetModifiersMultiplier(RunModifiers);
	// Everyone plays the daily on the same terms, so no bonuses there.
	RunBonuses = bDaily ? 0 : GetEquippedBonuses();
	bSecondWindUsed = false;
	NewUnlocks.Reset();
	ResetUpgrades(); // before anything reads them (Plating raises the most lives)
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
	LongestPerfectStreak = 0;
	PeakMultiplier = 1;
	PointsLostToLift = 0;
	ObstaclesHit = 0;
	ObstaclesCleared = 0;
	JamsFixed = 0;

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
	ObstacleLifeProgress = 0;
	bForceHighPost = false;
	LifeLostTime = -1000.f;
	LifeGainedTime = -1000.f;
	bOutOfLives = false;
	MissLives = GetRunMaxMissLives();
	MissLifeLostTime = -1000.f;
	MissLifeGainedTime = -1000.f;
	bOutOfMissLives = false;
	// A few seconds of invincibility (with the big countdown) to get oriented before anything can
	// go wrong, same as resuming after a mistake.
	bInvincible = true;
	InvincibleTimeRemaining = GetRunInvincibilityDuration();
	InvincibilityEndTime = -1000.f;
	LastMistake = ESpinMistake::None;
	bHandBroke = false;
	bHandWasInZone = false;
	bZoneTainted = false;
	bTutorialObstacleHit = false;
	LiftPointsDebt = 0.f;
	ResetJam();
	SetLevelEvent(ESpinLevelEvent::None);
	PlaceNextZone();
}

void ASpinGameMode::ContinueFromTutorial()
{
	// The hand doesn't stop: keep where it is, and reverse it like any other hit.
	const float KeptAngle = HandAngle;
	const float KeptDirection = Direction;

	StartGame();

	// A plain run - no modifiers, bonuses or fast start, whatever is picked on the modifier screen.
	bRunFromTutorial = true;
	RunModifiers = 0;
	RunPointsMultiplier = 1.f;
	RunBonuses = 0;
	bFastStart = false;
	Lives = GetRunMaxLives();
	MissLives = GetRunMaxMissLives();

	HandAngle = KeptAngle;
	Direction = -KeptDirection;

	// Skip ahead to the handoff level with the same ramp the skipped hits would have applied
	// (including the reset when a button unlocks), so it plays exactly like getting there normally.
	Hits = 0;
	CurrentSpeed = GetRunStartSpeed();
	ZoneHalfWidth = GetRunStartZoneHalfWidth();
	while (GetLevel() < TutorialHandoffLevel)
	{
		const int32 OldLevel = GetLevel();
		++Hits;
		CurrentSpeed = FMath::Min(CurrentSpeed * SpeedMultiplier, MaxSpeed);
		ZoneHalfWidth = FMath::Max(GetRunMinZoneHalfWidth(), ZoneHalfWidth - ZoneShrinkPerHit);
		for (int32 i = 1; i < Prompts.Num(); ++i)
		{
			if (OldLevel < GetButtonUnlockLevel(i) && GetLevel() >= GetButtonUnlockLevel(i))
			{
				CurrentSpeed = GetRunStartSpeed();
				ZoneHalfWidth = GetRunStartZoneHalfWidth();
			}
		}
	}

	// No countdown and no obstacle on the first zone. Play pauses on a screen explaining what's
	// different now (lives and misses), then picks up right where the hand is.
	bInvincible = false;
	InvincibleTimeRemaining = 0.f;
	bHandWasInZone = false;
	PromptStreak = 1;
	PickNextPrompt();
	PlaceNextZone(false);
	NoticeTime = -1000.f;
	State = ESpinState::EndlessIntro;
}

void ASpinGameMode::OnHit()
{
	const int32 OldLevel = GetLevel();

	// A shortened (fast start) level packs HitsPerLevel hits' worth of ramp into fewer hits, so
	// each of its hits counts for that many normal ones - in speed, zone shrink, and points.
	const float RampSteps = static_cast<float>(FMath::Max(1, HitsPerLevel)) / GetHitsForLevel(OldLevel);

	// Grade the hit before anything moves. Hits made in the dark are worth more.
	const ESpinHitGrade Grade = GradeHit();
	float PointsScale = RampSteps * (IsBlackoutDark() ? GetBlackoutPointsMultiplier() : 1.f);

	// Twin Engine: the second of a pair, after a PERFECT first, landing a PERFECT too.
	const int32 TwinStacks = GetUpgradeStacks(ESpinUpgrade::TwinEngine);
	if (bPairFirstPerfect && Grade == ESpinHitGrade::Perfect && TwinStacks > 0)
	{
		PointsScale *= 1.f + TwinEnginePairBonus * TwinStacks;
		ShowNotice(TEXT("PERFECT PAIR!"));
	}
	bPairFirstPerfect = false;

	ScoreHit(Grade, PointsScale);

	++Hits;
	RegisterHit();

	// Every roll after this hit comes from the seed and the hit count alone, so the same seed
	// always plays out the same - however the run went up to here.
	RunRandom.Initialize(static_cast<int32>(HashCombine(static_cast<uint32>(RunSeed), static_cast<uint32>(Hits))));

	const int32 NewLevel = GetLevel();

	// The first zone of a pair: the hand carries straight on to the second one - no turn, no new
	// zone. (If this hit finished the level, the pair ends here and the level-up goes as normal.)
	if (bHasSecondZone && NewLevel == OldLevel)
	{
		Shake(HitShake);
		CurrentSpeed = FMath::Min(CurrentSpeed * FMath::Pow(SpeedMultiplier, RampSteps), MaxSpeed);
		ZoneHalfWidth = FMath::Max(GetRunMinZoneHalfWidth(), ZoneHalfWidth - ZoneShrinkPerHit * RampSteps);
		ZoneCenter = SecondZoneCenter;
		ActivePromptIndex = SecondPromptIndex;
		bHasSecondZone = false;
		bHandWasInZone = false;
		bPairFirstPerfect = LastHitGrade == ESpinHitGrade::Perfect;
		return;
	}

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
	// screen wins, then obstacles, then jams, then high posts.
	const bool bButtonIntro = bUnlocksButton && BestLevel < GetButtonUnlockLevel(UnlockedButton);
	const bool bObstacleIntro = !bButtonIntro && bUnlocksObstacles && BestLevel < ObstacleStartLevel;
	const bool bJamIntro = !bButtonIntro && !bObstacleIntro && bUnlocksJams && BestLevel < GetRunJamStartLevel();
	// High posts are explained once per player, whenever they first come up - even if that player's
	// best was already past them (they're newer than some saves).
	const bool bHighObstacleIntro = !bButtonIntro && !bObstacleIntro && !bJamIntro && NewLevel >= HighObstacleStartLevel && !HasSeenHighPosts();
	const bool bPauseScreen = bButtonIntro || bObstacleIntro || bJamIntro || bHighObstacleIntro;

	// A new level rolls its event (none on a level that opens with an explanation screen). The
	// previous level's event, if any, ends here.
	if (NewLevel > OldLevel)
	{
		SetLevelEvent(bPauseScreen ? ESpinLevelEvent::None : RollLevelEvent(NewLevel, LevelEvent));
	}

	// Momentum: a level cleared without losing a miss life adds to it.
	if (NewLevel > OldLevel)
	{
		if (!bMissLostThisLevel && GetUpgradeStacks(ESpinUpgrade::Momentum) > 0)
		{
			++MomentumLevels;
		}
		bMissLostThisLevel = false;
	}

	// Every few levels of a normal run, a pick of upgrades (after any explanation screen).
	const bool bUpgradeDue = !bDailyRun && UpgradeEveryLevels > 0 && NewLevel / UpgradeEveryLevels > OldLevel / UpgradeEveryLevels;

	// The normal "LEVEL N" banner is skipped when a pause screen takes over instead.
	if (NewLevel > OldLevel && GetWorld() && !bPauseScreen)
	{
		LevelUpTime = GetWorld()->GetTimeSeconds();
	}

	// Every MissRegenLevels levels, one lost miss life comes back.
	if (MissRegenLevels > 0 && NewLevel / MissRegenLevels > OldLevel / MissRegenLevels && MissLives < GetRunMaxMissLives())
	{
		++MissLives;
		if (GetWorld())
		{
			MissLifeGainedTime = GetWorld()->GetTimeSeconds();
		}
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
	if (!bPauseScreen && !bUpgradeDue && JamPhase == EJamPhase::None && GetActiveUpgradeWindows(ESpinUpgrade::CoolRunning) == 0
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
	else if (bHighObstacleIntro)
	{
		State = ESpinState::HighObstacleIntro;
		bForceHighPost = true;
		if (SaveData && SaveData->Profiles.IsValidIndex(SaveData->ActiveProfile))
		{
			SaveData->Profiles[SaveData->ActiveProfile].bSeenHighPosts = true;
			WriteSaveData();
		}
	}

	if (bUpgradeDue)
	{
		if (State == ESpinState::Playing)
		{
			OpenUpgradePick();
		}
		else
		{
			bUpgradePending = true; // after the explanation screen, see ResumeAfterIntro
		}
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
		Hints.Add(TEXT("Sparks mean a jam is coming - mash {REPAIR} to repair it"));
	}
	if (Level == HighObstacleStartLevel)
	{
		Titles.Add(TEXT("HIGH POSTS"));
		Hints.Add(TEXT("Purple posts float - don't lift, pass under them"));
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
	// Missing in the dark is free - no life, the combo stays - but it ends the blackout (and its
	// bonus): the lights come back on and the zone shows again. Posts and jams count as usual.
	if (IsBlackoutDark() && (Mistake == ESpinMistake::MissedZone || Mistake == ESpinMistake::WrongButton))
	{
		bBlackoutBroken = true;
		bHandWasInZone = false;
		ZoneShownTime = EventTime;
		Shake(HitShake);

		// Borrowed Time: the debt comes due - twice what this blackout earned.
		if (bBorrowedBlackout)
		{
			const int32 Lost = FMath::Min(Points, BlackoutPointsEarned * 2);
			Points -= Lost;
			ShowNotice(FString::Printf(TEXT("BORROWED TIME - lost %d points"), Lost));
		}
		else
		{
			ShowNotice(TEXT("Missed in the dark - the lights are back on"));
		}
		return;
	}

	// A mistake breaks up a pair: only the zone the hand was heading for is left.
	bHasSecondZone = false;
	bPairFirstPerfect = false;

	LastMistake = Mistake;
	Shake(LifeLostShake);
	bHandWasInZone = false;
	BreakCombo();

	if (Mistake == ESpinMistake::HitObstacle)
	{
		++ObstaclesHit;
	}

	// SECOND WIND: the run's first obstacle hit is still a mistake (the combo is gone), but it's
	// free - no life lost.
	const bool bSecondWind = Mistake == ESpinMistake::HitObstacle && HasBonus(ESpinBonus::SecondWind) && !bSecondWindUsed;
	if (bSecondWind)
	{
		bSecondWindUsed = true;
	}

	// Obstacle hits spend the regenerating "Lives" pool. Running that out costs a miss and fills it
	// back up, so only the "MissLives" pool, which everything else spends, can end the run.
	bool bLivesRanOut = false;
	if (!bSecondWind && Mistake == ESpinMistake::HitObstacle)
	{
		Lives = FMath::Max(0, Lives - 1);
		if (GetWorld())
		{
			LifeLostTime = GetWorld()->GetTimeSeconds();
		}
		bLivesRanOut = Lives <= 0;
	}

	if (bLivesRanOut || (!bSecondWind && Mistake != ESpinMistake::HitObstacle))
	{
		MissLives = FMath::Max(0, MissLives - 1);
		if (GetWorld())
		{
			MissLifeLostTime = GetWorld()->GetTimeSeconds();
		}
		OnMissLifeLost();

		if (MissLives <= 0)
		{
			(bLivesRanOut ? bOutOfLives : bOutOfMissLives) = true;
			if (Mistake == ESpinMistake::HandBroke)
			{
				bHandBroke = true;
			}
			EndGame();
			return;
		}

		if (bLivesRanOut)
		{
			Lives = GetRunMaxLives();
			ObstacleLifeProgress = 0;
		}

		// The jam that broke the hand is fixed on the spot - the player is not left looking at a
		// still-jammed hand.
		if (Mistake == ESpinMistake::HandBroke)
		{
			ResetJam();
		}

		// No pause: straight into the invincibility countdown to get back in rhythm. An obstacle hit
		// that only spent a life skips this - the obstacle is cleared below and play simply carries on.
		bInvincible = true;
		InvincibleTimeRemaining = GetRunInvincibilityDuration();
		InvincibilityEndTime = -1000.f;
	}

	switch (Mistake)
	{
	case ESpinMistake::MissedZone:  ShowNotice(TEXT("You missed the zone!")); break;
	case ESpinMistake::WrongButton: ShowNotice(TEXT("Wrong button, or too early!")); break;
	case ESpinMistake::HitObstacle:
		ShowNotice(bSecondWind ? TEXT("SECOND WIND - that hit was free!")
			: bLivesRanOut ? TEXT("Out of lives - that cost a miss!")
			: bLastObstacleHigh ? TEXT("Don't lift under a purple post!")
			: TEXT("You hit an obstacle!"));
		break;
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
	if (bUpgradePending)
	{
		OpenUpgradePick();
	}
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
	SetLevelEvent(ESpinLevelEvent::None); // the lights come back on for the game-over screen

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
	RunBonuses = 0;
	RunRandom.Initialize(FMath::Rand());
	Hits = 0;
	Points = 0;
	LastHitGrade = ESpinHitGrade::None;
	Lives = MaxLives;
	ObstacleLifeProgress = 0;
	LifeLostTime = -1000.f;
	LifeGainedTime = -1000.f;
	bOutOfLives = false;
	bHandBroke = false;
	NoticeTime = -1000.f;
	Direction = 1.f;
	ResetJam();
	ResetUpgrades();
	SetLevelEvent(ESpinLevelEvent::None);
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
		ShowNotice(TEXT("Too early!"));
		return;
	}

	if (ButtonIndex != ActivePromptIndex)
	{
		ShowNotice(FString::Printf(TEXT("Wrong button - this zone needs {%d}"), ActivePromptIndex));
		return;
	}

	if (bZoneTainted)
	{
		ShowNotice(TEXT("You hit a post - go around again"));
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
			ContinueFromTutorial();
		}
		return;
	}

	// Next target: the hand reverses and a new zone appears. The zone never changes until a hit succeeds.
	Direction = -Direction;
	bHandWasInZone = false;
	bZoneMissForgiven = false;
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
	ShowNotice(TEXT("Missed - try again"));
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
	ResumeTimeRemaining = 0.f;
	bInvincible = false;
	InvincibilityEndTime = -1000.f;
	bHandBroke = false;
	ResetJam();
	if (Clock)
	{
		Clock->SetMusicDucked(false);
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

	case EMenuAction::NameKey:
		// Only what a name may hold - the on-screen keyboard offers nothing else, but a stray index mustn't sneak in.
		if (Index > 0 && Index < 128 && (FChar::IsAlnum(static_cast<TCHAR>(Index)) || Index == TEXT(' ')))
		{
			TypeNameChar(FChar::ToUpper(static_cast<TCHAR>(Index)));
		}
		break;

	case EMenuAction::NameBackspace:
		EraseNameChar();
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
		if (State == ESpinState::ModifierSelect)
		{
			StartGame();
		}
		break;

	case EMenuAction::ToggleBonus:
		if (State == ESpinState::ModifierSelect && SaveData && SaveData->Profiles.IsValidIndex(SaveData->ActiveProfile)
			&& Index >= 0 && Index < static_cast<int32>(ESpinBonus::Count) && IsBonusUnlocked(static_cast<ESpinBonus>(Index)))
		{
			// Switching one off always works; switching one on needs a free slot.
			FSpinProfile& Profile = SaveData->Profiles[SaveData->ActiveProfile];
			const int32 Bit = 1 << Index;
			const int32 Equipped = GetEquippedBonuses();
			if (Equipped & Bit)
			{
				Profile.EquippedBonuses = Equipped & ~Bit;
			}
			else if (FMath::CountBits(static_cast<uint32>(Equipped)) < GetBonusSlots())
			{
				Profile.EquippedBonuses = Equipped | Bit;
			}
			WriteSaveData();
		}
		break;

	case EMenuAction::OpenModifiers:
		if (State == ESpinState::ModeSelect)
		{
			OpenModifiers();
		}
		break;

	case EMenuAction::ToggleModifier:
		if (State == ESpinState::ModifierSelect && Index >= 0 && Index < static_cast<int32>(ESpinModifier::Count)
			&& IsModifierUnlocked(static_cast<ESpinModifier>(Index)))
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
		SaveVolume();
		break;

	case EMenuAction::VolumeUp:
		VolumePercent = FMath::Clamp(VolumePercent + 10, 0, 100);
		ApplyVolume();
		SaveVolume();
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
		if (IsGameOverMenuReady())
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

	case EMenuAction::ResumeGame:
		if (IsPauseMenuOpen())
		{
			TogglePauseMenu();
		}
		break;

	case EMenuAction::PickUpgrade:
		if (State == ESpinState::UpgradeSelect && UpgradeOffer.IsValidIndex(Index)
			&& GetWorld() && GetWorld()->GetTimeSeconds() - UpgradePickOpenTime >= 0.6f)
		{
			// Back into the run, with a short countdown to find the hand again.
			State = ESpinState::Playing;
			ApplyUpgrade(UpgradeOffer[Index]);
			UpgradeOffer.Reset();
			bHandWasInZone = false;
			ResumeTimeRemaining = bInvincible ? 0.f : UpgradeResumeTime;
		}
		break;

	case EMenuAction::RestartRun:
		if (IsPauseMenuOpen())
		{
			UGameplayStatics::SetGamePaused(this, false);
			// Clear what the abandoned run left on the dial before the new one starts.
			ResumeTimeRemaining = 0.f;
			bInvincible = false;
			InvincibilityEndTime = -1000.f;
			if (Clock)
			{
				Clock->StartNextRunTrack();
				Clock->RetireObstacles();
				Clock->SetLiftHeld(false, false);
			}
			if (IsTutorial())
			{
				StartTutorial();
			}
			else
			{
				StartGame(bDailyRun);
			}
		}
		break;

	case EMenuAction::QuitRunToMenu:
		if (IsPauseMenuOpen())
		{
			UGameplayStatics::SetGamePaused(this, false);
			ReturnToTitle();
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

void ASpinGameMode::SaveVolume()
{
	if (SaveData)
	{
		SaveData->VolumePercent = VolumePercent;
		WriteSaveData();
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

FString ASpinGameMode::GetRepairKey() const
{
	switch (InputStyle)
	{
	case ESpinInputStyle::Xbox:        return TEXT("Y");
	case ESpinInputStyle::PlayStation: return TEXT("/\\");
	default:                           return TEXT("SPACE");
	}
}

FString ASpinGameMode::GetPauseKey() const
{
	switch (InputStyle)
	{
	case ESpinInputStyle::Xbox:        return TEXT("MENU");
	case ESpinInputStyle::PlayStation: return TEXT("OPTIONS");
	default:                           return TEXT("P");
	}
}

void ASpinGameMode::TogglePauseMenu()
{
	// Only a run in motion can be paused - the intro screens already wait for the player.
	if (State != ESpinState::Playing)
	{
		return;
	}

	const bool bPause = !IsPauseMenuOpen();
	if (bPause && Clock)
	{
		// Drop the hand: the lift button's release won't be seen while paused, so a held lift
		// would otherwise still be up (and costing points) after resuming.
		Clock->SetLiftHeld(false, false);
	}
	if (Clock)
	{
		Clock->SetMusicDucked(bPause);
	}
	if (!bPause)
	{
		// Unpausing counts down before play resumes - unless the invincibility countdown was already
		// running (it froze while paused and simply carries on), so two countdowns never stack up.
		ResumeTimeRemaining = bInvincible ? 0.f : ResumeCountdownDuration;
	}
	UGameplayStatics::SetGamePaused(this, bPause);
}

bool ASpinGameMode::IsPauseMenuOpen() const
{
	return State == ESpinState::Playing && UGameplayStatics::IsGamePaused(this);
}

bool ASpinGameMode::IsResumeCountingDown() const
{
	return ResumeTimeRemaining > 0.f;
}

bool ASpinGameMode::IsInRun() const
{
	switch (State)
	{
	case ESpinState::Playing:
	case ESpinState::NewButtonIntro:
	case ESpinState::ObstacleIntro:
	case ESpinState::JamIntro:
	case ESpinState::HighObstacleIntro:
	case ESpinState::TutorialIntro:
	case ESpinState::EndlessIntro:
	case ESpinState::UpgradeSelect:
		return true;
	default:
		return false;
	}
}

FString ASpinGameMode::ExpandKeys(const FString& Text) const
{
	FString Result = Text.Replace(TEXT("{LIFT}"), *GetLiftKey(false));
	Result.ReplaceInline(TEXT("{REPAIR}"), *GetRepairKey());
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
		ShowNotice(TEXT("Sparks! A jam is coming"));
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
		if (JamTimer >= GetRunJamSparkTime())
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
				ShowNotice(TEXT("Jammed! Mash {REPAIR}"));
			}
		}
		return;
	}

	// Jammed: out of time?
	if (JamTimer >= GetRunJamTimeLimit())
	{
		if (IsTutorial())
		{
			// No penalty in the tutorial: start the attempt over.
			JamTimer = 0.f;
			JamPresses = 0;
			ShowNotice(TEXT("Too slow - try again"));
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

	if (JamPresses >= GetRunJamPressesRequired())
	{
		FixJam();
	}
}

void ASpinGameMode::FixJam()
{
	if (!IsTutorial())
	{
		++JamsFixed;
		if (const int32 Session = GetUpgradeStacks(ESpinUpgrade::JamSession))
		{
			AwardBonusPoints(JamSessionPayout * Session, TEXT("JAM SESSION"));
		}
	}
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
		ShowNotice(TEXT("Fixed!"));
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
	// Old obstacles glitch away whenever a new target is placed.
	if (Clock)
	{
		Clock->RetireObstacles();
	}

	ZoneShownTime = EventTime;
	bHasSecondZone = false;

	// A double-zone level keeps posts out of the way: the pair is the challenge. Clear Skies does too.
	const bool bDouble = !IsTutorial() && LevelEvent == ESpinLevelEvent::DoubleZone;
	if (bDouble || GetActiveUpgradeWindows(ESpinUpgrade::ClearSkies) > 0)
	{
		bAllowObstacle = false;
	}

	// Place the zone ahead of the hand in its current direction of travel.
	float Offset = RunRandom.FRandRange(MinZoneOffset, MaxZoneOffset);
	const float HW = GetZoneHalfWidth();

	if (bDouble)
	{
		// The second zone sits a gap beyond the first. Pull the pair back if needed so the second one
		// still ends well before the hand's own position.
		const float Span = HW * 2.f + RunRandom.FRandRange(DoubleZoneMinGap, FMath::Max(DoubleZoneMinGap, DoubleZoneMaxGap));
		const float MaxFirst = 360.f - 15.f - HW - Span;
		Offset = FMath::Min(Offset, FMath::Max(MinZoneOffset, MaxFirst));
		SecondZoneCenter = FMath::Fmod(HandAngle + Direction * (Offset + Span) + 720.f, 360.f);
		bHasSecondZone = true;

		// Its own button (same no-long-repeats rule). The current zone keeps the one already picked.
		const int32 FirstPrompt = ActivePromptIndex;
		PickNextPrompt();
		SecondPromptIndex = ActivePromptIndex;
		ActivePromptIndex = FirstPrompt;
	}

	float Chance = ObstacleChance;
	if (IsTutorial())
	{
		const FTutorialLevel* Level = GetTutorialLevel();
		Chance = Level ? Level->ObstacleChance : 0.f;
	}

	if (bAllowObstacle && Clock && ObstaclesEnabled() && RunRandom.FRand() < Chance)
	{
		// The zone must never start on top of the hand, so it can't wrap too close to where the hand is.
		const float MaxOffset = 360.f - HW - 12.f;

		// The obstacle sits far enough ahead that there is time to see it and lift the hand...
		float ObstacleDist = FMath::Max(GetHandSpeed() * ObstacleWarningTime, MinObstacleDistance) + RunRandom.FRandRange(0.f, 15.f);
		ObstacleDist = FMath::Min(ObstacleDist, MaxOffset - ObstacleClearGap - HW);

		// Twin Posts: sometimes a low post with a high one right behind it - lift, then drop straight
		// away. Only when the pair (and the zone after it) fits on the dial.
		const int32 TwinStacks = IsTutorial() ? 0 : GetUpgradeStacks(ESpinUpgrade::TwinPosts);
		const float TwinGap = FMath::Max(GetHandSpeed() * TwinPostsGapTime, 25.f) + 10.f;
		const bool bTwin = TwinStacks > 0 && ObstacleDist + TwinGap + ObstacleClearGap + HW <= MaxOffset
			&& RunRandom.FRand() < FMath::Min(0.9f, TwinPostsChance * TwinStacks);
		const float LastPostDist = bTwin ? ObstacleDist + TwinGap : ObstacleDist;

		// ...and the zone sits beyond it, with a gap after it.
		Offset = FMath::Clamp(FMath::Max(Offset, LastPostDist + ObstacleClearGap + HW), MinZoneOffset, MaxOffset);

		const float Radius = RunRandom.FRandRange(ObstacleMinRadiusFraction, ObstacleMaxRadiusFraction);
		if (bTwin)
		{
			Clock->SpawnObstacle(HandAngle + Direction * ObstacleDist, Radius, ObstacleDist, false);
			Clock->SpawnObstacle(HandAngle + Direction * LastPostDist, Radius, LastPostDist, true, true);
		}
		else
		{
			// The roll happens either way, so a forced one doesn't change the daily's sequence. High Wire
			// brings high posts in from any level, and makes most posts high.
			const bool bHighWire = !IsTutorial() && GetUpgradeStacks(ESpinUpgrade::HighWire) > 0;
			bool bHigh = false;
			if (!IsTutorial() && (GetLevel() >= HighObstacleStartLevel || bHighWire))
			{
				bHigh = RunRandom.FRand() < (bHighWire ? FMath::Max(HighWireChance, HighObstacleChance) : HighObstacleChance) || bForceHighPost;
				bForceHighPost = false;
			}
			Clock->SpawnObstacle(HandAngle + Direction * ObstacleDist, Radius, ObstacleDist, bHigh);
		}
	}

	ZoneCenter = FMath::Fmod(HandAngle + Direction * Offset + 360.f, 360.f);

	// Haunted Dial: sometimes a fake zone too, anywhere clear of the real one (a pair already uses
	// the second zone, so none on double-zone levels). It's only a look - a press in it is simply
	// a press outside the real zone.
	bHasDecoyZone = false;
	const int32 Haunted = IsTutorial() ? 0 : GetUpgradeStacks(ESpinUpgrade::HauntedDial);
	if (Haunted > 0 && !bDouble && RunRandom.FRand() < FMath::Min(0.8f, HauntedDialChance * Haunted))
	{
		for (int32 Try = 0; Try < 8 && !bHasDecoyZone; ++Try)
		{
			const float DecoyOffset = RunRandom.FRandRange(MinZoneOffset * 0.5f, 360.f - HW - 12.f);
			const float Center = FMath::Fmod(HandAngle + Direction * DecoyOffset + 360.f, 360.f);
			if (FMath::Abs(FMath::FindDeltaAngleDegrees(Center, ZoneCenter)) >= HW * 2.f + 15.f)
			{
				bHasDecoyZone = true;
				DecoyZoneCenter = Center;
				DecoyPromptIndex = RunRandom.RandRange(0, GetNumActiveButtons() - 1);
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Level events
// ---------------------------------------------------------------------------------------------

ESpinLevelEvent ASpinGameMode::RollLevelEvent(int32 Level, ESpinLevelEvent PreviousEvent) const
{
	// Never two in a row.
	if (PreviousEvent != ESpinLevelEvent::None)
	{
		return ESpinLevelEvent::None;
	}

	// A stream of its own (seed and level only), so adding events never shifts the run's other rolls.
	const FRandomStream Roll(static_cast<int32>(HashCombine(static_cast<uint32>(RunSeed), 0xE7E00000u + static_cast<uint32>(Level))));
	const float R = Roll.FRand();

	// Lights Out and Twin Engine make their event more likely, with every stack.
	const float Blackout = FMath::Min(0.8f, BlackoutChance * FMath::Pow(EventUpgradeChanceScale, static_cast<float>(GetUpgradeStacks(ESpinUpgrade::LightsOut))));
	const float Double = FMath::Min(0.8f, DoubleZoneChance * FMath::Pow(EventUpgradeChanceScale, static_cast<float>(GetUpgradeStacks(ESpinUpgrade::TwinEngine))));
	if (R < Blackout)
	{
		return Level >= BlackoutStartLevel ? ESpinLevelEvent::Blackout : ESpinLevelEvent::None;
	}
	if (R < Blackout + Double && Level >= DoubleZoneStartLevel)
	{
		return ESpinLevelEvent::DoubleZone;
	}
	return ESpinLevelEvent::None;
}

void ASpinGameMode::SetLevelEvent(ESpinLevelEvent Event)
{
	LevelEvent = Event;
	EventTime = 0.f;
	ZoneShownTime = 0.f;
	bBlackoutBroken = false;
	bHasSecondZone = false;
	bHasDecoyZone = false;
	bPairFirstPerfect = false;
	bBorrowedBlackout = false;
	BlackoutPointsEarned = 0;
}

float ASpinGameMode::GetBlackoutPointsMultiplier() const
{
	const float Base = BlackoutPointsMultiplier + LightsOutMultiplierBonus * GetUpgradeStacks(ESpinUpgrade::LightsOut);
	return Base * (bBorrowedBlackout ? 2.f : 1.f);
}

// ---------------------------------------------------------------------------------------------
// Upgrades
// ---------------------------------------------------------------------------------------------

const FSpinUpgradeInfo& ASpinGameMode::GetUpgradeInfo(ESpinUpgrade Upgrade)
{
	// Effects live where each one applies; keep the descriptions in step with them.
	static const FSpinUpgradeInfo Infos[] =
	{
		{ TEXT("PATCH JOB"),     TEXT("One miss life back."),                                                ESpinUpgradeKind::Boon },
		{ TEXT("STEADY GRIP"),   TEXT("Zones 10% wider for the next 3 levels."),                             ESpinUpgradeKind::Boon },
		{ TEXT("AFTERGLOW"),     TEXT("In a blackout, each zone stays visible 1 second longer."),           ESpinUpgradeKind::Boon },
		{ TEXT("BRAKE"),         TEXT("The hand is 10% slower for the next 3 levels."),                     ESpinUpgradeKind::Boon },
		{ TEXT("COMBO SAVER"),   TEXT("Your next combo break is ignored."),                                 ESpinUpgradeKind::Boon },
		{ TEXT("SWEET SPOT"),    TEXT("The PERFECT band is 25% wider."),                                    ESpinUpgradeKind::Boon },
		{ TEXT("SPRING HAND"),   TEXT("Lifting the hand costs a quarter of the points for the next 3 levels."), ESpinUpgradeKind::Boon },
		{ TEXT("COOL RUNNING"),  TEXT("No jams for the next 3 levels."),                                    ESpinUpgradeKind::Boon },
		{ TEXT("PLATING"),       TEXT("+1 life now, and +1 to your most lives."),                           ESpinUpgradeKind::Boon },
		{ TEXT("CLEAR SKIES"),   TEXT("No posts this level."),                                              ESpinUpgradeKind::Boon },
		{ TEXT("TIGHT LINES"),   TEXT("PERFECT hits pay 3x - but GOOD hits count as OK."),                   ESpinUpgradeKind::Gamble },
		{ TEXT("LIGHTS OUT"),    TEXT("Blackouts come twice as often and pay +1x."),                        ESpinUpgradeKind::Gamble },
		{ TEXT("TWIN ENGINE"),   TEXT("Double zones come twice as often. Two PERFECTs on a pair pay 3x."),  ESpinUpgradeKind::Gamble },
		{ TEXT("ALL IN"),        TEXT("Your combo builds twice as fast - but anything short of PERFECT resets it."), ESpinUpgradeKind::Gamble },
		{ TEXT("GLASS HAND"),    TEXT("Double points for 10 hits - on a single miss life. Your misses come back after."), ESpinUpgradeKind::Gamble },
		{ TEXT("TWIN POSTS"),    TEXT("Some posts come as a pair: low, then high. Lift, then drop - clear both for 300."), ESpinUpgradeKind::Gamble },
		{ TEXT("JAM SESSION"),   TEXT("Jams take 3 more presses to fix - but each fix pays 300."),          ESpinUpgradeKind::Gamble },
		{ TEXT("HIGH WIRE"),     TEXT("Most posts are high posts (don't lift!). Each one you pass under pays 200."), ESpinUpgradeKind::Gamble },
		{ TEXT("DOUBLE OR NOTHING"), TEXT("This level's points are doubled - lose a miss life in it and you lose them all."), ESpinUpgradeKind::Gamble },
		{ TEXT("MOMENTUM"),      TEXT("+0.1x points for every level you clear without losing a miss life. Losing one resets it."), ESpinUpgradeKind::Gamble },
		{ TEXT("DEBT"),          TEXT("+1000 points now - but your next 10 hits score half."),              ESpinUpgradeKind::Curse },
		{ TEXT("BLIND FAITH"),   TEXT("Zones are 50% dimmer for the rest of the run. +0.3x points."),       ESpinUpgradeKind::Curse, 0.3f },
		{ TEXT("BORROWED TIME"), TEXT("This level is a blackout paying double - but a miss in the dark costs twice what it earned."), ESpinUpgradeKind::Curse },
		{ TEXT("BLOOD PACT"),    TEXT("Lose a miss life now. +0.5x points for the rest of the run."),       ESpinUpgradeKind::Curse, 0.5f },
		{ TEXT("CURSED GOLD"),   TEXT("+2000 points now - but one fewer miss life at most, for the rest of the run."), ESpinUpgradeKind::Curse },
		{ TEXT("HAUNTED DIAL"),  TEXT("Fake zones appear beside some real ones - watch for the flicker. +0.4x points."), ESpinUpgradeKind::Curse, 0.4f },
		{ TEXT("DROUGHT"),       TEXT("Clearing posts no longer earns lives back. +0.4x points."),          ESpinUpgradeKind::Curse, 0.4f },
		{ TEXT("GLASS CEILING"), TEXT("Your combo can't go above x3. +0.5x points."),                       ESpinUpgradeKind::Curse, 0.5f },
		{ TEXT("HEAVY HAND"),    TEXT("The hand lifts much slower - lift earlier for posts. +0.3x points."), ESpinUpgradeKind::Curse, 0.3f },
		{ TEXT("GHOST POSTS"),   TEXT("Posts stay invisible until they're close. +0.3x points."),           ESpinUpgradeKind::Curse, 0.3f },
	};
	static_assert(UE_ARRAY_COUNT(Infos) == static_cast<int32>(ESpinUpgrade::Count), "One entry per upgrade");
	return Infos[FMath::Clamp(static_cast<int32>(Upgrade), 0, static_cast<int32>(ESpinUpgrade::Count) - 1)];
}

int32 ASpinGameMode::GetUpgradeStacks(ESpinUpgrade Upgrade) const
{
	const int32 Index = static_cast<int32>(Upgrade);
	return UpgradeStacks.IsValidIndex(Index) ? UpgradeStacks[Index] : 0;
}

void ASpinGameMode::ResetUpgrades()
{
	UpgradeStacks.Init(0, static_cast<int32>(ESpinUpgrade::Count));
	UpgradeOffer.Reset();
	bUpgradePending = false;
	UpgradeWindows.Reset();
	UpgradeWindows.SetNum(static_cast<int32>(ESpinUpgrade::Count));
	DebtHitsLeft = 0;
	ComboSaves = 0;
	ComboSavedTime = -1000.f;
	GlassHandHitsLeft = 0;
	GlassHandSavedMissLives = 0;
	DoubleOrNothingLevel = 0;
	bDoubleOrNothingBust = false;
	DoubleOrNothingEarned = 0;
	MomentumLevels = 0;
	bMissLostThisLevel = false;
	CursePointsEarned = 0;
}

float ASpinGameMode::GetCursePointsBonus() const
{
	float Bonus = 0.f;
	for (int32 i = 0; i < UpgradeStacks.Num(); ++i)
	{
		Bonus += GetUpgradeInfo(static_cast<ESpinUpgrade>(i)).PointsBonus * UpgradeStacks[i];
	}
	return Bonus;
}

FString ASpinGameMode::GetCurseNames() const
{
	TArray<FString> Names;
	for (int32 i = 0; i < UpgradeStacks.Num(); ++i)
	{
		const FSpinUpgradeInfo& Info = GetUpgradeInfo(static_cast<ESpinUpgrade>(i));
		if (Info.Kind == ESpinUpgradeKind::Curse && UpgradeStacks[i] > 0)
		{
			Names.Add(UpgradeStacks[i] > 1 ? FString::Printf(TEXT("%s x%d"), Info.Name, UpgradeStacks[i]) : FString(Info.Name));
		}
	}
	return FString::Join(Names, TEXT(" + "));
}

float ASpinGameMode::GetLiftRiseScale() const
{
	return IsTutorial() ? 1.f : FMath::Max(0.05f, FMath::Pow(HeavyHandLiftScale, static_cast<float>(GetUpgradeStacks(ESpinUpgrade::HeavyHand))));
}

float ASpinGameMode::GetMomentumBonus() const
{
	return MomentumPerLevel * GetUpgradeStacks(ESpinUpgrade::Momentum) * MomentumLevels;
}

void ASpinGameMode::AwardBonusPoints(int32 Amount, const TCHAR* Label)
{
	if (Amount <= 0)
	{
		return;
	}
	Points += Amount;
	ShowNotice(FString::Printf(TEXT("%s  +%d"), Label, Amount));
}

void ASpinGameMode::OnHighObstaclePassed(bool bTwin)
{
	if (State != ESpinState::Playing || IsTutorial())
	{
		return;
	}
	// A twin pair is only cleared once its high post is behind the hand (a hit on either ends it).
	if (bTwin)
	{
		AwardBonusPoints(TwinPostsPayout * GetUpgradeStacks(ESpinUpgrade::TwinPosts), TEXT("TWIN POSTS"));
	}
	else if (const int32 Wire = GetUpgradeStacks(ESpinUpgrade::HighWire))
	{
		AwardBonusPoints(HighWirePayout * Wire, TEXT("HIGH WIRE"));
	}
}

void ASpinGameMode::OnMissLifeLost()
{
	bMissLostThisLevel = true;

	// Momentum starts over.
	MomentumLevels = 0;

	// Double or Nothing: losing a miss life in its level takes back everything that level earned.
	if (DoubleOrNothingLevel > 0 && GetLevel() == DoubleOrNothingLevel && !bDoubleOrNothingBust)
	{
		bDoubleOrNothingBust = true;
		const int32 Lost = FMath::Min(Points, DoubleOrNothingEarned);
		Points -= Lost;
		DoubleOrNothingEarned = 0;
		ShowNotice(FString::Printf(TEXT("DOUBLE OR NOTHING - lost %d points"), Lost));
	}
}

void ASpinGameMode::OpenUpgradePick()
{
	bUpgradePending = false;

	// Fully random - not from the run's seed - from every upgrade that would do something now.
	TArray<ESpinUpgrade> Pool;
	for (int32 i = 0; i < static_cast<int32>(ESpinUpgrade::Count); ++i)
	{
		const ESpinUpgrade Upgrade = static_cast<ESpinUpgrade>(i);
		if (Upgrade == ESpinUpgrade::PatchJob && MissLives >= GetRunMaxMissLives())
		{
			continue; // nothing to patch
		}
		if ((Upgrade == ESpinUpgrade::BloodPact && MissLives <= 1) || (Upgrade == ESpinUpgrade::CursedGold && GetRunMaxMissLives() <= 1))
		{
			continue; // nothing left to give up
		}
		Pool.Add(Upgrade);
	}

	// One card of each kind, always in the same order: boon, gamble, curse.
	UpgradeOffer.Reset();
	for (const ESpinUpgradeKind Kind : { ESpinUpgradeKind::Boon, ESpinUpgradeKind::Gamble, ESpinUpgradeKind::Curse })
	{
		TArray<ESpinUpgrade> OfKind = Pool.FilterByPredicate([Kind](ESpinUpgrade Upgrade) { return GetUpgradeInfo(Upgrade).Kind == Kind; });
		if (OfKind.Num() > 0)
		{
			UpgradeOffer.Add(OfKind[FMath::RandRange(0, OfKind.Num() - 1)]);
		}
	}

	if (Clock)
	{
		Clock->SetLiftHeld(false, false); // a held lift won't see its release on the pick screen
	}
	UpgradePickOpenTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	State = ESpinState::UpgradeSelect;
}

void ASpinGameMode::ApplyUpgrade(ESpinUpgrade Upgrade)
{
	const int32 Index = static_cast<int32>(Upgrade);
	if (!UpgradeStacks.IsValidIndex(Index))
	{
		return;
	}
	++UpgradeStacks[Index];

	// Level-limited ones run from the level being started.
	if (const int32 Levels = GetUpgradeWindowLevels(Upgrade); Levels > 0 && UpgradeWindows.IsValidIndex(Index))
	{
		UpgradeWindows[Index].Add(GetLevel() + Levels - 1);
	}

	switch (Upgrade)
	{
	case ESpinUpgrade::PatchJob:
		if (MissLives < GetRunMaxMissLives())
		{
			++MissLives;
			MissLifeGainedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		}
		break;
	case ESpinUpgrade::ComboSaver:
		++ComboSaves;
		break;
	case ESpinUpgrade::GlassHand:
		// Down to one miss life for a while; the rest come back when it's over (see ScoreHit).
		if (GlassHandHitsLeft == 0)
		{
			GlassHandSavedMissLives = MissLives;
		}
		GlassHandHitsLeft += GlassHandHits;
		MissLives = FMath::Min(MissLives, 1);
		break;
	case ESpinUpgrade::DoubleOrNothing:
		DoubleOrNothingLevel = GetLevel();
		bDoubleOrNothingBust = false;
		DoubleOrNothingEarned = 0;
		break;
	case ESpinUpgrade::BloodPact:
		// Given up, not lost to a mistake - so Momentum and Double or Nothing don't count it.
		if (MissLives > 1)
		{
			--MissLives;
			MissLifeLostTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		}
		break;
	case ESpinUpgrade::CursedGold:
		Points += CursedGoldPayout;
		CursePointsEarned += CursedGoldPayout;
		MissLives = FMath::Min(MissLives, GetRunMaxMissLives()); // the most just went down by one
		break;
	case ESpinUpgrade::Plating:
		++Lives; // the most went up by one too (see GetRunMaxLives)
		LifeGainedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		break;
	case ESpinUpgrade::CoolRunning:
		if (JamPhase == EJamPhase::Sparking)
		{
			ResetJam(); // a jam that was about to happen doesn't
		}
		break;
	case ESpinUpgrade::ClearSkies:
		if (Clock)
		{
			Clock->RetireObstacles(); // the post already placed for this zone goes too
		}
		break;
	case ESpinUpgrade::Debt:
		Points += DebtPayout;
		CursePointsEarned += DebtPayout;
		DebtHitsLeft += DebtHits;
		break;
	case ESpinUpgrade::BorrowedTime:
		// This level becomes a blackout (whatever it rolled), paying double.
		SetLevelEvent(ESpinLevelEvent::Blackout);
		bBorrowedBlackout = true;
		break;
	default:
		break; // the rest act wherever they apply
	}
}

void ASpinGameMode::UpdateLights(float DeltaSeconds)
{
	const bool bBlackout = State == ESpinState::Playing && !IsTutorial() && LevelEvent == ESpinLevelEvent::Blackout && !bBlackoutBroken;
	if (!bBlackout)
	{
		// Back on, quickly but not instantly.
		LightsLevel = FMath::FInterpConstantTo(LightsLevel, 1.f, DeltaSeconds, 1.f / BlackoutRestoreTime);
		return;
	}

	if (EventTime >= BlackoutFlickerTime)
	{
		LightsLevel = BlackoutLightLevel;
		return;
	}

	// The flicker: short random drop-outs that come more often, until the lights die for good.
	// Seeded, so every light in the world (and every replay of the daily) flickers the same.
	const float T = EventTime / BlackoutFlickerTime;
	const int32 Slot = FMath::FloorToInt(EventTime * 14.f);
	const FRandomStream Noise(static_cast<int32>(HashCombine(static_cast<uint32>(RunSeed), 0xF11C0000u + static_cast<uint32>(Slot))));
	const bool bOn = T < 0.85f && Noise.FRand() < FMath::Lerp(0.8f, 0.2f, T);
	LightsLevel = bOn ? 1.f : FMath::Lerp(BlackoutLightLevel, 1.f, 0.12f);
}
