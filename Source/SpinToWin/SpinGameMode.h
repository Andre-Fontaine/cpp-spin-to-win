// Spin To Win - core game rules.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SpinGameMode.generated.h"

class ASpinClock;
class USpinSaveGame;

UENUM(BlueprintType)
enum class ESpinState : uint8
{
	/** Main menu: PLAY / SETTINGS. */
	WaitingToStart,
	/** PLAY was chosen: pick TUTORIAL or ENDLESS. */
	ModeSelect,
	/** SETTINGS was chosen: graphics quality, resolution, and volume. */
	SettingsMenu,
	/** LEADERBOARD was chosen: the saved best runs, by level and by score. */
	Leaderboard,
	/** "Who's playing?": pick, add, or delete a player profile. */
	ProfileSelect,
	/** Typing a new player's name (keyboard). */
	NameEntry,
	Playing,
	GameOver,
	/** Endless: paused on the "new button unlocked" screen until the player presses the new button. */
	NewButtonIntro,
	/** Endless: paused on the "obstacles" screen until the player holds the lift button. */
	ObstacleIntro,
	/** Endless: paused on the "jams" screen (first time jams can happen) until the player presses the first button. */
	JamIntro,
	/** Tutorial: paused on a level's explanation screen until the player presses its confirm button. */
	TutorialIntro,
	/** Tutorial finished. */
	TutorialComplete
};

/** What kind of mistake last cost a life, so the right message and feedback can be shown. */
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

/** How close to the centre of the zone a hit landed. Decides its points and the combo. */
UENUM(BlueprintType)
enum class ESpinHitGrade : uint8
{
	None,
	/** Near the edge of the zone: few points, and the combo resets. */
	Ok,
	/** Reasonably central: decent points, the combo holds but doesn't grow. */
	Good,
	/** Dead centre: full points, and builds the combo. */
	Perfect
};

/** An action a HUD menu button can trigger when clicked (or confirmed by keyboard/gamepad). */
UENUM(BlueprintType)
enum class EMenuAction : uint8
{
	None,
	Play,
	OpenSettings,
	OpenLeaderboard,
	Back,
	StartTutorial,
	StartEndless,
	QualityPrev,
	QualityNext,
	ResolutionPrev,
	ResolutionNext,
	VolumeDown,
	VolumeUp,
	/** Switches the gamepad prompts between Xbox and PlayStation names. */
	ToggleControllerIcons,
	RetryGame,
	ReturnToMainMenu,
	QuitGame,
	OpenProfiles,
	/** Index = which profile. */
	SelectProfile,
	NewProfile,
	/** Index = which profile. Asks for confirmation first. */
	DeleteProfile,
	ConfirmDeleteProfile,
	CancelDeleteProfile,
	ConfirmName,
	CancelName,
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

/** Which device's button names the on-screen prompts show. */
UENUM(BlueprintType)
enum class ESpinInputStyle : uint8
{
	Keyboard,
	Xbox,
	PlayStation
};

/** One button the player can be asked to press. Index in the Prompts array = button index. */
USTRUCT(BlueprintType)
struct FSpinPrompt
{
	GENERATED_BODY()

	/** What the button is called on each device, e.g. "W", "A", "X". The HUD shows whichever
	 *  matches the input the player is using (see ASpinGameMode::GetButtonKey). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spin")
	FString KeyboardKey;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spin")
	FString XboxButton;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spin")
	FString PlayStationButton;

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

	/** How many buttons are in play (1 to 3). */
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
 *    hit, mistakes cost lives or end the run. Hits drive the level; points (precision x combo) are the
 *    score. Finished runs go on a saved local leaderboard, ranked separately by level and by points.
 */
UCLASS()
class SPINTOWIN_API ASpinGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASpinGameMode();

	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Called by the player controller. ButtonIndex 0 = first button, 1 = second, 2 = third. */
	void HandleButtonPressed(int32 ButtonIndex);

	/** Called by the player controller when the lift button is pressed or released. */
	void SetLiftHeld(bool bHeld);

