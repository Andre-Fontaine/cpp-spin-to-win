// Spin To Win - input.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SpinPlayerController.generated.h"

/**
 * Maps physical inputs to game buttons and forwards presses to the game mode.
 *   Button 0: Space / Left Click / Gamepad A
 *   Button 1: F / Right Click / Gamepad B
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
	void PressButton(int32 ButtonIndex);
};
