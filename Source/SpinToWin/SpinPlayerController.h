// Spin To Win - input.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SpinPlayerController.generated.h"

/** Forwards the action button (Space / Left Click / Gamepad A) to the game mode. */
UCLASS()
class SPINTOWIN_API ASpinPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void SetupInputComponent() override;

private:
	void OnActionPressed();
};
