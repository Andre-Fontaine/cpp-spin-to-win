// Spin To Win - core game rules.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SpinGameMode.generated.h"

class ASpinClock;
class USpinSaveGame;
struct FSpinProfile;

UENUM(BlueprintType)
enum class ESpinState : uint8
{
	/** Main menu: PLAY / SETTINGS. */
	WaitingToStart,
	/** PLAY was chosen: pick TUTORIAL, ENDLESS or DAILY. */
	ModeSelect,
	/** ENDLESS was chosen: the run setup screen - switch modifiers and bonuses on or off, then start. */
	ModifierSelect,
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
	/** Endless: paused on the "jams" screen (first time jams can happen) until the player presses the repair button. */
	JamIntro,
	/** Endless: paused on the "high posts" screen until the player presses the first button. */
	HighObstacleIntro,
	/** Tutorial: paused on a level's explanation screen until the player presses its confirm button. */
	TutorialIntro,
	/** The tutorial just ended and the run carries on as Endless: paused on a screen explaining lives
	 *  and misses until the player presses the first button. */
	EndlessIntro,
	/** Endless: paused on the mid-run pick, three upgrade cards to choose one from. */
	UpgradeSelect
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
	/** Starts an Endless run with the selected modifiers. */
	StartEndless,
	/** From mode select: opens the modifier screen before an Endless run. */
	OpenModifiers,
	/** Index = which ESpinModifier to switch on or off. */
	ToggleModifier,
	/** Index = which ESpinBonus to equip or unequip. */
	ToggleBonus,
	StartDaily,
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
	/** Pause menu: back into the run. */
	ResumeGame,
	/** Pause menu: start the same kind of run over. */
	RestartRun,
	/** Pause menu: abandon the run (it isn't recorded) and go to the main menu. */
	QuitRunToMenu,
	/** Upgrade pick: Index = which of the offered cards. */
	PickUpgrade,
};

UENUM(BlueprintType)
enum class ESpinMode : uint8
{
	/** The 7 guided levels. Nothing can be failed. */
	Tutorial,
	/** The real game. Mistakes cost lives or end the run. */
	Endless
};

/** Optional rules for an Endless run. Each one makes the run harder and multiplies its points.
 *  Stored as bit flags (1 << value) in a run's modifier mask. */
UENUM(BlueprintType)
enum class ESpinModifier : uint8
{
	/** The hand starts a good deal faster. */
	FastHand,
	/** Target zones are much narrower. */
	TinyZones,
	/** After a hit the hand only sometimes reverses, so you can't count on the next direction. */
	WildSpin,
	/** Jams can happen from level 1, and more often. */
	EarlyJams,
	/** One obstacle life and one miss life: any mistake ends the run. */
	OneLife,
	Count UMETA(Hidden)
};

/** Perks unlocked through progression, which make an Endless run more forgiving. None of them
 *  touches scoring, but runs that used any are marked on the leaderboard. A run can use as many
 *  as the player has slots (see ASpinGameMode::GetBonusSlots), and none on the daily challenge.
 *  Stored as bit flags (1 << value), like modifiers. */
UENUM(BlueprintType)
enum class ESpinBonus : uint8
{
	/** Target zones start wider. */
	WideOpen,
	/** More miss lives. */
	ExtraMisses,
	/** The first obstacle hit of the run doesn't cost a life. */
	SecondWind,
	/** Jams need fewer presses and give more time. */
	SteadyHands,
	/** Longer invincibility after a miss. */
	Breather,
	/** Jam sparks last longer before the hand jams. (New bonuses go last: the values are saved as bits.) */
	EarlyWarning,
	Count UMETA(Hidden)
};

/** What a modifier or bonus unlock is measured by, on the player's profile. */
enum class ESpinUnlockStat : uint8
{
	BestLevel,
	ObstaclesCleared,
	JamsFixed,
	RunsPlayed,
	/** Curses picked, over every run. */
	CursesTaken,
	/** Points curses have earned, over every run. */
	CursePoints,
	/** Best level reached in a run with at least two curses. */
	BestCursedLevel
};

/** Display info and unlock rule for one bonus, see ASpinGameMode::GetBonusInfo. */
struct FSpinBonusInfo
{
	const TCHAR* Name;
	const TCHAR* Description;
	/** "Reach level 10", shown while it's locked. */
	const TCHAR* Requirement;
	ESpinUnlockStat Stat;
	int32 Target;
};

/** Display info and scoring for one modifier, see ASpinGameMode::GetModifierInfo. */
struct FSpinModifierInfo
{
	const TCHAR* Name;
	const TCHAR* Description;
	/** Points multiplier while it's on. Several multiply together. */
	float PointsMultiplier;
	/** "Reach level 5", shown while it's locked (modifiers unlock like bonuses). */
	const TCHAR* Requirement;
	ESpinUnlockStat Stat;
	int32 Target;
};

