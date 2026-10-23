// Spin To Win - input.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SpinPlayerController.generated.h"

/**
 * Maps physical inputs to game actions and forwards them to the game mode.
 *   Hit button 0: W / Gamepad A (Cross)
 *   Hit button 1: D / Gamepad B (Circle)
 *   Lift (hold):  Left Mouse Button / Gamepad Right Trigger (R2)
 */
UCLASS()
class SPINTOWIN_API ASpinPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void SetupInputComponent() override;

private:
	void OnButton0();
	void OnButton1();
	void OnLiftPressed();
	void OnLiftReleased();
	void PressButton(int32 ButtonIndex);
	void SetLift(bool bHeld);
};