	/** Called by the clock when the hand has hopped over an obstacle: gives a life back. */
	void OnObstacleCleared();

	/** Called by the player controller for a clicked (or keyboard-confirmed) menu button. Index is
	 *  for actions on one item of a list (e.g. which profile), INDEX_NONE otherwise. */
	void HandleMenuAction(EMenuAction Action, int32 Index = INDEX_NONE);

	/** Name entry (ESpinState::NameEntry), fed by the player controller's raw keyboard input. */
	void TypeNameChar(TCHAR Char);
	void EraseNameChar();
	void ConfirmName();
	void CancelName();

	/** Name of whoever is playing, or empty if no profile has been picked/created yet. */
	FString GetPlayerName() const;

	FString GetQualityLabel() const;
	FString GetResolutionLabel() const;
	FString GetControllerIconsLabel() const;

	// ---------------- Button names ----------------

	/** Called by the player controller whenever the player switches between keyboard/mouse and a gamepad. */
	void SetUsingGamepad(bool bGamepad);

	/** A button's name on the input the player is using right now, e.g. "W", "A" or "X". */
	FString GetButtonKey(int32 ButtonIndex) const;

	/** The lift's name on the input in use. Short for key caps ("LMB"), long for sentences ("LEFT CLICK"). */
	FString GetLiftKey(bool bShort = true) const;

	/** Replaces button tokens in text meant for the screen with the names for the input in use:
	 *  {0} {1} {2} = the buttons, {LIFT} = the lift. Text that names buttons is written with these
	 *  tokens so it always matches whatever the player is holding. */
	FString ExpandKeys(const FString& Text) const;

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

	// ---------------- Fast start (Endless) ----------------

	/** Once the saved best run has reached this level, the opening levels of every run go by
	 *  quicker, so returning players don't have to grind through the slow start each time. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|FastStart", meta = (ClampMin = "1"))
	int32 FastStartUnlockLevel = 5;

	/** How many opening levels are shortened on a fast start. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|FastStart", meta = (ClampMin = "0"))
	int32 FastStartLevels = 3;

	/** Hits needed per shortened level. Speed, zone shrink, and points per hit are all scaled up
	 *  to match, so a run arrives at the end of the fast start exactly as hard (and with comparable
	 *  points) as a normal one - only quicker. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|FastStart", meta = (ClampMin = "1"))
	int32 FastStartHitsPerLevel = 4;

	// ---------------- Scoring (Endless) ----------------

	/** A hit within this fraction of the zone's half-width from its centre is PERFECT. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Scoring", meta = (ClampMin = "0", ClampMax = "1"))
	float PerfectFraction = 0.3f;

	/** A hit within this fraction of the zone's half-width from its centre is GOOD (else OK). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Scoring", meta = (ClampMin = "0", ClampMax = "1"))
	float GoodFraction = 0.65f;

	/** Base points per hit grade, before the combo multiplier. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Scoring")
	int32 PerfectPoints = 100;

	UPROPERTY(EditDefaultsOnly, Category = "Spin|Scoring")
	int32 GoodPoints = 50;

	UPROPERTY(EditDefaultsOnly, Category = "Spin|Scoring")
	int32 OkPoints = 20;

	/** Every this many PERFECT hits in a row adds +1 to the combo multiplier. A GOOD hit holds the
	 *  streak; an OK hit or any mistake resets it. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Scoring", meta = (ClampMin = "1"))
	int32 PerfectsPerMultiplier = 5;

	UPROPERTY(EditDefaultsOnly, Category = "Spin|Scoring", meta = (ClampMin = "1"))
	int32 MaxMultiplier = 5;

	/** How many runs each leaderboard keeps. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Scoring", meta = (ClampMin = "1"))
	int32 LeaderboardSize = 10;

	/** The level at which the second button is introduced. Its explanation screen (like the
	 *  obstacle and jam ones) only shows until the saved best run has reached that level. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Levels", meta = (ClampMin = "1"))
	int32 SecondButtonLevel = 4;

	/** The level at which the third button is introduced (same rules as SecondButtonLevel). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Levels", meta = (ClampMin = "1"))
	int32 ThirdButtonLevel = 6;

	/** How long the "LEVEL N" banner stays on screen, in seconds. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Levels")
	float LevelBannerDuration = 1.5f;

	/** The buttons in the game. Index 0 is always available; index 1 unlocks at SecondButtonLevel
	 *  and index 2 at ThirdButtonLevel. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Levels")
	TArray<FSpinPrompt> Prompts;

	/** Once several buttons are in play, the same button can come up at most this many times in a row. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Levels", meta = (ClampMin = "1"))
	int32 MaxSameButtonInRow = 3;

	// ---------------- Obstacles ----------------

	/** Endless: the level at which obstacles (and the lift) are introduced.
	 *  If a button also unlocks on this level, the new-button screen is shown and the obstacle screen is skipped. */
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

