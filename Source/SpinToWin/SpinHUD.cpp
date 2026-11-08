// Spin To Win - the 2D overlay: the in-run HUD, and every menu and pause screen.
// The dial, hand, target zone and obstacles are 3D meshes (see SpinClock).

#include "SpinHUD.h"
#include "SpinGameMode.h"
#include "SpinClock.h"
#include "SpinSaveGame.h"
#include "CanvasItem.h"
#include "EngineFontServices.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "GameFramework/PlayerController.h"
#include "GlobalRenderResources.h"
#include "Misc/DateTime.h"
#include "TextureResource.h"

namespace
{
	// ---- Palette: neon on near-black, to match the city. Each colour has one job. ----
	const FLinearColor PanelFill(0.006f, 0.012f, 0.03f, 0.88f);
	const FLinearColor PanelFillLow(0.014f, 0.006f, 0.032f, 0.92f); // bottom of the panel gradient: a touch of violet
	const FLinearColor PanelEdge(0.3f, 0.55f, 0.8f, 0.28f);
	const FLinearColor TrackColor(0.05f, 0.07f, 0.12f, 0.95f);
	const FLinearColor BackdropColor(0.004f, 0.006f, 0.018f, 1.f);
	const FLinearColor TextMain(0.92f, 0.97f, 1.f);
	const FLinearColor TextDim(0.58f, 0.68f, 0.8f);
	const FLinearColor TextFaint(0.36f, 0.43f, 0.56f);
	const FLinearColor Cyan(0.f, 0.9f, 1.f);       // UI accent, level, the lift
	const FLinearColor Magenta(1.f, 0.12f, 0.78f); // second UI accent, glitches
	const FLinearColor Gold(1.f, 0.8f, 0.16f);     // score, bests, PERFECT
	const FLinearColor Green(0.2f, 1.f, 0.55f);    // good things: +1, repair, GO
	const FLinearColor Orange(1.f, 0.5f, 0.08f);   // warnings, miss lives, unlocks
	const FLinearColor Red(1.f, 0.2f, 0.32f);      // danger, obstacle lives, game over
	const FLinearColor Purple(0.66f, 0.32f, 1.f);  // run modifiers and the daily challenge
	const FLinearColor SparkColor(1.f, 0.72f, 0.3f);

	// ---- Type scale, in points at 1080p. ----
	constexpr float HeroSize = 72.f;
	constexpr float TitleSize = 40.f;
	constexpr float HeadingSize = 26.f;
	constexpr float BodySize = 19.f;
	constexpr float SmallSize = 15.f;
	constexpr float TinySize = 12.f;
	constexpr float ChromeSize = 11.f;

	// ---- Spacing, in design pixels. ----
	constexpr float Margin = 28.f;
	constexpr float Pad = 16.f;

	/** How long a screen takes to switch on, and the highlight to ease in or out. */
	constexpr float RevealDuration = 0.32f;
	constexpr float FocusEaseTime = 0.12f;
	constexpr float FlashDuration = 0.35f;

	constexpr int32 MaxSparks = 320;

	FLinearColor Fade(FLinearColor Color, float Alpha)
	{
		Color.A *= FMath::Clamp(Alpha, 0.f, 1.f);
		return Color;
	}

	/** A colour for additive drawing: brightness goes in the RGB (additive blending ignores alpha). */
	FLinearColor GlowOf(const FLinearColor& Color, float Intensity)
	{
		const float I = FMath::Max(0.f, Intensity);
		return FLinearColor(Color.R * I, Color.G * I, Color.B * I, 1.f);
	}

	/** 1.5 -> "x1.5", 2 -> "x2", 1.25 -> "x1.25". */
	FString FormatMultiplier(float Value)
	{
		FString Text = FString::Printf(TEXT("%.2f"), Value);
		while (Text.EndsWith(TEXT("0")))
		{
			Text.LeftChopInline(1);
		}
		Text.RemoveFromEnd(TEXT("."));
		return TEXT("x") + Text;
	}

	/** 12345 -> "12,345". */
	FString FormatPoints(int32 Value)
	{
		return FText::AsNumber(Value).ToString();
	}

	FLinearColor GetGradeColor(ESpinHitGrade Grade)
	{
		switch (Grade)
		{
		case ESpinHitGrade::Perfect: return Gold;
		case ESpinHitGrade::Good:    return Green;
		default:                     return TextDim;
		}
	}

	/** The combo gets hotter as it climbs: cyan -> gold -> red. */
	FLinearColor GetComboColor(int32 Multiplier, int32 MaxMultiplier)
	{
		const float T = FMath::Clamp((Multiplier - 1) / FMath::Max(1.f, MaxMultiplier - 1.f), 0.f, 1.f);
		return T < 0.5f ? FMath::Lerp(Cyan, Gold, T * 2.f) : FMath::Lerp(Gold, Red, (T - 0.5f) * 2.f);
	}

	/** A button's name on the input in use ("W", "A" or "X"). */
	FString KeyHintOf(const ASpinGameMode& GM, int32 Index)
	{
		return GM.GetButtonKey(Index);
	}

	FLinearColor KeyColorOf(const ASpinGameMode& GM, int32 Index)
	{
		return GM.Prompts.IsValidIndex(Index) ? GM.Prompts[Index].Color : TextMain;
	}

	bool IsPadInput(const ASpinGameMode& GM)
	{
		return GM.InputStyle != ESpinInputStyle::Keyboard;
	}

	/** What selects the highlighted menu item / backs out, on the input in use. */
	FString ConfirmKeyOf(const ASpinGameMode& GM)
	{
		return IsPadInput(GM) ? GM.GetButtonKey(0) : FString(TEXT("ENTER"));
	}

	FString BackKeyOf(const ASpinGameMode& GM)
	{
		return IsPadInput(GM) ? GM.GetButtonKey(1) : FString(TEXT("ESC"));
	}

	float Pulse01(float Time, float Speed)
	{
		return 0.5f + 0.5f * FMath::Sin(Time * Speed);
	}

	float EaseOut(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return 1.f - FMath::Cube(1.f - T);
	}

	/** A cheap, stable 0..1 pseudo-random value for an integer, for effects that must look random
	 *  but come out the same every frame (which LEDs are lit this beat, where an arc strikes). */
	float Hash01(uint32 X)
	{
		X ^= X >> 16;
		X *= 0x7feb352dU;
		X ^= X >> 15;
		X *= 0x846ca68bU;
		X ^= X >> 16;
		return static_cast<float>(X & 0xFFFFFFu) / 16777216.f;
	}

	uint32 SeedOf(float X, float Y)
	{
		return static_cast<uint32>(FMath::RoundToInt(X)) * 73856093u ^ static_cast<uint32>(FMath::RoundToInt(Y)) * 19349663u;
	}

	/** A neon tube's brightness: steady almost all the time, with the occasional stutter. */
	float NeonFlicker(float Time, uint32 Seed, float Rarity = 0.03f)
	{
		const int32 Slot = FMath::FloorToInt(Time * 9.f);
		const float R = Hash01(static_cast<uint32>(Slot) * 2654435761u + Seed);
		if (R < Rarity * 0.5f)
		{
			return 0.25f;
		}
		return R < Rarity ? 0.6f : 1.f;
	}

	/** A small greyscale mask texture (white, with the mask in every channel), so it works both as
	 *  a translucent alpha mask and as an additive glow. */
	UTexture2D* MakeMaskTexture(int32 W, int32 H, TextureFilter Filter, TextureAddress Address, TFunctionRef<float(float, float)> Mask)
	{
		TArray<uint8> Pixels;
		Pixels.SetNumUninitialized(W * H * 4);
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				const float A = FMath::Clamp(Mask((X + 0.5f) / W, (Y + 0.5f) / H), 0.f, 1.f);
				const uint8 Byte = static_cast<uint8>(FMath::RoundToInt(A * 255.f));
				uint8* P = &Pixels[(Y * W + X) * 4];
				P[0] = P[1] = P[2] = P[3] = Byte;
			}
		}

		UTexture2D* Texture = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8, NAME_None, TConstArrayView64<uint8>(Pixels.GetData(), Pixels.Num()));
		if (Texture)
		{
			Texture->SRGB = false;
			Texture->Filter = Filter;
			Texture->AddressX = Address;
			Texture->AddressY = Address;
			Texture->UpdateResource();
		}
		return Texture;
	}
}

// =============================================================================================
// Frame
// =============================================================================================

void ASpinHUD::DrawHUD()
{
	Super::DrawHUD();

	const ASpinGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ASpinGameMode>() : nullptr;
	if (!GM || !Canvas)
	{
		return;
	}

	ScreenW = Canvas->SizeX;
	ScreenH = Canvas->SizeY;
	S = FMath::Clamp(ScreenH / 1080.f, 0.5f, 4.f);
	Now = GetWorld()->GetTimeSeconds();
	const float RealNow = GetWorld()->GetRealTimeSeconds();
	UiDelta = FMath::Clamp(RealNow - UiTime, 0.f, 0.1f);
	UiTime = RealNow;
	EnsureTextures();
	DrawOpacity = 1.f;

	// Once a run ends, the game-over screen holds off while the dial collapses, then fades in. Until
	// it shows, the HUD stays on the run's screen (so its switch-on plays when it actually appears).
	const float GameOverAlpha = GM->State == ESpinState::GameOver ? GM->GetGameOverScreenAlpha() : 1.f;
	const bool bGameOverHeld = GM->State == ESpinState::GameOver && GameOverAlpha <= 0.f;

	bHaveMouse = PlayerOwner && PlayerOwner->GetMousePosition(MousePos.X, MousePos.Y);

	// Which screen is up. Switching screens replays the switch-on and resets (or restores) the highlight.
	const bool bPaused = GM->IsPauseMenuOpen();
	const int32 Screen = bPaused ? PauseScreen : static_cast<int32>(bGameOverHeld ? ESpinState::Playing : GM->State);
	if (Screen != CurrentScreen)
	{
		OnScreenChanged(Screen);
	}

	// The mouse takes the highlight only when it actually moves, so a cursor left resting over a
	// button doesn't fight the controller. Tested against last frame's items, before they're rebuilt.
	if (bHaveMouse)
	{
		if (bHadMouse && FVector2D::DistSquared(MousePos, LastMousePos) > 1.f)
		{
			if (const FSpinMenuRegion* Hovered = FindRegionAt(MousePos, true))
			{
				SetFocus(Hovered->GetKey());
			}
		}
		LastMousePos = MousePos;
	}
	bHadMouse = bHaveMouse;

	MenuRegions.Reset();
	bHasDefaultFocus = false;
	AmbientLevel = 1.f;

	switch (GM->State)
	{
	case ESpinState::WaitingToStart:   DrawMainMenu(*GM); break;
	case ESpinState::ModeSelect:       DrawModeSelect(*GM); break;
	case ESpinState::ModifierSelect:   DrawRunSetup(*GM); break;
	case ESpinState::SettingsMenu:     DrawSettings(*GM); break;
	case ESpinState::Leaderboard:      DrawLeaderboard(*GM); break;
	case ESpinState::ProfileSelect:    DrawProfileSelect(*GM); break;
	case ESpinState::NameEntry:        DrawNameEntry(*GM); break;
	case ESpinState::Playing:
		AmbientLevel = 0.45f;
		DrawRunHUD(*GM);
		if (bPaused)
		{
			AmbientLevel = 1.f;
			DrawPauseMenu(*GM);
		}
		break;
	case ESpinState::GameOver:
		if (bGameOverHeld)
		{
			// Nothing but the dial breaking down.
			DrawGlitches(*GM);
		}
		else
		{
			DrawOpacity = GameOverAlpha;
			DrawGameOver(*GM);
		}
		break;

	case ESpinState::NewButtonIntro:
		if (GM->Prompts.IsValidIndex(GM->NewButtonIndex))
		{
			// Every button now in play, in its zone colour, with the new one last.
			const int32 NumButtons = GM->NewButtonIndex + 1;
			const FSpinPrompt& New = GM->Prompts[GM->NewButtonIndex];
			TArray<FSpinBriefLine> Lines;
			Lines.Add({ TEXT("Match the zone's colour:"), TextMain });
			for (int32 i = 0; i < NumButtons; ++i)
			{
				Lines.Add({ GM->GetButtonKey(i), GM->Prompts[i].Color });
			}
			Lines.Add({ TEXT("Speed and zone size reset to help you adjust."), TextDim });

			DrawBriefing(TEXT("NEW MECHANIC"), NumButtons == 2 ? TEXT("SECOND BUTTON") : TEXT("THIRD BUTTON"),
				Lines, TEXT("Press"), GM->GetButtonKey(GM->NewButtonIndex), New.Color);
		}
		break;

	case ESpinState::ObstacleIntro:
		DrawBriefing(TEXT("NEW MECHANIC"), TEXT("OBSTACLES"),
			{
				{ FString::Printf(TEXT("Hold %s to lift the hand over posts."), *GM->GetLiftKey(false)), Cyan },
				{ TEXT("Lifting costs points - only lift when needed."), TextDim },
				{ FString::Printf(TEXT("Hit a post: -1 life. Clear %d: +1 life."), GM->ObstaclesPerLife), FLinearColor(1.f, 0.65f, 0.65f) },
			},
			TEXT("Hold"), GM->GetLiftKey(), Cyan);
		break;

	case ESpinState::HighObstacleIntro:
		DrawBriefing(TEXT("NEW HAZARD"), TEXT("HIGH POSTS"),
			{
				{ TEXT("Purple posts float above the dial."), Purple },
				{ TEXT("Don't lift - the hand passes right under them."), TextMain },
				{ TEXT("Lift into one: -1 life. They never give lives back."), FLinearColor(1.f, 0.65f, 0.65f) },
			},
			TEXT("Press"), KeyHintOf(*GM, 0), KeyColorOf(*GM, 0));
		break;

	case ESpinState::JamIntro:
	{
		const FString Key = GM->GetRepairKey();
		DrawBriefing(TEXT("NEW MECHANIC"), TEXT("JAMS"),
			{
				{ TEXT("Sparks mean a jam is coming."), Orange },
				{ FString::Printf(TEXT("When it jams, mash %s to fix it."), *Key), Gold },
				{ TEXT("Too slow costs a miss."), TextDim },
			},
			TEXT("Press"), Key, Gold);
		break;
	}

	case ESpinState::EndlessIntro:
	{
		TArray<FSpinBriefLine> Lines;
		Lines.Add({ TEXT("Mistakes count now."), TextMain });
		Lines.Add({ FString::Printf(TEXT("LIVES (left): %d. Hit a post -1, clear %d for +1."), GM->GetRunMaxLives(), GM->ObstaclesPerLife), Red });
		Lines.Add({ FString::Printf(TEXT("MISSES (right): %d. Missed zone or wrong button -1."), GM->GetRunMaxMissLives()), Orange });
		if (GM->MissRegenLevels > 0)
		{
			Lines.Add({ FString::Printf(TEXT("+1 miss back every %d levels."), GM->MissRegenLevels), TextDim });
		}
		Lines.Add({ TEXT("Lose every life: -1 miss, and your lives refill."), TextDim });
		Lines.Add({ TEXT("Run out of misses and it's game over."), TextDim });
		DrawBriefing(TEXT("TUTORIAL COMPLETE"), FString::Printf(TEXT("ENDLESS - LEVEL %d"), GM->GetLevel()),
			Lines, TEXT("Press"), KeyHintOf(*GM, 0), KeyColorOf(*GM, 0));
		break;
	}

	case ESpinState::TutorialIntro:
		if (const FTutorialLevel* Level = GM->GetTutorialLevel())
		{
			TArray<FSpinBriefLine> Lines;
			for (const FString& Line : Level->IntroLines)
			{
				Lines.Add({ GM->ExpandKeys(Line), TextMain });
			}

			const FString Eyebrow = FString::Printf(TEXT("TUTORIAL  -  STAGE %d / %d"), GM->TutorialIndex + 1, GM->TutorialLevels.Num());
			switch (Level->Confirm)
			{
			case ETutorialConfirm::FirstButton:
				DrawBriefing(Eyebrow, Level->Title, Lines, TEXT("Press"), KeyHintOf(*GM, 0), KeyColorOf(*GM, 0));
				break;
			case ETutorialConfirm::SecondButton:
				DrawBriefing(Eyebrow, Level->Title, Lines, TEXT("Press"), KeyHintOf(*GM, 1), KeyColorOf(*GM, 1));
				break;
			case ETutorialConfirm::Repair:
				DrawBriefing(Eyebrow, Level->Title, Lines, TEXT("Press"), GM->GetRepairKey(), Gold);
				break;
			case ETutorialConfirm::Lift:
				DrawBriefing(Eyebrow, Level->Title, Lines, TEXT("Hold"), GM->GetLiftKey(), Cyan);
				break;
			}
		}
		break;

	default:
		break;
	}

	// Menu chrome on every screen but the run itself.
	const bool bInRun = (GM->State == ESpinState::Playing && !bPaused) || bGameOverHeld;
	if (!bInRun)
	{
		AmbientLevel = 1.f;
		DrawScreenFrame(*GM);
		if (GM->IsMenuScreen())
		{
			DrawNavLegend(*GM);
		}
	}

	ValidateFocus();
	UpdateFocusAnimations();

	if (!bInRun)
	{
		DrawScreenReveal();
	}
	UpdateAndDrawSparks();
	DrawOpacity = 1.f;
}

FLinearColor ASpinHUD::WithOpacity(const FLinearColor& Color, bool bAdditive) const
{
	if (DrawOpacity >= 1.f)
	{
		return Color;
	}
	if (bAdditive)
	{
		return FLinearColor(Color.R * DrawOpacity, Color.G * DrawOpacity, Color.B * DrawOpacity, Color.A * DrawOpacity);
	}
	return FLinearColor(Color.R, Color.G, Color.B, Color.A * DrawOpacity);
}

void ASpinHUD::DrawRect(FLinearColor RectColor, float RectX, float RectY, float RectW, float RectH)
{
	Super::DrawRect(WithOpacity(RectColor), RectX, RectY, RectW, RectH);
}

// =============================================================================================
// Primitives
// =============================================================================================

FSlateFontInfo ASpinHUD::MakeFont(float Size, bool bBold) const
{
	// The engine's default HUD font is a runtime (composite) font with Regular and Bold faces, so
	// text can be rasterized at exactly the size it's shown - no scaled-up, soft-edged glyphs.
	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	FSlateFontInfo Info = Font ? Font->GetLegacySlateFontInfo() : FSlateFontInfo();

	// Whole points only: every distinct size is its own set of cached glyphs.
	Info.Size = FMath::Max(6.f, FMath::RoundToFloat(Size * S));
	Info.TypefaceFontName = bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
	return Info;
}

FVector2D ASpinHUD::MeasureText(const FString& Text, float Size, bool bBold) const
{
	if (Text.IsEmpty())
	{
		return FVector2D::ZeroVector;
	}

	const TSharedPtr<FSlateFontMeasure> Measure = FEngineFontServices::Get().GetFontMeasure();
	if (!Measure.IsValid())
	{
		return FVector2D::ZeroVector;
	}

	const FVector2D Result = Measure->Measure(Text, MakeFont(Size, bBold));
	return Result;
}

float ASpinHUD::LineH(float Size) const
{
	const float Measured = MeasureText(TEXT("Ag"), Size).Y;
	return Measured > 0.f ? Measured : U(Size) * 1.5f;
}

float ASpinHUD::DrawLabel(const FString& Text, float X, float Y, float Size, const FLinearColor& Color, ESpinTextAlign Align, bool bBold, bool bShadow)
{
	if (Text.IsEmpty() || !Canvas || Color.A <= 0.f)
	{
		return 0.f;
	}

	const FSlateFontInfo Font = MakeFont(Size, bBold);
	if (!Font.FontObject)
	{
		return 0.f;
	}

	const float Width = MeasureText(Text, Size, bBold).X;
	float DrawX = X;
	if (Align == ESpinTextAlign::Center)
	{
		DrawX -= Width * 0.5f;
	}
	else if (Align == ESpinTextAlign::Right)
	{
		DrawX -= Width;
	}

	FCanvasTextItem Item(FVector2D(FMath::RoundToFloat(DrawX), FMath::RoundToFloat(Y)), FText::FromString(Text), Font, WithOpacity(Color));

	// A soft shadow lifts light text off the busy 3D scene. Dark text (on a lit key cap) doesn't need one.
	if (bShadow && Color.GetLuminance() > 0.3f)
	{
		const float Offset = FMath::Max(1.f, U(2.f));
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.75f * Color.A * DrawOpacity), FVector2D(Offset, Offset));
	}

	Canvas->DrawItem(Item);
	return Width;
}

float ASpinHUD::DrawNeonText(const FString& Text, float X, float Y, float Size, const FLinearColor& Color, const FLinearColor& GlowColor,
	float Glow, ESpinTextAlign Align, float Aberration)
{
	if (Text.IsEmpty() || Color.A <= 0.f)
	{
		return 0.f;
	}

	const float Width = MeasureText(Text, Size, true).X;
	const float Height = LineH(Size);
	const float CenterX = Align == ESpinTextAlign::Center ? X : Align == ESpinTextAlign::Right ? X - Width * 0.5f : X + Width * 0.5f;

	// The tube's halo: a soft wash behind the whole word, wider than it is tall.
	if (Glow > 0.f)
	{
		DrawGlowSprite(CenterX, Y + Height * 0.5f, Width * 1.3f + U(Size * 1.2f), Height * 1.9f, GlowColor, Glow * Color.A);
	}

	if (Aberration > 0.f)
	{
		DrawLabel(Text, X - Aberration, Y, Size, Fade(Magenta, 0.55f * Color.A), Align, true, false);
		DrawLabel(Text, X + Aberration, Y, Size, Fade(Cyan, 0.55f * Color.A), Align, true, false);
	}
	DrawLabel(Text, X, Y, Size, Color, Align, true);
	return Width;
}

