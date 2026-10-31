// Spin To Win - input.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SpinPlayerController.generated.h"

/**
 * Maps physical inputs to game actions and forwards them to the game mode.
 *   Hit button 0: W / Gamepad A (Cross)
 *   Hit button 1: D / Gamepad B (Circle)
 *   Hit button 2: A / Gamepad X (Square)
 *   Lift (hold):  Left Mouse Button / Gamepad Right Trigger (R2)
 *   Repair:       Space / Gamepad Y (Triangle) - mashes a jammed hand free
 *
 * The same Left Mouse Button also drives the menus: on the menu screens (main menu, mode select,
 * settings, leaderboard, profiles, name entry, game over) a click is tested against the HUD's clickable regions instead of being
 * treated as a lift. The mouse cursor is shown automatically whenever one of those screens is up.
 */
UCLASS()
class SPINTOWIN_API ASpinPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASpinPlayerController();

protected:
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Routes raw keyboard input to name entry while a name is being typed, bypassing the bindings. */
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

private:
	void OnButton0();
	void OnButton1();
	void OnButton2();
	void OnLiftPressed();
	void OnLiftReleased();
	void OnRepair();
	void PressButton(int32 ButtonIndex);
	void SetLift(bool bHeld);

	/** One name-entry keystroke: letters, digits, space, backspace, Enter (confirm), Escape (cancel). */
	void HandleNameKey(class ASpinGameMode& GM, const FKey& Key, bool bRepeat);

	/** Tests the current mouse position against the HUD's clickable regions and, on a hit,
	 *  forwards the action to the game mode. Returns true if a menu screen was up at all
	 *  (consuming the click regardless of whether anything was actually under the cursor). */
	bool TryHandleMenuClick();

	/** Whether the current game state should show a mouse cursor and accept menu clicks. */
	bool IsInMenuState() const;
};
