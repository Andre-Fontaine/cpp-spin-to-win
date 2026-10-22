// Spin To Win - core game rules.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SpinGameMode.generated.h"

class ASpinClock;

UENUM(BlueprintType)
enum class ESpinState : uint8
{
	WaitingToStart,
	Playing,
	GameOver,
	/** Paused on the "new button unlocked" screen until the player presses the new button. */
	NewButtonIntro,
	/** Paused on the "obstacles" tutorial screen until the player presses the lift button. */
	ObstacleIntro
};

/** One button the player can be asked to press. Index in the Prompts array = button index. */
USTRUCT(BlueprintType)
struct FSpinPrompt
{
	GENERATED_BODY()

	/** Short text drawn on the target zone, e.g. "SPACE". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spin")
	FString Label;

	/** Longer text for the on-screen hint, e.g. "SPACE / A / X". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spin")
	FString Hint;

	/** Colour of the target zone when this button is required. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spin")
	FLinearColor Color = FLinearColor(0.1f, 0.8f, 0.2f);
};

/**
 * Owns all the rules for the spinner game.
 * Angles are in degrees: 0 = 12 o'clock, positive = clockwise.
 *
 * Levels: every HitsPerLevel hits moves you up one level. The hit counter is cumulative
 * (level 1 = 0..10, level 2 = 10..20, ...). Difficulty ramps continuously with every hit,
 * so levels get steadily harder without any hard reset.
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

	// ---------------- Levels ----------------

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

	/** The level at which obstacles (and the lift) are introduced. Raise it to delay them.
	 *  If this equals SecondButtonLevel, the new-button screen is shown and the obstacle tutorial is skipped. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Obstacles", meta = (ClampMin = "1"))
	int32 ObstacleStartLevel = 2;

	/** Chance (0..1) that a given zone comes with an obstacle in front of it. */
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

	/** Obstacles sit between these fractions of the dial radius. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Obstacles")
	float ObstacleMinRadiusFraction = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category = "Spin|Obstacles")
	float ObstacleMaxRadiusFraction = 0.75f;

	/** Lives the player starts a run with. They only come into play once obstacles are available. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Obstacles", meta = (ClampMin = "1"))
	int32 MaxLives = 5;

	/** Camera shake when a life is lost. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Obstacles", meta = (ClampMin = "0", ClampMax = "1"))
	float LifeLostShake = 0.35f;

	// ---------------- Tuning ----------------

	/** Hand speed at the start of a run, in degrees per second. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float StartSpeed = 100.f;

	/** Hand speed will never exceed this. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float MaxSpeed = 400.f;

	/** Speed is multiplied by this after every successful hit. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float SpeedMultiplier = 1.025f;

	/** Half-width of the target zone at the start, in degrees. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float StartZoneHalfWidth = 32.f;

	/** The target zone never gets narrower than this (half-width, degrees). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float MinZoneHalfWidth = 12.f;

	/** How much the zone half-width shrinks after each hit. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float ZoneShrinkPerHit = 0.35f;

	/** Next zone appears at least this many degrees ahead of the hand. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float MinZoneOffset = 70.f;

	/** Next zone appears at most this many degrees ahead of the hand (obstacles can push it further). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float MaxZoneOffset = 200.f;

	/** Seconds after game over before a button press restarts (avoids accidental restarts). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float RestartDelay = 0.6f;

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

	/** Total hits this run (cumulative across levels). */
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

	/** Lives left this run. Only relevant once obstacles are available. */
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

	/** True while the hand is inside the target zone. */
	bool IsHandInZone() const;

	/** Current level, starting at 1. */
	int32 GetLevel() const;

	/** Total hits needed to finish the current level (10, 20, 30, ...). */
	int32 GetLevelTarget() const;

	/** How many buttons are in play at the current level. */
	int32 GetNumActiveButtons() const;

	/** True for a short time after a level-up. */
	bool IsShowingLevelBanner() const;

	/** The in-world dial, spawned when play starts. */
	ASpinClock* GetClock() const;

private:
	void Shake(float Intensity);

	void StartGame();
	void OnHit();
	void EndGame();
	void PickNextPrompt();
	void PlaceNextZone(bool bAllowObstacle = true);
	void ResumeAfterIntro();
	void LoseLife();

	UPROPERTY()
	TObjectPtr<ASpinClock> Clock;

	bool bHandWasInZone = false;
	float GameOverTime = 0.f;

	/** How many zones in a row have needed the current button. */
	int32 PromptStreak = 1;
};