void ASpinHUD::DrawNeonOutlineText(const FString& Text, float X, float Y, float Size, const FLinearColor& Tube, float Brightness, ESpinTextAlign Align)
{
	if (Text.IsEmpty())
	{
		return;
	}

	// Canvas text has no outline option, so the outline is built by drawing the word in a ring of
	// offsets: each ring is the letters grown by that radius. Widest first, then the dark fill on top
	// hollows the letters out, leaving only the tube around them.
	struct FLayer { float Radius; FLinearColor Color; };
	const FLinearColor Core = FMath::Lerp(Tube, FLinearColor::White, 0.75f);
	const FLayer TubeLayers[] =
	{
		{ 10.f, Fade(Tube, 0.025f * Brightness) }, // halo hugging the letter shapes
		{ 6.5f, Fade(Tube, 0.06f * Brightness) },
		{ 3.6f, Fade(Tube, Brightness) },          // the tube
		{ 1.8f, Fade(Core, Brightness) },          // its white-hot inner edge
	};
	const int32 Steps = 16;
	for (const FLayer& Layer : TubeLayers)
	{
		const float R = U(Layer.Radius);
		for (int32 i = 0; i < Steps; ++i)
		{
			const float Angle = UE_TWO_PI * i / Steps;
			DrawLabel(Text, X + FMath::Cos(Angle) * R, Y + FMath::Sin(Angle) * R, Size, Layer.Color, Align, true, false);
		}
	}
	DrawLabel(Text, X, Y, Size, FLinearColor(0.02f, 0.008f, 0.035f, 0.92f), Align, true, false);
}

TArray<FString> ASpinHUD::WrapText(const FString& Text, float Size, bool bBold, float MaxWidth) const
{
	TArray<FString> Words;
	Text.ParseIntoArray(Words, TEXT(" "), true);

	TArray<FString> Lines;
	FString Line;
	for (const FString& Word : Words)
	{
		const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
		if (!Line.IsEmpty() && MeasureText(Candidate, Size, bBold).X > MaxWidth)
		{
			Lines.Add(Line);
			Line = Word;
		}
		else
		{
			Line = Candidate;
		}
	}
	if (!Line.IsEmpty())
	{
		Lines.Add(Line);
	}
	return Lines;
}

float ASpinHUD::DrawWrapped(const FString& Text, float CenterX, float Y, float Size, const FLinearColor& Color, float MaxWidth, bool bBold)
{
	for (const FString& Line : WrapText(Text, Size, bBold, MaxWidth))
	{
		DrawLabel(Line, CenterX, Y, Size, Color, ESpinTextAlign::Center, bBold);
		Y += LineH(Size);
	}
	return Y;
}

void ASpinHUD::DrawBigText(const FString& Text, const FLinearColor& Color, float Scale, float CenterX, float CenterY)
{
	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	if (!Font || !Canvas || Color.A <= 0.f)
	{
		return;
	}

	// A neon halo behind it, scaled with the text.
	DrawGlowSprite(CenterX, CenterY, U(Scale * 34.f), U(Scale * 20.f), Color, 0.4f * Color.A);

	// One fixed raster size (a single font-cache entry however the size animates), scaled from there.
	const float RenderSize = 160.f;
	FSlateFontInfo Info = Font->GetLegacySlateFontInfo();
	Info.Size = RenderSize;
	Info.TypefaceFontName = FName(TEXT("Bold"));

	FCanvasTextItem Item(FVector2D(CenterX, CenterY), FText::FromString(Text), Info, WithOpacity(Color));
	Item.Scale = FVector2D(Scale * 10.f * S / RenderSize);
	Item.bCentreX = true;
	Item.bCentreY = true;
	Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.6f * Color.A * DrawOpacity), FVector2D(U(4.f), U(4.f)));
	Canvas->DrawItem(Item);
}

// ---------------------------------------------------------------------------------------------
// Glow and shape drawing
// ---------------------------------------------------------------------------------------------

void ASpinHUD::EnsureTextures()
{
	if (!GlowTexture)
	{
		// A round glow that falls off softly to nothing at the edge.
		GlowTexture = MakeMaskTexture(64, 64, TF_Bilinear, TA_Clamp, [](float U0, float V0)
		{
			const float D = FMath::Sqrt(FMath::Square(U0 - 0.5f) + FMath::Square(V0 - 0.5f)) * 2.f;
			return FMath::Pow(FMath::Clamp(1.f - D, 0.f, 1.f), 2.2f);
		});
	}
	if (!BeamTexture)
	{
		// Across a neon tube: a bright core inside a wide soft halo. Constant along its length.
		BeamTexture = MakeMaskTexture(2, 32, TF_Bilinear, TA_Clamp, [](float, float V0)
		{
			const float D = FMath::Abs(V0 - 0.5f) * 2.f;
			return (1.f - D) * (0.55f * FMath::Exp(-D * D * 5.f) + 0.45f * FMath::Exp(-D * D * 50.f));
		});
	}
	if (!FadeRightTexture)
	{
		FadeRightTexture = MakeMaskTexture(64, 2, TF_Bilinear, TA_Clamp, [](float U0, float) { return FMath::Pow(1.f - U0, 1.6f); });
	}
	if (!FadeDownTexture)
	{
		FadeDownTexture = MakeMaskTexture(2, 64, TF_Bilinear, TA_Clamp, [](float, float V0) { return FMath::Pow(1.f - V0, 1.6f); });
	}
	if (!ScanTexture)
	{
		// One dark line every three pixels, tiled across the screen.
		ScanTexture = MakeMaskTexture(1, 3, TF_Nearest, TA_Wrap, [](float, float V0) { return V0 < 0.34f ? 1.f : 0.f; });
	}
}

void ASpinHUD::DrawTex(UTexture2D* Texture, float X, float Y, float W, float H, const FLinearColor& Color, bool bAdditive,
	const FVector2D& UV0, const FVector2D& UV1, float AngleDeg)
{
	const FTexture* Resource = Texture ? Texture->GetResource() : nullptr;
	if (!Canvas || !Resource || W <= 0.f || H <= 0.f)
	{
		return;
	}

	FCanvasTileItem Item(FVector2D(X, Y), Resource, FVector2D(W, H), UV0, UV1, WithOpacity(Color, bAdditive));
	Item.BlendMode = bAdditive ? SE_BLEND_Additive : SE_BLEND_Translucent;
	if (AngleDeg != 0.f)
	{
		Item.Rotation = FRotator(0.f, AngleDeg, 0.f);
		Item.PivotPoint = FVector2D(0.5f, 0.5f);
	}
	Canvas->DrawItem(Item);
}

void ASpinHUD::DrawGlowSprite(float CenterX, float CenterY, float W, float H, const FLinearColor& Color, float Intensity)
{
	if (Intensity > 0.001f)
	{
		DrawTex(GlowTexture, CenterX - W * 0.5f, CenterY - H * 0.5f, W, H, GlowOf(Color, Intensity), true);
	}
}

void ASpinHUD::DrawBeam(const FVector2D& A, const FVector2D& B, float Width, const FLinearColor& Color, float Intensity)
{
	const FVector2D Delta = B - A;
	const float Length = Delta.Size();
	if (Length < 0.5f || Intensity <= 0.001f)
	{
		return;
	}

	const FVector2D Mid = (A + B) * 0.5f;
	const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
	DrawTex(BeamTexture, Mid.X - Length * 0.5f, Mid.Y - Width * 0.5f, Length, Width, GlowOf(Color, Intensity), true,
		FVector2D(0.f, 0.f), FVector2D(1.f, 1.f), Angle);
}

void ASpinHUD::DrawNeonLine(const FVector2D& A, const FVector2D& B, const FLinearColor& Color, float Thickness, float Glow)
{
	if (Glow > 0.f)
	{
		DrawBeam(A, B, Thickness * 2.f + U(12.f), Color, Glow);
	}
	// Drawn as a thin quad rather than with AHUD::DrawLine: the engine's line drawing forces alpha
	// to 1 (FBatchedElements::AddLine), so lines would ignore fades and translucency. FillPolygon
	// honours both (and applies DrawOpacity itself).
	const FVector2D Delta = B - A;
	const float Length = Delta.Size();
	if (Color.A <= 0.f || Length < KINDA_SMALL_NUMBER)
	{
		return;
	}
	const FVector2D Dir = Delta / Length;
	const float Half = FMath::Max(0.5f, Thickness * 0.5f);
	const FVector2D Side = FVector2D(-Dir.Y, Dir.X) * Half;
	const FVector2D Ext = Dir * Half; // reach past the ends, so joined segments meet at the corners
	const FVector2D Quad[4] = { A - Ext + Side, B + Ext + Side, B + Ext - Side, A - Ext - Side };
	FillPolygon(Quad, 4, Color, Color);
}

void ASpinHUD::FillPolygon(const FVector2D* Points, int32 NumPoints, const FLinearColor& Top, const FLinearColor& Bottom, bool bAdditive)
{
	if (!Canvas || NumPoints < 3 || !GWhiteTexture)
	{
		return;
	}

	FVector2D Center = FVector2D::ZeroVector;
	float MinY = Points[0].Y;
	float MaxY = Points[0].Y;
	for (int32 i = 0; i < NumPoints; ++i)
	{
		Center += Points[i];
		MinY = FMath::Min(MinY, Points[i].Y);
		MaxY = FMath::Max(MaxY, Points[i].Y);
	}
	Center /= NumPoints;

	const float SpanY = FMath::Max(1.f, MaxY - MinY);
	auto ColorAt = [&](const FVector2D& P) { return WithOpacity(FMath::Lerp(Top, Bottom, (P.Y - MinY) / SpanY), bAdditive); };

	// A fan from the centre - fine for the convex shapes drawn here.
	TArray<FCanvasUVTri> Tris;
	Tris.Reserve(NumPoints);
	for (int32 i = 0; i < NumPoints; ++i)
	{
		const FVector2D& P0 = Points[i];
		const FVector2D& P1 = Points[(i + 1) % NumPoints];
		FCanvasUVTri Tri;
		Tri.V0_Pos = Center;
		Tri.V1_Pos = P0;
		Tri.V2_Pos = P1;
		Tri.V0_UV = Tri.V1_UV = Tri.V2_UV = FVector2D(0.5f, 0.5f);
		Tri.V0_Color = ColorAt(Center);
		Tri.V1_Color = ColorAt(P0);
		Tri.V2_Color = ColorAt(P1);
		Tris.Add(Tri);
	}

	FCanvasTriangleItem Item(Tris, GWhiteTexture);
	Item.BlendMode = bAdditive ? SE_BLEND_Additive : SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void ASpinHUD::ChamferRect(float X, float Y, float W, float H, float Cut, FVector2D (&Out)[6])
{
	Cut = FMath::Clamp(Cut, 0.f, FMath::Min(W, H) * 0.5f);
	Out[0] = FVector2D(X + Cut, Y);
	Out[1] = FVector2D(X + W, Y);
	Out[2] = FVector2D(X + W, Y + H - Cut);
	Out[3] = FVector2D(X + W - Cut, Y + H);
	Out[4] = FVector2D(X, Y + H);
	Out[5] = FVector2D(X, Y + Cut);
}

void ASpinHUD::DrawOutline(float X, float Y, float W, float H, const FLinearColor& Color, float Thickness)
{
	DrawRect(Color, X, Y, W, Thickness);
	DrawRect(Color, X, Y + H - Thickness, W, Thickness);
	DrawRect(Color, X, Y + Thickness, Thickness, H - Thickness * 2.f);
	DrawRect(Color, X + W - Thickness, Y + Thickness, Thickness, H - Thickness * 2.f);
}

void ASpinHUD::DrawCornerBrackets(float X, float Y, float W, float H, const FLinearColor& Color, float Length, float Thickness, float Glow)
{
	// All four corners, as L shapes.
	const FVector2D Corners[4] = { FVector2D(X, Y), FVector2D(X + W, Y), FVector2D(X + W, Y + H), FVector2D(X, Y + H) };
	const FVector2D Dirs[4] = { FVector2D(1.f, 1.f), FVector2D(-1.f, 1.f), FVector2D(-1.f, -1.f), FVector2D(1.f, -1.f) };
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector2D& C = Corners[i];
		DrawNeonLine(C, C + FVector2D(Dirs[i].X * Length, 0.f), Color, Thickness, Glow);
		DrawNeonLine(C, C + FVector2D(0.f, Dirs[i].Y * Length), Color, Thickness, Glow);
	}
}

void ASpinHUD::DrawPanel(float X, float Y, float W, float H, const FLinearColor& Accent, float Opacity)
{
	const float Cut = FMath::Min(U(16.f), FMath::Min(W, H) * 0.3f);
	FVector2D Pts[6];
	ChamferRect(X, Y, W, H, Cut, Pts);

	// Body: a dark gradient, with a faint wash of the accent just under the top edge.
	FillPolygon(Pts, 6, Fade(PanelFill, Opacity), Fade(PanelFillLow, Opacity));
	DrawTex(FadeDownTexture, X + Cut, Y, W - Cut, FMath::Min(H, U(44.f)), GlowOf(Accent, 0.07f * Opacity), true);

	// The neon edge, which stutters now and then like a real tube.
	const uint32 Seed = SeedOf(X, Y);
	const float Flicker = FMath::Lerp(1.f, NeonFlicker(UiTime, Seed, 0.02f), AmbientLevel);
	const float Thin = FMath::Max(1.f, U(1.2f));
	for (int32 i = 0; i < 6; ++i)
	{
		DrawNeonLine(Pts[i], Pts[(i + 1) % 6], Fade(Accent, 0.38f * Opacity * Flicker), Thin, 0.1f * Opacity * Flicker);
	}

	// Accent bar along the start of the top edge, and three slanted ticks at its far end.
	const float BarW = (W - Cut) * 0.42f;
	DrawNeonLine(Pts[0], Pts[0] + FVector2D(BarW, 0.f), Fade(Accent, 0.95f * Opacity), FMath::Max(2.f, U(3.f)), 0.4f * Opacity * Flicker);
	if (W > U(120.f))
	{
		const float TickW = U(5.f);
		const float TickH = U(6.f);
		for (int32 i = 0; i < 3; ++i)
		{
			const float TX = X + W - U(14.f) - U(10.f) * (2 - i);
			const FVector2D Tick[4] = { FVector2D(TX + TickH * 0.6f, Y + U(4.f)), FVector2D(TX + TickH * 0.6f + TickW, Y + U(4.f)),
				FVector2D(TX + TickW, Y + U(4.f) + TickH), FVector2D(TX, Y + U(4.f) + TickH) };
			FillPolygon(Tick, 4, Fade(Accent, 0.55f * Opacity), Fade(Accent, 0.55f * Opacity));
		}
	}

	// Brackets on the two square corners.
	const float Len = FMath::Min(U(14.f), FMath::Min(W, H) * 0.25f);
	const float Thick = FMath::Max(1.f, U(2.f));
	const FLinearColor BracketColor = Fade(Accent, 0.8f * Opacity);
	DrawNeonLine(Pts[1], Pts[1] + FVector2D(-Len, 0.f), BracketColor, Thick, 0.f);
	DrawNeonLine(Pts[1], Pts[1] + FVector2D(0.f, Len), BracketColor, Thick, 0.f);
	DrawNeonLine(Pts[4], Pts[4] + FVector2D(Len, 0.f), BracketColor, Thick, 0.f);
	DrawNeonLine(Pts[4], Pts[4] + FVector2D(0.f, -Len), BracketColor, Thick, 0.f);

	// A light running around the edge, each panel at its own pace.
	if (AmbientLevel > 0.f && W > U(60.f) && H > U(30.f))
	{
		float Lengths[6];
		float Perimeter = 0.f;
		for (int32 i = 0; i < 6; ++i)
		{
			Lengths[i] = FVector2D::Distance(Pts[i], Pts[(i + 1) % 6]);
			Perimeter += Lengths[i];
		}
		const float Speed = U(170.f) * (0.8f + 0.4f * Hash01(Seed));
		float Along = FMath::Fmod(UiTime * Speed + Hash01(Seed + 7u) * Perimeter, Perimeter);
		for (int32 i = 0; i < 6; ++i)
		{
			if (Along <= Lengths[i])
			{
				const FVector2D P = FMath::Lerp(Pts[i], Pts[(i + 1) % 6], Along / FMath::Max(1.f, Lengths[i]));
				DrawGlowSprite(P.X, P.Y, U(46.f), U(46.f), Accent, 0.3f * Opacity * AmbientLevel);
				DrawGlowSprite(P.X, P.Y, U(10.f), U(10.f), FLinearColor::White, 0.5f * Opacity * AmbientLevel);
				break;
			}
			Along -= Lengths[i];
		}
	}
}

void ASpinHUD::DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Fill)
{
	Fraction = FMath::Clamp(Fraction, 0.f, 1.f);
	DrawRect(TrackColor, X, Y, W, H);

	// Faint tick marks every tenth, on bars tall enough to show them.
	if (H >= U(8.f))
	{
		for (int32 i = 1; i < 10; ++i)
		{
			DrawRect(Fade(TextFaint, 0.35f), X + W * i / 10.f, Y, FMath::Max(1.f, U(1.f)), H);
		}
	}

	const float FillW = W * Fraction;
	if (FillW <= 0.f)
	{
		return;
	}
	DrawRect(Fill, X, Y, FillW, H);
	DrawBeam(FVector2D(X, Y + H * 0.5f), FVector2D(X + FillW, Y + H * 0.5f), H * 3.f + U(6.f), Fill, 0.22f * Fill.A);

	// A shimmer drifting along the fill, and a hot head at its end.
	const float ShimmerX = X + FillW * FMath::Fmod(UiTime * 0.45f, 1.f);
	DrawGlowSprite(ShimmerX, Y + H * 0.5f, U(40.f), H * 2.5f, FLinearColor::White, 0.12f * Fill.A);
	if (Fraction < 1.f)
	{
		DrawRect(Fade(FLinearColor::White, 0.85f * Fill.A), X + FillW - FMath::Max(1.f, U(2.f)), Y, FMath::Max(1.f, U(2.f)), H);
		DrawGlowSprite(X + FillW, Y + H * 0.5f, H * 4.f + U(10.f), H * 4.f + U(10.f), Fill, 0.45f * Fill.A);
	}
}

void ASpinHUD::DrawSegments(float X, float Y, float W, float H, int32 Count, int32 Lit, const FLinearColor& On)
{
	Count = FMath::Max(1, Count);
	const float Skew = FMath::Min(H * 0.6f, U(8.f));
	const float Gap = FMath::Max(2.f, U(4.f));
	const float SegW = (W - Skew - Gap * (Count - 1)) / Count;
	for (int32 i = 0; i < Count; ++i)
	{
		const float SX = X + i * (SegW + Gap);
		const FVector2D Seg[4] = { FVector2D(SX + Skew, Y), FVector2D(SX + Skew + SegW, Y), FVector2D(SX + SegW, Y + H), FVector2D(SX, Y + H) };
		const bool bLit = i < Lit;
		FillPolygon(Seg, 4, bLit ? On : TrackColor, bLit ? Fade(On, 0.8f) : TrackColor);
	}
	if (Lit > 0)
	{
		const float LitW = FMath::Min(Lit, Count) * (SegW + Gap) - Gap + Skew;
		DrawBeam(FVector2D(X, Y + H * 0.5f), FVector2D(X + LitW, Y + H * 0.5f), H * 3.f + U(6.f), On, 0.2f * On.A);
	}
}

void ASpinHUD::DrawCheckBox(float X, float Y, float Size, bool bOn, const FLinearColor& Color)
{
	DrawRect(TrackColor, X, Y, Size, Size);
	DrawOutline(X, Y, Size, Size, bOn ? Color : PanelEdge, FMath::Max(1.f, U(2.f)));
	if (bOn)
	{
		const float Inset = U(5.f);
		DrawRect(Color, X + Inset, Y + Inset, Size - Inset * 2.f, Size - Inset * 2.f);
		DrawGlowSprite(X + Size * 0.5f, Y + Size * 0.5f, Size * 2.4f, Size * 2.4f, Color, 0.4f);
	}
}

void ASpinHUD::DrawLockIcon(float X, float Y, float Size, const FLinearColor& Color)
{
	const float Thick = FMath::Max(1.f, U(2.f));
	const float BodyX = X + Size * 0.12f;
	const float BodyW = Size * 0.76f;
	const float BodyH = Size * 0.52f;
	const float BodyY = Y + Size - BodyH;

	// The shackle: an upside-down U standing on the body.
	const float ShackleW = Size * 0.5f;
	const float ShackleX = X + (Size - ShackleW) * 0.5f;
	const float ShackleY = Y + Size * 0.06f;
	DrawRect(Color, ShackleX, ShackleY, ShackleW, Thick);
	DrawRect(Color, ShackleX, ShackleY, Thick, BodyY - ShackleY);
	DrawRect(Color, ShackleX + ShackleW - Thick, ShackleY, Thick, BodyY - ShackleY);

	// The body, with a keyhole.
	DrawRect(Fade(Color, 0.25f), BodyX, BodyY, BodyW, BodyH);
	DrawOutline(BodyX, BodyY, BodyW, BodyH, Color, Thick);
	DrawRect(Color, X + Size * 0.5f - Thick * 0.5f, BodyY + BodyH * 0.3f, Thick, BodyH * 0.4f);
}

float ASpinHUD::KeyCapWidth(const FString& Key, float Size) const
{
	return MeasureText(Key, Size, true).X + U(22.f);
}

float ASpinHUD::KeyCapHeight(float Size) const
{
	return LineH(Size) + U(8.f);
}

float ASpinHUD::DrawKeyCap(const FString& Key, float X, float Y, float Size, const FLinearColor& Color, bool bLit)
{
	const float W = KeyCapWidth(Key, Size);
	const float H = KeyCapHeight(Size);

	FVector2D Pts[6];
	ChamferRect(X, Y, W, H, FMath::Min(U(6.f), H * 0.3f), Pts);
	const FLinearColor Fill = bLit ? Color : FLinearColor(Color.R * 0.12f, Color.G * 0.12f, Color.B * 0.12f, 0.9f * Color.A);
	if (bLit)
	{
		DrawGlowSprite(X + W * 0.5f, Y + H * 0.5f, W * 1.8f, H * 2.4f, Color, 0.35f * Color.A);
	}
	FillPolygon(Pts, 6, Fill, Fill);
	const float Thick = FMath::Max(1.f, U(1.5f));
	for (int32 i = 0; i < 6; ++i)
	{
		DrawNeonLine(Pts[i], Pts[(i + 1) % 6], Color, Thick, 0.f);
	}

	const FLinearColor TextColor = bLit ? FLinearColor(0.02f, 0.03f, 0.06f, Color.A) : Color;
	DrawLabel(Key, X + W * 0.5f, Y + U(4.f), Size, TextColor, ESpinTextAlign::Center, true);
	return W;
}