	/** Endless: points lost per second while the hand is lifted. The score never drops below 0. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Obstacles", meta = (ClampMin = "0"))
	float LiftPointsPerSecond = 50.f;

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

	/** Seconds after game over before the game-over menu accepts input (avoids an accidental retry
	 *  from a press that was meant for the hand). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float RestartDelay = 1.f;

	/** Seconds of invincibility (with a countdown) after losing a miss life - obstacles, zone
	 *  misses, jams, and button presses are all ignored until this runs out. Obstacle hits don't
	 *  trigger it. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning", meta = (ClampMin = "0"))
	float InvincibilityDuration = 5.f;

	// ---------------- Feel (camera shake strength, 0..1) ----------------

	/** Small shake on every successful hit. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Feel", meta = (ClampMin = "0", ClampMax = "1"))
	float HitShake = 0.15f;

	/** Big shake when the run ends. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Feel", meta = (ClampMin = "0", ClampMax = "1"))
	float GameOverShake = 1.0f;

	/** How long the light pulse after a successful hit takes to fade out, in seconds. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Feel", meta = (ClampMin = "0.01"))
	float HitPulseDuration = 0.45f;

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

	/** Total hits this Endless run (cumulative across levels). Drives the level. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 Hits = 0;

	/** Points this Endless run: each hit's grade points times the combo multiplier. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 Points = 0;

	/** Current combo multiplier (1..MaxMultiplier). */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 ComboMultiplier = 1;

	/** PERFECT hits in a row (GOOD hits hold it, OK hits and mistakes reset it). */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 PerfectStreak = 0;

	/** Grade and points of the most recent hit (and LastHitTime), for the HUD's popup. In the
	 *  tutorial the grade is still shown, but no points are scored. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	ESpinHitGrade LastHitGrade = ESpinHitGrade::None;

	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 LastHitPoints = 0;

	/** World time a combo above x1 was lost, for the HUD. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float ComboLostTime = -1000.f;

	/** The player's personal bests as they stood when this run started - what the run is chasing. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 BestLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 BestPoints = 0;

	/** World time this run's points first passed BestPoints (or -1000), for the HUD's flash. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float PassedBestPointsTime = -1000.f;

	/** True if this run shortened its opening levels (see FastStartUnlockLevel). */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	bool bFastStart = false;

	/** Name entry: what's been typed so far, and why the last confirm was refused (if it was). */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	FString NameEntryText;

	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	FString NameEntryError;

	/** Profile screen: the profile waiting for "delete? yes / no", or INDEX_NONE. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 PendingDeleteProfile = INDEX_NONE;

	/** 1-based rank the last finished run got on each leaderboard, or 0 if it didn't make it. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 LastRunLevelRank = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 LastRunScoreRank = 0;

	/** Which button (index into Prompts) the current zone requires. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 ActivePromptIndex = 0;

	/** The button the new-button screen (ESpinState::NewButtonIntro) is introducing. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 NewButtonIndex = 1;

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

	/** True during the invincibility window after losing a miss life (see
	 *  InvincibilityDuration): zones, obstacles, jams, and button presses are all ignored, giving
	 *  the player a few seconds to get re-oriented with the hand before play resumes for real. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	bool bInvincible = false;

	/** Seconds left in the current invincibility window (see bInvincible). */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float InvincibleTimeRemaining = 0.f;

