// Spin To Win - core game rules.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SpinGameMode.generated.h"

UENUM(BlueprintType)
enum class ESpinState : uint8
{
	WaitingToStart,
	Playing,
	GameOver,
	/** Paused on the "new button unlocked" screen until the player presses the new button. */
	NewButtonIntro
};

/** One button the player can be asked to press. Index in the Prompts array = button index. */
USTRUCT(BlueprintType)
struct FSpinPrompt
{
	GENERATED_BODY()

	/** Short text drawn on the target zone, e.g. "SPACE". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spin")
	FString Label;

	/** Longer text for the on-screen hint, e.g. "SPACE / Left Click". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spin")
	FString Hint;

	/** Colour of the target zone when this button is required. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spin")
	FLinearColor Color = FLinearColor(0.1f, 0.8f, 0.2f);
};

/**
 * Owns all the rules for the clock-spinner game.
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

	virtual void Tick(float DeltaSeconds) override;

	/** Called by the player controller. ButtonIndex 0 = first button, 1 = second button. */
	void HandleButtonPressed(int32 ButtonIndex);

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

	/** Next zone appears at most this many degrees ahead of the hand. */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float MaxZoneOffset = 200.f;

	/** Seconds after game over before a button press restarts (avoids accidental restarts). */
	UPROPERTY(EditDefaultsOnly, Category = "Spin|Tuning")
	float RestartDelay = 0.6f;

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

private:
	void StartGame();
	void OnHit();
	void EndGame();
	void PlaceNextZone();
	void ResumeAfterIntro();

	bool bHandWasInZone = false;
	float GameOverTime = 0.f;
};
