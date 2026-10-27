// Spin To Win - core game rules.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SpinGameMode.generated.h"

class ASpinClock;

UENUM(BlueprintType)
enum class ESpinState : uint8
{
	/** Main menu: PLAY / SETTINGS. */
	WaitingToStart,
	/** PLAY was chosen: pick TUTORIAL or ENDLESS. */
	ModeSelect,
	/** SETTINGS was chosen: graphics quality, resolution, and volume. */
	SettingsMenu,
	Playing,
	GameOver,
	/** Endless: paused on the "new button unlocked" screen until the player presses the new button. */
	NewButtonIntro,
	/** Endless: paused on the "obstacles" screen until the player holds the lift button. */
	ObstacleIntro,
	/** Endless: paused after any life-costing mistake (missed zone, wrong button, obstacle hit,
	 *  or a jam that broke the hand), until the player confirms to continue. */
	MissPause,
	/** Tutorial: paused on a level's explanation screen until the player presses its confirm button. */
	TutorialIntro,
	/** Tutorial finished. */
	TutorialComplete
};

/** What kind of mistake triggered the life-lost pause screen (ESpinState::MissPause), purely so
 *  the HUD can show the right message and the right life pool. */
UENUM(BlueprintType)
enum class ESpinMistake : uint8
{
	None,
	/** The hand left the target zone without a press. */
	MissedZone,
	/** A button was pressed too early, or the wrong one. */
	WrongButton,
	/** The hand touched an obstacle without being lifted over it. */
	HitObstacle,
	/** A jam was not fixed before the hand broke. */
	HandBroke
};

/** An action a HUD menu button can trigger when clicked (or confirmed by keyboard/gamepad). */
UENUM(BlueprintType)
enum class EMenuAction : uint8
{
	None,
	Play,
	OpenSettings,
	Back,
	StartTutorial,
	StartEndless,
	QualityPrev,
	QualityNext,
	ResolutionPrev,
	ResolutionNext,
	VolumeDown,
	VolumeUp,
	ContinueAfterMiss,
};

UENUM(BlueprintType)
enum class ESpinMode : uint8
{
	/** The 7 guided levels. Nothing can be failed. */
	Tutorial,
	/** The real game. Mistakes cost lives or end the run. */
	Endless
};

UENUM(BlueprintType)
enum class EJamPhase : uint8
{
	None,
	/** Sparks at the centre, the hand still works normally. */
	Sparking,
	/** The hand is broken: it keeps spinning, ignores everything, and must be fixed by mashing a button. */
	Jammed
};

/** Which input confirms a tutorial level's explanation screen. */
UENUM(BlueprintType)
enum class ETutorialConfirm : uint8
{
	FirstButton,
	SecondButton,
	Lift
};

/** One button the player can be asked to press. Index in the Prompts array = button index. */
USTRUCT(BlueprintType)
struct FSpinPrompt
{
	GENERATED_BODY()

	/** Short text drawn on the target zone, e.g. "W". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spin")
	FString Label;

	/** Longer text for the on-screen hint, e.g. "W / A / X". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spin")
	FString Hint;

	/** Colour of the target zone when this button is required. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spin")
	FLinearColor Color = FLinearColor(0.1f, 0.8f, 0.2f);
};

/** One tutorial level. Edit the list in SpinGameMode.cpp (BuildTutorialLevels) to retune the tutorial. */
USTRUCT(BlueprintType)
struct FTutorialLevel
{
	GENERATED_BODY()

