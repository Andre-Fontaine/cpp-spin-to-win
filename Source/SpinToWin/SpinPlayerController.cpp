// Spin To Win - input.

#include "SpinPlayerController.h"
#include "SpinGameMode.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"

void ASpinPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!InputComponent)
	{
		return;
	}

	// Button 0
	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ASpinPlayerController::OnButton0);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ASpinPlayerController::OnButton0);
	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, this, &ASpinPlayerController::OnButton0);

	// Button 1
	InputComponent->BindKey(EKeys::F, IE_Pressed, this, &ASpinPlayerController::OnButton1);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ASpinPlayerController::OnButton1);
	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Right, IE_Pressed, this, &ASpinPlayerController::OnButton1);
}

void ASpinPlayerController::OnButton0()
{
	PressButton(0);
}

void ASpinPlayerController::OnButton1()
{
	PressButton(1);
}

void ASpinPlayerController::PressButton(int32 ButtonIndex)
{
	if (ASpinGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASpinGameMode>() : nullptr)
	{
		GM->HandleButtonPressed(ButtonIndex);
	}
}