/** A twist that takes over one whole Endless level, rolled when the level starts. */
UENUM(BlueprintType)
enum class ESpinLevelEvent : uint8
{
	None,
	/** The lights flicker and go out. Each new zone shows for a moment, then has to be hit from
	 *  memory. Points are multiplied; a miss in the dark costs nothing but ends the blackout. */
	Blackout,
	/** Every zone comes as a pair: hit both on the same pass before the hand turns around. */
	DoubleZone
};

/** Upgrades picked mid-run (every few levels of a normal Endless run, never the daily). Picking one
 *  again stacks it. Effects live where each one applies; see ASpinGameMode::GetUpgradeInfo. */
UENUM(BlueprintType)
enum class ESpinUpgrade : uint8
{
	/** Boon: one miss life back. */
	PatchJob,
	/** Boon: zones wider for a few levels. */
	SteadyGrip,
	/** Boon: in a blackout, each zone stays visible longer. */
	Afterglow,
	/** Boon: the hand slower for a few levels. */
	Brake,
	/** Boon: the next combo break is ignored. */
	ComboSaver,
	/** Boon: a wider PERFECT band. */
	SweetSpot,
	/** Boon: lifting the hand costs far fewer points for a few levels. */
	SpringHand,
	/** Boon: no jams for a few levels. */
	CoolRunning,
	/** Boon: one more (obstacle) life, now and at most. */
	Plating,
	/** Boon: no posts this level. */
	ClearSkies,
	/** Gamble: PERFECT hits pay far more, but GOOD hits count as OK. */
	TightLines,
	/** Gamble: blackouts come more often and pay more. */
	LightsOut,
	/** Gamble: double zones come more often, and a pair of PERFECTs pays a bonus. */
	TwinEngine,
	/** Gamble: the combo builds faster, but anything short of PERFECT resets it. */
	AllIn,
	/** Gamble: one miss life for a stretch of hits, at double points - then the old ones come back. */
	GlassHand,
	/** Gamble: some posts come as a low-then-high pair, paying out when both are cleared. */
	TwinPosts,
	/** Gamble: jams take more presses to fix, and each fix pays out. */
	JamSession,
	/** Gamble: most posts are high posts, and each one passed under pays out. */
	HighWire,
	/** Gamble: this level's points doubled - lose a miss life in it and the level's points are gone. */
	DoubleOrNothing,
	/** Gamble: a growing multiplier for every level cleared without losing a miss life. */
	Momentum,
	/** Curse: points now, half points on the next hits. */
	Debt,
	/** Curse: dimmer zones for a points multiplier. */
	BlindFaith,
	/** Curse: the level after the pick is a blackout paying double - but a miss in it costs double what it earned. */
	BorrowedTime,
	/** Curse: a miss life given up now, for points. */
	BloodPact,
	/** Curse: points now, one fewer miss life at most. */
	CursedGold,
	/** Curse: fake zones beside some real ones. */
	HauntedDial,
	/** Curse: clearing posts no longer earns lives back. */
	Drought,
	/** Curse: the combo is capped low. */
	GlassCeiling,
	/** Curse: the hand lifts slowly. */
	HeavyHand,
	/** Curse: posts stay invisible until they're close. */
	GhostPosts,
	Count UMETA(Hidden)
};

enum class ESpinUpgradeKind : uint8
{
	Boon,
	Gamble,
	Curse
};

/** Display info for one upgrade, see ASpinGameMode::GetUpgradeInfo. */
struct FSpinUpgradeInfo
{
	const TCHAR* Name;
	const TCHAR* Description;
	ESpinUpgradeKind Kind;
	/** Points multiplier added for the rest of the run, per stack (the curses' payment). */
	float PointsBonus = 0.f;
};

UENUM(BlueprintType)
enum class EJamPhase : uint8
{
	None,
	/** Sparks at the centre, the hand still works normally. */
	Sparking,
	/** The hand is broken: it keeps spinning, ignores everything, and must be fixed by mashing the repair button. */
	Jammed
};

/** Which input confirms a tutorial level's explanation screen. */
UENUM(BlueprintType)
enum class ETutorialConfirm : uint8
{
	FirstButton,
	SecondButton,
	Lift,
	Repair
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

	/** Called by the player controller for the repair button, which mashes a jammed hand free. It has
	 *  no other use while playing, so pressing it too often never costs a life. */
	void HandleRepairPressed();

	/** Called by the player controller when the lift button is pressed or released. */
	void SetLiftHeld(bool bHeld);

	/** Called by the clock when the hand has hopped over a (low) obstacle: every ObstaclesPerLife
	 *  of these gives a life back. */
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

	/** The repair button's name on the input in use ("SPACE", "Y", ...). */
	FString GetRepairKey() const;

	/** The pause button's name on the input in use ("P", "MENU", "OPTIONS"). */
	FString GetPauseKey() const;

	/** Replaces button tokens in text meant for the screen with the names for the input in use:
	 *  {0} {1} {2} = the buttons, {LIFT} = the lift, {REPAIR} = the repair button. Text that names buttons is written with these
	 *  tokens so it always matches whatever the player is holding. */
	FString ExpandKeys(const FString& Text) const;