void ASpinHUD::DrawKeyPrompt(const FString& Before, const FString& Key, const FString& After, float CenterX, float Y, float Size, const FLinearColor& KeyColor)
{
	const float Space = U(10.f);
	const float BeforeW = MeasureText(Before, Size).X;
	const float AfterW = MeasureText(After, Size).X;
	const float CapW = KeyCapWidth(Key, Size);
	const float TotalW = BeforeW + (Before.IsEmpty() ? 0.f : Space) + CapW + (After.IsEmpty() ? 0.f : Space + AfterW);

	const float CapH = KeyCapHeight(Size);
	const float TextY = Y + (CapH - LineH(Size)) * 0.5f;
	const FLinearColor TextColor = Fade(TextMain, KeyColor.A);

	float X = CenterX - TotalW * 0.5f;
	if (!Before.IsEmpty())
	{
		DrawLabel(Before, X, TextY, Size, TextColor);
		X += BeforeW + Space;
	}
	X += DrawKeyCap(Key, X, Y, Size, KeyColor, false);
	if (!After.IsEmpty())
	{
		DrawLabel(After, X + Space, TextY, Size, TextColor);
	}
}

bool ASpinHUD::IsHovered(float X, float Y, float W, float H) const
{
	return bHaveMouse && MousePos.X >= X && MousePos.X <= X + W && MousePos.Y >= Y && MousePos.Y <= Y + H;
}

void ASpinHUD::AddRegion(float X, float Y, float W, float H, EMenuAction Action, int32 Index,
	EMenuAction LeftAction, EMenuAction RightAction, bool bFocusable, bool bClickable)
{
	FSpinMenuRegion Region;
	Region.Rect = FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H));
	Region.Action = Action;
	Region.Index = Index;
	Region.LeftAction = LeftAction;
	Region.RightAction = RightAction;
	Region.bFocusable = bFocusable;
	Region.bClickable = bClickable;
	MenuRegions.Add(Region);
}

void ASpinHUD::MarkDefaultFocus()
{
	if (MenuRegions.Num() > 0)
	{
		DefaultFocus = MenuRegions.Last().GetKey();
		bHasDefaultFocus = true;
	}
}

float ASpinHUD::FocusAmount(const FSpinFocusKey& Key) const
{
	const float* Amount = FocusAnim.Find(Key);
	return Amount ? EaseOut(*Amount) : 0.f;
}

float ASpinHUD::ActivateFlash(const FSpinFocusKey& Key) const
{
	if (!(Key == LastActivated))
	{
		return 0.f;
	}
	return FMath::Clamp(1.f - (UiTime - ActivatedTime) / FlashDuration, 0.f, 1.f);
}

void ASpinHUD::DrawMenuButton(float X, float Y, float W, float H, const FString& Label, EMenuAction Action,
	const FLinearColor& Accent, const FString& KeyHint, bool bEnabled, int32 Index)
{
	const FSpinFocusKey Key{ Action, Index };
	const float F = bEnabled ? FocusAmount(Key) : 0.f;
	const float Flash = bEnabled ? ActivateFlash(Key) : 0.f;
	const float Opacity = bEnabled ? 1.f : 0.35f;
	const float CX = X + W * 0.5f;
	const float CY = Y + H * 0.5f;

	FVector2D Pts[6];
	ChamferRect(X, Y, W, H, FMath::Min(U(12.f), H * 0.3f), Pts);

	// Highlighted: a halo around it, and the body takes on the accent.
	if (F > 0.f)
	{
		DrawGlowSprite(CX, CY, W * 1.25f, H * 2.4f, Accent, 0.2f * F);
	}
	// (Vertex colours are linear - these read a lot brighter on screen than the numbers suggest.)
	const FLinearColor TintTop(Accent.R * 0.07f, Accent.G * 0.07f, Accent.B * 0.07f, 0.94f);
	const FLinearColor TintBottom(Accent.R * 0.02f, Accent.G * 0.02f, Accent.B * 0.03f, 0.94f);
	FillPolygon(Pts, 6, Fade(FMath::Lerp(PanelFill, TintTop, F), Opacity), Fade(FMath::Lerp(PanelFillLow, TintBottom, F), Opacity));

	// A band of light sweeping across while it's highlighted.
	if (F > 0.f)
	{
		const float Sweep = FMath::Fmod(UiTime / 2.f, 1.f);
		const float SweepX = X + W * Sweep;
		DrawGlowSprite(SweepX, CY, U(70.f), H * 0.9f, FLinearColor::White, 0.09f * F * FMath::Sin(Sweep * UE_PI));
	}

	// Edge: dim at rest, a bright glowing tube when highlighted.
	const float Breathe = 0.85f + 0.15f * FMath::Sin(UiTime * 5.f);
	const FLinearColor EdgeColor = Fade(FMath::Lerp(PanelEdge, Accent, 0.35f + 0.65f * F), Opacity * FMath::Lerp(0.8f, 1.f, F));
	const float EdgeThick = FMath::Max(1.f, U(FMath::Lerp(1.f, 2.f, F)));
	for (int32 i = 0; i < 6; ++i)
	{
		DrawNeonLine(Pts[i], Pts[(i + 1) % 6], EdgeColor, EdgeThick, (0.06f + 0.4f * F * Breathe) * Opacity);
	}

	// Accent strip down the left edge.
	DrawNeonLine(Pts[5] + FVector2D(U(3.f), U(2.f)), Pts[4] + FVector2D(U(3.f), -U(4.f)), Fade(Accent, (0.7f + 0.3f * F) * Opacity),
		FMath::Max(2.f, U(3.f)), (0.15f + 0.3f * F) * Opacity);

	// Pressed: a white flash.
	if (Flash > 0.f)
	{
		FillPolygon(Pts, 6, GlowOf(Accent, 0.6f * Flash), GlowOf(FLinearColor::White, 0.35f * Flash), true);
	}

	// The label is centred in the space the key cap (if any) leaves free, and shrinks until it fits
	// there, so the two never overlap on narrow buttons. Highlighted, it nudges right to make room
	// for the chevrons.
	const float KeySize = 13.f;
	const float KeyW = KeyHint.IsEmpty() ? 0.f : KeyCapWidth(KeyHint, KeySize);
	const float KeyX = X + W - U(12.f) - KeyW;
	const float LabelLeft = X + U(12.f);
	const float LabelRight = KeyHint.IsEmpty() ? X + W - U(12.f) : KeyX - U(8.f);
	const float LabelRoom = LabelRight - LabelLeft;

	float LabelSize = H >= U(60.f) ? 24.f : 20.f;
	while (LabelSize > 12.f && MeasureText(Label, LabelSize, true).X > LabelRoom)
	{
		LabelSize -= 1.f;
	}
	// Stay centred on the whole button when the label fits there without reaching the key cap.
	const float LabelW = MeasureText(Label, LabelSize, true).X;
	const float Nudge = W > U(120.f) ? U(6.f) * F : 0.f;
	const float LabelCX = ((X + W * 0.5f + LabelW * 0.5f <= LabelRight) ? X + W * 0.5f : (LabelLeft + LabelRight) * 0.5f) + Nudge;
	const float LabelY = Y + (H - LineH(LabelSize)) * 0.5f;
	const FLinearColor LabelColor = Fade(FMath::Lerp(TextMain * 0.9f, FLinearColor::White, F), Opacity);
	DrawNeonText(Label, LabelCX, LabelY, LabelSize, LabelColor, Accent, 0.18f * F, ESpinTextAlign::Center);

	// Chevrons pointing at the label while highlighted.
	if (F > 0.01f && W > U(120.f))
	{
		const float ChevH = FMath::Min(H * 0.32f, U(14.f));
		const float Bob = U(3.f) * FMath::Sin(UiTime * 7.f);
		const float BaseX = LabelCX - LabelW * 0.5f - U(16.f) - ChevH * 1.4f + Bob;
		for (int32 i = 0; i < 2; ++i)
		{
			const float VX = BaseX + i * ChevH * 0.7f;
			const FVector2D Chev[3] = { FVector2D(VX, CY - ChevH * 0.5f), FVector2D(VX + ChevH * 0.6f, CY), FVector2D(VX, CY + ChevH * 0.5f) };
			FillPolygon(Chev, 3, Fade(Accent, F * (0.55f + 0.45f * i)), Fade(Accent, F * (0.55f + 0.45f * i)));
		}
	}

	if (!KeyHint.IsEmpty())
	{
		DrawKeyCap(KeyHint, KeyX, Y + (H - KeyCapHeight(KeySize)) * 0.5f, KeySize, Fade(Accent, Opacity), false);
	}

	if (bEnabled)
	{
		AddRegion(X, Y, W, H, Action, Index);
	}
}

void ASpinHUD::DrawFocusFrame(float X, float Y, float W, float H, const FLinearColor& Accent, const FSpinFocusKey& Key)
{
	const float F = FocusAmount(Key);
	const float Flash = ActivateFlash(Key);
	if (F <= 0.001f && Flash <= 0.f)
	{
		return;
	}

	DrawRect(Fade(Accent, 0.07f * F + 0.15f * Flash), X, Y, W, H);
	DrawGlowSprite(X + W * 0.5f, Y + H * 0.5f, W * 1.1f, H * 1.8f, Accent, 0.07f * F + 0.25f * Flash);

	// Target brackets that close in from outside as the highlight arrives.
	const float Out = FMath::Lerp(U(16.f), U(3.f), F);
	const float Len = FMath::Min(U(18.f), FMath::Min(W, H) * 0.35f);
	const float Pulse = 0.7f + 0.3f * FMath::Sin(UiTime * 6.f);
	DrawCornerBrackets(X - Out, Y - Out, W + Out * 2.f, H + Out * 2.f, Fade(Accent, F * Pulse), Len, FMath::Max(1.f, U(2.f)), 0.3f * F);

	// A marker bar on the left edge.
	DrawNeonLine(FVector2D(X, Y + U(4.f)), FVector2D(X, Y + H - U(4.f)), Fade(Accent, F), FMath::Max(2.f, U(3.f)), 0.35f * F);
}

void ASpinHUD::DrawArrowButton(float X, float Y, float Size, bool bRight, EMenuAction Action, float RowFocus)
{
	const float Flash = ActivateFlash({ Action, INDEX_NONE });
	const float Lit = FMath::Max3(RowFocus * 0.6f, IsHovered(X, Y, Size, Size) ? 1.f : 0.f, Flash);

	FVector2D Pts[6];
	ChamferRect(X, Y, Size, Size, Size * 0.22f, Pts);
	const FLinearColor Tint(Cyan.R * 0.07f, Cyan.G * 0.07f, Cyan.B * 0.07f, 0.94f);
	FillPolygon(Pts, 6, FMath::Lerp(PanelFill, Tint, Lit), FMath::Lerp(PanelFillLow, Tint, Lit * 0.6f));
	for (int32 i = 0; i < 6; ++i)
	{
		DrawNeonLine(Pts[i], Pts[(i + 1) % 6], FMath::Lerp(PanelEdge, Cyan, 0.4f + 0.6f * Lit), FMath::Max(1.f, U(1.5f)), 0.25f * Lit);
	}
	if (Flash > 0.f)
	{
		FillPolygon(Pts, 6, GlowOf(Cyan, 0.6f * Flash), GlowOf(FLinearColor::White, 0.4f * Flash), true);
	}

	// The arrow head, nudged in its direction when pressed.
	const float CX = X + Size * 0.5f + (bRight ? 1.f : -1.f) * U(3.f) * Flash;
	const float CY = Y + Size * 0.5f;
	const float A = Size * 0.2f;
	const float Dir = bRight ? 1.f : -1.f;
	const FVector2D Head[3] = { FVector2D(CX - Dir * A * 0.6f, CY - A), FVector2D(CX + Dir * A * 0.8f, CY), FVector2D(CX - Dir * A * 0.6f, CY + A) };
	const FLinearColor HeadColor = FMath::Lerp(TextDim, FLinearColor::White, Lit);
	FillPolygon(Head, 3, HeadColor, HeadColor);
}

void ASpinHUD::DrawBonusBadge(float X, float CenterY)
{
	const float Size = U(18.f);
	const float Y = CenterY - Size * 0.5f;
	DrawRect(Fade(Green, 0.2f), X, Y, Size, Size);
	DrawOutline(X, Y, Size, Size, Green, FMath::Max(1.f, U(1.5f)));

	// A plus sign.
	const float Arm = Size * 0.5f;
	const float Thick = FMath::Max(2.f, U(2.5f));
	DrawRect(Green, X + (Size - Arm) * 0.5f, CenterY - Thick * 0.5f, Arm, Thick);
	DrawRect(Green, X + (Size - Thick) * 0.5f, CenterY - Arm * 0.5f, Thick, Arm);
}

void ASpinHUD::DrawBackdrop(float Opacity)
{
	// Only a light dimming: on the menus the camera sits low over the dial (see ASpinClock's menu
	// shot), and the scene is the backdrop - the panels carry their own dark fills for legibility.
	DrawRect(Fade(BackdropColor, Opacity * 0.45f), 0.f, 0.f, ScreenW, ScreenH);
	const float CX = ScreenW * 0.5f;

	// A slow band of light scanning down the screen.
	const float BandY = FMath::Fmod(UiTime / 8.f, 1.f) * (ScreenH + U(300.f)) - U(150.f);
	DrawGlowSprite(CX, BandY, ScreenW * 1.6f, U(220.f), Cyan, 0.035f * Opacity);

	// Vignette: darker towards every edge.
	const float VW = ScreenW * 0.22f;
	const float VH = ScreenH * 0.22f;
	const FLinearColor Vignette = Fade(FLinearColor::Black, 0.55f * Opacity);
	DrawTex(FadeRightTexture, 0.f, 0.f, VW, ScreenH, Vignette, false);
	DrawTex(FadeRightTexture, ScreenW - VW, 0.f, VW, ScreenH, Vignette, false, FVector2D(1.f, 0.f), FVector2D(0.f, 1.f));
	DrawTex(FadeDownTexture, 0.f, 0.f, ScreenW, VH, Vignette, false);
	DrawTex(FadeDownTexture, 0.f, ScreenH - VH, ScreenW, VH, Vignette, false, FVector2D(0.f, 1.f), FVector2D(1.f, 0.f));

	// Scanlines over everything.
	DrawTex(ScanTexture, 0.f, 0.f, ScreenW, ScreenH, Fade(FLinearColor::Black, 0.22f * Opacity), false,
		FVector2D(0.f, 0.f), FVector2D(1.f, ScreenH / 3.f));
}

float ASpinHUD::DrawScreenTitle(const FString& Title, const FString& Subtitle, float CenterX, float Y, const FLinearColor& Accent)
{
	const float SinceEnter = UiTime - ScreenEnterTime;

	// The title glitches as the screen switches on, and now and then after that.
	const int32 GlitchSlot = FMath::FloorToInt(UiTime * 6.f);
	const bool bGlitch = SinceEnter < 0.45f || Hash01(static_cast<uint32>(GlitchSlot) * 911u) < 0.03f;
	const float Shake = bGlitch ? U(4.f) * (Hash01(static_cast<uint32>(FMath::FloorToInt(UiTime * 40.f))) - 0.5f) : 0.f;
	DrawNeonText(Title, CenterX + Shake, Y, TitleSize, TextMain, Accent, 0.32f, ESpinTextAlign::Center, bGlitch ? U(3.f) : U(1.f));
	Y += LineH(TitleSize) + U(6.f);

	// Underline: draws itself out from the centre, with thin rails and end diamonds either side.
	const float Grow = EaseOut((SinceEnter - 0.05f) / 0.45f);
	const float Half = U(40.f) * Grow;
	const float Thick = FMath::Max(2.f, U(3.f));
	DrawNeonLine(FVector2D(CenterX - Half, Y), FVector2D(CenterX + Half, Y), Accent, Thick, 0.5f);
	const float Rail = U(200.f) * Grow;
	const FLinearColor RailColor = Fade(Accent, 0.35f);
	DrawNeonLine(FVector2D(CenterX - Half - U(10.f) - Rail, Y), FVector2D(CenterX - Half - U(10.f), Y), RailColor, FMath::Max(1.f, U(1.f)), 0.f);
	DrawNeonLine(FVector2D(CenterX + Half + U(10.f), Y), FVector2D(CenterX + Half + U(10.f) + Rail, Y), RailColor, FMath::Max(1.f, U(1.f)), 0.f);
	for (const float Side : { -1.f, 1.f })
	{
		const float DX = CenterX + Side * (Half + U(14.f) + Rail);
		const float D = U(4.f);
		const FVector2D Diamond[4] = { FVector2D(DX, Y - D), FVector2D(DX + D, Y), FVector2D(DX, Y + D), FVector2D(DX - D, Y) };
		FillPolygon(Diamond, 4, Accent, Accent);
	}
	// A spark of light running along the rails.
	const float RunT = FMath::Fmod(UiTime * 0.6f, 1.f);
	const float RunX = FMath::Lerp(-1.f, 1.f, RunT) * (Half + U(14.f) + Rail);
	DrawGlowSprite(CenterX + RunX, Y, U(30.f), U(14.f), Accent, 0.5f * FMath::Sin(RunT * UE_PI));
	Y += U(16.f);

	if (!Subtitle.IsEmpty())
	{
		DrawLabel(Subtitle, CenterX, Y, BodySize, TextDim, ESpinTextAlign::Center);
		Y += LineH(BodySize);
	}
	return Y + U(20.f);
}

// =============================================================================================
// Ambient effects
// =============================================================================================

void ASpinHUD::DrawScreenFrame(const ASpinGameMode& GM)
{
	const float M = U(18.f);
	const float L = U(80.f);
	const float Thick = FMath::Max(2.f, U(2.5f));
	const float Left = M;
	const float Top = M;
	const float Right = ScreenW - M;
	const float Bottom = ScreenH - M;

	// Corner brackets: cyan top-left / bottom-right, magenta the other two.
	struct FCorner { FVector2D P; FVector2D Dir; FLinearColor Color; };
	const FCorner Corners[4] = {
		{ FVector2D(Left, Top), FVector2D(1.f, 1.f), Cyan },
		{ FVector2D(Right, Top), FVector2D(-1.f, 1.f), Magenta },
		{ FVector2D(Right, Bottom), FVector2D(-1.f, -1.f), Cyan },
		{ FVector2D(Left, Bottom), FVector2D(1.f, -1.f), Magenta },
	};
	for (int32 i = 0; i < 4; ++i)
	{
		const FCorner& C = Corners[i];
		const float Flicker = NeonFlicker(UiTime, 1000u + i, 0.04f);
		const FLinearColor Color = Fade(C.Color, 0.85f * Flicker);
		DrawNeonLine(C.P, C.P + FVector2D(C.Dir.X * L, 0.f), Color, Thick, 0.45f * Flicker);
		DrawNeonLine(C.P, C.P + FVector2D(0.f, C.Dir.Y * L), Color, Thick, 0.45f * Flicker);
		// A short inner notch.
		DrawNeonLine(C.P + FVector2D(C.Dir.X * U(8.f), C.Dir.Y * U(8.f)), C.P + FVector2D(C.Dir.X * U(28.f), C.Dir.Y * U(8.f)), Fade(C.Color, 0.5f * Flicker), FMath::Max(1.f, U(1.f)), 0.f);
	}

	// Faint rails joining the brackets, with tick marks.
	const FLinearColor RailColor = Fade(Cyan, 0.14f);
	const float Rail = FMath::Max(1.f, U(1.f));
	DrawRect(RailColor, Left + L + U(12.f), Top, Right - Left - (L + U(12.f)) * 2.f, Rail);
	DrawRect(RailColor, Left + L + U(12.f), Bottom - Rail, Right - Left - (L + U(12.f)) * 2.f, Rail);
	DrawRect(RailColor, Left, Top + L + U(12.f), Rail, Bottom - Top - (L + U(12.f)) * 2.f);
	DrawRect(RailColor, Right - Rail, Top + L + U(12.f), Rail, Bottom - Top - (L + U(12.f)) * 2.f);
	for (float TX = Left + L + U(40.f); TX < Right - L - U(30.f); TX += U(48.f))
	{
		DrawRect(Fade(Cyan, 0.2f), TX, Top, Rail, U(5.f));
		DrawRect(Fade(Cyan, 0.2f), TX, Bottom - U(5.f), Rail, U(5.f));
	}

	// Real read-outs only: the time, and who's playing.
	const float TextY = Top + U(12.f);
	DrawLabel(FDateTime::Now().ToString(TEXT("%H:%M")), Right - U(16.f), TextY, ChromeSize, Fade(Magenta, 0.75f), ESpinTextAlign::Right, true);
	const FString Player = GM.GetPlayerName();
	if (!Player.IsEmpty())
	{
		DrawLabel(Player.ToUpper(), Right - U(16.f), TextY + LineH(ChromeSize), ChromeSize, Fade(TextFaint, 0.9f), ESpinTextAlign::Right, true);
	}

	// Now and then, an electric arc crackles along a stretch of the frame.
	const float ArcRate = 0.9f;
	const int32 ArcSlot = FMath::FloorToInt(UiTime * ArcRate);
	const float IntoSlot = UiTime * ArcRate - ArcSlot;
	if (Hash01(static_cast<uint32>(ArcSlot) * 7u + 3u) < 0.45f && IntoSlot < 0.15f)
	{
		const uint32 Slot = static_cast<uint32>(ArcSlot);
		const int32 Edge = FMath::Min(3, static_cast<int32>(Hash01(Slot * 13u) * 4.f));
		const float Along = FMath::Lerp(0.15f, 0.85f, Hash01(Slot * 29u));
		const float Len = U(FMath::Lerp(140.f, 280.f, Hash01(Slot * 37u)));
		FVector2D A, B;
		switch (Edge)
		{
		case 0:  A = FVector2D(FMath::Lerp(Left, Right, Along), Top); B = A + FVector2D(Len, 0.f); break;
		case 1:  A = FVector2D(Right, FMath::Lerp(Top, Bottom, Along)); B = A + FVector2D(0.f, Len); break;
		case 2:  A = FVector2D(FMath::Lerp(Left, Right, Along), Bottom); B = A + FVector2D(Len, 0.f); break;
		default: A = FVector2D(Left, FMath::Lerp(Top, Bottom, Along)); B = A + FVector2D(0.f, Len); break;
		}
		B.X = FMath::Min(B.X, Right);
		B.Y = FMath::Min(B.Y, Bottom);

		// A jagged line that re-forms every few frames.
		const FVector2D Dir = (B - A).GetSafeNormal();
		const FVector2D Perp(-Dir.Y, Dir.X);
		const uint32 Jitter = Slot * 101u + static_cast<uint32>(FMath::FloorToInt(UiTime * 40.f));
		const int32 Segments = 9;
		const FLinearColor ArcColor = Edge % 2 == 0 ? Cyan : Magenta;
		const float Strength = 1.f - IntoSlot / 0.15f;
		FVector2D Prev = A;
		for (int32 i = 1; i <= Segments; ++i)
		{
			const float T = static_cast<float>(i) / Segments;
			const float Offset = i == Segments ? 0.f : (Hash01(Jitter + i * 17u) - 0.5f) * U(16.f);
			const FVector2D P = FMath::Lerp(A, B, T) + Perp * Offset;
			DrawNeonLine(Prev, P, Fade(FLinearColor::White, 0.9f * Strength), FMath::Max(1.f, U(1.5f)), 0.7f * Strength);
			DrawBeam(Prev, P, U(26.f), ArcColor, 0.5f * Strength);
			Prev = P;
		}
		if (ArcSlot != LastArcSlot)
		{
			LastArcSlot = ArcSlot;
			EmitSparks(A, 6, SparkColor, 260.f, Edge == 2 ? -90.f : 90.f, 120.f);
			EmitSparks(B, 6, ArcColor, 260.f, Edge == 2 ? -90.f : 90.f, 120.f);
		}
	}

	// Sparks spitting from the corners every so often.
	CornerSparkTimer -= UiDelta;
	if (CornerSparkTimer <= 0.f)
	{
		CornerSparkTimer = FMath::FRandRange(0.35f, 1.4f);
		const int32 Which = FMath::RandRange(0, 3);
		const FCorner& C = Corners[Which];
		const float Angle = FMath::RadiansToDegrees(FMath::Atan2(C.Dir.Y, C.Dir.X));
		EmitSparks(C.P + FVector2D(C.Dir.X * U(4.f), C.Dir.Y * U(4.f)), FMath::RandRange(4, 9), FMath::FRand() < 0.7f ? SparkColor : C.Color, 320.f, Angle, 70.f);
	}
}

