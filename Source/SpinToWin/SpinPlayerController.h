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
 *   Pause:        P / Gamepad Start (Xbox Menu, PlayStation Options) - starts the run on the modifier and bonus screens
 *
 * On the menu screens (main menu, mode select, modifiers, bonuses, settings, leaderboard, profiles,
 * name entry, game over, pause - see ASpinGameMode::IsMenuScreen) the same inputs drive the menu instead:
 *   Move highlight:  Arrow keys / D-pad / left stick (left / right change a settings value)
 *   Select:          Gamepad A (Cross) / W / Enter - presses the highlighted item
 *   Back:            Gamepad B (Circle) / D / Esc / Backspace
 *   Mouse:           hovering moves the highlight, Left Mouse Button presses what's under the cursor
 * The mouse cursor is shown on those screens while the keyboard and mouse are in use, and hidden
 * once a gamepad is picked up.
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
	void OnPause();
	void OnNavUp();
	void OnNavDown();
	void OnNavLeft();
	void OnNavRight();
	void Navigate(int32 DX, int32 DY);
	void OnMenuConfirm();
	void OnMenuBack();
	void PressButton(int32 ButtonIndex);

	/** Presses the menu item the HUD has highlighted. */
	void ConfirmFocusedItem();
	void SetLift(bool bHeld);

	/** One name-entry keystroke: letters, digits, space, backspace, Enter (confirm), Escape (cancel). */
	void HandleNameKey(class ASpinGameMode& GM, const FKey& Key, bool bRepeat);

	/** Tests the current mouse position against the HUD's clickable regions and, on a hit,
	 *  forwards the action to the game mode. Returns true if a menu screen was up at all
	 *  (consuming the click regardless of whether anything was actually under the cursor). */
	bool TryHandleMenuClick();

	/** Whether a menu screen is up (see ASpinGameMode::IsMenuScreen). */
	bool IsInMenuState() const;

	class ASpinGameMode* GetSpinGameMode() const;
	class ASpinHUD* GetSpinHUD() const;
};