	// ---------------- Tutorial ----------------

	/** The tutorial levels, built in BuildTutorialLevels(). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tutorial")
	TArray<FTutorialLevel> TutorialLevels;

	/** How long a tutorial message ("Try again!") stays on screen, in seconds. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tutorial")
	float NoticeDuration = 2.5f;

	/** When the tutorial ends, the hand keeps going and the run carries straight on as Endless from
	 *  this level - the first one that matches everything the tutorial taught (two buttons, obstacles). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tutorial", meta = (ClampMin = "1"))
	int32 TutorialHandoffLevel = 5;

	// ---------------- Jam ----------------

	/** Seconds of sparks before the hand jams. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Jam")
	float JamSparkTime = 3.f;

	/** Presses of the repair button needed to fix a jam. */
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
	float JamChance = 0.1f;

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

	/** Best levels that open a second and a third bonus slot. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Bonuses", meta = (ClampMin = "1"))
	int32 SecondSlotLevel = 15;

	UPROPERTY(EditDefaultsOnly, Category = "Spin|Bonuses", meta = (ClampMin = "1"))
	int32 ThirdSlotLevel = 25;

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

	/** Endless: from this level on, some obstacles are high posts - they float above the dial,
	 *  and the hand must NOT be lifted to get past them. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Obstacles", meta = (ClampMin = "1"))
	int32 HighObstacleStartLevel = 12;

	/** Clearing this many (low) posts earns one life back. High posts never do. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Obstacles", meta = (ClampMin = "1"))
	int32 ObstaclesPerLife = 3;

	/** Endless: chance (0..1) that an obstacle is a high post, once they've unlocked. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Obstacles", meta = (ClampMin = "0", ClampMax = "1"))
	float HighObstacleChance = 0.35f;

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

	/** Lives the player starts a run with. Losing them all costs a miss and fills them back up. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Obstacles", meta = (ClampMin = "1"))
	int32 MaxLives = 5;

	/** Camera shake when a life is lost. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Obstacles", meta = (ClampMin = "0", ClampMax = "1"))
	float LifeLostShake = 0.35f;

	// ---------------- Level events (Endless) ----------------
	//
	// Each level from the start level on rolls once (from the run's seed, so the daily challenge gets
	// the same events for everyone). Never two event levels in a row, and never on a level that opens
	// with an explanation screen.

	/** Blackout: first level it can happen on, and the chance per level. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Events", meta = (ClampMin = "1"))
	int32 BlackoutStartLevel = 10;

	UPROPERTY(EditDefaultsOnly, Category = "Spin|Events", meta = (ClampMin = "0", ClampMax = "1"))
	float BlackoutChance = 0.15f;

	/** Seconds the lights flicker before they go out. Play is normal while they flicker. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Events", meta = (ClampMin = "0.1"))
	float BlackoutFlickerTime = 1.6f;

	/** In the dark, each new zone shows for this long, then fades out over BlackoutZoneFadeTime. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Events", meta = (ClampMin = "0"))
	float BlackoutZoneShowTime = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Spin|Events", meta = (ClampMin = "0.01"))
	float BlackoutZoneFadeTime = 0.25f;

	/** How bright the lights stay once they're out (0 = pitch black, 1 = normal). The hand stays lit. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Events", meta = (ClampMin = "0", ClampMax = "1"))
	float BlackoutLightLevel = 0.03f;

	/** Seconds the lights take to come back on after a blackout. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Events", meta = (ClampMin = "0.01"))
	float BlackoutRestoreTime = 0.6f;

	/** Points from hits made in the dark are multiplied by this. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Events", meta = (ClampMin = "1"))
	float BlackoutPointsMultiplier = 2.f;

	/** Double zone: first level it can happen on, and the chance per level. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Events", meta = (ClampMin = "1"))
	int32 DoubleZoneStartLevel = 12;

	UPROPERTY(EditDefaultsOnly, Category = "Spin|Events", meta = (ClampMin = "0", ClampMax = "1"))
	float DoubleZoneChance = 0.15f;

	/** Gap in degrees between the two zones of a pair (edge to edge). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Events", meta = (ClampMin = "0"))
	float DoubleZoneMinGap = 20.f;

	UPROPERTY(EditDefaultsOnly, Category = "Spin|Events", meta = (ClampMin = "0"))
	float DoubleZoneMaxGap = 60.f;

	// ---------------- Upgrades (normal Endless runs, not the daily) ----------------

	/** A pick of upgrades comes up every this many levels (on reaching 5, 10, 15, ...). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades", meta = (ClampMin = "1"))
	int32 UpgradeEveryLevels = 5;

	/** Cards offered at each pick. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades", meta = (ClampMin = "1", ClampMax = "5"))
	int32 UpgradeChoices = 3;

	/** Seconds of countdown after a pick before play picks back up. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades", meta = (ClampMin = "0"))
	float UpgradeResumeTime = 3.f;

	/** Steady Grip: zones this much wider (per stack), for this many levels. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades")
	float SteadyGripWidthBonus = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades", meta = (ClampMin = "1"))
	int32 SteadyGripLevels = 3;

	/** Afterglow: seconds longer each zone stays visible in a blackout (per stack). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades")
	float AfterglowTime = 1.f;

	/** Brake: the hand this much slower, for this many levels. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades", meta = (ClampMin = "0", ClampMax = "0.5"))
	float BrakeSlowdown = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades", meta = (ClampMin = "1"))
	int32 BrakeLevels = 3;

	/** Sweet Spot: the PERFECT band this much wider (per stack). It never grows past most of the GOOD band. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades")
	float SweetSpotBonus = 0.25f;

	/** Spring Hand: lifting drains points at this share of the usual rate, for this many levels. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades", meta = (ClampMin = "0", ClampMax = "1"))
	float SpringHandDrain = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades", meta = (ClampMin = "1"))
	int32 SpringHandLevels = 3;

	/** Cool Running: levels without jams. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades", meta = (ClampMin = "1"))
	int32 CoolRunningLevels = 3;

	/** Tight Lines: PERFECT hits pay 1 + this many times more (per stack). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades")
	float TightLinesPerfectBonus = 2.f;

	/** Lights Out / Twin Engine: event chance multiplied by this (per stack). Lights Out adds this
	 *  much to the blackout multiplier; Twin Engine's PERFECT pair pays 1 + this much more (per stack). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades")
	float EventUpgradeChanceScale = 2.f;

	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades")
	float LightsOutMultiplierBonus = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades")
	float TwinEnginePairBonus = 2.f;

	/** Glass Hand: hits on a single miss life, and the points multiplier added meanwhile (per stack). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades", meta = (ClampMin = "1"))
	int32 GlassHandHits = 10;

	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades")
	float GlassHandPointsBonus = 1.f;

	/** Twin Posts: chance (per stack) that a post comes as a pair, the payout (per stack) for clearing
	 *  both, and the seconds of hand travel between them (time to drop the hand after the first). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades", meta = (ClampMin = "0", ClampMax = "1"))
	float TwinPostsChance = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades")
	int32 TwinPostsPayout = 300;

	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades", meta = (ClampMin = "0.1"))
	float TwinPostsGapTime = 0.5f;

	/** Jam Session: extra presses per jam, and the payout for each fix (per stack). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades")
	int32 JamSessionExtraPresses = 3;

	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades")
	int32 JamSessionPayout = 300;

	/** High Wire: chance a post is a high post (from any level), and the payout per one passed (per stack). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades", meta = (ClampMin = "0", ClampMax = "1"))
	float HighWireChance = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades")
	int32 HighWirePayout = 200;

	/** Momentum: points multiplier added per clean level (per stack). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades")
	float MomentumPerLevel = 0.1f;

	/** Debt: points paid out now, and how many following hits score at half (per stack). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades")
	int32 DebtPayout = 1000;

	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades", meta = (ClampMin = "1"))
	int32 DebtHits = 10;

	/** Blind Faith: zone brightness multiplied by this (per stack). (Each curse's points bonus is in
	 *  ASpinGameMode::GetUpgradeInfo, next to its description.) */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades", meta = (ClampMin = "0", ClampMax = "1"))
	float BlindFaithDim = 0.5f;