void ASpinHUD::DrawRunChrome(const ASpinGameMode& GM)
{
	// Thin corner brackets that flash with each hit.
	const float Pulse = GM.GetHitPulse();
	const float M = U(14.f);
	const float L = U(46.f);
	const float Alpha = 0.25f + 0.6f * Pulse;
	const FLinearColor ColorA = Fade(FMath::Lerp(Cyan, FLinearColor::White, Pulse * 0.5f), Alpha);
	const FLinearColor ColorB = Fade(FMath::Lerp(Magenta, FLinearColor::White, Pulse * 0.5f), Alpha);
	const float Thick = FMath::Max(1.f, U(2.f));
	const float Glow = 0.12f + 0.5f * Pulse;

	DrawNeonLine(FVector2D(M, M), FVector2D(M + L, M), ColorA, Thick, Glow);
	DrawNeonLine(FVector2D(M, M), FVector2D(M, M + L), ColorA, Thick, Glow);
	DrawNeonLine(FVector2D(ScreenW - M, M), FVector2D(ScreenW - M - L, M), ColorB, Thick, Glow);
	DrawNeonLine(FVector2D(ScreenW - M, M), FVector2D(ScreenW - M, M + L), ColorB, Thick, Glow);
	DrawNeonLine(FVector2D(ScreenW - M, ScreenH - M), FVector2D(ScreenW - M - L, ScreenH - M), ColorA, Thick, Glow);
	DrawNeonLine(FVector2D(ScreenW - M, ScreenH - M), FVector2D(ScreenW - M, ScreenH - M - L), ColorA, Thick, Glow);
	DrawNeonLine(FVector2D(M, ScreenH - M), FVector2D(M + L, ScreenH - M), ColorB, Thick, Glow);
	DrawNeonLine(FVector2D(M, ScreenH - M), FVector2D(M, ScreenH - M - L), ColorB, Thick, Glow);
}

void ASpinHUD::DrawNavLegend(const ASpinGameMode& GM)
{
	const bool bPad = IsPadInput(GM);
	const bool bPaused = GM.IsPauseMenuOpen();

	// Typing a name has its own key row; the arrows type rather than move there.
	if (!bPad && GM.State == ESpinState::NameEntry)
	{
		return;
	}

	const bool bHasBack = bPaused || GM.State != ESpinState::WaitingToStart;
	const TCHAR* BackLabel = bPaused ? TEXT("RESUME")
		: GM.State == ESpinState::GameOver ? TEXT("MAIN MENU")
		: GM.State == ESpinState::NameEntry ? TEXT("CANCEL")
		: TEXT("BACK");

	TArray<TPair<FString, FString>> Items;
	auto AddItem = [&Items](const FString& Key, const FString& Label) { Items.Emplace(Key, Label); };
	if (bPad)
	{
		AddItem(TEXT("D-PAD"), TEXT("MOVE"));
		AddItem(GM.GetButtonKey(0), TEXT("SELECT"));
	}
	else
	{
		AddItem(TEXT("MOUSE"), TEXT("POINT + CLICK"));
		AddItem(TEXT("ARROWS"), TEXT("MOVE"));
		AddItem(TEXT("ENTER"), TEXT("SELECT"));
	}
	if (bHasBack)
	{
		AddItem(BackKeyOf(GM), BackLabel);
	}
	if (!bPaused && GM.State == ESpinState::ModifierSelect)
	{
		AddItem(GM.GetPauseKey(), TEXT("START RUN"));
	}

	const float Size = 13.f;
	const float Space = U(8.f);
	const float ItemGap = U(28.f);
	float TotalW = 0.f;
	for (const TPair<FString, FString>& Item : Items)
	{
		TotalW += KeyCapWidth(Item.Key, Size) + Space + MeasureText(Item.Value, Size, true).X;
	}
	TotalW += ItemGap * (Items.Num() - 1);

	const float CapH = KeyCapHeight(Size);
	const float Y = ScreenH - U(30.f) - CapH;
	float X = ScreenW * 0.5f - TotalW * 0.5f;
	for (const TPair<FString, FString>& Item : Items)
	{
		X += DrawKeyCap(Item.Key, X, Y, Size, Fade(Cyan, 0.85f), false) + Space;
		X += DrawLabel(Item.Value, X, Y + (CapH - LineH(Size)) * 0.5f, Size, TextDim, ESpinTextAlign::Left, true) + ItemGap;
	}
}

void ASpinHUD::DrawScreenReveal()
{
	const float T = (UiTime - ScreenEnterTime) / RevealDuration;
	if (T >= 1.f)
	{
		return;
	}

	// The picture opens out from a bright line across the middle.
	const float CY = ScreenH * 0.5f;
	const float Half = ScreenH * 0.5f * EaseOut(T);
	const FLinearColor Cover = Fade(BackdropColor, 0.97f);
	DrawRect(Cover, 0.f, 0.f, ScreenW, CY - Half);
	DrawRect(Cover, 0.f, CY + Half, ScreenW, ScreenH - (CY + Half));

	const float Bright = 1.f - T;
	for (const float EdgeY : { CY - Half, CY + Half })
	{
		DrawBeam(FVector2D(0.f, EdgeY), FVector2D(ScreenW, EdgeY), U(40.f), Cyan, 0.8f * Bright);
		DrawRect(Fade(FLinearColor::White, Bright), 0.f, EdgeY - FMath::Max(1.f, U(1.f)), ScreenW, FMath::Max(2.f, U(2.f)));
	}
	// A flash in the middle as it switches on.
	DrawGlowSprite(ScreenW * 0.5f, CY, ScreenW * 0.9f, U(120.f), FLinearColor::White, 0.35f * FMath::Square(Bright));
}

void ASpinHUD::EmitSparks(const FVector2D& Pos, int32 Count, const FLinearColor& Color, float Speed, float AngleDeg, float SpreadDeg)
{
	for (int32 i = 0; i < Count && Sparks.Num() < MaxSparks; ++i)
	{
		FSpinSpark& Spark = Sparks.AddDefaulted_GetRef();
		const float Angle = FMath::DegreesToRadians(AngleDeg + FMath::FRandRange(-0.5f, 0.5f) * SpreadDeg);
		const float V = U(Speed) * FMath::FRandRange(0.4f, 1.2f);
		Spark.Pos = Pos;
		Spark.Vel = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * V;
		Spark.Life = FMath::FRandRange(0.35f, 0.9f);
		Spark.Size = U(FMath::FRandRange(1.5f, 3.f));
		Spark.Color = Color;
	}
}

void ASpinHUD::UpdateAndDrawSparks()
{
	const float Gravity = U(1300.f);
	const float Dt = UiDelta;
	for (int32 i = Sparks.Num() - 1; i >= 0; --i)
	{
		FSpinSpark& Spark = Sparks[i];
		Spark.Age += Dt;
		if (Spark.Age >= Spark.Life)
		{
			Sparks.RemoveAtSwap(i);
			continue;
		}
		Spark.Vel.Y += Gravity * Dt;
		Spark.Vel *= FMath::Max(0.f, 1.f - 1.8f * Dt);
		Spark.Pos += Spark.Vel * Dt;

		// A streak along its path, white-hot at the head and cooling as it dies.
		const float Life = 1.f - Spark.Age / Spark.Life;
		const FVector2D Tail = Spark.Pos - Spark.Vel * 0.03f;
		DrawBeam(Tail, Spark.Pos, Spark.Size * 4.f, Spark.Color, 1.1f * Life);
		DrawGlowSprite(Spark.Pos.X, Spark.Pos.Y, Spark.Size * 5.f, Spark.Size * 5.f, FMath::Lerp(Spark.Color, FLinearColor::White, Life * 0.6f), 0.9f * Life);
	}
}

// =============================================================================================
// Focus
// =============================================================================================

namespace
{
	/** Screens that remember where the highlight was, so backing out lands back on the same item.
	 *  The others (game over, pause, profiles, name entry) always open on their default. */
	bool RemembersFocus(int32 Screen)
	{
		// Screen is an ESpinState, or something outside its range (the pause menu, or none yet).
		if (Screen < 0 || Screen > static_cast<int32>(ESpinState::EndlessIntro))
		{
			return false;
		}
		switch (static_cast<ESpinState>(Screen))
		{
		case ESpinState::WaitingToStart:
		case ESpinState::ModeSelect:
		case ESpinState::ModifierSelect:
		case ESpinState::SettingsMenu:
			return true;
		default:
			return false;
		}
	}

	/** How far apart two intervals are (0 if they overlap). */
	float IntervalGap(float AMin, float AMax, float BMin, float BMax)
	{
		return FMath::Max(0.f, FMath::Max(AMin, BMin) - FMath::Min(AMax, BMax));
	}
}

void ASpinHUD::OnScreenChanged(int32 NewScreen)
{
	if (bHasFocus && RemembersFocus(CurrentScreen))
	{
		RememberedFocus.Add(CurrentScreen, Focus);
	}

	CurrentScreen = NewScreen;
	ScreenEnterTime = UiTime;
	FocusAnim.Reset();
	bHasFocus = false;
	if (RemembersFocus(NewScreen))
	{
		if (const FSpinFocusKey* Remembered = RememberedFocus.Find(NewScreen))
		{
			Focus = *Remembered;
			bHasFocus = true;
		}
	}
}

void ASpinHUD::ValidateFocus()
{
	const FSpinMenuRegion* First = nullptr;
	bool bFound = false;
	bool bDefaultFound = false;
	for (const FSpinMenuRegion& Region : MenuRegions)
	{
		if (!Region.bFocusable)
		{
			continue;
		}
		First = First ? First : &Region;
		bFound |= bHasFocus && Region.GetKey() == Focus;
		bDefaultFound |= bHasDefaultFocus && Region.GetKey() == DefaultFocus;
	}

	if (First && !bFound)
	{
		Focus = bDefaultFound ? DefaultFocus : First->GetKey();
		bHasFocus = true;
	}
}

void ASpinHUD::UpdateFocusAnimations()
{
	const float Step = UiDelta / FocusEaseTime;
	for (auto It = FocusAnim.CreateIterator(); It; ++It)
	{
		const bool bTarget = bHasFocus && It.Key() == Focus;
		It.Value() = FMath::Clamp(It.Value() + (bTarget ? Step : -Step), 0.f, 1.f);
		if (!bTarget && It.Value() <= 0.f)
		{
			It.RemoveCurrent();
		}
	}
	if (bHasFocus && !FocusAnim.Contains(Focus) && GetFocusedRegion())
	{
		FocusAnim.Add(Focus, FMath::Min(1.f, Step));
	}

	// The highlighted item spits the odd spark from a corner.
	if (const FSpinMenuRegion* Focused = GetFocusedRegion())
	{
		FocusSparkTimer -= UiDelta;
		if (FocusSparkTimer <= 0.f)
		{
			FocusSparkTimer = FMath::FRandRange(0.5f, 1.6f);
			const bool bRight = FMath::RandBool();
			const FVector2D Corner(bRight ? Focused->Rect.Max.X : Focused->Rect.Min.X, FMath::RandBool() ? Focused->Rect.Min.Y : Focused->Rect.Max.Y);
			EmitSparks(Corner, FMath::RandRange(2, 5), FMath::FRand() < 0.6f ? SparkColor : Cyan, 220.f, bRight ? -40.f : -140.f, 80.f);
		}
	}
}

const FSpinMenuRegion* ASpinHUD::GetFocusedRegion() const
{
	if (!bHasFocus)
	{
		return nullptr;
	}
	for (const FSpinMenuRegion& Region : MenuRegions)
	{
		if (Region.bFocusable && Region.GetKey() == Focus)
		{
			return &Region;
		}
	}
	return nullptr;
}

const FSpinMenuRegion* ASpinHUD::FindRegionAt(const FVector2D& Point, bool bFocusable) const
{
	// Topmost first: later items are drawn over earlier ones (the < > arrows over their row).
	for (int32 i = MenuRegions.Num() - 1; i >= 0; --i)
	{
		const FSpinMenuRegion& Region = MenuRegions[i];
		if ((bFocusable ? Region.bFocusable : Region.bClickable) && Region.Rect.IsInside(Point))
		{
			return &Region;
		}
	}
	return nullptr;
}

void ASpinHUD::SetFocus(const FSpinFocusKey& Key)
{
	Focus = Key;
	bHasFocus = true;
}

void ASpinHUD::MoveFocus(int32 DX, int32 DY)
{
	const FSpinMenuRegion* Current = GetFocusedRegion();
	if (!Current)
	{
		ValidateFocus();
		return;
	}

	// The nearest item in that direction, preferring ones lined up with the current one. Scores
	// are distance along the direction plus a heavy penalty for being off to the side.
	const FVector2D From = Current->Rect.GetCenter();
	const bool bVertical = DY != 0;
	auto SideGap = [&](const FSpinMenuRegion& Region)
	{
		return bVertical
			? IntervalGap(Current->Rect.Min.X, Current->Rect.Max.X, Region.Rect.Min.X, Region.Rect.Max.X)
			: IntervalGap(Current->Rect.Min.Y, Current->Rect.Max.Y, Region.Rect.Min.Y, Region.Rect.Max.Y);
	};

	const FSpinMenuRegion* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (const FSpinMenuRegion& Region : MenuRegions)
	{
		if (!Region.bFocusable || &Region == Current)
		{
			continue;
		}
		const FVector2D Delta = Region.Rect.GetCenter() - From;
		const float Along = Delta.X * DX + Delta.Y * DY;
		if (Along <= 1.f)
		{
			continue;
		}
		const float Off = bVertical ? FMath::Abs(Delta.X) : FMath::Abs(Delta.Y);
		const float Score = Along + SideGap(Region) * 4.f + Off * 0.5f;
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = &Region;
		}
	}

	// Nothing that way: wrap round to the farthest item the other way, in the same row or column
	// if there is one.
	if (!Best)
	{
		for (const bool bSameLine : { true, false })
		{
			for (const FSpinMenuRegion& Region : MenuRegions)
			{
				if (!Region.bFocusable || &Region == Current)
				{
					continue;
				}
				const float Gap = SideGap(Region);
				if (bSameLine && Gap > 0.f)
				{
					continue;
				}
				const FVector2D Delta = Region.Rect.GetCenter() - From;
				const float Back = -(Delta.X * DX + Delta.Y * DY);
				if (Back <= 1.f)
				{
					continue;
				}
				const float Off = bVertical ? FMath::Abs(Delta.X) : FMath::Abs(Delta.Y);
				const float Score = -Back + Gap * 4.f + Off * 0.5f;
				if (Score < BestScore)
				{
					BestScore = Score;
					Best = &Region;
				}
			}
			if (Best)
			{
				break;
			}
		}
	}

	if (Best)
	{
		SetFocus(Best->GetKey());
	}
}

void ASpinHUD::NotifyActivated(const FSpinFocusKey& Key, const FBox2D& Rect)
{
	LastActivated = Key;
	ActivatedTime = UiTime;

	// A burst of sparks off the edges of whatever was pressed.
	const FVector2D Size = Rect.GetSize();
	for (int32 i = 0; i < 14; ++i)
	{
		const bool bTopOrBottom = FMath::RandBool();
		const bool bFar = FMath::RandBool();
		const FVector2D P = bTopOrBottom
			? FVector2D(Rect.Min.X + FMath::FRand() * Size.X, bFar ? Rect.Max.Y : Rect.Min.Y)
			: FVector2D(bFar ? Rect.Max.X : Rect.Min.X, Rect.Min.Y + FMath::FRand() * Size.Y);
		const float Angle = bTopOrBottom ? (bFar ? 90.f : -90.f) : (bFar ? 0.f : 180.f);
		EmitSparks(P, 1, FMath::FRand() < 0.5f ? SparkColor : Cyan, 360.f, Angle, 90.f);
	}
}

// =============================================================================================
// In-run HUD
// =============================================================================================

void ASpinHUD::DrawRunHUD(const ASpinGameMode& GM)
{
	const float CX = ScreenW * 0.5f;
	const float CY = ScreenH * 0.5f;
	const float Top = U(Margin);

	DrawGlitches(GM);
	DrawZoneLabel(GM);
	DrawRunChrome(GM);

	// The top modules: run (score) on the left, level in the middle. Health sits beside the dial,
	// where the eyes already are.
	if (!GM.IsTutorial())
	{
		DrawRunModule(GM, U(Margin), Top);
	}
	DrawLevelModule(GM, CX, Top);
	DrawHealthColumns(GM);

	DrawControlStrip(GM, CX, ScreenH - U(Margin));
	DrawCallout(GM, CX, U(196.f));
	DrawHitPopup(GM, CX, CY - U(150.f));

	// The countdown after unpausing, or the invincibility one, then "GO!" (obstacle hits don't start one).
	if (GM.IsResumeCountingDown())
	{
		DrawInvincibilityCountdown(GM.ResumeTimeRemaining, CX, CY);
	}
	else if (GM.bInvincible)
	{
		DrawInvincibilityCountdown(GM.InvincibleTimeRemaining, CX, CY);
	}
	else
	{
		DrawGo(Now - GM.InvincibilityEndTime, CX, CY);
	}
}

void ASpinHUD::DrawGlitches(const ASpinGameMode& GM)
{
	const ASpinClock* Clock = GM.GetClock();
	if (!Clock)
	{
		return;
	}

	// Each obstacle glitching in or out, in smaller pixels than the dial's.
	TArray<FSpinObstacleGlitch> Obstacles;
	Clock->GetObstacleGlitches(Obstacles);
	for (const FSpinObstacleGlitch& Obstacle : Obstacles)
	{
		const FVector Center = Project(Obstacle.WorldCenter);
		const FVector Edge = Project(Obstacle.WorldCenter + FVector(0.f, Obstacle.WorldRadius, 0.f));
		const float HalfW = FMath::Abs(Edge.X - Center.X);
		DrawGlitchBlocks(Center.X, Center.Y, HalfW, HalfW * 0.8f, Obstacle.Strength, Obstacle.Seed, FMath::Max(3.f, U(9.f)), 0.65f, 2);
	}

	const float Glitch = Clock->GetDialGlitch();
	if (Glitch <= 0.f)
	{
		return;
	}

	// The dial's rim on screen, as an ellipse.
	const FVector Left = Project(Clock->GetDialWorldLocation(270.f, 1.f));
	const FVector Right = Project(Clock->GetDialWorldLocation(90.f, 1.f));
	const FVector Near = Project(Clock->GetDialWorldLocation(180.f, 1.f));
	const FVector Far = Project(Clock->GetDialWorldLocation(0.f, 1.f));
	const float CenterX = (Left.X + Right.X) * 0.5f;
	const float CenterY = (Near.Y + Far.Y) * 0.5f;
	const float HalfW = FMath::Max(1.f, FMath::Abs(Right.X - Left.X) * 0.5f);
	const float HalfH = FMath::Max(1.f, FMath::Abs(Near.Y - Far.Y) * 0.5f);
	DrawGlitchBlocks(CenterX, CenterY, HalfW, HalfH, Glitch, Clock->GetDialGlitchSeed(), FMath::Max(4.f, U(26.f)), 0.4f, 4);
}

void ASpinHUD::DrawGlitchBlocks(float CenterX, float CenterY, float HalfW, float HalfH, float Strength, int32 Seed, float Tile, float MaxDensity, int32 NumTears)
{
	if (HalfW < 1.f || HalfH < 1.f)
	{
		return;
	}

	const FLinearColor Colors[] = { Cyan, Magenta, Gold, FLinearColor::White };
	FRandomStream Noise(Seed);

	// Chunky pixels on a fixed grid, thick at first and thinning out as it settles. See-through, so
	// the zone and the hand can still be read through them.
	const float Density = MaxDensity * Strength;
	for (float Y = FMath::GridSnap(CenterY - HalfH, Tile); Y < CenterY + HalfH; Y += Tile)
	{
		for (float X = FMath::GridSnap(CenterX - HalfW, Tile); X < CenterX + HalfW; X += Tile)
		{
			const float NX = (X + Tile * 0.5f - CenterX) / HalfW;
			const float NY = (Y + Tile * 0.5f - CenterY) / HalfH;
			if (NX * NX + NY * NY > 1.f || Noise.FRand() > Density)
			{
				continue;
			}
			DrawRect(Fade(Colors[Noise.RandHelper(UE_ARRAY_COUNT(Colors))], Noise.FRandRange(0.25f, 0.6f)), X, Y, Tile, Tile);
		}
	}

	// A few torn scanlines running past the edges.
	for (int32 i = 0; i < NumTears; ++i)
	{
		const float Y = FMath::GridSnap(Noise.FRandRange(CenterY - HalfH, CenterY + HalfH), Tile);
		const float W = HalfW * Noise.FRandRange(0.8f, 2.4f);
		const float X = CenterX - W * 0.5f + Noise.FRandRange(-HalfW, HalfW) * 0.4f;
		DrawRect(Fade(Colors[Noise.RandHelper(UE_ARRAY_COUNT(Colors))], 0.5f * Strength), X, Y, W, Tile * 0.35f);
	}
}

