// Spin To Win - input.

#include "SpinPlayerController.h"
#include "SpinGameMode.h"
#include "SpinHUD.h"
#include "Components/InputComponent.h"
#include "InputKeyEventArgs.h"
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

	if (GM->IsPauseMenuOpen())
	{
		return true;
	}

	switch (GM->State)
	{
	case ESpinState::WaitingToStart:
	case ESpinState::ModeSelect:
	case ESpinState::ModifierSelect:
	case ESpinState::BonusSelect:
	case ESpinState::SettingsMenu:
	case ESpinState::Leaderboard:
	case ESpinState::ProfileSelect:
	case ESpinState::NameEntry:
	case ESpinState::GameOver:
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
				GM->HandleMenuAction(Region.Action, Region.Index);
				break;
			}
		}
	}

	return true;
}

bool ASpinPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	// While a name is being typed, the keyboard is a keyboard: every key goes to the name instead of
	// the bindings below (otherwise typing W or D would also count as a button press). The mouse
	// still reaches the OK / CANCEL buttons.
	ASpinGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASpinGameMode>() : nullptr;

	// The on-screen button names follow whichever device was used last. Analog input only counts
	// once it's deliberate, so a drifting stick or a nudged mouse doesn't flip the prompts.
	if (GM)
	{
		const bool bAnalog = Params.Key.IsAxis1D() || Params.Key.IsAxis2D();
		const bool bDeliberate = bAnalog
			? FMath::Abs(Params.AmountDepressed) > (Params.Key.IsGamepadKey() ? 0.5f : 3.f)
			: Params.Event == IE_Pressed;
		if (bDeliberate)
		{
			GM->SetUsingGamepad(Params.Key.IsGamepadKey());
		}
	}

	if (GM && GM->State == ESpinState::NameEntry && !Params.Key.IsMouseButton() && !Params.Key.IsGamepadKey())
	{
		if (Params.Event == IE_Pressed || Params.Event == IE_Repeat)
		{
			HandleNameKey(*GM, Params.Key, Params.Event == IE_Repeat);
		}
		return true;
	}

	return Super::InputKey(Params);
}