	/** Cursed Gold: points paid out now. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades")
	int32 CursedGoldPayout = 2000;

	/** Haunted Dial: chance (per stack) a zone comes with a fake beside it. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades", meta = (ClampMin = "0", ClampMax = "1"))
	float HauntedDialChance = 0.35f;

	/** Glass Ceiling: the highest combo multiplier (one lower per extra stack, never below x2). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades", meta = (ClampMin = "2"))
	int32 GlassCeilingCap = 3;

	/** Heavy Hand: the lift's rise stiffness is multiplied by this (per stack) - lower is slower. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades", meta = (ClampMin = "0.05", ClampMax = "1"))
	float HeavyHandLiftScale = 0.3f;

	/** Ghost Posts: posts show only once the hand is this many seconds of travel away. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Upgrades", meta = (ClampMin = "0.1"))
	float GhostPostsRevealTime = 0.4f;

	// ---------------- Misses (Endless) ----------------

	/** Separate lives for missed target zones (and wrong presses, and broken hands). Endless only -
	 *  the tutorial can never be failed. Running out of lives costs one too. Once these run out, the run ends. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Misses", meta = (ClampMin = "1"))
	int32 MaxMissLives = 3;

	/** One lost miss life comes back every time the run reaches a multiple of this many levels
	 *  (5, 10, 15, ...). 0 = never. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Misses", meta = (ClampMin = "0"))
	int32 MissRegenLevels = 5;

	// ---------------- Tuning (Endless) ----------------

	/** Hand speed at the start of a run, in degrees per second. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float StartSpeed = 85.f;

	/** Hand speed will never exceed this (reached around level 28). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float MaxSpeed = 260.f;

	/** Speed is multiplied by this after every successful hit. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float SpeedMultiplier = 1.005f;

	/** Half-width of the target zone at the start, in degrees. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float StartZoneHalfWidth = 32.f;

	/** The target zone never gets narrower than this (half-width, degrees). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float MinZoneHalfWidth = 14.f;

	/** How much the zone half-width shrinks after each hit. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float ZoneShrinkPerHit = 0.1f;

	/** Next zone appears at least this many degrees ahead of the hand. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float MinZoneOffset = 70.f;

	/** Next zone appears at most this many degrees ahead of the hand (obstacles can push it further). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float MaxZoneOffset = 200.f;

	/** Seconds after the game-over screen starts fading in before it accepts input (avoids an
	 *  accidental retry from a press that was meant for the hand). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float RestartDelay = 1.f;

	/** Seconds after the run ends before the game-over screen appears, so the dial's collapse
	 *  (ASpinClock::CollapseTime) plays out in the clear. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning", meta = (ClampMin = "0"))
	float GameOverScreenDelay = 3.f;

	/** Seconds the game-over screen takes to fade in once it appears. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning", meta = (ClampMin = "0"))
	float GameOverFadeTime = 2.f;

	/** Seconds of invincibility (with a countdown) after losing a miss life - obstacles, zone
	 *  misses, jams, and button presses are all ignored until this runs out. Obstacle hits don't
	 *  trigger it. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning", meta = (ClampMin = "0"))
	float InvincibilityDuration = 5.f;

	/** Seconds of countdown after unpausing, with the run held still, before play picks back up. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning", meta = (ClampMin = "0"))
	float ResumeCountdownDuration = 5.f;

	/** While the hand is jammed it turns at this share of its normal speed. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning", meta = (ClampMin = "0.05", ClampMax = "1"))
	float JamSlowdown = 0.35f;

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

	/** World time Combo Saver last caught a combo break, for the HUD. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float ComboSavedTime = -1000.f;

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

	/** True if this run is today's daily challenge: same seed and modifier for everyone today. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	bool bDailyRun = false;

	/** Modifiers in effect this run (bit mask of 1 << ESpinModifier), and their combined points multiplier. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 RunModifiers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float RunPointsMultiplier = 1.f;

	/** Bonuses in effect this run (bit mask of 1 << ESpinBonus). */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 RunBonuses = 0;