void ASpinHUD::DrawZoneLabel(const ASpinGameMode& GM)
{
	const ASpinClock* Clock = GM.GetClock();
	if (!Clock || GM.JamPhase == EJamPhase::Jammed || !GM.Prompts.IsValidIndex(GM.ActivePromptIndex))
	{
		return;
	}

	const float Size = 20.f;
	auto DrawCapAt = [&](int32 PromptIndex, const FVector& World, bool bLit)
	{
		const FString Key = GM.GetButtonKey(PromptIndex);
		const FVector Screen = Project(World);
		const float W = KeyCapWidth(Key, Size);
		const float H = KeyCapHeight(Size);
		DrawKeyCap(Key, Screen.X - W * 0.5f, Screen.Y - H * 0.5f, Size, GM.Prompts[PromptIndex].Color, bLit);
	};

	// A key cap riding on the zone itself; it lights up while the hand is inside - "press now". Once
	// a blackout has hidden the zone, the cap moves to the middle of the dial and stays unlit, so it
	// says what to press without giving away where.
	if (GM.GetZoneVisibility() >= 0.5f)
	{
		DrawCapAt(GM.ActivePromptIndex, Clock->GetZoneWorldLocation(0.5f), GM.IsHandInZone());
	}
	else
	{
		DrawCapAt(GM.ActivePromptIndex, Clock->GetDialWorldLocation(0.f, 0.f), false);
	}

	// A pair's second zone carries its own cap.
	if (GM.bHasSecondZone && GM.Prompts.IsValidIndex(GM.SecondPromptIndex))
	{
		DrawCapAt(GM.SecondPromptIndex, Clock->GetDialWorldLocation(GM.SecondZoneCenter, 0.5f), false);
	}
}

void ASpinHUD::DrawRunModule(const ASpinGameMode& GM, float X, float Y)
{
	const float W = U(330.f);
	const float P = U(Pad);
	const bool bHasBest = GM.BestPoints > 0;

	// Layout top to bottom first, so the panel can be drawn behind it at the right height.
	const float HeaderY = Y + P;
	const float ScoreY = HeaderY + LineH(TinySize) + U(2.f);
	const float StreakY = ScoreY + LineH(TitleSize);
	const float SegY = StreakY + LineH(TinySize) + U(3.f);
	const float SegH = U(6.f);
	const float BestY = SegY + SegH + U(14.f);
	const float BestBarY = BestY + LineH(SmallSize) + U(3.f);
	const float Bottom = (bHasBest ? BestBarY + U(4.f) : SegY + SegH) + P;

	DrawPanel(X, Y, W, Bottom - Y, Gold);

	// Header: what this is (and what kind of run, with its modifier bonus), and whose run it is.
	FString Header = GM.bDailyRun ? TEXT("DAILY SCORE") : TEXT("SCORE");
	if (GM.RunModifiers != 0)
	{
		Header += TEXT("   MODS ") + FormatMultiplier(GM.RunPointsMultiplier);
	}
	if (GM.RunBonuses != 0)
	{
		Header += TEXT("   +BONUS");
	}
	DrawLabel(Header, X + P, HeaderY, TinySize, GM.RunModifiers != 0 ? Purple : TextDim, ESpinTextAlign::Left, true);
	DrawLabel(GM.GetPlayerName(), X + W - P, HeaderY, TinySize, Cyan, ESpinTextAlign::Right, true);

	// Points, with the combo multiplier beside them: "how much" and "how fast it's growing" at one glance.
	DrawNeonText(FormatPoints(GM.Points), X + P, ScoreY, TitleSize, TextMain, Gold, 0.12f, ESpinTextAlign::Left);
	const bool bCombo = GM.ComboMultiplier > 1;
	const FString ComboText = FString::Printf(TEXT("x%d"), GM.ComboMultiplier);
	const float ChipW = KeyCapWidth(ComboText, HeadingSize);
	DrawKeyCap(ComboText, X + W - P - ChipW, ScoreY + (LineH(TitleSize) - KeyCapHeight(HeadingSize)) * 0.5f, HeadingSize,
		bCombo ? GetComboColor(GM.ComboMultiplier, GM.MaxMultiplier) : TextFaint, bCombo);

	// PERFECT streak towards the next multiplier.
	const int32 PipCount = FMath::Max(1, GM.PerfectsPerMultiplier);
	const bool bMaxed = GM.ComboMultiplier >= GM.MaxMultiplier;
	const int32 PipsLit = bMaxed ? PipCount : GM.PerfectStreak % PipCount;
	DrawLabel(TEXT("PERFECT STREAK"), X + P, StreakY, TinySize, TextDim, ESpinTextAlign::Left, true);
	DrawLabel(bMaxed ? FString::Printf(TEXT("MAX  x%d"), GM.MaxMultiplier) : FString::Printf(TEXT("%d / %d  >  x%d"), PipsLit, PipCount, GM.ComboMultiplier + 1),
		X + W - P, StreakY, TinySize, bMaxed ? Gold : TextDim, ESpinTextAlign::Right, true);
	DrawSegments(X + P, SegY, W - P * 2.f, SegH, PipCount, PipsLit, Gold);

	// The best to beat, as a bar that fills as the run closes in on it - then the moment it's beaten.
	if (bHasBest)
	{
		if (GM.Points > GM.BestPoints)
		{
			const float SincePassed = Now - GM.PassedBestPointsTime;
			const float Flash = SincePassed < 1.5f ? 0.55f + 0.45f * FMath::Cos(SincePassed * 18.f) : 1.f;
			DrawNeonText(TEXT("NEW BEST!"), X + P, BestY, SmallSize, Fade(Gold, Flash), Gold, 0.25f, ESpinTextAlign::Left);
			DrawLabel(FString::Printf(TEXT("+%s"), *FormatPoints(GM.Points - GM.BestPoints)), X + W - P, BestY, SmallSize, Gold, ESpinTextAlign::Right, true);
			DrawBar(X + P, BestBarY, W - P * 2.f, U(4.f), 1.f, Gold);
		}
		else
		{
			DrawLabel(FString::Printf(TEXT("%s  %s"), GM.bDailyRun ? TEXT("TODAY") : TEXT("BEST"), *FormatPoints(GM.BestPoints)), X + P, BestY, SmallSize, TextDim, ESpinTextAlign::Left, true);
			DrawLabel(FString::Printf(TEXT("%s to go"), *FormatPoints(GM.BestPoints - GM.Points)), X + W - P, BestY, SmallSize, TextDim, ESpinTextAlign::Right);
			DrawBar(X + P, BestBarY, W - P * 2.f, U(4.f), static_cast<float>(GM.Points) / GM.BestPoints, Fade(Gold, 0.7f));
		}
	}
}

void ASpinHUD::DrawLevelModule(const ASpinGameMode& GM, float CenterX, float Y)
{
	const float W = U(440.f);
	const float X = CenterX - W * 0.5f;
	const float P = U(Pad);

	FString Heading, HeadRight, CountText, NextText, Hint;
	FLinearColor HeadRightColor = TextDim;
	FLinearColor NextColor = TextFaint;
	FLinearColor Accent = Cyan;
	float Fraction = 0.f;

	if (GM.IsTutorial())
	{
		const FTutorialLevel* Level = GM.GetTutorialLevel();
		if (!Level)
		{
			return;
		}
		Heading = TEXT("TUTORIAL");
		HeadRight = FString::Printf(TEXT("STAGE %d / %d"), GM.TutorialIndex + 1, GM.TutorialLevels.Num());
		Fraction = static_cast<float>(GM.TutorialHits) / FMath::Max(1, Level->Hits);
		CountText = FString::Printf(TEXT("%d / %d HITS"), GM.TutorialHits, Level->Hits);
		NextText = Level->Title;
		Accent = Green;
	}
	else
	{
		const int32 Level = GM.GetLevel();
		const int32 PerLevel = GM.GetHitsForLevel(Level);
		const int32 IntoLevel = PerLevel - (GM.GetLevelTarget() - GM.Hits);

		Heading = FString::Printf(TEXT("LEVEL %d"), Level);
		if (GM.BestLevel > 0)
		{
			const bool bPastBest = Level > GM.BestLevel;
			HeadRight = bPastBest ? FString(TEXT("NEW BEST LEVEL")) : FString::Printf(TEXT("BEST  %d"), GM.BestLevel);
			HeadRightColor = bPastBest ? Gold : TextDim;
		}
		Fraction = static_cast<float>(IntoLevel) / FMath::Max(1, PerLevel);
		CountText = FString::Printf(TEXT("%d / %d HITS"), IntoLevel, PerLevel);

		// What waits at the end of the bar: the next level, or the mechanic it unlocks (with a how-to).
		FString UnlockTitle;
		if (GM.GetUnlockAtLevel(Level + 1, UnlockTitle, Hint))
		{
			NextText = FString::Printf(TEXT("NEXT  >  %s"), *UnlockTitle);
			NextColor = Fade(Orange, FMath::Lerp(0.65f, 1.f, Pulse01(Now, 4.f)));
		}
		else
		{
			NextText = FString::Printf(TEXT("NEXT  >  LEVEL %d"), Level + 1);
		}
	}

	const TArray<FString> HintLines = Hint.IsEmpty() ? TArray<FString>() : WrapText(Hint, TinySize, false, W - P * 2.f);

	const float HeadY = Y + P;
	const float BarY = HeadY + LineH(HeadingSize) + U(4.f);
	const float BarH = U(10.f);
	const float InfoY = BarY + BarH + U(6.f);
	const float HintY = InfoY + LineH(SmallSize) + U(2.f);
	const float Bottom = (HintLines.Num() > 0 ? HintY + HintLines.Num() * LineH(TinySize) : InfoY + LineH(SmallSize)) + P;

	DrawPanel(X, Y, W, Bottom - Y, Accent);

	DrawNeonText(Heading, X + P, HeadY, HeadingSize, Accent, Accent, 0.15f, ESpinTextAlign::Left);
	DrawLabel(HeadRight, X + W - P, HeadY + (LineH(HeadingSize) - LineH(SmallSize)) * 0.5f, SmallSize, HeadRightColor, ESpinTextAlign::Right, true);
	DrawBar(X + P, BarY, W - P * 2.f, BarH, Fraction, Accent);
	DrawLabel(CountText, X + P, InfoY, SmallSize, TextDim, ESpinTextAlign::Left, true);
	DrawLabel(NextText, X + W - P, InfoY, SmallSize, NextColor, ESpinTextAlign::Right, true);

	float LineY = HintY;
	for (const FString& Line : HintLines)
	{
		DrawLabel(Line, CenterX, LineY, TinySize, TextMain, ESpinTextAlign::Center);
		LineY += LineH(TinySize);
	}
}

void ASpinHUD::DrawHealthColumns(const ASpinGameMode& GM)
{
	// Endless always shows both, so they don't pop in when obstacles unlock. The tutorial has no
	// misses, and only shows lives on the stages with obstacles.
	const bool bMisses = !GM.IsTutorial();
	const bool bLives = !GM.IsTutorial() || GM.ObstaclesEnabled();
	if (!bMisses && !bLives)
	{
		return;
	}

	// Find the dial's left and right rim on screen (9 and 3 o'clock), so the columns hug it at any
	// resolution or camera framing. Without a dial, fall back to either side of the screen centre.
	float DialLeft = ScreenW * 0.5f - U(420.f);
	float DialRight = ScreenW * 0.5f + U(420.f);
	float DialCY = ScreenH * 0.5f;
	if (const ASpinClock* Clock = GM.GetClock())
	{
		const FVector A = Project(Clock->GetDialWorldLocation(270.f, 1.f));
		const FVector B = Project(Clock->GetDialWorldLocation(90.f, 1.f));
		DialLeft = FMath::Min(A.X, B.X);
		DialRight = FMath::Max(A.X, B.X);
		DialCY = (A.Y + B.Y) * 0.5f;
	}

	const float W = U(96.f);
	const float P = U(Pad);
	const float Pip = U(26.f);
	const float Gap = U(8.f);
	const float RimGap = U(48.f);

	// See-through, so the columns sit quietly beside the dial. The -1 / +1 feedback stays opaque.
	const float Alpha = 0.55f;

	auto DrawColumn = [&](float X, const TCHAR* Label, const TCHAR* Sub, int32 Count, int32 Max, const FLinearColor& Color, float LostTime, float GainedTime)
	{
		const float PipsH = Max * Pip + (Max - 1) * Gap;
		const float H = P * 2.f + LineH(SmallSize) + U(10.f) + PipsH + U(10.f) + LineH(TinySize);
		const float Y = DialCY - H * 0.5f;
		const float CX = X + W * 0.5f;

		DrawPanel(X, Y, W, H, Color, Alpha);

		// The column flashes when it loses one.
		const float SinceLost = Now - LostTime;
		if (SinceLost >= 0.f && SinceLost < 0.6f)
		{
			DrawRect(Fade(Color, 0.25f * (1.f - SinceLost / 0.6f)), X + U(4.f), Y + U(4.f), W - U(8.f), H - U(8.f));
		}

		DrawLabel(Label, CX, Y + P, SmallSize, Fade(Color, Alpha), ESpinTextAlign::Center, true);

		// Pips stack bottom-up like a health bar, so a lost one goes from the top. Lit ones glow.
		const float PipX = CX - Pip * 0.5f;
		const float PipsBottom = Y + P + LineH(SmallSize) + U(10.f) + PipsH;
		auto PipY = [&](int32 i) { return PipsBottom - Pip - i * (Pip + Gap); };
		for (int32 i = 0; i < Max; ++i)
		{
			FVector2D Pts[6];
			ChamferRect(PipX, PipY(i), Pip, Pip, Pip * 0.25f, Pts);
			if (i < Count)
			{
				DrawGlowSprite(CX, PipY(i) + Pip * 0.5f, Pip * 2.2f, Pip * 2.2f, Color, 0.22f);
				FillPolygon(Pts, 6, Fade(FMath::Lerp(Color, FLinearColor::White, 0.25f), Alpha + 0.2f), Fade(Color, Alpha));
			}
			else
			{
				FillPolygon(Pts, 6, Fade(TrackColor, Alpha), Fade(TrackColor, Alpha));
				for (int32 Edge = 0; Edge < 6; ++Edge)
				{
					DrawNeonLine(Pts[Edge], Pts[(Edge + 1) % 6], Fade(Color, 0.35f * Alpha), FMath::Max(1.f, U(1.f)), 0.f);
				}
			}
		}

		DrawLabel(Sub, CX, PipsBottom + U(10.f), TinySize, Fade(TextFaint, Alpha), ESpinTextAlign::Center, true);

		// "-1" / "+1" rises off the pip that just changed.
		auto Float = [&](const TCHAR* Text, int32 PipIndex, float Since, const FLinearColor& FloatColor)
		{
			if (Since >= 0.f && Since < 1.2f && PipIndex >= 0 && PipIndex < Max)
			{
				const float T = Since / 1.2f;
				DrawNeonText(Text, CX, PipY(PipIndex) + (Pip - LineH(SmallSize)) * 0.5f - U(18.f) * T,
					SmallSize, Fade(FloatColor, 1.f - T * T), FloatColor, 0.3f, ESpinTextAlign::Center);
			}
		};
		Float(TEXT("-1"), Count, SinceLost, Color);
		Float(TEXT("+1"), Count - 1, Now - GainedTime, Green);
	};

	// Left of the dial: obstacle lives. Right of it: miss lives.
	if (bLives)
	{
		// How close the next life is: posts cleared out of the ObstaclesPerLife needed.
		const FString LivesSub = GM.IsTutorial() ? FString(TEXT("PRACTICE"))
			: GM.Lives >= GM.GetRunMaxLives() ? FString(TEXT("FULL"))
			: FString::Printf(TEXT("%d/%d TO +1"), GM.ObstacleLifeProgress, GM.ObstaclesPerLife);
		DrawColumn(DialLeft - RimGap - W, TEXT("LIVES"), *LivesSub,
			GM.Lives, GM.GetRunMaxLives(), Red, GM.LifeLostTime, GM.LifeGainedTime);
	}
	if (bMisses)
	{
		const FString MissSub = GM.MissRegenLevels > 0 ? FString::Printf(TEXT("+1 PER %d LV"), GM.MissRegenLevels) : FString(TEXT("NO REFILLS"));
		DrawColumn(DialRight + RimGap, TEXT("MISSES"), *MissSub, GM.MissLives, GM.GetRunMaxMissLives(), Orange, GM.MissLifeLostTime, GM.MissLifeGainedTime);
	}
}

void ASpinHUD::DrawControlStrip(const ASpinGameMode& GM, float CenterX, float BottomY)
{
	if (GM.JamPhase == EJamPhase::Jammed)
	{
		DrawRepairPanel(GM, CenterX, BottomY);
		return;
	}

	// No key list - the key cap on the zone already says what to press. Only the tutorial's
	// one-line reminder for the stage is left down here.
	const FTutorialLevel* Level = GM.IsTutorial() ? GM.GetTutorialLevel() : nullptr;
	const FString Instruction = Level ? GM.ExpandKeys(Level->Instruction) : FString();
	if (Instruction.IsEmpty())
	{
		return;
	}

	const float P = U(Pad);
	const float PanelW = MeasureText(Instruction, SmallSize).X + P * 3.f;
	const float PanelH = P + LineH(SmallSize) + P;
	const float X = CenterX - PanelW * 0.5f;
	const float Y = BottomY - PanelH;

	DrawPanel(X, Y, PanelW, PanelH, KeyColorOf(GM, GM.ActivePromptIndex));
	DrawLabel(Instruction, CenterX, Y + P, SmallSize, TextMain, ESpinTextAlign::Center);
}

void ASpinHUD::DrawRepairPanel(const ASpinGameMode& GM, float CenterX, float BottomY)
{
	// Kept to the bare minimum so it never hides the dial: the key to mash, flashing, and a slim bar
	// of the time left before the hand breaks, just above the dial. No panel behind them.
	const FString Key = GM.GetRepairKey();
	const float KeySize = 34.f;
	const float CapW = KeyCapWidth(Key, KeySize);
	const float CapH = KeyCapHeight(KeySize);
	const float BarW = FMath::Max(U(220.f), CapW * 1.6f);
	const float BarH = U(6.f);
	const float StackH = CapH + U(12.f) + BarH;

	// Sits just above the dial: over its far rim (12 o'clock) on screen, so it follows the dial at
	// any framing. Kept below the top modules, and on screen; without a dial, along the bottom.
	float BarY = BottomY - BarH;
	if (const ASpinClock* Clock = GM.GetClock())
	{
		const FVector Far = Project(Clock->GetDialWorldLocation(0.f, 1.f));
		const FVector Centre = Project(Clock->GetDialWorldLocation(0.f, 0.f));
		CenterX = Centre.X;
		const float MinTop = U(200.f); // clear of the level / score modules
		BarY = FMath::Clamp(Far.Y - U(24.f) - BarH, MinTop + StackH - BarH, BottomY - BarH);
	}
	const float CapY = BarY - U(12.f) - CapH;

	// The key flashes between lit and outlined, with a glow pulsing behind it.
	const float Throb = Pulse01(Now, 22.f);
	const FLinearColor KeyColor = FMath::Lerp(Orange, Gold, Throb);
	DrawGlowSprite(CenterX, CapY + CapH * 0.5f, CapW * 2.6f, CapH * 2.8f, KeyColor, 0.25f + 0.3f * Throb);
	DrawKeyCap(Key, CenterX - CapW * 0.5f, CapY, KeySize, KeyColor, Throb > 0.5f);

	// Time left, draining from the right. It turns from orange to red as it runs out.
	const float TimeFraction = GM.GetJamTimeFraction();
	DrawBar(CenterX - BarW * 0.5f, BarY, BarW, BarH, TimeFraction, FMath::Lerp(Red, Orange, TimeFraction));
}

