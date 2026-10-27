// Spin To Win - input.

#include "SpinPlayerController.h"
#include "SpinGameMode.h"
#include "SpinHUD.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"

ASpinPlayerController::ASpinPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = false;
}

void ASpinPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Only touch the flag when it actually needs to change - setting it every frame regardless
	// would fight the engine's own cursor handling.
	const bool bWantCursor = IsInMenuState();
	if (bShowMouseCursor != bWantCursor)
	{
		bShowMouseCursor = bWantCursor;
	}
}

bool ASpinPlayerController::IsInMenuState() const
{
	const ASpinGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASpinGameMode>() : nullptr;
	if (!GM)
	{
		return false;
	}

	switch (GM->State)
	{
	case ESpinState::WaitingToStart:
	case ESpinState::ModeSelect:
	case ESpinState::SettingsMenu:
	case ESpinState::MissPause:
		return true;
	default:
		return false;
	}
}

bool ASpinPlayerController::TryHandleMenuClick()
{
	if (!IsInMenuState())
	{
		return false;
	}

	ASpinGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASpinGameMode>() : nullptr;
	const ASpinHUD* SpinHUD = Cast<ASpinHUD>(GetHUD());
	if (!GM || !SpinHUD)
	{
		return true; // still a menu state, so consume the click either way
	}

	float MouseX = 0.f, MouseY = 0.f;
	if (GetMousePosition(MouseX, MouseY))
	{
		for (const FSpinMenuRegion& Region : SpinHUD->GetMenuRegions())
		{
			if (Region.Action != EMenuAction::None && Region.Rect.IsInside(FVector2D(MouseX, MouseY)))
			{
				GM->HandleMenuAction(Region.Action);
				break;
			}
		}
	}

	return true;
}

void ASpinPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!InputComponent)
	{
		return;
	}

	// Hit button 0
	InputComponent->BindKey(EKeys::W, IE_Pressed, this, &ASpinPlayerController::OnButton0);
	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, this, &ASpinPlayerController::OnButton0);

	// Hit button 1
	InputComponent->BindKey(EKeys::D, IE_Pressed, this, &ASpinPlayerController::OnButton1);
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
	if (TryHandleMenuClick())
	{
		return;
	}
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