	/** What the last finished run unlocked (bonus names, new slots), for the game-over screen. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	TArray<FString> NewUnlocks;

	/** The modifiers picked on the modifier screen for the next Endless run (saved). */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 SelectedModifiers = 0;

	/** 1-based rank the last finished daily run got on today's board, or 0. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 LastRunDailyRank = 0;

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

	// ---------------- Run stats (Endless, for the game-over breakdown) ----------------

	/** Longest run of PERFECT hits in a row this run, and the highest combo multiplier it reached. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|Stats")
	int32 LongestPerfectStreak = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Spin|Stats")
	int32 PeakMultiplier = 1;

	/** Points the lift actually took off the score (the score never drops below 0, so it can be
	 *  less than the time lifted would suggest). */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|Stats")
	int32 PointsLostToLift = 0;

	/** Obstacles the hand ran into, and obstacles it got over. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|Stats")
	int32 ObstaclesHit = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Spin|Stats")
	int32 ObstaclesCleared = 0;

	/** Jams repaired in time this run. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|Stats")
	int32 JamsFixed = 0;

	/** Which button (index into Prompts) the current zone requires. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 ActivePromptIndex = 0;

	/** The event running on the current level, if any. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	ESpinLevelEvent LevelEvent = ESpinLevelEvent::None;

	/** Double zone: the pair's second zone, which the hand reaches after the current one. Its width is
	 *  ZoneHalfWidth too. Once the first is hit, it becomes the current zone. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	bool bHasSecondZone = false;

	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float SecondZoneCenter = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 SecondPromptIndex = 0;

	/** Haunted Dial: a fake zone on the dial (looks real but flickers; pressing in it is a wrong press). */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	bool bHasDecoyZone = false;

	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float DecoyZoneCenter = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 DecoyPromptIndex = 0;

	/** How lit the world is, 0..1: 1 normally, flickering and dropping to BlackoutLightLevel during a
	 *  blackout. The dial (all but the hand) and the city scale their glow by it. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float LightsLevel = 1.f;

	/** The cards on offer at the current upgrade pick. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	TArray<ESpinUpgrade> UpgradeOffer;

	/** Points this run's curses have earned: their share of every hit, plus their lump sums. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 CursePointsEarned = 0;

	/** The button the new-button screen (ESpinState::NewButtonIntro) is introducing. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 NewButtonIndex = 1;

	/** World time of the last level-up, used for the banner. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float LevelUpTime = -1000.f;

	/** Lives left this run. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 Lives = 5;

	/** Posts cleared towards the next life (0..ObstaclesPerLife-1). */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 ObstacleLifeProgress = 0;