void ASpinHUD::DrawCallout(const ASpinGameMode& GM, float CenterX, float Y)
{
	FString Title;
	float TitleFontSize = 30.f;
	FLinearColor Color = Gold;
	float Alpha = 1.f;
	TArray<FSpinBriefLine> Subs;

	// Highest priority first - only one message ever occupies the slot.
	if (GM.JamPhase != EJamPhase::None)
	{
		// Nothing here during a jam: the dial's sparks warn it's coming, and the flashing key and
		// timer at the bottom cover the jam itself (see DrawRepairPanel). The dial stays clear.
		return;
	}
	if (GM.IsShowingNotice())
	{
		Title = GM.ExpandKeys(GM.NoticeText);
		TitleFontSize = 20.f;
		Alpha = FMath::Clamp((GM.NoticeDuration - (Now - GM.NoticeTime)) / 0.4f, 0.f, 1.f);
	}
	else if (!GM.IsTutorial())
	{
		// Level-up, folded together with "new best level" and whatever the level unlocks.
		FString UnlockTitle, UnlockHint;
		const bool bUnlock = GM.GetUnlockAtLevel(GM.GetLevel(), UnlockTitle, UnlockHint);
		const float Duration = bUnlock ? 4.f : GM.LevelBannerDuration;
		const float Since = Now - GM.LevelUpTime;
		if (Since >= 0.f && Since < Duration)
		{
			Title = FString::Printf(TEXT("LEVEL %d"), GM.GetLevel());
			TitleFontSize = TitleSize;
			Alpha = FMath::Clamp((Duration - Since) / 0.4f, 0.f, 1.f);
			if (GM.BestLevel > 0 && GM.GetLevel() == GM.BestLevel + 1)
			{
				Subs.Add({ TEXT("NEW BEST LEVEL!"), Gold });
			}
			if (bUnlock)
			{
				Subs.Add({ FString::Printf(TEXT("NEW:  %s"), *UnlockTitle), Orange });
				Subs.Add({ UnlockHint, TextMain });
			}
			// (A blackout arrives unannounced - the flickering lights are the warning.)
			if (GM.LevelEvent == ESpinLevelEvent::DoubleZone)
			{
				Subs.Add({ TEXT("DOUBLE ZONE  -  hit both zones before the hand turns"), Orange });
			}
		}
	}

	if (Title.IsEmpty() || Alpha <= 0.f)
	{
		return;
	}

	const float SubSize = 16.f;
	const float MaxTextW = FMath::Min(U(760.f), ScreenW - U(Margin) * 4.f);
	const TArray<FString> TitleLines = WrapText(Title, TitleFontSize, true, MaxTextW);

	TArray<TPair<FString, FLinearColor>> SubLines;
	for (const FSpinBriefLine& Sub : Subs)
	{
		for (const FString& Line : WrapText(Sub.Text, SubSize, true, MaxTextW))
		{
			SubLines.Add({ Line, Sub.Color });
		}
	}

	float TextW = 0.f;
	for (const FString& Line : TitleLines)
	{
		TextW = FMath::Max(TextW, MeasureText(Line, TitleFontSize, true).X);
	}
	for (const TPair<FString, FLinearColor>& Line : SubLines)
	{
		TextW = FMath::Max(TextW, MeasureText(Line.Key, SubSize, true).X);
	}

	const float P = U(Pad);
	const float PanelW = TextW + P * 3.f;
	const float PanelH = P + TitleLines.Num() * LineH(TitleFontSize) + (SubLines.Num() > 0 ? U(4.f) + SubLines.Num() * LineH(SubSize) : 0.f) + P;
	DrawPanel(CenterX - PanelW * 0.5f, Y, PanelW, PanelH, Color, Alpha);

	float LineY = Y + P;
	for (const FString& Line : TitleLines)
	{
		DrawNeonText(Line, CenterX, LineY, TitleFontSize, Fade(Color, Alpha), Color, 0.22f, ESpinTextAlign::Center);
		LineY += LineH(TitleFontSize);
	}
	LineY += U(4.f);
	for (const TPair<FString, FLinearColor>& Line : SubLines)
	{
		DrawLabel(Line.Key, CenterX, LineY, SubSize, Fade(Line.Value, Alpha), ESpinTextAlign::Center, true);
		LineY += LineH(SubSize);
	}
}

void ASpinHUD::DrawHitPopup(const ASpinGameMode& GM, float CenterX, float Y)
{
	// Grade and points, drifting up and fading out.
	const float Duration = 0.7f;
	const float Age = Now - GM.LastHitTime;
	if (GM.LastHitGrade != ESpinHitGrade::None && Age >= 0.f && Age < Duration)
	{
		const float T = Age / Duration;
		const float Alpha = 1.f - T * T;
		const float Rise = U(30.f) * T;
		const bool bPerfect = GM.LastHitGrade == ESpinHitGrade::Perfect;
		const float Size = bPerfect ? TitleSize : HeadingSize;
		const FLinearColor GradeColor = GetGradeColor(GM.LastHitGrade);

		// PERFECT lands with a chromatic split that snaps together.
		const TCHAR* Label = bPerfect ? TEXT("PERFECT!") : GM.LastHitGrade == ESpinHitGrade::Good ? TEXT("GOOD") : TEXT("OK");
		DrawNeonText(Label, CenterX, Y - Rise, Size, Fade(GradeColor, Alpha), GradeColor, bPerfect ? 0.4f : 0.18f,
			ESpinTextAlign::Center, bPerfect ? U(5.f) * FMath::Square(1.f - T) : 0.f);

		if (!GM.IsTutorial() && GM.LastHitPoints > 0)
		{
			const FString PointsText = GM.ComboMultiplier > 1
				? FString::Printf(TEXT("+%s   x%d"), *FormatPoints(GM.LastHitPoints), GM.ComboMultiplier)
				: FString::Printf(TEXT("+%s"), *FormatPoints(GM.LastHitPoints));
			DrawLabel(PointsText, CenterX, Y + LineH(Size) - Rise, 16.f, Fade(TextMain, Alpha), ESpinTextAlign::Center, true);
		}
	}

	const float SinceComboLost = Now - GM.ComboLostTime;
	if (!GM.IsTutorial() && SinceComboLost >= 0.f && SinceComboLost < 1.f)
	{
		DrawNeonText(TEXT("COMBO LOST"), CenterX, Y + U(80.f), 16.f, Fade(Red, 1.f - SinceComboLost), Red, 0.25f, ESpinTextAlign::Center,
			U(3.f) * (1.f - SinceComboLost));
	}
}

void ASpinHUD::DrawInvincibilityCountdown(float TimeRemaining, float CenterX, float CenterY)
{
	// Which whole second are we in ("5", "4", ... "1"), and how far into that second's display.
	const int32 Number = FMath::Max(1, FMath::CeilToInt(TimeRemaining - KINDA_SMALL_NUMBER));
	const float TimeIntoNumber = Number - TimeRemaining;

	// The punch is much shorter than the full second: the number slams in huge and is gone well
	// before the next one, rather than lingering.
	const float PunchDuration = 0.6f;
	if (TimeIntoNumber < 0.f || TimeIntoNumber >= PunchDuration)
	{
		return;
	}

	const float T = TimeIntoNumber / PunchDuration;
	DrawBigText(FString::FromInt(Number), Fade(Gold, FMath::Square(1.f - T)), FMath::Lerp(16.f, 6.f, T * T), CenterX, CenterY);
}

void ASpinHUD::DrawGo(float TimeSinceEnd, float CenterX, float CenterY)
{
	// Same punch-in as the countdown numbers, but a little longer so it registers, and green.
	const float Duration = 0.8f;
	if (TimeSinceEnd < 0.f || TimeSinceEnd >= Duration)
	{
		return;
	}

	const float T = TimeSinceEnd / Duration;
	DrawBigText(TEXT("GO!"), Fade(Green, 1.f - T * T), FMath::Lerp(16.f, 9.f, FMath::Sqrt(T)), CenterX, CenterY);
}

// =============================================================================================
// Screens
// =============================================================================================

void ASpinHUD::DrawMainMenu(const ASpinGameMode& GM)
{
	DrawBackdrop(0.72f);

	const float CX = ScreenW * 0.5f;
	float Y = ScreenH * 0.17f;
	const float SinceEnter = UiTime - ScreenEnterTime;

	// A neon sign: each word its own tube, and the "TO" in the middle has a bad connection.
	{
		const FString Words[3] = { TEXT("SPIN"), TEXT("TO"), TEXT("WIN") };
		const FLinearColor Tubes[3] = { Magenta, Cyan, Magenta };
		const float SpaceW = MeasureText(TEXT(" "), HeroSize, true).X * 1.4f; // a little extra room for the tubes
		float TotalW = SpaceW * 2.f;
		float WordW[3];
		for (int32 i = 0; i < 3; ++i)
		{
			WordW[i] = MeasureText(Words[i], HeroSize, true).X;
			TotalW += WordW[i];
		}

		// The whole sign jolts sideways now and then.
		const int32 GlitchSlot = FMath::FloorToInt(UiTime * 5.f);
		const bool bGlitch = SinceEnter < 0.5f || Hash01(static_cast<uint32>(GlitchSlot) * 613u) < 0.04f;
		const float Jolt = bGlitch ? U(6.f) * (Hash01(static_cast<uint32>(FMath::FloorToInt(UiTime * 30.f))) - 0.5f) : 0.f;

		float WX = CX - TotalW * 0.5f + Jolt;
		for (int32 i = 0; i < 3; ++i)
		{
			const float Flicker = i == 1 ? NeonFlicker(UiTime, 4242u, 0.12f) : NeonFlicker(UiTime, 77u + i, 0.02f);
			DrawNeonOutlineText(Words[i], WX, Y, HeroSize, Tubes[i], 0.3f + 0.7f * Flicker, ESpinTextAlign::Left);
			WX += WordW[i] + SpaceW;
		}
	}
	Y += LineH(HeroSize);

	// The tagline types itself out.
	{
		const FString Tagline = TEXT("Hit the zone.  Flip the spin.  Don't slow down.");
		const int32 Shown = FMath::Clamp(FMath::FloorToInt((SinceEnter - 0.2f) * 50.f), 0, Tagline.Len());
		const float FullW = MeasureText(Tagline, BodySize).X;
		const bool bTyping = Shown < Tagline.Len();
		const bool bCursor = bTyping || FMath::Fmod(UiTime, 1.f) < 0.5f;
		const float TextW = DrawLabel(Tagline.Left(Shown), CX - FullW * 0.5f, Y, BodySize, TextDim);
		if (bCursor)
		{
			DrawRect(Fade(Cyan, 0.8f), CX - FullW * 0.5f + TextW + U(3.f), Y + U(4.f), U(9.f), LineH(BodySize) - U(8.f));
		}
	}
	Y += LineH(BodySize) + U(36.f);

	const float BtnW = FMath::Min(U(380.f), ScreenW - U(Margin) * 2.f);
	const float BtnX = CX - BtnW * 0.5f;

	// Who's playing - select to switch or add a player.
	const FString Name = GM.GetPlayerName();
	DrawMenuButton(BtnX, Y, BtnW, U(50.f), Name.IsEmpty() ? FString(TEXT("ADD A PLAYER")) : FString::Printf(TEXT("PLAYER:  %s"), *Name), EMenuAction::OpenProfiles, Cyan);
	Y += U(56.f);
	DrawLabel(Name.IsEmpty() ? TEXT("Runs are saved under your name") : TEXT("Select to switch player"), CX, Y, TinySize, TextFaint, ESpinTextAlign::Center);
	Y += LineH(TinySize) + U(24.f);

	const float BtnH = U(64.f);
	const float Gap = U(12.f);
	DrawMenuButton(BtnX, Y, BtnW, BtnH, TEXT("PLAY"), EMenuAction::Play, Gold);
	MarkDefaultFocus();
	Y += BtnH + Gap;
	DrawMenuButton(BtnX, Y, BtnW, BtnH, TEXT("LEADERBOARD"), EMenuAction::OpenLeaderboard, Cyan);
	Y += BtnH + Gap;
	DrawMenuButton(BtnX, Y, BtnW, BtnH, TEXT("SETTINGS"), EMenuAction::OpenSettings, Magenta);
}

void ASpinHUD::DrawModeSelect(const ASpinGameMode& GM)
{
	DrawBackdrop(0.75f);

	const float CX = ScreenW * 0.5f;
	const FString Name = GM.GetPlayerName();
	float Y = DrawScreenTitle(TEXT("CHOOSE A MODE"), Name.IsEmpty() ? FString() : FString::Printf(TEXT("Playing as %s"), *Name), CX, ScreenH * 0.15f, Cyan);
	Y += U(10.f);

	const float Gap = U(28.f);
	const float CardW = FMath::Min(U(380.f), (ScreenW - U(Margin) * 2.f - Gap * 2.f) / 3.f);
	const float CardH = U(260.f);
	const bool bPad = IsPadInput(GM);
	const FString ConfirmKey = ConfirmKeyOf(GM);

	auto DrawCard = [&](float X, const FString& Title, const FLinearColor& Accent, const FString& Description, EMenuAction Action)
	{
		const FSpinFocusKey Key{ Action, INDEX_NONE };
		const float F = FocusAmount(Key);
		const float CardCX = X + CardW * 0.5f;

		if (F > 0.f)
		{
			DrawGlowSprite(CardCX, Y + CardH * 0.5f, CardW * 1.5f, CardH * 1.4f, Accent, 0.14f * F);
		}
		DrawPanel(X, Y, CardW, CardH, Accent);
		DrawFocusFrame(X, Y, CardW, CardH, Accent, Key);

		float LineY = Y + U(26.f);
		DrawNeonText(Title, CardCX, LineY, TitleSize, FMath::Lerp(Accent, FLinearColor::White, 0.3f * F), Accent, 0.15f + 0.35f * F,
			ESpinTextAlign::Center, U(2.f) * F);
		LineY += LineH(TitleSize) + U(8.f);
		DrawWrapped(Description, CardCX, LineY, BodySize, FMath::Lerp(TextDim, TextMain, F), CardW - U(48.f));

		// What selecting it does - brightest while it's highlighted.
		const float PromptY = Y + CardH - U(22.f) - KeyCapHeight(SmallSize);
		const float PromptAlpha = 0.35f + 0.65f * F;
		if (bPad)
		{
			DrawKeyPrompt(TEXT("Press"), ConfirmKey, TEXT("to start"), CardCX, PromptY, SmallSize, Fade(Accent, PromptAlpha));
		}
		else
		{
			DrawLabel(TEXT("CLICK TO START"), CardCX, PromptY + (KeyCapHeight(SmallSize) - LineH(SmallSize)) * 0.5f, SmallSize, Fade(Accent, PromptAlpha), ESpinTextAlign::Center, true);
		}
		AddRegion(X, Y, CardW, CardH, Action);
	};

	const float LeftX = CX - CardW * 1.5f - Gap;
	const float MidX = CX - CardW * 0.5f;
	const float RightX = CX + CardW * 0.5f + Gap;

	// New players start on the tutorial; everyone else on Endless.
	const USpinSaveGame* Save = GM.GetSaveData();
	const FSpinProfile* Profile = Save ? Save->GetActiveProfile() : nullptr;
	const bool bVeteran = Profile && Profile->RunsPlayed > 0;

	DrawCard(LeftX, TEXT("TUTORIAL"), Green,
		FString::Printf(TEXT("Learn the ropes in %d short stages. Nothing can go wrong here."), GM.TutorialLevels.Num()),
		EMenuAction::StartTutorial);
	if (!bVeteran)
	{
		MarkDefaultFocus();
	}
	DrawCard(MidX, TEXT("ENDLESS"), Cyan,
		TEXT("The real run. Pick optional modifiers for bonus points, then see how far you get."),
		EMenuAction::OpenModifiers);
	if (bVeteran)
	{
		MarkDefaultFocus();
	}
	DrawCard(RightX, TEXT("DAILY"), Purple,
		TEXT("Today's run is the same for everyone, with a twist. A new one every day."),
		EMenuAction::StartDaily);
	Y += CardH + U(18.f);

	// Each mode's record goes on its own lines under its card, clear of the key prompts.
	float InfoBottom = Y;
	auto DrawInfo = [&](float CardX, const TArray<TPair<FString, FLinearColor>>& Lines)
	{
		float LineY = Y;
		for (const TPair<FString, FLinearColor>& Line : Lines)
		{
			DrawLabel(Line.Key, CardX + CardW * 0.5f, LineY, SmallSize, Line.Value, ESpinTextAlign::Center, true);
			LineY += LineH(SmallSize) + U(4.f);
		}
		InfoBottom = FMath::Max(InfoBottom, LineY);
	};

	if (bVeteran)
	{
		DrawInfo(MidX, { { FString::Printf(TEXT("YOUR BEST:  LV %d  /  %s"), Profile->BestLevel, *FormatPoints(Profile->BestPoints)), Gold } });
	}

	const int32 DailyBest = GM.GetDailyBest();
	DrawInfo(RightX, {
		{ FString::Printf(TEXT("TODAY'S TWIST:  %s"), *ASpinGameMode::GetModifierNames(ASpinGameMode::GetDailyModifiers())), Purple },
		{ DailyBest > 0 ? FString::Printf(TEXT("YOUR BEST TODAY:  %s"), *FormatPoints(DailyBest)) : FString(TEXT("Not played yet today")), DailyBest > 0 ? Gold : TextDim },
	});
	Y = InfoBottom + U(20.f);

	const float BackW = U(220.f);
	DrawMenuButton(CX - BackW * 0.5f, Y, BackW, U(50.f), TEXT("BACK"), EMenuAction::Back, TextDim, BackKeyOf(GM));
}

void ASpinHUD::DrawRunSetup(const ASpinGameMode& GM)
{
	DrawBackdrop(0.78f);

	const float CX = ScreenW * 0.5f;
	const float Y = DrawScreenTitle(TEXT("RUN SETUP"), TEXT("Modifiers make the run harder for more points - bonuses make it more forgiving"), CX, ScreenH * 0.07f, Purple);

	const int32 NumModifiers = static_cast<int32>(ESpinModifier::Count);
	const int32 NumBonuses = static_cast<int32>(ESpinBonus::Count);
	const float Gap = U(28.f);
	const float PanelW = FMath::Min(U(680.f), (ScreenW - U(Margin) * 2.f - Gap) * 0.5f);
	const float ModX = CX - Gap * 0.5f - PanelW;
	const float BonusX = CX + Gap * 0.5f;
	const float P = U(Pad);
	const float RowH = U(64.f);
	const float HeaderH = LineH(HeadingSize) + LineH(TinySize) + U(14.f);
	// Both panels are the same height: the modifiers plus their multiplier footer, or the bonuses.
	const int32 NumRows = FMath::Max(NumModifiers + 1, NumBonuses);
	const float PanelH = P * 2.f + HeaderH + NumRows * RowH;
	const float RowsY = Y + P + HeaderH;
	const float RowW = PanelW - P * 2.f;

	auto DrawHeader = [&](float PanelX, const TCHAR* Title, const FString& Sub, const FLinearColor& Accent)
	{
		DrawPanel(PanelX, Y, PanelW, PanelH, Accent);
		DrawNeonText(Title, PanelX + P + U(4.f), Y + P, HeadingSize, Accent, Accent, 0.18f, ESpinTextAlign::Left);
		DrawLabel(Sub, PanelX + P + U(4.f), Y + P + LineH(HeadingSize), TinySize, TextFaint);
	};

	// One row: a check box, the name and what it does, and its value on the right. A locked one
	// stays hidden: a padlock, what unlocks it, and a bar of how close the player is.
	auto DrawRow = [&](float PanelX, int32 Row, const FLinearColor& Accent, EMenuAction Action, int32 Index, bool bUnlocked, bool bOn,
		const TCHAR* Name, const TCHAR* Description, const TCHAR* Requirement, int32 Current, int32 Target,
		TFunctionRef<void(float ValueRight, float RowY)> DrawValue)
	{
		const float RowX = PanelX + P;
		const float RowY = RowsY + Row * RowH;
		const FSpinFocusKey Key{ Action, Index };
		const float F = FocusAmount(Key);

		if (Row > 0)
		{
			DrawRect(PanelEdge, RowX, RowY, RowW, FMath::Max(1.f, U(1.f)));
		}
		DrawFocusFrame(RowX, RowY, RowW, RowH, bUnlocked ? Accent : TextDim, Key);

		const float Box = U(26.f);
		const float BoxX = RowX + U(16.f);
		const float BoxY = RowY + (RowH - Box) * 0.5f;
		const float TextX = BoxX + Box + U(16.f);
		const float ValueRight = RowX + RowW - U(12.f);

		if (bUnlocked)
		{
			DrawCheckBox(BoxX, BoxY, Box, bOn, Accent);
			const float NameY = RowY + (RowH - LineH(BodySize) - LineH(SmallSize)) * 0.5f;
			DrawLabel(Name, TextX, NameY, BodySize, bOn || F > 0.5f ? TextMain : TextDim, ESpinTextAlign::Left, true);
			DrawLabel(Description, TextX, NameY + LineH(BodySize), SmallSize, TextDim);
			DrawValue(ValueRight, RowY);
		}
		else if (Current >= Target)
		{
			// Earned, but waiting its turn: only one modifier or bonus unlocks per run.
			DrawLockIcon(BoxX, BoxY, Box, FMath::Lerp(TextDim, Gold, 0.5f + 0.5f * F));
			const float LabelY = RowY + (RowH - LineH(TinySize) - LineH(BodySize)) * 0.5f;
			DrawLabel(FString::Printf(TEXT("EARNED  -  %s"), Name), TextX, LabelY, TinySize, Gold, ESpinTextAlign::Left, true);
			DrawLabel(TEXT("UNLOCKS AFTER A RUN"), TextX, LabelY + LineH(TinySize), BodySize, FMath::Lerp(TextDim, TextMain, F), ESpinTextAlign::Left, true);
		}
		else
		{
			DrawLockIcon(BoxX, BoxY, Box, FMath::Lerp(TextFaint, TextDim, F));
			const float LabelY = RowY + (RowH - LineH(TinySize) - LineH(BodySize)) * 0.5f;
			DrawLabel(TEXT("LOCKED"), TextX, LabelY, TinySize, TextFaint, ESpinTextAlign::Left, true);
			DrawLabel(FString(Requirement).ToUpper(), TextX, LabelY + LineH(TinySize), BodySize, FMath::Lerp(TextDim, TextMain, F), ESpinTextAlign::Left, true);

			const float BarW = FMath::Min(U(190.f), RowW * 0.34f);
			const float BarH = U(6.f);
			const float CountY = RowY + (RowH - LineH(TinySize) - U(4.f) - BarH) * 0.5f;
			const int32 Shown = FMath::Min(Current, Target);
			DrawLabel(FString::Printf(TEXT("%d / %d"), Shown, Target), ValueRight, CountY, TinySize, TextDim, ESpinTextAlign::Right, true);
			DrawBar(ValueRight - BarW, CountY + LineH(TinySize) + U(4.f), BarW, BarH, static_cast<float>(Shown) / FMath::Max(1, Target), Fade(Gold, 0.8f));
		}

		// Locked rows can still be highlighted, to read what they need (the game mode ignores
		// switching one on, or a bonus with no free slot).
		AddRegion(RowX, RowY, RowW, RowH, Action, Index);
	};

	// ---- Modifiers, on the left ----
	const int32 Selected = GM.SelectedModifiers & GM.GetUnlockedModifiers();
	DrawHeader(ModX, TEXT("MODIFIERS"), TEXT("Harder runs - every point multiplied"), Purple);
	for (int32 i = 0; i < NumModifiers; ++i)
	{
		const ESpinModifier Modifier = static_cast<ESpinModifier>(i);
		const FSpinModifierInfo& Info = ASpinGameMode::GetModifierInfo(Modifier);
		const bool bOn = (Selected & (1 << i)) != 0;
		int32 Current = 0, Target = 1;
		GM.GetModifierProgress(Modifier, Current, Target);
		DrawRow(ModX, i, Purple, EMenuAction::ToggleModifier, i, GM.IsModifierUnlocked(Modifier), bOn,
			Info.Name, Info.Description, Info.Requirement, Current, Target,
			[&](float ValueRight, float RowY)
			{
				const float MultY = RowY + (RowH - LineH(HeadingSize)) * 0.5f;
				if (bOn)
				{
					DrawNeonText(FormatMultiplier(Info.PointsMultiplier), ValueRight, MultY, HeadingSize, Gold, Gold, 0.2f, ESpinTextAlign::Right);
				}
				else
				{
					DrawLabel(FormatMultiplier(Info.PointsMultiplier), ValueRight, MultY, HeadingSize, TextFaint, ESpinTextAlign::Right, true);
				}
			});
	}

	// The combined multiplier, along the bottom of the modifier panel.
	{
		const float FooterY = RowsY + (NumRows - 1) * RowH;
		DrawRect(Fade(Purple, 0.5f), ModX + P, FooterY, RowW, FMath::Max(1.f, U(1.f)));
		const float Total = ASpinGameMode::GetModifiersMultiplier(Selected);
		const float TextY = FooterY + (RowH - LineH(HeadingSize)) * 0.5f;
		DrawLabel(TEXT("POINTS MULTIPLIER"), ModX + P + U(16.f), TextY + (LineH(HeadingSize) - LineH(SmallSize)) * 0.5f, SmallSize, TextDim, ESpinTextAlign::Left, true);
		DrawNeonText(FormatMultiplier(Total), ModX + P + RowW - U(12.f), TextY, HeadingSize, Selected != 0 ? Gold : TextFaint, Gold,
			Selected != 0 ? 0.3f : 0.f, ESpinTextAlign::Right);
	}

	// ---- Bonuses, on the right ----
	const int32 Slots = GM.GetBonusSlots();
	const int32 Equipped = GM.GetEquippedBonuses();
	const int32 Used = FMath::CountBits(static_cast<uint32>(Equipped));
	const bool bFull = Used >= Slots;
	const int32 NextSlotLevel = GM.GetNextBonusSlotLevel();
	DrawHeader(BonusX, TEXT("BONUSES"), NextSlotLevel > 0 ? FString::Printf(TEXT("Reach level %d for another slot"), NextSlotLevel) : FString(TEXT("All slots open")), Green);

	// Slot boxes in the header's corner: filled when used, the level that opens each locked one.
	{
		const float Box = U(26.f);
		const float BoxGap = U(8.f);
		const int32 SlotLevels[] = { 0, GM.SecondSlotLevel, GM.ThirdSlotLevel };
		float BoxX = BonusX + PanelW - P - U(8.f) - 3.f * Box - 2.f * BoxGap;
		const float BoxY = Y + P + (HeaderH - U(14.f) - Box) * 0.5f;
		DrawLabel(TEXT("SLOTS"), BoxX - U(10.f), BoxY + (Box - LineH(TinySize)) * 0.5f, TinySize, TextDim, ESpinTextAlign::Right, true);
		for (int32 i = 0; i < 3; ++i, BoxX += Box + BoxGap)
		{
			if (i < Slots)
			{
				DrawCheckBox(BoxX, BoxY, Box, i < Used, Green);
			}
			else
			{
				DrawOutline(BoxX, BoxY, Box, Box, PanelEdge, FMath::Max(1.f, U(1.f)));
				DrawLabel(FString::Printf(TEXT("%d"), SlotLevels[i]), BoxX + Box * 0.5f, BoxY + (Box - LineH(TinySize)) * 0.5f, TinySize, TextFaint, ESpinTextAlign::Center, true);
			}
		}
	}

	for (int32 i = 0; i < NumBonuses; ++i)
	{
		const ESpinBonus Bonus = static_cast<ESpinBonus>(i);
		const FSpinBonusInfo& Info = ASpinGameMode::GetBonusInfo(Bonus);
		const bool bOn = (Equipped & (1 << i)) != 0;
		int32 Current = 0, Target = 1;
		GM.GetBonusProgress(Bonus, Current, Target);
		DrawRow(BonusX, i, Green, EMenuAction::ToggleBonus, i, GM.IsBonusUnlocked(Bonus), bOn,
			Info.Name, Info.Description, Info.Requirement, Current, Target,
			[&](float ValueRight, float RowY)
			{
				const float StatusY = RowY + (RowH - LineH(SmallSize)) * 0.5f;
				if (bOn)
				{
					DrawNeonText(TEXT("ON"), ValueRight, StatusY, SmallSize, Green, Green, 0.3f, ESpinTextAlign::Right);
				}
				else
				{
					DrawLabel(bFull ? TEXT("SLOTS FULL") : TEXT("OFF"), ValueRight, StatusY, SmallSize, TextFaint, ESpinTextAlign::Right, true);
				}
			});
	}

	// ---- Start / back ----
	const float BtnY = Y + PanelH + U(26.f);
	const float BtnW = U(260.f);
	const float BtnH = U(60.f);
	const float BtnGap = U(20.f);
	const float BtnX = CX - BtnGap * 0.5f - BtnW;
	DrawMenuButton(BtnX, BtnY, BtnW, BtnH, TEXT("START"), EMenuAction::StartEndless, Gold);
	MarkDefaultFocus();
	DrawMenuButton(BtnX + BtnW + BtnGap, BtnY, BtnW, BtnH, TEXT("BACK"), EMenuAction::Back, TextDim, BackKeyOf(GM));
}

