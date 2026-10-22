// Spin To Win - input.

#include "SpinPlayerController.h"
#include "SpinGameMode.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"

void ASpinPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ASpinPlayerController::OnActionPressed);
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ASpinPlayerController::OnActionPressed);
		InputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, this, &ASpinPlayerController::OnActionPressed);
	}
}

void ASpinPlayerController::OnActionPressed()
{
	if (ASpinGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASpinGameMode>() : nullptr)
	{
		GM->HandleActionPressed();
	}
}