	/** Big heading on the explanation screen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial")
	FString Title;

	/** Lines of explanation on the screen shown before the level starts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial")
	TArray<FString> IntroLines;

	/** One short reminder shown at the top while playing the level. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial")
	FString Instruction;

	/** What the player does to dismiss the explanation screen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial")
	ETutorialConfirm Confirm = ETutorialConfirm::FirstButton;

	/** Successful hits needed to finish the level. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial")
	int32 Hits = 10;

	/** Hand speed in degrees per second. It does not change during the level. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial")
	float Speed = 70.f;

	/** Half-width of the target zone in degrees. It does not change during the level. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial")
	float ZoneHalfWidth = 45.f;

	/** How many buttons are in play (1 or 2). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial")
	int32 NumButtons = 1;

	/** Which button the first zone of the level needs (use 1 to showcase a new second button). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial")
	int32 FirstPrompt = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial")
	bool bObstacles = false;

	/** Chance (0..1) that a zone comes with an obstacle, when obstacles are on. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial")
	float ObstacleChance = 0.7f;

	/** A jam starts (sparks) right after the player reaches each of these hit counts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial")
	TArray<int32> JamAtHits;
};

/**
 * Owns all the rules for the spinner game.
 * Angles are in degrees: 0 = 12 o'clock, positive = clockwise.
 *
 * Two modes:
 *  - Tutorial: 7 short, unfailable levels, each with fixed speed and zone size.
 *  - Endless: every HitsPerLevel hits moves you up a level (cumulative counter), difficulty ramps with every
 *    hit, mistakes cost lives or end the run.
 */
UCLASS()
class SPINTOWIN_API ASpinGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASpinGameMode();

	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Called by the player controller. ButtonIndex 0 = first button, 1 = second button. */
	void HandleButtonPressed(int32 ButtonIndex);

	/** Called by the player controller when the lift button is pressed or released. */
	void SetLiftHeld(bool bHeld);

	/** Called by the clock when the hand has hopped over an obstacle: gives a life back. */
	void OnObstacleCleared();

	/** Called by the player controller for a clicked (or keyboard-confirmed) menu button. */
	void HandleMenuAction(EMenuAction Action);

	FString GetQualityLabel() const;
	FString GetResolutionLabel() const;

	// ---------------- Tutorial ----------------