void ASpinHUD::DrawSettings(const ASpinGameMode& GM)
{
	DrawBackdrop(0.75f);

	const float CX = ScreenW * 0.5f;
	float Y = DrawScreenTitle(TEXT("SETTINGS"), FString(), CX, ScreenH * 0.15f, Magenta) + U(10.f);

	const float PanelW = FMath::Min(U(640.f), ScreenW - U(Margin) * 2.f);
	const float X = CX - PanelW * 0.5f;
	const float RowH = U(76.f);
	const int32 NumRows = 4;
	const float P = U(Pad);
	DrawPanel(X, Y, PanelW, NumRows * RowH + P * 2.f, Magenta);

	// Each row is one menu item: left / right (or the < > buttons) change its value, and selecting
	// it steps forward.
	auto DrawRow = [&](int32 Index, const TCHAR* Label, const FString& Value, EMenuAction Prev, EMenuAction Next)
	{
		const float RowX = X + P;
		const float RowY = Y + P + Index * RowH;
		const float RowW = PanelW - P * 2.f;
		const FSpinFocusKey Key{ Next, INDEX_NONE };
		const float F = FocusAmount(Key);

		if (Index > 0)
		{
			DrawRect(PanelEdge, RowX, RowY, RowW, FMath::Max(1.f, U(1.f)));
		}
		DrawFocusFrame(RowX, RowY, RowW, RowH, Cyan, Key);

		DrawLabel(Label, X + U(28.f), RowY + (RowH - LineH(SmallSize)) * 0.5f, SmallSize, FMath::Lerp(TextDim, TextMain, F), ESpinTextAlign::Left, true);

		// [<]  VALUE  [>] on the right.
		const float Arrow = U(44.f);
		const float ValueW = U(200.f);
		const float NextX = X + PanelW - U(24.f) - Arrow;
		const float PrevX = NextX - ValueW - Arrow;
		const float ArrowY = RowY + (RowH - Arrow) * 0.5f;
		DrawArrowButton(PrevX, ArrowY, Arrow, false, Prev, F);
		DrawNeonText(Value, PrevX + Arrow + ValueW * 0.5f, RowY + (RowH - LineH(22.f)) * 0.5f, 22.f, TextMain, Cyan, 0.08f + 0.22f * F, ESpinTextAlign::Center);
		DrawArrowButton(NextX, ArrowY, Arrow, true, Next, F);

		AddRegion(RowX, RowY, RowW, RowH, Next, INDEX_NONE, Prev, Next, true, false);
		AddRegion(PrevX, ArrowY, Arrow, Arrow, Prev, INDEX_NONE, EMenuAction::None, EMenuAction::None, false, true);
		AddRegion(NextX, ArrowY, Arrow, Arrow, Next, INDEX_NONE, EMenuAction::None, EMenuAction::None, false, true);
	};

	DrawRow(0, TEXT("GRAPHICS QUALITY"), GM.GetQualityLabel(), EMenuAction::QualityPrev, EMenuAction::QualityNext);
	DrawRow(1, TEXT("RESOLUTION"), GM.GetResolutionLabel(), EMenuAction::ResolutionPrev, EMenuAction::ResolutionNext);
	DrawRow(2, TEXT("VOLUME"), FString::Printf(TEXT("%d%%"), GM.VolumePercent), EMenuAction::VolumeDown, EMenuAction::VolumeUp);
	// Only two choices, so both arrows just flip it.
	DrawRow(3, TEXT("CONTROLLER ICONS"), GM.GetControllerIconsLabel(), EMenuAction::ToggleControllerIcons, EMenuAction::ToggleControllerIcons);

	const float BackW = U(220.f);
	DrawMenuButton(CX - BackW * 0.5f, Y + NumRows * RowH + P * 2.f + U(32.f), BackW, U(50.f), TEXT("BACK"), EMenuAction::Back, TextDim, BackKeyOf(GM));
}

void ASpinHUD::DrawLeaderboard(const ASpinGameMode& GM)
{
	DrawBackdrop(0.82f);

	const float CX = ScreenW * 0.5f;
	const int32 MaxRows = FMath::Max(1, GM.LeaderboardSize);
	float Y = DrawScreenTitle(TEXT("LEADERBOARD"), FString::Printf(TEXT("Top %d runs from everyone who plays here"), MaxRows), CX, ScreenH * 0.09f, Gold);

	const USpinSaveGame* Save = GM.GetSaveData();
	const FString Me = GM.GetPlayerName();

	const float Gap = U(24.f);
	const float PanelW = FMath::Min(U(580.f), (ScreenW - U(Margin) * 2.f - Gap * 2.f) / 3.f);
	const float P = U(Pad) + U(4.f);
	const float RowH = U(36.f);
	const float HeaderH = LineH(HeadingSize) + U(8.f) + LineH(TinySize) + U(8.f);
	const float PanelH = P * 2.f + HeaderH + MaxRows * RowH;

	auto DrawBoard = [&](float X, const TCHAR* Heading, const FLinearColor& Accent, const TArray<FSpinRunRecord>* Board, int32 LatestRank, bool bByLevel)
	{
		DrawPanel(X, Y, PanelW, PanelH, Accent);

		float RowY = Y + P;
		DrawNeonText(Heading, X + P, RowY, HeadingSize, Accent, Accent, 0.18f, ESpinTextAlign::Left);
		RowY += LineH(HeadingSize) + U(8.f);

		// Fixed columns so names and numbers line up.
		const float RankX = X + P;
		const float NameX = X + P + U(44.f);
		const float PrimaryRight = X + PanelW - P - U(120.f);
		const float SecondaryRight = X + PanelW - P;
		DrawLabel(TEXT("#"), RankX, RowY, TinySize, TextFaint, ESpinTextAlign::Left, true);
		DrawLabel(TEXT("PLAYER"), NameX, RowY, TinySize, TextFaint, ESpinTextAlign::Left, true);
		DrawLabel(bByLevel ? TEXT("LEVEL") : TEXT("SCORE"), PrimaryRight, RowY, TinySize, TextFaint, ESpinTextAlign::Right, true);
		DrawLabel(bByLevel ? TEXT("SCORE") : TEXT("LEVEL"), SecondaryRight, RowY, TinySize, TextFaint, ESpinTextAlign::Right, true);
		RowY += LineH(TinySize) + U(8.f);

		if (!Board || Board->Num() == 0)
		{
			DrawLabel(TEXT("No runs yet - be the first!"), X + PanelW * 0.5f, RowY + U(24.f), BodySize, TextDim, ESpinTextAlign::Center);
			return;
		}

		for (int32 i = 0; i < Board->Num() && i < MaxRows; ++i)
		{
			const FSpinRunRecord& Run = (*Board)[i];
			const float ThisRowY = RowY + i * RowH;
			const bool bLatest = i + 1 == LatestRank;
			const bool bMine = !Me.IsEmpty() && Run.PlayerName.Equals(Me, ESearchCase::IgnoreCase);

			if (i % 2 == 0)
			{
				DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.03f), X + U(6.f), ThisRowY, PanelW - U(12.f), RowH);
			}
			if (bLatest)
			{
				const float Glow = 0.75f + 0.25f * FMath::Sin(UiTime * 4.f);
				DrawRect(Fade(Green, 0.12f * Glow), X + U(6.f), ThisRowY, PanelW - U(12.f), RowH);
				DrawNeonLine(FVector2D(X + U(7.f), ThisRowY), FVector2D(X + U(7.f), ThisRowY + RowH), Green, FMath::Max(2.f, U(3.f)), 0.4f * Glow);
			}

			const float TextY = ThisRowY + (RowH - LineH(SmallSize)) * 0.5f;
			const FString Level = FString::Printf(TEXT("%d"), Run.Level);
			const FString Points = FormatPoints(Run.Points);
			if (i == 0)
			{
				DrawNeonText(TEXT("1"), RankX, TextY, SmallSize, Gold, Gold, 0.3f, ESpinTextAlign::Left);
			}
			else
			{
				DrawLabel(FString::FromInt(i + 1), RankX, TextY, SmallSize, TextDim, ESpinTextAlign::Left, true);
			}
			const float NameW = DrawLabel(Run.PlayerName.IsEmpty() ? FString(TEXT("???")) : Run.PlayerName, NameX, TextY, SmallSize, bMine ? Cyan : TextMain, ESpinTextAlign::Left, bMine);
			// Runs played with modifiers carry their bonus, so a boosted score reads as one, and runs
			// that used bonuses carry a badge.
			float TagX = NameX + NameW + U(8.f);
			if (Run.Modifiers != 0)
			{
				TagX += DrawLabel(FormatMultiplier(ASpinGameMode::GetModifiersMultiplier(Run.Modifiers)), TagX, TextY, TinySize, Purple, ESpinTextAlign::Left, true) + U(6.f);
			}
			if (Run.Bonuses != 0)
			{
				DrawBonusBadge(TagX, ThisRowY + RowH * 0.5f);
			}
			DrawLabel(bByLevel ? Level : Points, PrimaryRight, TextY, SmallSize, i == 0 ? Gold : TextMain, ESpinTextAlign::Right, true);
			DrawLabel(bByLevel ? Points : Level, SecondaryRight, TextY, SmallSize, TextDim, ESpinTextAlign::Right);
		}
	};

	DrawBoard(CX - PanelW * 1.5f - Gap, TEXT("HIGHEST LEVEL"), Cyan, Save ? &Save->LevelBoard : nullptr, GM.LastRunLevelRank, true);
	DrawBoard(CX - PanelW * 0.5f, TEXT("HIGHEST SCORE"), Gold, Save ? &Save->ScoreBoard : nullptr, GM.LastRunScoreRank, false);
	DrawBoard(CX + PanelW * 0.5f + Gap, TEXT("TODAY'S DAILY"), Purple, Save ? Save->GetDailyBoard(ASpinGameMode::GetTodayKey()) : nullptr, GM.LastRunDailyRank, false);

	Y += PanelH + U(14.f);
	const FString Legend = TEXT("Green row = your last run      Cyan name = you      ");
	const FString BonusLegend = TEXT("= played with bonuses");
	const float BadgeW = U(18.f) + U(6.f);
	const float LegendW = MeasureText(Legend, TinySize).X + BadgeW + MeasureText(BonusLegend, TinySize).X;
	float LegendX = CX - LegendW * 0.5f;
	LegendX += DrawLabel(Legend, LegendX, Y, TinySize, TextFaint);
	DrawBonusBadge(LegendX, Y + LineH(TinySize) * 0.5f);
	DrawLabel(BonusLegend, LegendX + BadgeW, Y, TinySize, TextFaint);
	Y += LineH(TinySize) + U(20.f);

	const float BackW = U(220.f);
	DrawMenuButton(CX - BackW * 0.5f, Y, BackW, U(50.f), TEXT("BACK"), EMenuAction::Back, TextDim, BackKeyOf(GM));
}

void ASpinHUD::DrawProfileSelect(const ASpinGameMode& GM)
{
	DrawBackdrop(0.82f);

	const float CX = ScreenW * 0.5f;
	float Y = DrawScreenTitle(TEXT("WHO'S PLAYING?"), TEXT("Pick a player, or add a new one"), CX, ScreenH * 0.12f, Cyan);

	const USpinSaveGame* Save = GM.GetSaveData();
	const int32 NumProfiles = Save ? Save->Profiles.Num() : 0;
	const bool bConfirming = Save && Save->Profiles.IsValidIndex(GM.PendingDeleteProfile);

	const float RowH = U(58.f);
	const float Gap = U(10.f);
	const float DeleteW = U(58.f);
	const float TotalW = FMath::Min(U(700.f), ScreenW - U(Margin) * 2.f);
	const float RowW = TotalW - Gap - DeleteW;
	const float X = CX - TotalW * 0.5f;

	for (int32 i = 0; i < NumProfiles; ++i, Y += RowH + Gap)
	{
		const FSpinProfile& Profile = Save->Profiles[i];

		if (i == GM.PendingDeleteProfile)
		{
			// Confirmation in place of the row - deleting also takes their runs off the boards. NO is
			// highlighted first, so a stray press can't delete anyone.
			DrawPanel(X, Y, TotalW, RowH, Red);
			DrawLabel(FString::Printf(TEXT("Delete %s and all their scores?"), *Profile.Name), X + U(20.f), Y + (RowH - LineH(BodySize)) * 0.5f, BodySize, TextMain, ESpinTextAlign::Left, true);
			const float BtnW = U(96.f);
			const float BtnH = RowH - U(16.f);
			DrawMenuButton(X + TotalW - U(10.f) - BtnW * 2.f - U(8.f), Y + U(8.f), BtnW, BtnH, TEXT("YES"), EMenuAction::ConfirmDeleteProfile, Red);
			DrawMenuButton(X + TotalW - U(10.f) - BtnW, Y + U(8.f), BtnW, BtnH, TEXT("NO"), EMenuAction::CancelDeleteProfile, TextDim);
			MarkDefaultFocus();
			continue;
		}

		// The whole row picks the player; the current one is lit.
		const bool bActive = Save->ActiveProfile == i;
		const FSpinFocusKey RowKey{ EMenuAction::SelectProfile, i };
		const float F = FocusAmount(RowKey);
		DrawPanel(X, Y, RowW, RowH, bActive ? Cyan : TextFaint);
		DrawFocusFrame(X, Y, RowW, RowH, Cyan, RowKey);
		AddRegion(X, Y, RowW, RowH, EMenuAction::SelectProfile, i);
		if (bActive && !bConfirming)
		{
			MarkDefaultFocus();
		}

		const float NameSize = 24.f;
		const float NameW = DrawNeonText(Profile.Name, X + U(20.f), Y + (RowH - LineH(NameSize)) * 0.5f, NameSize, bActive ? Cyan : FMath::Lerp(TextMain * 0.9f, FLinearColor::White, F),
			Cyan, bActive ? 0.2f : 0.15f * F, ESpinTextAlign::Left);
		if (bActive)
		{
			DrawLabel(TEXT("PLAYING"), X + U(20.f) + NameW + U(12.f), Y + (RowH - LineH(TinySize)) * 0.5f, TinySize, Cyan, ESpinTextAlign::Left, true);
		}
		DrawLabel(Profile.RunsPlayed > 0
				? FString::Printf(TEXT("BEST  LV %d  /  %s"), Profile.BestLevel, *FormatPoints(Profile.BestPoints))
				: FString(TEXT("No runs yet")),
			X + RowW - U(20.f), Y + (RowH - LineH(SmallSize)) * 0.5f, SmallSize, TextDim, ESpinTextAlign::Right, true);

		// Delete.
		const float DelX = X + RowW + Gap;
		const FSpinFocusKey DelKey{ EMenuAction::DeleteProfile, i };
		const float DelF = FocusAmount(DelKey);
		FVector2D DelPts[6];
		ChamferRect(DelX, Y, DeleteW, RowH, U(10.f), DelPts);
		FillPolygon(DelPts, 6, Fade(Red, 0.1f + 0.25f * DelF), Fade(Red, 0.06f + 0.15f * DelF));
		for (int32 Edge = 0; Edge < 6; ++Edge)
		{
			DrawNeonLine(DelPts[Edge], DelPts[(Edge + 1) % 6], Fade(Red, 0.45f + 0.55f * DelF), FMath::Max(1.f, U(1.f + DelF)), 0.35f * DelF);
		}
		DrawFocusFrame(DelX, Y, DeleteW, RowH, Red, DelKey);
		DrawNeonText(TEXT("X"), DelX + DeleteW * 0.5f, Y + (RowH - LineH(HeadingSize)) * 0.5f, HeadingSize, Red, Red, 0.15f + 0.3f * DelF, ESpinTextAlign::Center);
		AddRegion(DelX, Y, DeleteW, RowH, EMenuAction::DeleteProfile, i);
	}

	Y += U(6.f);
	if (NumProfiles < USpinSaveGame::MaxProfiles)
	{
		DrawMenuButton(X, Y, TotalW, RowH, TEXT("+   NEW PLAYER"), EMenuAction::NewProfile, Green);
		if (!bHasDefaultFocus)
		{
			MarkDefaultFocus();
		}
	}
	else
	{
		DrawLabel(FString::Printf(TEXT("%d players max - delete one to add another"), USpinSaveGame::MaxProfiles), CX, Y + (RowH - LineH(SmallSize)) * 0.5f, SmallSize, TextDim, ESpinTextAlign::Center);
	}
	Y += RowH + U(28.f);

	const float BackW = U(220.f);
	DrawMenuButton(CX - BackW * 0.5f, Y, BackW, U(50.f), TEXT("BACK"), EMenuAction::Back, TextDim, BackKeyOf(GM));
}