	/** World time the last invincibility window ran out, used for the HUD's "GO!". */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float InvincibilityEndTime = -1000.f;

	/** World time of the last successful hit (Endless or tutorial), used for the light pulse. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float LastHitTime = -1000.f;

	/** Which kind of mistake most recently cost a life. */
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

	/** Gamepad prompts use PlayStation names instead of Xbox ones. Windows can't reliably tell the
	 *  two pads apart, so this is a setting (saved with the profiles) rather than detected. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|Settings")
	bool bPlayStationIcons = false;

	/** The input the player used last, which decides the names shown for buttons. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	ESpinInputStyle InputStyle = ESpinInputStyle::Keyboard;

	bool IsTutorial() const;

	/** The tutorial level being played, or null. */
	const FTutorialLevel* GetTutorialLevel() const;

	/** True while the hand is inside the target zone. */
	bool IsHandInZone() const;

	/** Current Endless level, starting at 1. */
	int32 GetLevel() const;

	/** Total hits needed to finish the current Endless level (10, 20, 30, ... - or fewer per
	 *  level during a fast start). */
	int32 GetLevelTarget() const;

	/** Hits needed to clear one particular Endless level. */
	int32 GetHitsForLevel(int32 Level) const;

	/** Endless level at which a button (index into Prompts) unlocks. Button 0 is there from level 1. */
	int32 GetButtonUnlockLevel(int32 ButtonIndex) const;

	/** What unlocks at an Endless level (obstacles, new buttons, jams): a short title and a
	 *  one-line how-to, joined if several land on the same level. False if nothing unlocks there. */
	bool GetUnlockAtLevel(int32 Level, FString& OutTitle, FString& OutHint) const;

	/** The saved leaderboard (never null after StartPlay). */
	const USpinSaveGame* GetSaveData() const;

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

	/** 1 right after a successful hit, fading to 0 over HitPulseDuration. Drives the light pulse. */
	float GetHitPulse() const;

	/** True once the game-over menu has been up long enough (RestartDelay) to accept input. */
	bool IsGameOverMenuReady() const;

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
	/** A life-costing mistake: spends a life from the right pool, then either ends the run or
	 *  carries straight on (with the invincibility countdown, unless it was an obstacle hit). */
	void Fail(ESpinMistake Mistake);
	void EndGame();
	void ResumeAfterIntro();
	void RegisterHit();
	void QuitGame();

	// Scoring / leaderboard
	ESpinHitGrade GradeHit() const;
	void ScoreHit(ESpinHitGrade Grade, float PointsScale);
	void BreakCombo();
	void LoadSaveData();
	void WriteSaveData();
	void RecordRun();

	// Profiles
	void OpenProfiles();
	void BeginNameEntry(bool bThenPlay);

	/** Name entry was opened from PLAY with no profile yet: continue to mode select once confirmed. */
	bool bNameEntryThenPlay = false;

	// Menus
	void OpenModeSelect();
	void OpenSettings();
	void OpenLeaderboard();
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

	UPROPERTY()
	TObjectPtr<USpinSaveGame> SaveData;

	bool bHandWasInZone = false;
	float GameOverTime = 0.f;

	/** Whether the last input came from a gamepad (InputStyle then follows bPlayStationIcons). */
	bool bUsingGamepad = false;

	/** Fraction of a point the lift has cost but not yet taken off the score. */
	float LiftPointsDebt = 0.f;

	/** How many zones in a row have needed the current button. */
	int32 PromptStreak = 1;

	/** Tutorial: the hand touched an obstacle on this lap, so the zone does not count until it goes around again. */
	bool bZoneTainted = false;

	/** Tutorial: stops the "you hit an obstacle" message from repeating every frame. */
	bool bTutorialObstacleHit = false;
};
