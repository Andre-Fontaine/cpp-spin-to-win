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

	// Hit button 0
	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ASpinPlayerController::OnButton0);
	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, this, &ASpinPlayerController::OnButton0);

	// Hit button 1
	InputComponent->BindKey(EKeys::F, IE_Pressed, this, &ASpinPlayerController::OnButton1);
	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Right, IE_Pressed, this, &ASpinPlayerController::OnButton1);

	// Lift: hold to keep the hand raised.
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ASpinPlayerController::OnLiftPressed);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ASpinPlayerController::OnLiftReleased);
	InputComponent->BindKey(EKeys::Gamepad_RightTrigger, IE_Pressed, this, &ASpinPlayerController::OnLiftPressed);
	InputComponent->BindKey(EKeys::Gamepad_RightTrigger, IE_Released, this, &ASpinPlayerController::OnLiftReleased);
}

void ASpinPlayerController::OnButton0()
{
	PressButton(0);
}

void ASpinPlayerController::OnButton1()
{
	PressButton(1);
}

void ASpinPlayerController::OnLiftPressed()
{
	SetLift(true);
}

void ASpinPlayerController::OnLiftReleased()
{
	SetLift(false);
}

void ASpinPlayerController::PressButton(int32 ButtonIndex)
{
	if (ASpinGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASpinGameMode>() : nullptr)
	{
		GM->HandleButtonPressed(ButtonIndex);
	}
}

void ASpinPlayerController::SetLift(bool bHeld)
{
	if (ASpinGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASpinGameMode>() : nullptr)
	{
		GM->SetLiftHeld(bHeld);
	}
}
