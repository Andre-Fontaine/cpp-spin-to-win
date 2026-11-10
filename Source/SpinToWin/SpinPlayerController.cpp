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

ASpinGameMode* ASpinPlayerController::GetSpinGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<ASpinGameMode>() : nullptr;
}

ASpinHUD* ASpinPlayerController::GetSpinHUD() const
{
	return Cast<ASpinHUD>(GetHUD());
}

void ASpinPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The cursor is for mouse users: shown on the menus, hidden once a gamepad is in use (it comes
	// back as soon as the mouse moves). Only touch the flag when it actually needs to change -
	// setting it every frame regardless would fight the engine's own cursor handling.
	const ASpinGameMode* GM = GetSpinGameMode();
	const bool bWantCursor = IsInMenuState() && GM && GM->InputStyle == ESpinInputStyle::Keyboard;
	if (bShowMouseCursor != bWantCursor)
	{
		bShowMouseCursor = bWantCursor;
	}
}

bool ASpinPlayerController::IsInMenuState() const
{
	const ASpinGameMode* GM = GetSpinGameMode();
	return GM && GM->IsMenuScreen();
}

bool ASpinPlayerController::TryHandleMenuClick()
{
	if (!IsInMenuState())
	{
		return false;
	}

	ASpinGameMode* GM = GetSpinGameMode();
	ASpinHUD* SpinHUD = GetSpinHUD();
	if (!GM || !SpinHUD)
	{
		return true; // still a menu state, so consume the click either way
	}

	float MouseX = 0.f, MouseY = 0.f;
	if (GetMousePosition(MouseX, MouseY))
	{
		const FVector2D Mouse(MouseX, MouseY);
		if (const FSpinMenuRegion* Hit = SpinHUD->FindRegionAt(Mouse, false))
		{
			if (Hit->Action != EMenuAction::None)
			{
				// Copy it: the region list belongs to the HUD and is rebuilt every frame.
				const FSpinMenuRegion Region = *Hit;

				// Clicking moves the highlight there too (for a click-only part like a settings
				// arrow, onto the row it belongs to).
				if (const FSpinMenuRegion* Focusable = SpinHUD->FindRegionAt(Mouse, true))
				{
					SpinHUD->SetFocus(Focusable->GetKey(), /*bPlaySound*/ false);
				}
				// The only click-only parts are a settings row's < > arrows: those nudge a value.
				SpinHUD->NotifyActivated(Region.GetKey(), Region.Rect, /*bAdjust*/ !Region.bFocusable);
				GM->HandleMenuAction(Region.Action, Region.Index);
			}
		}
	}

	return true;
}

void ASpinPlayerController::ConfirmFocusedItem()
{
	ASpinGameMode* GM = GetSpinGameMode();
	ASpinHUD* SpinHUD = GetSpinHUD();
	const FSpinMenuRegion* Focused = SpinHUD ? SpinHUD->GetFocusedRegion() : nullptr;
	if (!GM || !Focused || Focused->Action == EMenuAction::None)
	{
		return;
	}

	const FSpinMenuRegion Region = *Focused;
	SpinHUD->NotifyActivated(Region.GetKey(), Region.Rect);
	GM->HandleMenuAction(Region.Action, Region.Index);
}