	/** World time of the last life lost, used for HUD feedback. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float LifeLostTime = -1000.f;

	/** World time of the last life gained, used for HUD feedback. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float LifeGainedTime = -1000.f;

	/** True if the run ended on a post: it took the last life, and that cost the last miss. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	bool bOutOfLives = false;

	/** Miss lives left this run (Endless only). One comes back every MissRegenLevels levels. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	int32 MissLives = 3;

	/** World time of the last missed zone, used for HUD feedback. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float MissLifeLostTime = -1000.f;

	/** World time a miss life last came back, used for HUD feedback. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float MissLifeGainedTime = -1000.f;

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

	/** World time the last invincibility window or resume countdown ran out, used for the HUD's "GO!". */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float InvincibilityEndTime = -1000.f;

	/** Seconds left in the countdown after unpausing, or 0. */
	UPROPERTY(BlueprintReadOnly, Category = "Spin|State")
	float ResumeTimeRemaining = 0.f;

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

	/** The hand's speed multiplier from jamming: eases toward JamSlowdown while jammed, back to 1 after. */
	float JamSpeedFactor = 1.f;

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

	/** Master volume, 0-100. Applies immediately, and is saved with the profiles. */
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

	/** True once a blackout's lights are out (after the flicker), until the level ends or a miss
	 *  ends it early. Zones hide and points are multiplied only then. */
	bool IsBlackoutDark() const;

	/** How visible the current zone is, 0..1: 1 normally; in the dark it shows briefly, then fades. */
	float GetZoneVisibility() const;

	/** The zones' half-width as played (ZoneHalfWidth, widened by Steady Grip). */
	float GetZoneHalfWidth() const;

	/** How bright the zones are drawn, 0..1 (dimmed by Blind Faith). */
	float GetZoneBrightness() const;

	// ---------------- Upgrades ----------------

	static const FSpinUpgradeInfo& GetUpgradeInfo(ESpinUpgrade Upgrade);

	/** How many times this run has picked an upgrade. */
	int32 GetUpgradeStacks(ESpinUpgrade Upgrade) const;

	/** How many picks of a level-limited upgrade (Steady Grip, Brake, ...) are still running. */
	int32 GetActiveUpgradeWindows(ESpinUpgrade Upgrade) const;

	/** The PERFECT band as played (PerfectFraction, widened by Sweet Spot). */
	float GetRunPerfectFraction() const;

	/** The hand's speed as played (CurrentSpeed, slowed by Brake). */
	float GetHandSpeed() const;

	/** Hits left at half points from Debt. */
	int32 GetDebtHitsLeft() const { return DebtHitsLeft; }

	/** Hits left on Glass Hand's single miss life, or 0. */
	int32 GetGlassHandHitsLeft() const { return GlassHandHitsLeft; }

	/** Momentum's current points multiplier bonus (0 without it). */
	float GetMomentumBonus() const;

	/** Multiplier on how fast the lift raises the hand (Heavy Hand slows it). */
	float GetLiftRiseScale() const;

	/** What the run's curses add to its points multiplier (0 with none). */
	float GetCursePointsBonus() const;

	/** "BLOOD PACT + DROUGHT x2", or empty with no curses. */
	FString GetCurseNames() const;

	/** True while posts stay hidden until close (Ghost Posts). */
	bool HasGhostPosts() const { return !IsTutorial() && GetUpgradeStacks(ESpinUpgrade::GhostPosts) > 0; }

	/** Called by the clock when the hand has passed under a high post. bTwin = the second of a twin pair. */
	void OnHighObstaclePassed(bool bTwin);

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

	// ---------------- Modifiers and the daily challenge ----------------

	static const FSpinModifierInfo& GetModifierInfo(ESpinModifier Modifier);

	/** Combined points multiplier of a modifier mask (1 for none). */
	static float GetModifiersMultiplier(int32 Mask);

	/** "TINY ZONES + ONE LIFE", or empty for none. */
	static FString GetModifierNames(int32 Mask);

	/** True if this modifier is in effect for the current run. */
	bool HasModifier(ESpinModifier Modifier) const;

	/** The active player's progress towards unlocking a modifier (Current can pass Target). */
	void GetModifierProgress(ESpinModifier Modifier, int32& OutCurrent, int32& OutTarget) const;

	/** Unlocked: claimed at the end of a run (only one unlock is claimed per run). */
	bool IsModifierUnlocked(ESpinModifier Modifier) const;

	/** Earned: the requirement is met, even if it hasn't been claimed yet. */
	bool IsModifierEarned(ESpinModifier Modifier) const;

	/** The modifiers unlocked by the active player (bit mask). */
	int32 GetUnlockedModifiers() const;

	/** The modifiers whose requirement the active player meets (bit mask), claimed or not. */
	int32 GetEarnedModifiers() const;

	/** The active player's lifetime value of an unlock stat (best level, jams fixed, ...). */
	int32 GetUnlockStat(ESpinUnlockStat Stat) const;

	/** Today's date as YYYYMMDD (local time): the daily challenge's id and seed. */
	static int32 GetTodayKey();