void ASpinHUD::DrawNameEntry(const ASpinGameMode& GM)
{
	DrawBackdrop(0.86f);

	const float CX = ScreenW * 0.5f;
	float Y = DrawScreenTitle(TEXT("WHAT'S YOUR NAME?"), TEXT("It goes on the leaderboard next to your runs"), CX, ScreenH * 0.2f, Cyan) + U(10.f);

	// The text box, with a blinking block cursor.
	const float BoxW = FMath::Min(U(540.f), ScreenW - U(Margin) * 2.f);
	const float BoxH = U(84.f);
	const float BoxX = CX - BoxW * 0.5f;
	DrawGlowSprite(CX, Y + BoxH * 0.5f, BoxW * 1.2f, BoxH * 2.f, Cyan, 0.08f + 0.04f * Pulse01(UiTime, 3.f));
	DrawPanel(BoxX, Y, BoxW, BoxH, Cyan);

	const bool bCursorOn = FMath::Fmod(UiTime, 1.f) < 0.55f;
	const bool bFull = GM.NameEntryText.Len() >= USpinSaveGame::MaxNameLength;
	const float TextY = Y + (BoxH - LineH(TitleSize)) * 0.5f;
	if (GM.NameEntryText.IsEmpty())
	{
		DrawLabel(TEXT("TYPE A NAME"), CX, TextY, TitleSize, Fade(TextFaint, bCursorOn ? 1.f : 0.6f), ESpinTextAlign::Center, true);
	}
	else
	{
		const float NameW = DrawNeonText(GM.NameEntryText, CX, TextY, TitleSize, TextMain, Cyan, 0.2f, ESpinTextAlign::Center);
		if (bCursorOn && !bFull)
		{
			const float CursorH = LineH(TitleSize) - U(14.f);
			DrawRect(Fade(Cyan, 0.9f), CX + NameW * 0.5f + U(4.f), TextY + U(7.f), U(14.f), CursorH);
			DrawGlowSprite(CX + NameW * 0.5f + U(11.f), TextY + U(7.f) + CursorH * 0.5f, U(40.f), CursorH * 1.6f, Cyan, 0.3f);
		}
	}
	DrawLabel(FString::Printf(TEXT("%d / %d"), GM.NameEntryText.Len(), USpinSaveGame::MaxNameLength), BoxX + BoxW - U(12.f), Y + BoxH + U(6.f), TinySize, TextFaint, ESpinTextAlign::Right);
	Y += BoxH + U(34.f);

	// [ENTER] confirm   [BKSP] erase   [ESC] cancel
	{
		const float Size = SmallSize;
		const TPair<const TCHAR*, const TCHAR*> Keys[] = { { TEXT("ENTER"), TEXT("confirm") }, { TEXT("BKSP"), TEXT("erase") }, { TEXT("ESC"), TEXT("cancel") } };
		const float Space = U(8.f);
		const float ItemGap = U(30.f);
		float TotalW = 0.f;
		for (const auto& Key : Keys)
		{
			TotalW += KeyCapWidth(Key.Key, Size) + Space + MeasureText(Key.Value, Size).X;
		}
		TotalW += ItemGap * static_cast<float>(UE_ARRAY_COUNT(Keys) - 1);

		float KeyX = CX - TotalW * 0.5f;
		const float CapH = KeyCapHeight(Size);
		for (const auto& Key : Keys)
		{
			KeyX += DrawKeyCap(Key.Key, KeyX, Y, Size, Cyan, false) + Space;
			KeyX += DrawLabel(Key.Value, KeyX, Y + (CapH - LineH(Size)) * 0.5f, Size, TextDim) + ItemGap;
		}
		Y += CapH + U(14.f);
	}

	DrawLabel(TEXT("Letters, numbers and spaces  -  keyboard only"), CX, Y, TinySize, TextFaint, ESpinTextAlign::Center);
	Y += LineH(TinySize) + U(10.f);

	if (!GM.NameEntryError.IsEmpty())
	{
		DrawNeonText(GM.NameEntryError, CX, Y, SmallSize, Red, Red, 0.2f, ESpinTextAlign::Center);
	}
	Y += LineH(SmallSize) + U(16.f);

	const float BtnW = U(200.f);
	const float BtnH = U(56.f);
	const float Gap = U(20.f);
	DrawMenuButton(CX - Gap * 0.5f - BtnW, Y, BtnW, BtnH, TEXT("OK"), EMenuAction::ConfirmName, Green);
	MarkDefaultFocus();
	DrawMenuButton(CX + Gap * 0.5f, Y, BtnW, BtnH, TEXT("CANCEL"), EMenuAction::CancelName, TextDim);
}

void ASpinHUD::DrawGameOver(const ASpinGameMode& GM)
{
	DrawBackdrop(0.82f);

	const float CX = ScreenW * 0.5f;
	const float CardW = FMath::Min(U(820.f), ScreenW - U(Margin) * 2.f);
	const float P = U(32.f);
	const float TileH = U(156.f);
	const float BtnH = U(60.f);
	const float GameOverSize = 56.f;

	const FString Name = GM.GetPlayerName();
	FString Reason;
	if (GM.bHandBroke)
	{
		Reason = TEXT("The hand broke - the jam wasn't fixed in time.");
	}
	else if (GM.bOutOfLives)
	{
		Reason = TEXT("Out of lives!");
	}
	else if (GM.bOutOfMissLives)
	{
		Reason = TEXT("Missed too many zones!");
	}

	// What kind of run it was, if not a plain one.
	FString RunKind;
	if (GM.bDailyRun)
	{
		RunKind = FString::Printf(TEXT("DAILY CHALLENGE  -  %s"), *ASpinGameMode::GetModifierNames(GM.RunModifiers));
	}
	else if (GM.RunModifiers != 0)
	{
		RunKind = FString::Printf(TEXT("%s  (%s POINTS)"), *ASpinGameMode::GetModifierNames(GM.RunModifiers), *FormatMultiplier(GM.RunPointsMultiplier));
	}
	const float RunKindH = RunKind.IsEmpty() ? 0.f : LineH(SmallSize) + U(4.f);
	const FString Bonuses = GM.RunBonuses != 0 ? FString::Printf(TEXT("BONUSES:  %s"), *ASpinGameMode::GetBonusNames(GM.RunBonuses)) : FString();
	const float BonusesH = Bonuses.IsEmpty() ? 0.f : LineH(SmallSize) + U(4.f);

	// What this run unlocked, in a strip of its own above the buttons.
	const FString Unlocks = FString::Join(GM.NewUnlocks, TEXT("   +   "));
	const float UnlockPad = U(12.f);
	const float UnlockH = Unlocks.IsEmpty() ? 0.f : UnlockPad * 2.f + LineH(TinySize) + LineH(HeadingSize) + LineH(TinySize) + U(16.f);

	const float StatPad = U(12.f);
	const float StatH = StatPad * 2.f + LineH(TinySize) * 2.f + LineH(HeadingSize);
	const float CardH = P + LineH(TinySize) + U(2.f) + LineH(GameOverSize) + U(4.f) + LineH(BodySize) + RunKindH + BonusesH + U(26.f) + TileH + U(16.f) + StatH + UnlockH + U(28.f) + BtnH + P;
	const float X = CX - CardW * 0.5f;
	float Y = (ScreenH - CardH) * 0.5f;
	DrawPanel(X, Y, CardW, CardH, Red);
	Y += P;

	DrawLabel(Name, CX, Y, TinySize, Cyan, ESpinTextAlign::Center, true);
	Y += LineH(TinySize) + U(2.f);

	// GAME OVER glitches hard as the screen comes up, then settles into a buzzing red sign.
	const float SinceEnter = UiTime - ScreenEnterTime;
	const bool bGlitch = SinceEnter < 0.8f || Hash01(static_cast<uint32>(FMath::FloorToInt(UiTime * 6.f)) * 389u) < 0.05f;
	const float Shake = bGlitch ? U(8.f) * (Hash01(static_cast<uint32>(FMath::FloorToInt(UiTime * 30.f))) - 0.5f) : 0.f;
	DrawNeonText(TEXT("GAME OVER"), CX + Shake, Y, GameOverSize, Fade(Red, NeonFlicker(UiTime, 666u, 0.05f)), Red, 0.5f, ESpinTextAlign::Center,
		bGlitch ? U(5.f) : U(1.5f));
	Y += LineH(GameOverSize) + U(4.f);
	DrawLabel(Reason, CX, Y, BodySize, TextMain, ESpinTextAlign::Center);
	Y += LineH(BodySize);
	if (!RunKind.IsEmpty())
	{
		DrawLabel(RunKind, CX, Y + U(4.f), SmallSize, Purple, ESpinTextAlign::Center, true);
		Y += RunKindH;
	}
	if (!Bonuses.IsEmpty())
	{
		DrawLabel(Bonuses, CX, Y + U(4.f), SmallSize, Green, ESpinTextAlign::Center, true);
		Y += BonusesH;
	}
	Y += U(26.f);

	// The two results the leaderboards rank by, as tiles - each with how it stacks up.
	// BestLevel / BestPoints are the player's bests from before this run (for a daily run,
	// BestPoints is today's daily best, and the level isn't a record at all).
	const int32 Level = GM.GetLevel();
	const bool bNewBestLevel = !GM.bDailyRun && GM.Hits > 0 && Level > GM.BestLevel;
	const bool bNewBestPoints = GM.Hits > 0 && GM.Points > GM.BestPoints;
	const int32 ScoreRank = GM.bDailyRun ? GM.LastRunDailyRank : GM.LastRunScoreRank;

	FString LevelStatus;
	if (GM.bDailyRun)
	{
		LevelStatus = TEXT("Daily runs rank by score");
	}
	else if (bNewBestLevel)
	{
		LevelStatus = TEXT("NEW BEST!");
	}
	else if (GM.BestLevel > 0)
	{
		LevelStatus = GM.LastRunLevelRank > 0
			? FString::Printf(TEXT("#%d on the board  -  your best is %d"), GM.LastRunLevelRank, GM.BestLevel)
			: FString::Printf(TEXT("Your best is %d"), GM.BestLevel);
	}

	FString PointsStatus;
	if (bNewBestPoints)
	{
		PointsStatus = !GM.bDailyRun ? FString(TEXT("NEW BEST!"))
			: ScoreRank > 0 ? FString::Printf(TEXT("NEW BEST TODAY!  -  #%d today"), ScoreRank)
			: FString(TEXT("NEW BEST TODAY!"));
	}
	else if (GM.BestPoints > 0)
	{
		const FString Short = FString::Printf(TEXT("%s short of your best%s"), *FormatPoints(GM.BestPoints - GM.Points), GM.bDailyRun ? TEXT(" today") : TEXT(""));
		PointsStatus = ScoreRank > 0 ? FString::Printf(TEXT("#%d  -  %s"), ScoreRank, *Short) : Short;
	}

	const float TileGap = U(20.f);
	const float TileW = (CardW - P * 2.f - TileGap) * 0.5f;
	auto DrawTile = [&](float TileX, const TCHAR* Label, const FString& Value, const FString& Status, bool bNewBest)
	{
		DrawRect(bNewBest ? Fade(Gold, 0.08f + 0.04f * Pulse01(UiTime, 4.f)) : FLinearColor(1.f, 1.f, 1.f, 0.03f), TileX, Y, TileW, TileH);
		if (bNewBest)
		{
			DrawCornerBrackets(TileX, Y, TileW, TileH, Gold, U(18.f), FMath::Max(1.f, U(2.f)), 0.35f);
		}
		DrawOutline(TileX, Y, TileW, TileH, bNewBest ? Fade(Gold, 0.6f) : PanelEdge, FMath::Max(1.f, U(1.f)));

		const float TileCX = TileX + TileW * 0.5f;
		DrawLabel(Label, TileCX, Y + U(14.f), TinySize, TextDim, ESpinTextAlign::Center, true);
		DrawNeonText(Value, TileCX, Y + U(14.f) + LineH(TinySize), 48.f, bNewBest ? Gold : TextMain, bNewBest ? Gold : Cyan, bNewBest ? 0.4f : 0.12f, ESpinTextAlign::Center);
		DrawLabel(Status, TileCX, Y + TileH - U(14.f) - LineH(SmallSize), SmallSize, bNewBest ? Gold : TextDim, ESpinTextAlign::Center, true);
	};
	DrawTile(X + P, TEXT("LEVEL"), FString::FromInt(Level), LevelStatus, bNewBestLevel);
	DrawTile(X + P + TileW + TileGap, TEXT("SCORE"), FormatPoints(GM.Points), PointsStatus, bNewBestPoints);
	Y += TileH + U(16.f);

	// The run breakdown: how the score was built and where it leaked.
	const FString ComboDetail = GM.PeakMultiplier > 1 ? FString::Printf(TEXT("PERFECTs in a row  -  peak x%d"), GM.PeakMultiplier) : FString(TEXT("PERFECTs in a row"));
	const FString ObstacleDetail = FString::Printf(TEXT("%d cleared"), GM.ObstaclesCleared);

	const float StatGap = U(12.f);
	const float StatW = (CardW - P * 2.f - StatGap * 2.f) / 3.f;
	auto DrawStat = [&](int32 Slot, const TCHAR* Label, const FString& Value, const FString& Detail, const FLinearColor& ValueColor)
	{
		const float StatX = X + P + (StatW + StatGap) * Slot;
		const float StatCX = StatX + StatW * 0.5f;
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.03f), StatX, Y, StatW, StatH);
		DrawOutline(StatX, Y, StatW, StatH, PanelEdge, FMath::Max(1.f, U(1.f)));
		float StatY = Y + StatPad;
		DrawLabel(Label, StatCX, StatY, TinySize, TextDim, ESpinTextAlign::Center, true);
		StatY += LineH(TinySize);
		DrawLabel(Value, StatCX, StatY, HeadingSize, ValueColor, ESpinTextAlign::Center, true);
		StatY += LineH(HeadingSize);
		DrawLabel(Detail, StatCX, StatY, TinySize, TextDim, ESpinTextAlign::Center);
	};
	DrawStat(0, TEXT("LONGEST COMBO"), FString::FromInt(GM.LongestPerfectStreak), ComboDetail, GM.PeakMultiplier > 1 ? Gold : TextMain);
	DrawStat(1, TEXT("LOST TO LIFTING"), GM.PointsLostToLift > 0 ? TEXT("-") + FormatPoints(GM.PointsLostToLift) : FString(TEXT("0")), TEXT("points"), GM.PointsLostToLift > 0 ? Red : TextMain);
	DrawStat(2, TEXT("OBSTACLES HIT"), FString::FromInt(GM.ObstaclesHit), ObstacleDetail, GM.ObstaclesHit > 0 ? Red : TextMain);
	Y += StatH;

	if (!Unlocks.IsEmpty())
	{
		Y += U(16.f);
		const float StripX = X + P;
		const float StripW = CardW - P * 2.f;
		const float StripH = UnlockH - U(16.f);
		const float Glow = Pulse01(UiTime, 4.f);
		DrawRect(Fade(Green, 0.08f + 0.06f * Glow), StripX, Y, StripW, StripH);
		DrawGlowSprite(CX, Y + StripH * 0.5f, StripW * 1.1f, StripH * 1.8f, Green, 0.1f + 0.08f * Glow);
		DrawCornerBrackets(StripX, Y, StripW, StripH, Fade(Green, 0.6f + 0.4f * Glow), U(16.f), FMath::Max(1.f, U(2.f)), 0.3f);

		float StripY = Y + UnlockPad;
		DrawLabel(TEXT("UNLOCKED"), CX, StripY, TinySize, Green, ESpinTextAlign::Center, true);
		StripY += LineH(TinySize);
		DrawNeonText(Unlocks, CX, StripY, HeadingSize, TextMain, Green, 0.25f, ESpinTextAlign::Center);
		StripY += LineH(HeadingSize);
		DrawLabel(TEXT("Switch them on in run setup, before your next Endless run"), CX, StripY, TinySize, TextDim, ESpinTextAlign::Center);
		Y += StripH;
	}
	Y += U(28.f);

	// Shown straight away but only selectable after the short delay, so a press meant for the hand
	// can't land on one by accident.
	const bool bReady = GM.IsGameOverMenuReady();
	const float BtnGap = U(16.f);
	const float BtnW = (CardW - P * 2.f - BtnGap * 2.f) / 3.f;
	DrawMenuButton(X + P, Y, BtnW, BtnH, TEXT("RETRY"), EMenuAction::RetryGame, Gold, FString(), bReady);
	MarkDefaultFocus();
	DrawMenuButton(X + P + BtnW + BtnGap, Y, BtnW, BtnH, TEXT("MAIN MENU"), EMenuAction::ReturnToMainMenu, Cyan, bReady ? BackKeyOf(GM) : FString(), bReady);
	DrawMenuButton(X + P + (BtnW + BtnGap) * 2.f, Y, BtnW, BtnH, TEXT("QUIT"), EMenuAction::QuitGame, Red, FString(), bReady);
}

void ASpinHUD::DrawPauseMenu(const ASpinGameMode& GM)
{
	DrawBackdrop(0.7f);

	const float CX = ScreenW * 0.5f;
	const float CardW = FMath::Min(U(520.f), ScreenW - U(Margin) * 2.f);
	const float P = U(32.f);
	const float BtnH = U(60.f);
	const float BtnGap = U(14.f);
	const float PromptSize = 18.f;

	// Where the run is, so the player knows what they'd be restarting or leaving.
	FString Eyebrow;
	if (GM.IsTutorial())
	{
		Eyebrow = FString::Printf(TEXT("TUTORIAL  -  STAGE %d / %d"), GM.TutorialIndex + 1, GM.TutorialLevels.Num());
	}
	else
	{
		Eyebrow = FString::Printf(TEXT("%s  -  LEVEL %d"), GM.bDailyRun ? TEXT("DAILY CHALLENGE") : TEXT("ENDLESS"), GM.GetLevel());
	}
	const FString Warning = GM.IsTutorial() ? FString() : FString(TEXT("Restarting or leaving won't save this run."));
	const float WarningH = Warning.IsEmpty() ? 0.f : LineH(SmallSize) + U(14.f);

	const float CardH = P + LineH(TinySize) + U(6.f) + LineH(TitleSize) + U(24.f) + BtnH * 3.f + BtnGap * 2.f + U(20.f) + WarningH + KeyCapHeight(PromptSize) + P;
	const float X = CX - CardW * 0.5f;
	float Y = (ScreenH - CardH) * 0.5f;
	DrawPanel(X, Y, CardW, CardH, Cyan);
	Y += P;

	DrawLabel(Eyebrow, CX, Y, TinySize, Cyan, ESpinTextAlign::Center, true);
	Y += LineH(TinySize) + U(6.f);
	DrawNeonText(TEXT("PAUSED"), CX, Y, TitleSize, Fade(Gold, 0.75f + 0.25f * Pulse01(UiTime, 2.5f)), Gold, 0.35f, ESpinTextAlign::Center, U(1.5f));
	Y += LineH(TitleSize) + U(24.f);

	// RESUME is highlighted first, and backing out resumes too - so a habitual press of B / O can't
	// throw the run away.
	const float BtnW = CardW - P * 2.f;
	DrawMenuButton(X + P, Y, BtnW, BtnH, TEXT("RESUME"), EMenuAction::ResumeGame, Gold);
	MarkDefaultFocus();
	Y += BtnH + BtnGap;
	DrawMenuButton(X + P, Y, BtnW, BtnH, TEXT("RESTART"), EMenuAction::RestartRun, Cyan);
	Y += BtnH + BtnGap;
	DrawMenuButton(X + P, Y, BtnW, BtnH, TEXT("MAIN MENU"), EMenuAction::QuitRunToMenu, Red);
	Y += BtnH + U(20.f);

	if (!Warning.IsEmpty())
	{
		DrawLabel(Warning, CX, Y, SmallSize, TextDim, ESpinTextAlign::Center);
		Y += WarningH;
	}

	DrawKeyPrompt(TEXT("Press"), GM.GetPauseKey(), TEXT("to resume"), CX, Y, PromptSize, Gold);
}

void ASpinHUD::DrawBriefing(const FString& Eyebrow, const FString& Title, const TArray<FSpinBriefLine>& Lines,
	const FString& ConfirmVerb, const FString& ConfirmKey, const FLinearColor& ConfirmColor)
{
	DrawBackdrop(0.82f);

	const float CX = ScreenW * 0.5f;
	const float CardW = FMath::Min(U(860.f), ScreenW - U(Margin) * 2.f);
	const float P = U(32.f);
	const float LineGap = U(10.f);
	const float PromptSize = 20.f;

	// Wrap every line up front to know the card's height.
	TArray<TArray<FString>> Wrapped;
	float LinesH = 0.f;
	for (const FSpinBriefLine& Line : Lines)
	{
		Wrapped.Add(WrapText(Line.Text, BodySize, false, CardW - P * 2.f));
		LinesH += Wrapped.Last().Num() * LineH(BodySize) + LineGap;
	}

	const float CardH = P + LineH(TinySize) + U(6.f) + LineH(TitleSize) + U(20.f) + LinesH + U(18.f) + KeyCapHeight(PromptSize) + P;
	float Y = (ScreenH - CardH) * 0.5f;
	DrawPanel(CX - CardW * 0.5f, Y, CardW, CardH, Cyan);
	Y += P;

	// "// NEW MECHANIC //" eyebrow, then the title.
	DrawLabel(FString::Printf(TEXT("//  %s  //"), *Eyebrow), CX, Y, TinySize, Fade(Cyan, 0.65f + 0.35f * Pulse01(UiTime, 5.f)), ESpinTextAlign::Center, true);
	Y += LineH(TinySize) + U(6.f);
	const bool bGlitch = UiTime - ScreenEnterTime < 0.45f;
	DrawNeonText(Title, CX, Y, TitleSize, Gold, Gold, 0.35f, ESpinTextAlign::Center, bGlitch ? U(4.f) : U(1.f));
	Y += LineH(TitleSize) + U(20.f);

	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		for (const FString& Line : Wrapped[i])
		{
			DrawLabel(Line, CX, Y, BodySize, Lines[i].Color, ESpinTextAlign::Center);
			Y += LineH(BodySize);
		}
		Y += LineGap;
	}
	Y += U(18.f);

	// The prompt pulses, so it's clear the game is waiting on it.
	const float PromptGlow = Pulse01(UiTime, 4.f);
	DrawGlowSprite(CX, Y + KeyCapHeight(PromptSize) * 0.5f, U(360.f), KeyCapHeight(PromptSize) * 2.2f, ConfirmColor, 0.06f + 0.08f * PromptGlow);
	DrawKeyPrompt(ConfirmVerb, ConfirmKey, TEXT("to continue"), CX, Y, PromptSize, FMath::Lerp(ConfirmColor, FLinearColor::White, 0.25f * PromptGlow));
}