void ASpinPlayerController::HandleNameKey(ASpinGameMode& GM, const FKey& Key, bool bRepeat)
{
	// Holding a key only repeats the backspace - repeating letters is never what anyone wants here.
	if (Key == EKeys::BackSpace || Key == EKeys::Delete)
	{
		GM.EraseNameChar();
		return;
	}
	if (bRepeat)
	{
		return;
	}

	if (Key == EKeys::Enter)
	{
		GM.ConfirmName();
	}
	else if (Key == EKeys::Escape)
	{
		GM.CancelName();
	}
	else if (Key == EKeys::SpaceBar)
	{
		GM.TypeNameChar(TEXT(' '));
	}
	else
	{
		// Letters are named "A".."Z"; digits (top row and numpad) are spelled out.
		static const FKey Digits[] = { EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
		static const FKey NumPadDigits[] = { EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine };
		for (int32 i = 0; i < 10; ++i)
		{
			if (Key == Digits[i] || Key == NumPadDigits[i])
			{
				GM.TypeNameChar(static_cast<TCHAR>(TEXT('0') + i));
				return;
			}
		}

		const FString KeyName = Key.GetFName().ToString();
		if (KeyName.Len() == 1 && FChar::IsAlpha(KeyName[0]))
		{
			GM.TypeNameChar(FChar::ToUpper(KeyName[0]));
		}
	}
}

void ASpinPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!InputComponent)
	{
		return;
	}

	// The hit buttons and the left click also drive the pause menu, so they have to keep working
	// while the game is paused. Everything else is ignored until it's resumed.

	// Hit button 0
	InputComponent->BindKey(EKeys::W, IE_Pressed, this, &ASpinPlayerController::OnButton0).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, this, &ASpinPlayerController::OnButton0).bExecuteWhenPaused = true;

	// Hit button 1
	InputComponent->BindKey(EKeys::D, IE_Pressed, this, &ASpinPlayerController::OnButton1).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Right, IE_Pressed, this, &ASpinPlayerController::OnButton1).bExecuteWhenPaused = true;

	// Hit button 2
	InputComponent->BindKey(EKeys::A, IE_Pressed, this, &ASpinPlayerController::OnButton2).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Left, IE_Pressed, this, &ASpinPlayerController::OnButton2).bExecuteWhenPaused = true;

	// Pause: P / Start (Xbox Menu, PlayStation Options) opens and closes the pause menu.
	InputComponent->BindKey(EKeys::P, IE_Pressed, this, &ASpinPlayerController::OnPause).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Gamepad_Special_Right, IE_Pressed, this, &ASpinPlayerController::OnPause).bExecuteWhenPaused = true;

	// Menu navigation on the modifier and bonus screens (ignored everywhere else). Holding a key repeats.
	for (const EInputEvent Event : { IE_Pressed, IE_Repeat })
	{
		InputComponent->BindKey(EKeys::Up, Event, this, &ASpinPlayerController::OnNavUp);
		InputComponent->BindKey(EKeys::Down, Event, this, &ASpinPlayerController::OnNavDown);
		InputComponent->BindKey(EKeys::Left, Event, this, &ASpinPlayerController::OnNavLeft);
		InputComponent->BindKey(EKeys::Right, Event, this, &ASpinPlayerController::OnNavRight);
		InputComponent->BindKey(EKeys::Gamepad_DPad_Up, Event, this, &ASpinPlayerController::OnNavUp);
		InputComponent->BindKey(EKeys::Gamepad_DPad_Down, Event, this, &ASpinPlayerController::OnNavDown);
		InputComponent->BindKey(EKeys::Gamepad_DPad_Left, Event, this, &ASpinPlayerController::OnNavLeft);
		InputComponent->BindKey(EKeys::Gamepad_DPad_Right, Event, this, &ASpinPlayerController::OnNavRight);
		InputComponent->BindKey(EKeys::Gamepad_LeftStick_Up, Event, this, &ASpinPlayerController::OnNavUp);
		InputComponent->BindKey(EKeys::Gamepad_LeftStick_Down, Event, this, &ASpinPlayerController::OnNavDown);
		InputComponent->BindKey(EKeys::Gamepad_LeftStick_Left, Event, this, &ASpinPlayerController::OnNavLeft);
		InputComponent->BindKey(EKeys::Gamepad_LeftStick_Right, Event, this, &ASpinPlayerController::OnNavRight);
	}
	// Enter presses the highlighted item there too (W / A / Cross already do, as the first button).
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ASpinPlayerController::OnMenuConfirm);

	// Lift: hold to keep the hand raised.
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ASpinPlayerController::OnLiftPressed).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ASpinPlayerController::OnLiftReleased);
	InputComponent->BindKey(EKeys::Gamepad_RightTrigger, IE_Pressed, this, &ASpinPlayerController::OnLiftPressed);
	InputComponent->BindKey(EKeys::Gamepad_RightTrigger, IE_Released, this, &ASpinPlayerController::OnLiftReleased);

	// Repair: mashes a jammed hand free. Its own button, so over-mashing can't land on a zone.
	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ASpinPlayerController::OnRepair);
	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Top, IE_Pressed, this, &ASpinPlayerController::OnRepair);
}

void ASpinPlayerController::OnButton0()
{
	PressButton(0);
}

void ASpinPlayerController::OnButton1()
{
	PressButton(1);
}

void ASpinPlayerController::OnButton2()
{
	PressButton(2);
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

void ASpinPlayerController::OnRepair()
{
	if (ASpinGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASpinGameMode>() : nullptr)
	{
		GM->HandleRepairPressed();
	}
}

void ASpinPlayerController::OnPause()
{
	if (ASpinGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASpinGameMode>() : nullptr)
	{
		GM->HandleStartButton();
	}
}

void ASpinPlayerController::OnNavUp()    { Navigate(0, -1); }
void ASpinPlayerController::OnNavDown()  { Navigate(0, 1); }
void ASpinPlayerController::OnNavLeft()  { Navigate(-1, 0); }
void ASpinPlayerController::OnNavRight() { Navigate(1, 0); }

void ASpinPlayerController::Navigate(int32 DX, int32 DY)
{
	if (ASpinGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASpinGameMode>() : nullptr)
	{
		GM->HandleMenuNav(DX, DY);
	}
}

void ASpinPlayerController::OnMenuConfirm()
{
	// Only on the screens with a highlight - elsewhere Enter would land as a hit.
	ASpinGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASpinGameMode>() : nullptr;
	if (GM && (GM->State == ESpinState::ModifierSelect || GM->State == ESpinState::BonusSelect))
	{
		GM->HandleButtonPressed(0);
	}
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