	/** Today's daily modifier mask - one modifier, picked from the date. */
	static int32 GetDailyModifiers();

	/** The active player's best score in today's daily challenge, or 0. */
	int32 GetDailyBest() const;

	/** Lives this run starts with (and can be topped back up to) - fewer with some modifiers. */
	int32 GetRunMaxLives() const;
	int32 GetRunMaxMissLives() const;

	/** Jam repair for this run - easier with STEADY HANDS. */
	int32 GetRunJamPressesRequired() const;
	float GetRunJamTimeLimit() const;

	/** Seconds of sparks before the hand jams this run - longer with EARLY WARNING. */
	float GetRunJamSparkTime() const;

	// ---------------- Bonuses ----------------

	static const FSpinBonusInfo& GetBonusInfo(ESpinBonus Bonus);

	/** "WIDE OPEN + BREATHER", or empty for none. */
	static FString GetBonusNames(int32 Mask);

	/** The active player's progress towards unlocking a bonus (Current can pass Target). */
	void GetBonusProgress(ESpinBonus Bonus, int32& OutCurrent, int32& OutTarget) const;

	/** Unlocked: claimed at the end of a run (only one unlock is claimed per run). */
	bool IsBonusUnlocked(ESpinBonus Bonus) const;

	/** Earned: the requirement is met, even if it hasn't been claimed yet. */
	bool IsBonusEarned(ESpinBonus Bonus) const;

	/** The bonuses whose requirement the active player meets (bit mask), claimed or not. */
	int32 GetEarnedBonuses() const;

	/** Unlocks the first earned-but-unclaimed modifier or bonus on a profile, if any. Returns its
	 *  name, or empty if nothing was waiting. */
	FString ClaimNextUnlock(FSpinProfile& Profile) const;

	/** Earned modifiers and bonuses the active player hasn't claimed yet. */
	int32 GetPendingUnlockCount() const;

	/** How many bonuses the active player can have on at once (1, more at SecondSlotLevel / ThirdSlotLevel). */
	int32 GetBonusSlots() const;

	/** Best level that opens the next bonus slot, or 0 if they're all open. */
	int32 GetNextBonusSlotLevel() const;

	/** The bonuses the active player has switched on for their next Endless run (bit mask). */
	int32 GetEquippedBonuses() const;

	/** True if this bonus is in effect for the current run. */
	bool HasBonus(ESpinBonus Bonus) const;

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

	/** How far the game-over screen has faded in: 0 through GameOverScreenDelay, rising to 1 over
	 *  GameOverFadeTime. 1 outside game over. */
	float GetGameOverScreenAlpha() const;

	/** The pause button: opens the pause menu during a run, or closes it again. Ignored anywhere else. */
	void TogglePauseMenu();

	/** Start / P: starts the run from the modifier and bonus screens, pauses during one. */
	void HandleStartButton();

	/** True on the screens that are driven like a menu - a highlight to move, select and back - rather
	 *  than by the hit buttons: every menu, game over, and the pause menu. The briefing screens
	 *  aren't: they wait for one particular button. */
	bool IsMenuScreen() const;

	/** Back (B / Circle / Esc) on a menu screen: up one level, cancel, or resume from the pause menu. */
	void HandleMenuBack();

	/** True while a run is paused behind the pause menu. The whole world is paused (see
	 *  UGameplayStatics::SetGamePaused), so the hand, timers and effects all freeze. */
	bool IsPauseMenuOpen() const;

	/** True during the countdown after unpausing, while the run holds still (see ResumeTimeRemaining). */
	bool IsResumeCountingDown() const;

	/** True during a run, explanation screens included - anything but the menus and game over. */
	bool IsInRun() const;

	/** The in-world dial, spawned when play starts. */
	ASpinClock* GetClock() const;

private:
	void BuildTutorialLevels();
	void Shake(float Intensity);
	void ShowNotice(const FString& Text);

	// Endless
	/** Starts an Endless run: today's daily challenge, or a normal run with SelectedModifiers. */
	void StartGame(bool bDaily = false);

	/** The tutorial's last hit: carries straight on as a plain Endless run from TutorialHandoffLevel,
	 *  without stopping or resetting the hand. */
	void ContinueFromTutorial();
	void OpenModifiers();

	/** Run tuning with this run's modifiers applied. */
	float GetRunStartSpeed() const;
	float GetRunStartZoneHalfWidth() const;
	float GetRunMinZoneHalfWidth() const;
	int32 GetRunJamStartLevel() const;
	float GetRunJamChance() const;
	float GetRunInvincibilityDuration() const;

	/** The bonuses unlocked by the active player (bit mask), see IsBonusUnlocked. */
	int32 GetUnlockedBonuses() const;
	void OnHit();
	void OnZoneMissed();
	/** A life-costing mistake: spends from the right pool (obstacle lives roll over into misses), then either ends the run or
	 *  carries straight on (with the invincibility countdown, if it cost a miss). */
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
	void SaveVolume();

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