	/** The tutorial levels, built in BuildTutorialLevels(). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tutorial")
	TArray<FTutorialLevel> TutorialLevels;

	/** How long a tutorial message ("Try again!") stays on screen, in seconds. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tutorial")
	float NoticeDuration = 2.5f;

	// ---------------- Jam ----------------

	/** Seconds of sparks before the hand jams. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Jam")
	float JamSparkTime = 3.f;

	/** Presses of the first button needed to fix a jam. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Jam", meta = (ClampMin = "1"))
	int32 JamPressesRequired = 5;

	/** Seconds the player has to fix a jam before the hand breaks (Endless). The tutorial just restarts the attempt. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Jam")
	float JamTimeLimit = 3.f;

	/** Endless: jams can start from this level on. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Jam", meta = (ClampMin = "1"))
	int32 JamStartLevel = 7;

	/** Endless: chance (0..1) that a successful hit is followed by a jam. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Jam", meta = (ClampMin = "0", ClampMax = "1"))
	float JamChance = 0.12f;

	// ---------------- Levels (Endless) ----------------

	/** Hits needed to clear each level. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Levels", meta = (ClampMin = "1"))
	int32 HitsPerLevel = 10;

	/** The level at which the second button is introduced. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Levels", meta = (ClampMin = "1"))
	int32 SecondButtonLevel = 5;

	/** How long the "LEVEL N" banner stays on screen, in seconds. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Levels")
	float LevelBannerDuration = 1.5f;

	/** The buttons in the game. Index 0 is always available; index 1 unlocks at SecondButtonLevel. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Levels")
	TArray<FSpinPrompt> Prompts;

	/** Once two buttons are in play, the same button can come up at most this many times in a row. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Levels", meta = (ClampMin = "1"))
	int32 MaxSameButtonInRow = 3;

	// ---------------- Obstacles ----------------

	/** Endless: the level at which obstacles (and the lift) are introduced.
	 *  If this equals SecondButtonLevel, the new-button screen is shown and the obstacle screen is skipped. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Obstacles", meta = (ClampMin = "1"))
	int32 ObstacleStartLevel = 2;

	/** Endless: chance (0..1) that a given zone comes with an obstacle in front of it. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Obstacles", meta = (ClampMin = "0", ClampMax = "1"))
	float ObstacleChance = 0.6f;

	/** The obstacle is placed this many seconds of hand travel ahead of the hand, so there is time to react. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Obstacles")
	float ObstacleWarningTime = 0.7f;

	/** Closest an obstacle can be to the hand, in degrees. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Obstacles")
	float MinObstacleDistance = 35.f;

	/** Gap in degrees between the obstacle and the edge of the target zone. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Obstacles")
	float ObstacleClearGap = 25.f;

	/** Obstacles sit between these fractions of the dial radius. Kept close to the rim and away
	 *  from the centre pillar; the upper bound is capped so the obstacle mesh doesn't poke past the rim. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Obstacles")
	float ObstacleMinRadiusFraction = 0.78f;

	UPROPERTY(EditDefaultsOnly, Category = "Spin|Obstacles")
	float ObstacleMaxRadiusFraction = 0.88f;

	/** Lives the player starts a run with. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Obstacles", meta = (ClampMin = "1"))
	int32 MaxLives = 5;

	/** Camera shake when a life is lost. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Obstacles", meta = (ClampMin = "0", ClampMax = "1"))
	float LifeLostShake = 0.35f;

	// ---------------- Misses (Endless) ----------------

	/** Separate, non-regenerating lives for missed target zones. Endless only - the tutorial can
	 *  never be failed. Once these run out, the next miss ends the run. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Misses", meta = (ClampMin = "1"))
	int32 MaxMissLives = 3;

	// ---------------- Tuning (Endless) ----------------

	/** Hand speed at the start of a run, in degrees per second. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float StartSpeed = 85.f;

	/** Hand speed will never exceed this. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float MaxSpeed = 400.f;

	/** Speed is multiplied by this after every successful hit. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float SpeedMultiplier = 1.012f;

	/** Half-width of the target zone at the start, in degrees. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float StartZoneHalfWidth = 32.f;

	/** The target zone never gets narrower than this (half-width, degrees). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float MinZoneHalfWidth = 12.f;

	/** How much the zone half-width shrinks after each hit. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float ZoneShrinkPerHit = 0.18f;

	/** Next zone appears at least this many degrees ahead of the hand. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float MinZoneOffset = 70.f;

	/** Next zone appears at most this many degrees ahead of the hand (obstacles can push it further). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float MaxZoneOffset = 200.f;

	/** Seconds after game over before a button press restarts (avoids accidental restarts). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float RestartDelay = 0.6f;

	/** Seconds of invincibility after confirming a life-lost pause screen - obstacles, zone
	 *  misses, jams, and button presses are all ignored until this runs out. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning", meta = (ClampMin = "0"))
	float InvincibilityDuration = 5.f;

	// ---------------- Feel (camera shake strength, 0..1) ----------------

	/** Small shake on every successful hit. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Feel", meta = (ClampMin = "0", ClampMax = "1"))
	float HitShake = 0.15f;

	/** Medium shake on a level-up. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Feel", meta = (ClampMin = "0", ClampMax = "1"))
	float LevelUpShake = 0.4f;

	/** Big shake when the run ends. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Feel", meta = (ClampMin = "0", ClampMax = "1"))
	float GameOverShake = 1.0f;

	// ---------------- State (read by the HUD) ----------------

	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	ESpinState State = ESpinState::WaitingToStart;

	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	ESpinMode Mode = ESpinMode::Endless;

	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float HandAngle = 0.f;

	/** +1 = clockwise, -1 = counter-clockwise. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float Direction = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float CurrentSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float ZoneCenter = 90.f;

	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float ZoneHalfWidth = 32.f;

	/** Total hits this Endless run (cumulative across levels). */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 Score = 0;

	/** Best score this session. Saved leaderboard comes in a later stage. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 HighScore = 0;

	/** Which button (index into Prompts) the current zone requires. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 ActivePromptIndex = 0;

	/** World time of the last level-up, used for the banner. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float LevelUpTime = -1000.f;

	/** Lives left this run. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 Lives = 5;

	/** World time of the last life lost, used for HUD feedback. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float LifeLostTime = -1000.f;

	/** World time of the last life gained, used for HUD feedback. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float LifeGainedTime = -1000.f;

	/** True if the run ended because the player ran out of lives. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	bool bOutOfLives = false;

	/** Miss lives left this run (Endless only). These never regenerate. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 MissLives = 3;

	/** World time of the last missed zone, used for HUD feedback. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float MissLifeLostTime = -1000.f;

	/** True if the run ended because the player missed too many target zones. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	bool bOutOfMissLives = false;

	/** True during the invincibility window after confirming a life-lost pause screen (see
	 *  InvincibilityDuration): zones, obstacles, jams, and button presses are all ignored, giving
	 *  the player a few seconds to get re-oriented with the hand before play resumes for real. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	bool bInvincible = false;

	/** Seconds left in the current invincibility window (see bInvincible). */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float InvincibleTimeRemaining = 0.f;