bool ASpinPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	// While a name is being typed, the keyboard is a keyboard: every key goes to the name instead of
	// the bindings below (otherwise typing W or D would also count as a button press). The mouse
	// still reaches the OK / CANCEL buttons, and a gamepad can still move between them.
	ASpinGameMode* GM = GetSpinGameMode();

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

	// The menu inputs (hit buttons as select / back, navigation, the left click) also drive the
	// pause menu, so they have to keep working while the game is paused. Everything else is
	// ignored until it's resumed.

	// Hit button 0 (select on menus)
	InputComponent->BindKey(EKeys::W, IE_Pressed, this, &ASpinPlayerController::OnButton0).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, this, &ASpinPlayerController::OnButton0).bExecuteWhenPaused = true;

	// Hit button 1 (back on menus)
	InputComponent->BindKey(EKeys::D, IE_Pressed, this, &ASpinPlayerController::OnButton1).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Right, IE_Pressed, this, &ASpinPlayerController::OnButton1).bExecuteWhenPaused = true;

	// Hit button 2
	InputComponent->BindKey(EKeys::A, IE_Pressed, this, &ASpinPlayerController::OnButton2).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Left, IE_Pressed, this, &ASpinPlayerController::OnButton2).bExecuteWhenPaused = true;

	// Pause: P / Start (Xbox Menu, PlayStation Options) opens and closes the pause menu.
	InputComponent->BindKey(EKeys::P, IE_Pressed, this, &ASpinPlayerController::OnPause).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Gamepad_Special_Right, IE_Pressed, this, &ASpinPlayerController::OnPause).bExecuteWhenPaused = true;

	// Menu navigation (ignored outside the menus). Holding a direction repeats.
	for (const EInputEvent Event : { IE_Pressed, IE_Repeat })
	{
		InputComponent->BindKey(EKeys::Up, Event, this, &ASpinPlayerController::OnNavUp).bExecuteWhenPaused = true;
		InputComponent->BindKey(EKeys::Down, Event, this, &ASpinPlayerController::OnNavDown).bExecuteWhenPaused = true;
		InputComponent->BindKey(EKeys::Left, Event, this, &ASpinPlayerController::OnNavLeft).bExecuteWhenPaused = true;
		InputComponent->BindKey(EKeys::Right, Event, this, &ASpinPlayerController::OnNavRight).bExecuteWhenPaused = true;
		InputComponent->BindKey(EKeys::Gamepad_DPad_Up, Event, this, &ASpinPlayerController::OnNavUp).bExecuteWhenPaused = true;
		InputComponent->BindKey(EKeys::Gamepad_DPad_Down, Event, this, &ASpinPlayerController::OnNavDown).bExecuteWhenPaused = true;
		InputComponent->BindKey(EKeys::Gamepad_DPad_Left, Event, this, &ASpinPlayerController::OnNavLeft).bExecuteWhenPaused = true;
		InputComponent->BindKey(EKeys::Gamepad_DPad_Right, Event, this, &ASpinPlayerController::OnNavRight).bExecuteWhenPaused = true;
		InputComponent->BindKey(EKeys::Gamepad_LeftStick_Up, Event, this, &ASpinPlayerController::OnNavUp).bExecuteWhenPaused = true;
		InputComponent->BindKey(EKeys::Gamepad_LeftStick_Down, Event, this, &ASpinPlayerController::OnNavDown).bExecuteWhenPaused = true;
		InputComponent->BindKey(EKeys::Gamepad_LeftStick_Left, Event, this, &ASpinPlayerController::OnNavLeft).bExecuteWhenPaused = true;
		InputComponent->BindKey(EKeys::Gamepad_LeftStick_Right, Event, this, &ASpinPlayerController::OnNavRight).bExecuteWhenPaused = true;
	}

	// Enter selects, Esc / Backspace go back (W and D, as the first and second buttons, do too).
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ASpinPlayerController::OnMenuConfirm).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ASpinPlayerController::OnMenuBack).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::BackSpace, IE_Pressed, this, &ASpinPlayerController::OnMenuBack).bExecuteWhenPaused = true;

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
	// On menus the first button (A / Cross / W) selects; everywhere else it's a hit.
	if (IsInMenuState())
	{
		ConfirmFocusedItem();
		return;
	}
	PressButton(0);
}

void ASpinPlayerController::OnButton1()
{
	// On menus the second button (B / Circle / D) goes back.
	if (IsInMenuState())
	{
		OnMenuBack();
		return;
	}
	PressButton(1);
}

void ASpinPlayerController::OnButton2()
{
	if (IsInMenuState())
	{
		// On name entry, X (Square) is backspace for the on-screen keyboard.
		ASpinGameMode* GM = GetSpinGameMode();
		if (GM && GM->State == ESpinState::NameEntry)
		{
			GM->EraseNameChar();
		}
		return;
	}
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
	if (ASpinGameMode* GM = GetSpinGameMode())
	{
		GM->HandleRepairPressed();
	}
}

void ASpinPlayerController::OnPause()
{
	if (ASpinGameMode* GM = GetSpinGameMode())
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
	ASpinGameMode* GM = GetSpinGameMode();
	ASpinHUD* SpinHUD = GetSpinHUD();
	if (!GM || !SpinHUD || !IsInMenuState())
	{
		return;
	}

	// Left / right on an item with a value (a settings row) change the value; otherwise every
	// direction moves the highlight.
	if (DX != 0)
	{
		if (const FSpinMenuRegion* Focused = SpinHUD->GetFocusedRegion())
		{
			const EMenuAction Action = DX < 0 ? Focused->LeftAction : Focused->RightAction;
			if (Action != EMenuAction::None)
			{
				SpinHUD->NotifyActivated({ Action, INDEX_NONE }, Focused->Rect, /*bAdjust*/ true);
				GM->HandleMenuAction(Action);
				return;
			}
		}
	}
	SpinHUD->MoveFocus(DX, DY);
}

void ASpinPlayerController::OnMenuConfirm()
{
	// Only on menus - elsewhere Enter does nothing.
	if (IsInMenuState())
	{
		ConfirmFocusedItem();
	}
}

void ASpinPlayerController::OnMenuBack()
{
	ASpinGameMode* GM = GetSpinGameMode();
	if (GM && IsInMenuState())
	{
		GM->HandleMenuBack();
	}
}

void ASpinPlayerController::PressButton(int32 ButtonIndex)
{
	if (ASpinGameMode* GM = GetSpinGameMode())
	{
		GM->HandleButtonPressed(ButtonIndex);
	}
}

void ASpinPlayerController::SetLift(bool bHeld)
{
	if (ASpinGameMode* GM = GetSpinGameMode())
	{
		GM->SetLiftHeld(bHeld);
	}
}