	// Level events
	/** Rolls the event for a level that has just started (from the seed and the level alone). */
	ESpinLevelEvent RollLevelEvent(int32 Level, ESpinLevelEvent PreviousEvent) const;
	void SetLevelEvent(ESpinLevelEvent Event);
	void UpdateLights(float DeltaSeconds);

	/** Seconds of play since the current level's event started (stops while paused). */
	float EventTime = 0.f;

	/** A miss in the dark ended this level's blackout early. */
	bool bBlackoutBroken = false;

	/** EventTime when the current zone was last shown (placed, or shown again after a pause or the
	 *  invincibility countdown), for the blackout's brief look at it. */
	float ZoneShownTime = 0.f;

	// Upgrades
	/** Deals UpgradeOffer and opens the pick screen. */
	void OpenUpgradePick();
	void ApplyUpgrade(ESpinUpgrade Upgrade);
	void ResetUpgrades();
	float GetBlackoutPointsMultiplier() const;

	/** Times each upgrade has been picked this run (indexed by ESpinUpgrade). */
	TArray<int32> UpgradeStacks;

	/** A pick is due but an explanation screen came first: it opens once that's dismissed. */
	bool bUpgradePending = false;

	/** World time the pick screen opened: picks are ignored for a moment, so a press meant for the
	 *  hand can't choose a card by accident. */
	float UpgradePickOpenTime = -1000.f;

	/** Level-limited upgrades: the last level of each pick, indexed by ESpinUpgrade (one entry per pick). */
	TArray<TArray<int32>> UpgradeWindows;

	/** How many levels a level-limited upgrade lasts, or 0 for one that lasts the run. */
	int32 GetUpgradeWindowLevels(ESpinUpgrade Upgrade) const;

	/** Combo breaks Combo Saver will still catch. */
	int32 ComboSaves = 0;

	/** Glass Hand: hits left on one miss life, and the miss lives to give back after. */
	int32 GlassHandHitsLeft = 0;
	int32 GlassHandSavedMissLives = 0;

	/** Double or Nothing: the level it covers (0 = none), whether it's been lost, and what that level has earned. */
	int32 DoubleOrNothingLevel = 0;
	bool bDoubleOrNothingBust = false;
	int32 DoubleOrNothingEarned = 0;

	/** Momentum: clean levels in a row, and whether the current level has cost a miss life. */
	int32 MomentumLevels = 0;
	bool bMissLostThisLevel = false;

	/** Pays a flat upgrade bonus straight onto the score, with a notice. */
	void AwardBonusPoints(int32 Amount, const TCHAR* Label);

	/** Called whenever a miss life is spent (Double or Nothing, Momentum). */
	void OnMissLifeLost();

	int32 DebtHitsLeft = 0;

	/** This level's blackout came from Borrowed Time, and the points its hits have earned so far. */
	bool bBorrowedBlackout = false;
	int32 BlackoutPointsEarned = 0;

	/** Double zone: the pair's first hit was a PERFECT (for Twin Engine's bonus). */
	bool bPairFirstPerfect = false;

	UPROPERTY()
	TObjectPtr<ASpinClock> Clock;

	UPROPERTY()
	TObjectPtr<USpinSaveGame> SaveData;

	bool bHandWasInZone = false;

	/** Set when invincibility ends with the hand already inside the zone: leaving that zone without
	 *  a press isn't a miss. Cleared once the hand leaves it, or on a hit. */
	bool bZoneMissForgiven = false;
	float GameOverTime = 0.f;

	/** Whether the last input came from a gamepad (InputStyle then follows bPlayStationIcons). */
	bool bUsingGamepad = false;

	/** All gameplay randomness (zones, buttons, obstacles, jams) comes from this stream. It's
	 *  re-seeded from RunSeed and the hit count on every hit, so for a given seed, hit N always
	 *  rolls the same - which is what makes the daily challenge the same run for everyone. */
	FRandomStream RunRandom;
	int32 RunSeed = 0;

	/** The day (YYYYMMDD) a daily run was started on - its board, even if it ends after midnight. */
	int32 RunDayKey = 0;

	/** This run carried on from the tutorial, which already taught jams: they can happen from the
	 *  handoff level, and their explanation screen is skipped. */
	bool bRunFromTutorial = false;

	/** The obstacle just run into was a high post (for the message). */
	bool bLastObstacleHigh = false;

	/** The high-post screen was just shown: the next obstacle is a high post, so it's seen right away. */
	bool bForceHighPost = false;

	/** Whether the active player has already had high posts explained (saved per profile). */
	bool HasSeenHighPosts() const;

	/** Second Wind has already saved a life this run. */
	bool bSecondWindUsed = false;

	/** Fraction of a point the lift has cost but not yet taken off the score. */
	float LiftPointsDebt = 0.f;

	/** How many zones in a row have needed the current button. */
	int32 PromptStreak = 1;

	/** Tutorial: the hand touched an obstacle on this lap, so the zone does not count until it goes around again. */
	bool bZoneTainted = false;

	/** Tutorial: stops the "you hit an obstacle" message from repeating every frame. */
	bool bTutorialObstacleHit = false;
};