	/** Which kind of mistake caused the current (or most recent) life-lost pause screen. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	ESpinMistake LastMistake = ESpinMistake::None;

	/** True if the run ended because a jam was not fixed in time. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	bool bHandBroke = false;

	/** Index into TutorialLevels of the level being played. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 TutorialIndex = 0;

	/** Successful hits so far in the current tutorial level. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 TutorialHits = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	EJamPhase JamPhase = EJamPhase::None;

	/** Seconds spent in the current jam phase. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float JamTimer = 0.f;

	/** Presses made so far towards fixing the current jam. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 JamPresses = 0;

	/** The latest on-screen message and when it appeared. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	FString NoticeText;

	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float NoticeTime = -1000.f;

	// ---------------- Settings (read by the HUD's Settings screen) ----------------

	/** 0=Low, 1=Medium, 2=High, 3=Epic. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|Settings")
	int32 QualityIndex = 2;

	/** Index into a fixed list of common resolutions. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|Settings")
	int32 ResolutionIndex = 2;

	/** Master volume, 0-100. Applies immediately but isn't saved between sessions - there's no
	 *  audio content in the project yet for a saved value to matter to. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|Settings")
	int32 VolumePercent = 100;

	bool IsTutorial() const;

	/** The tutorial level being played, or null. */
	const FTutorialLevel* GetTutorialLevel() const;

	/** True while the hand is inside the target zone. */
	bool IsHandInZone() const;

	/** Current Endless level, starting at 1. */
	int32 GetLevel() const;

	/** Total hits needed to finish the current Endless level (10, 20, 30, ...). */
	int32 GetLevelTarget() const;

	/** How many buttons are in play right now. */
	int32 GetNumActiveButtons() const;

	/** True when obstacles (and the lift) are part of the current level. */
	bool ObstaclesEnabled() const;

	/** True for a short time after an Endless level-up. */
	bool IsShowingLevelBanner() const;

	/** True for a short time after ShowNotice. */
	bool IsShowingNotice() const;

	/** 0..1 progress towards fixing the current jam. */
	float GetJamProgress() const;

	/** 1..0 fraction of the jam time limit that is left. */
	float GetJamTimeFraction() const;

	/** The in-world dial, spawned when play starts. */
	ASpinClock* GetClock() const;

private:
	void BuildTutorialLevels();
	void Shake(float Intensity);
	void ShowNotice(const FString& Text);

	// Endless
	void StartGame();
	void OnHit();
	void OnZoneMissed();
	void FailAndPause(ESpinMistake Mistake);
	void ResumeAfterMiss();
	void EndGame();
	void ResumeAfterIntro();
	void LoseLife();

	// Menus
	void OpenModeSelect();
	void OpenSettings();
	void CloseMenus();
	void InitSettingsFromCurrent();
	void ApplyQuality();
	void ApplyResolution();
	void ApplyVolume();

	// Tutorial
	void StartTutorial();
	void BeginTutorialLevel(int32 Index);
	void BeginTutorialPlay();
	void HandleTutorialPress(int32 ButtonIndex);
	void OnTutorialHit();
	void OnTutorialMiss();
	void RespawnObstacleForCurrentZone();
	void ReturnToTitle();

	// Jam
	void StartJamSparks();
	void UpdateJam(float DeltaSeconds);
	void RegisterJamPress();
	void FixJam();
	void ResetJam();

	// Shared
	void PickNextPrompt();
	void PlaceNextZone(bool bAllowObstacle = true);

	UPROPERTY()
	TObjectPtr<ASpinClock> Clock;

	bool bHandWasInZone = false;
	float GameOverTime = 0.f;

	/** How many zones in a row have needed the current button. */
	int32 PromptStreak = 1;

	/** Tutorial: the hand touched an obstacle on this lap, so the zone does not count until it goes around again. */
	bool bZoneTainted = false;

	/** Tutorial: stops the "you hit an obstacle" message from repeating every frame. */
	bool bTutorialObstacleHit = false;
};
