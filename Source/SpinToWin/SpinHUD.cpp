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
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "GameFramework/PlayerController.h"

namespace
{
	// ---- Palette: neon on near-black, to match the city. Each colour has one job. ----
	const FLinearColor PanelFill(0.012f, 0.018f, 0.04f, 0.86f);
	const FLinearColor PanelEdge(0.35f, 0.5f, 0.7f, 0.3f);
	const FLinearColor TrackColor(0.09f, 0.11f, 0.16f, 0.95f);
	const FLinearColor TextMain(0.94f, 0.96f, 1.f);
	const FLinearColor TextDim(0.62f, 0.68f, 0.78f);
	const FLinearColor TextFaint(0.4f, 0.45f, 0.55f);
	const FLinearColor Cyan(0.25f, 0.85f, 1.f);   // UI accent, level, the lift
	const FLinearColor Gold(1.f, 0.8f, 0.22f);    // score, bests, PERFECT
	const FLinearColor Green(0.35f, 1.f, 0.5f);   // good things: +1, repair, GO
	const FLinearColor Orange(1.f, 0.58f, 0.14f); // warnings, miss lives, unlocks
	const FLinearColor Red(1.f, 0.3f, 0.32f);     // danger, obstacle lives, game over
	const FLinearColor Purple(0.78f, 0.45f, 1.f); // run modifiers and the daily challenge

	// ---- Type scale, in points at 1080p. ----
	constexpr float HeroSize = 72.f;
	constexpr float TitleSize = 40.f;
	constexpr float HeadingSize = 26.f;
	constexpr float BodySize = 19.f;
	constexpr float SmallSize = 15.f;
	constexpr float TinySize = 12.f;

	// ---- Spacing, in design pixels. ----
	constexpr float Margin = 28.f;
	constexpr float Pad = 16.f;


	FLinearColor Fade(FLinearColor Color, float Alpha)
	{
		Color.A *= FMath::Clamp(Alpha, 0.f, 1.f);
		return Color;
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

	float Pulse01(float Time, float Speed)
	{
		return 0.5f + 0.5f * FMath::Sin(Time * Speed);
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

	MenuRegions.Reset();
	bHaveMouse = PlayerOwner && PlayerOwner->GetMousePosition(MousePos.X, MousePos.Y);

	ScreenW = Canvas->SizeX;
	ScreenH = Canvas->SizeY;
	S = FMath::Clamp(ScreenH / 1080.f, 0.5f, 4.f);
	Now = GetWorld()->GetTimeSeconds();

	switch (GM->State)
	{
	case ESpinState::WaitingToStart:   DrawMainMenu(*GM); break;
	case ESpinState::ModeSelect:       DrawModeSelect(*GM); break;
	case ESpinState::ModifierSelect:   DrawModifierSelect(*GM); break;
	case ESpinState::BonusSelect:      DrawBonusSelect(*GM); break;
	case ESpinState::SettingsMenu:     DrawSettings(*GM); break;
	case ESpinState::Leaderboard:      DrawLeaderboard(*GM); break;
	case ESpinState::ProfileSelect:    DrawProfileSelect(*GM); break;
	case ESpinState::NameEntry:        DrawNameEntry(*GM); break;
	case ESpinState::Playing:
		DrawRunHUD(*GM);
		if (GM->IsPauseMenuOpen())
		{
			DrawPauseMenu(*GM);
		}
		break;
	case ESpinState::GameOver:         DrawGameOver(*GM); break;

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

float ASpinHUD::DrawLabel(const FString& Text, float X, float Y, float Size, const FLinearColor& Color, ESpinTextAlign Align, bool bBold)
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

	FCanvasTextItem Item(FVector2D(FMath::RoundToFloat(DrawX), FMath::RoundToFloat(Y)), FText::FromString(Text), Font, Color);

	// A soft shadow lifts light text off the busy 3D scene. Dark text (on a lit key cap) doesn't need one.
	if (Color.GetLuminance() > 0.3f)
	{
		const float Offset = FMath::Max(1.f, U(2.f));
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.75f * Color.A), FVector2D(Offset, Offset));
	}

	Canvas->DrawItem(Item);
	return Width;
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

	// One fixed raster size (a single font-cache entry however the size animates), scaled from there.
	const float RenderSize = 160.f;
	FSlateFontInfo Info = Font->GetLegacySlateFontInfo();
	Info.Size = RenderSize;
	Info.TypefaceFontName = FName(TEXT("Bold"));

	FCanvasTextItem Item(FVector2D(CenterX, CenterY), FText::FromString(Text), Info, Color);
	Item.Scale = FVector2D(Scale * 10.f * S / RenderSize);
	Item.bCentreX = true;
	Item.bCentreY = true;
	Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.6f * Color.A), FVector2D(U(4.f), U(4.f)));
	Canvas->DrawItem(Item);
}

void ASpinHUD::DrawOutline(float X, float Y, float W, float H, const FLinearColor& Color, float Thickness)
{
	DrawRect(Color, X, Y, W, Thickness);
	DrawRect(Color, X, Y + H - Thickness, W, Thickness);
	DrawRect(Color, X, Y + Thickness, Thickness, H - Thickness * 2.f);
	DrawRect(Color, X + W - Thickness, Y + Thickness, Thickness, H - Thickness * 2.f);
}

void ASpinHUD::DrawCornerBrackets(float X, float Y, float W, float H, const FLinearColor& Color, float Length, float Thickness)
{
	// Bottom corners only - the top edge already carries the accent bar.
	DrawRect(Color, X, Y + H - Thickness, Length, Thickness);
	DrawRect(Color, X, Y + H - Length, Thickness, Length);
	DrawRect(Color, X + W - Length, Y + H - Thickness, Length, Thickness);
	DrawRect(Color, X + W - Thickness, Y + H - Length, Thickness, Length);
}

void ASpinHUD::DrawPanel(float X, float Y, float W, float H, const FLinearColor& Accent, float Opacity)
{
	const float Thin = FMath::Max(1.f, U(1.f));
	DrawRect(Fade(PanelFill, Opacity), X, Y, W, H);
	DrawOutline(X, Y, W, H, Fade(PanelEdge, Opacity), Thin);
	DrawRect(Fade(Accent, 0.9f * Opacity), X, Y, W, FMath::Max(2.f, U(3.f)));
	DrawCornerBrackets(X, Y, W, H, Fade(Accent, 0.7f * Opacity), U(14.f), FMath::Max(1.f, U(2.f)));
}

void ASpinHUD::DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Fill)
{
	DrawRect(TrackColor, X, Y, W, H);
	DrawRect(Fill, X, Y, W * FMath::Clamp(Fraction, 0.f, 1.f), H);
}

void ASpinHUD::DrawSegments(float X, float Y, float W, float H, int32 Count, int32 Lit, const FLinearColor& On)
{
	Count = FMath::Max(1, Count);
	const float Gap = FMath::Max(2.f, U(4.f));
	const float SegW = (W - Gap * (Count - 1)) / Count;
	for (int32 i = 0; i < Count; ++i)
	{
		DrawRect(i < Lit ? On : TrackColor, X + i * (SegW + Gap), Y, SegW, H);
	}
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

	const FLinearColor Fill = bLit ? Color : FLinearColor(Color.R * 0.14f, Color.G * 0.14f, Color.B * 0.14f, 0.9f * Color.A);
	DrawRect(Fill, X, Y, W, H);
	DrawOutline(X, Y, W, H, Color, FMath::Max(1.f, U(2.f)));

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

	float X = CenterX - TotalW * 0.5f;
	if (!Before.IsEmpty())
	{
		DrawLabel(Before, X, TextY, Size, TextMain);
		X += BeforeW + Space;
	}
	X += DrawKeyCap(Key, X, Y, Size, KeyColor, false);
	if (!After.IsEmpty())
	{
		DrawLabel(After, X + Space, TextY, Size, TextMain);
	}
}

bool ASpinHUD::IsHovered(float X, float Y, float W, float H) const
{
	return bHaveMouse && MousePos.X >= X && MousePos.X <= X + W && MousePos.Y >= Y && MousePos.Y <= Y + H;
}

void ASpinHUD::AddRegion(float X, float Y, float W, float H, EMenuAction Action, int32 Index)
{
	FSpinMenuRegion Region;
	Region.Rect = FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H));
	Region.Action = Action;
	Region.Index = Index;
	MenuRegions.Add(Region);
}

void ASpinHUD::DrawMenuButton(float X, float Y, float W, float H, const FString& Label, EMenuAction Action,
	const FLinearColor& Accent, const FString& KeyHint, bool bEnabled, int32 Index)
{
	const bool bHover = bEnabled && IsHovered(X, Y, W, H);
	const float Opacity = bEnabled ? 1.f : 0.35f;

	DrawRect(Fade(bHover ? FLinearColor(0.05f, 0.09f, 0.15f, 0.95f) : PanelFill, Opacity), X, Y, W, H);
	DrawOutline(X, Y, W, H, Fade(bHover ? Accent : PanelEdge, Opacity), FMath::Max(1.f, U(bHover ? 2.f : 1.f)));
	DrawRect(Fade(Accent, (bHover ? 1.f : 0.75f) * Opacity), X, Y, FMath::Max(2.f, U(4.f)), H);

	// The label is centred in the space the key cap (if any) leaves free, and shrinks until it fits
	// there, so the two never overlap on narrow buttons.
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
	const float LabelCX = (X + W * 0.5f + LabelW * 0.5f <= LabelRight) ? X + W * 0.5f : (LabelLeft + LabelRight) * 0.5f;
	DrawLabel(Label, LabelCX, Y + (H - LineH(LabelSize)) * 0.5f, LabelSize, Fade(TextMain, Opacity), ESpinTextAlign::Center, true);

	if (!KeyHint.IsEmpty())
	{
		DrawKeyCap(KeyHint, KeyX, Y + (H - KeyCapHeight(KeySize)) * 0.5f, KeySize, Fade(Accent, Opacity), false);
	}

	if (bEnabled)
	{
		AddRegion(X, Y, W, H, Action, Index);
	}
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
	DrawRect(FLinearColor(0.004f, 0.008f, 0.02f, Opacity), 0.f, 0.f, ScreenW, ScreenH);

	// Thin neon rules framing the screen.
	const float Rule = FMath::Max(1.f, U(2.f));
	DrawRect(Fade(Cyan, 0.35f), 0.f, U(10.f), ScreenW, Rule);
	DrawRect(Fade(Cyan, 0.35f), 0.f, ScreenH - U(10.f) - Rule, ScreenW, Rule);
}

float ASpinHUD::DrawScreenTitle(const FString& Title, const FString& Subtitle, float CenterX, float Y, const FLinearColor& Accent)
{
	DrawLabel(Title, CenterX, Y, TitleSize, TextMain, ESpinTextAlign::Center, true);
	Y += LineH(TitleSize) + U(4.f);

	DrawRect(Accent, CenterX - U(36.f), Y, U(72.f), FMath::Max(2.f, U(3.f)));
	Y += U(14.f);

	if (!Subtitle.IsEmpty())
	{
		DrawLabel(Subtitle, CenterX, Y, BodySize, TextDim, ESpinTextAlign::Center);
		Y += LineH(BodySize);
	}
	return Y + U(20.f);
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

	// The top modules: run (score) on the left, level in the middle. Health sits beside the dial,
	// where the eyes already are.
	if (!GM.IsTutorial())
	{
		DrawRunModule(GM, U(Margin), Top);
	}
	DrawLevelModule(GM, CX, Top);
	DrawHealthColumns(GM);

	if (GM.JamPhase == EJamPhase::Sparking)
	{
		DrawEdgeGlow(Orange, 14.f);
	}
	else if (GM.JamPhase == EJamPhase::Jammed)
	{
		DrawEdgeGlow(Red, 20.f);
	}

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

	const FLinearColor Magenta(1.f, 0.1f, 0.75f);
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

	// A key cap riding on the zone itself; it lights up while the hand is inside - "press now".
	const FSpinPrompt& Prompt = GM.Prompts[GM.ActivePromptIndex];
	const FString Key = GM.GetButtonKey(GM.ActivePromptIndex);
	const FVector Screen = Project(Clock->GetZoneWorldLocation(0.5f));
	const float Size = 20.f;
	const float W = KeyCapWidth(Key, Size);
	const float H = KeyCapHeight(Size);
	DrawKeyCap(Key, Screen.X - W * 0.5f, Screen.Y - H * 0.5f, Size, Prompt.Color, GM.IsHandInZone());
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
	DrawLabel(FormatPoints(GM.Points), X + P, ScoreY, TitleSize, TextMain, ESpinTextAlign::Left, true);
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
			DrawLabel(TEXT("NEW BEST!"), X + P, BestY, SmallSize, Fade(Gold, Flash), ESpinTextAlign::Left, true);
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

	DrawLabel(Heading, X + P, HeadY, HeadingSize, Accent, ESpinTextAlign::Left, true);
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

		// Pips stack bottom-up like a health bar, so a lost one goes from the top.
		const float PipX = CX - Pip * 0.5f;
		const float PipsBottom = Y + P + LineH(SmallSize) + U(10.f) + PipsH;
		auto PipY = [&](int32 i) { return PipsBottom - Pip - i * (Pip + Gap); };
		for (int32 i = 0; i < Max; ++i)
		{
			if (i < Count)
			{
				DrawRect(Fade(Color, Alpha), PipX, PipY(i), Pip, Pip);
			}
			else
			{
				DrawRect(Fade(TrackColor, Alpha), PipX, PipY(i), Pip, Pip);
				DrawOutline(PipX, PipY(i), Pip, Pip, Fade(Color, 0.35f * Alpha), FMath::Max(1.f, U(1.f)));
			}
		}

		DrawLabel(Sub, CX, PipsBottom + U(10.f), TinySize, Fade(TextFaint, Alpha), ESpinTextAlign::Center, true);

		// "-1" / "+1" rises off the pip that just changed.
		auto Float = [&](const TCHAR* Text, int32 PipIndex, float Since, const FLinearColor& FloatColor)
		{
			if (Since >= 0.f && Since < 1.2f && PipIndex >= 0 && PipIndex < Max)
			{
				const float T = Since / 1.2f;
				DrawLabel(Text, CX, PipY(PipIndex) + (Pip - LineH(SmallSize)) * 0.5f - U(18.f) * T,
					SmallSize, Fade(FloatColor, 1.f - T * T), ESpinTextAlign::Center, true);
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
	const float PanelW = MeasureText(Instruction, SmallSize).X + P * 2.f;
	const float PanelH = P + LineH(SmallSize) + P;
	const float X = CenterX - PanelW * 0.5f;
	const float Y = BottomY - PanelH;

	DrawPanel(X, Y, PanelW, PanelH, KeyColorOf(GM, GM.ActivePromptIndex));
	DrawLabel(Instruction, CenterX, Y + P, SmallSize, TextMain, ESpinTextAlign::Center);
}

void ASpinHUD::DrawRepairPanel(const ASpinGameMode& GM, float CenterX, float BottomY)
{
	const FString Key = GM.GetRepairKey();
	const float P = U(20.f);
	const float W = FMath::Min(U(600.f), ScreenW - U(Margin) * 2.f);
	const float X = CenterX - W * 0.5f;

	// "MASH [W]" - the instruction is the headline, right above the bar it fills.
	const float CapH = KeyCapHeight(HeadingSize);
	const float MashRowH = FMath::Max(LineH(TitleSize), CapH);
	const float SegH = U(18.f);
	const float TimeH = U(6.f);

	const float MashY = 0.f; // offsets from the top, resolved once the panel height is known
	const float RepairLabelY = MashY + MashRowH + U(10.f);
	const float SegY = RepairLabelY + LineH(TinySize) + U(4.f);
	const float TimeY = SegY + SegH + U(10.f);
	const float TimeLabelY = TimeY + TimeH + U(4.f);
	const float RuleY = TimeLabelY + LineH(TinySize) + U(8.f);
	const float PanelH = P + RuleY + LineH(TinySize) + P;
	const float Y = BottomY - PanelH + P;

	DrawPanel(X, Y - P, W, PanelH, Red);

	const float Throb = Pulse01(Now, 22.f);
	const float MashW = MeasureText(TEXT("MASH"), TitleSize, true).X + U(14.f);
	const float CapW = KeyCapWidth(Key, HeadingSize);
	const float MashX = CenterX - (MashW + CapW) * 0.5f;
	DrawLabel(TEXT("MASH"), MashX, Y + MashY + (MashRowH - LineH(TitleSize)) * 0.5f, TitleSize, FMath::Lerp(Orange, Gold, Throb), ESpinTextAlign::Left, true);
	DrawKeyCap(Key, MashX + MashW, Y + MashY + (MashRowH - CapH) * 0.5f, HeadingSize, FMath::Lerp(Orange, Gold, Throb), Throb > 0.5f);

	DrawLabel(TEXT("REPAIR"), X + P, Y + RepairLabelY, TinySize, TextDim, ESpinTextAlign::Left, true);
	DrawLabel(FString::Printf(TEXT("%d / %d"), GM.JamPresses, GM.GetRunJamPressesRequired()), X + W - P, Y + RepairLabelY, TinySize, TextMain, ESpinTextAlign::Right, true);
	DrawSegments(X + P, Y + SegY, W - P * 2.f, SegH, GM.GetRunJamPressesRequired(), GM.JamPresses, Green);

	const float TimeFraction = GM.GetJamTimeFraction();
	DrawBar(X + P, Y + TimeY, W - P * 2.f, TimeH, TimeFraction, Red);
	DrawLabel(TEXT("BEFORE THE HAND BREAKS"), X + P, Y + TimeLabelY, TinySize, Red, ESpinTextAlign::Left, true);
	DrawLabel(FString::Printf(TEXT("%.1fs"), GM.GetRunJamTimeLimit() * TimeFraction), X + W - P, Y + TimeLabelY, TinySize, Red, ESpinTextAlign::Right, true);

	DrawLabel(FString::Printf(TEXT("Only %s works while jammed - zones and obstacles are ignored"), *Key), CenterX, Y + RuleY, TinySize, TextFaint, ESpinTextAlign::Center);
}

void ASpinHUD::DrawCallout(const ASpinGameMode& GM, float CenterX, float Y)
{
	FString Title;
	float TitleFontSize = 30.f;
	FLinearColor Color = Gold;
	float Alpha = 1.f;
	TArray<FSpinBriefLine> Subs;

	// Highest priority first - only one message ever occupies the slot.
	const FString RepairKey = GM.GetRepairKey();
	if (GM.JamPhase == EJamPhase::Sparking)
	{
		Title = TEXT("OVERHEATING!");
		Color = FMath::Lerp(Orange, Gold, Pulse01(Now, 14.f));
		Subs.Add({ FString::Printf(TEXT("The hand jams in %.1fs - get ready to mash %s"), FMath::Max(0.f, GM.GetRunJamSparkTime() - GM.JamTimer), *RepairKey), TextMain });
	}
	else if (GM.JamPhase == EJamPhase::Jammed)
	{
		Title = TEXT("JAMMED!");
		Color = Red;
		Subs.Add({ FString::Printf(TEXT("Mash %s to repair it before it breaks"), *RepairKey), TextMain });
	}
	else if (GM.IsShowingNotice())
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
		DrawLabel(Line, CenterX, LineY, TitleFontSize, Fade(Color, Alpha), ESpinTextAlign::Center, true);
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

		const TCHAR* Label = bPerfect ? TEXT("PERFECT!") : GM.LastHitGrade == ESpinHitGrade::Good ? TEXT("GOOD") : TEXT("OK");
		DrawLabel(Label, CenterX, Y - Rise, Size, Fade(GetGradeColor(GM.LastHitGrade), Alpha), ESpinTextAlign::Center, true);

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
		DrawLabel(TEXT("COMBO LOST"), CenterX, Y + U(80.f), 16.f, Fade(Red, 1.f - SinceComboLost), ESpinTextAlign::Center, true);
	}
}

void ASpinHUD::DrawEdgeGlow(const FLinearColor& Color, float Speed)
{
	const FLinearColor Edge = Fade(Color, FMath::Lerp(0.15f, 0.6f, Pulse01(Now, Speed)));
	const float T = U(12.f);
	DrawRect(Edge, 0.f, 0.f, ScreenW, T);
	DrawRect(Edge, 0.f, ScreenH - T, ScreenW, T);
	DrawRect(Edge, 0.f, T, T, ScreenH - T * 2.f);
	DrawRect(Edge, ScreenW - T, T, T, ScreenH - T * 2.f);
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

	// Neon title: a soft cyan glow behind gold letters.
	const float Glow = U(3.f);
	for (const FVector2D& Offset : { FVector2D(-Glow, 0.f), FVector2D(Glow, 0.f), FVector2D(0.f, -Glow), FVector2D(0.f, Glow) })
	{
		DrawLabel(TEXT("SPIN TO WIN"), CX + Offset.X, Y + Offset.Y, HeroSize, Fade(Cyan, 0.25f), ESpinTextAlign::Center, true);
	}
	DrawLabel(TEXT("SPIN TO WIN"), CX, Y, HeroSize, Gold, ESpinTextAlign::Center, true);
	Y += LineH(HeroSize);
	DrawLabel(TEXT("Hit the zone.  Flip the spin.  Don't slow down."), CX, Y, BodySize, TextDim, ESpinTextAlign::Center);
	Y += LineH(BodySize) + U(36.f);

	const float BtnW = FMath::Min(U(380.f), ScreenW - U(Margin) * 2.f);
	const float BtnX = CX - BtnW * 0.5f;

	// Who's playing - click to switch or add a player.
	const FString Name = GM.GetPlayerName();
	DrawMenuButton(BtnX, Y, BtnW, U(50.f), Name.IsEmpty() ? FString(TEXT("ADD A PLAYER")) : FString::Printf(TEXT("PLAYER:  %s"), *Name), EMenuAction::OpenProfiles, Cyan);
	Y += U(56.f);
	DrawLabel(Name.IsEmpty() ? TEXT("Runs are saved under your name") : TEXT("Click to switch player"), CX, Y, TinySize, TextFaint, ESpinTextAlign::Center);
	Y += LineH(TinySize) + U(24.f);

	const float BtnH = U(64.f);
	const float Gap = U(12.f);
	DrawMenuButton(BtnX, Y, BtnW, BtnH, TEXT("PLAY"), EMenuAction::Play, Gold, KeyHintOf(GM, 0));
	Y += BtnH + Gap;
	DrawMenuButton(BtnX, Y, BtnW, BtnH, TEXT("LEADERBOARD"), EMenuAction::OpenLeaderboard, Cyan);
	Y += BtnH + Gap;
	DrawMenuButton(BtnX, Y, BtnW, BtnH, TEXT("SETTINGS"), EMenuAction::OpenSettings, TextDim, KeyHintOf(GM, 1));
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

	auto DrawCard = [&](float X, const FString& Title, const FLinearColor& Accent, const FString& Description, const FString& Key, EMenuAction Action)
	{
		DrawPanel(X, Y, CardW, CardH, Accent);
		if (IsHovered(X, Y, CardW, CardH))
		{
			DrawOutline(X, Y, CardW, CardH, Accent, FMath::Max(1.f, U(2.f)));
		}

		float LineY = Y + U(26.f);
		DrawLabel(Title, X + CardW * 0.5f, LineY, TitleSize, Accent, ESpinTextAlign::Center, true);
		LineY += LineH(TitleSize) + U(8.f);
		DrawWrapped(Description, X + CardW * 0.5f, LineY, BodySize, TextDim, CardW - U(48.f));

		DrawKeyPrompt(TEXT("Click or press"), Key, FString(), X + CardW * 0.5f, Y + CardH - U(22.f) - KeyCapHeight(SmallSize), SmallSize, Accent);
		AddRegion(X, Y, CardW, CardH, Action);
	};

	const float LeftX = CX - CardW * 1.5f - Gap;
	const float MidX = CX - CardW * 0.5f;
	const float RightX = CX + CardW * 0.5f + Gap;

	DrawCard(LeftX, TEXT("TUTORIAL"), KeyColorOf(GM, 0),
		FString::Printf(TEXT("Learn the ropes in %d short stages. Nothing can go wrong here."), GM.TutorialLevels.Num()),
		KeyHintOf(GM, 0), EMenuAction::StartTutorial);
	DrawCard(MidX, TEXT("ENDLESS"), KeyColorOf(GM, 1),
		TEXT("The real run. Pick optional modifiers for bonus points, then see how far you get."),
		KeyHintOf(GM, 1), EMenuAction::OpenModifiers);
	DrawCard(RightX, TEXT("DAILY"), KeyColorOf(GM, 2),
		TEXT("Today's run is the same for everyone, with a twist. A new one every day."),
		KeyHintOf(GM, 2), EMenuAction::StartDaily);
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

	const USpinSaveGame* Save = GM.GetSaveData();
	const FSpinProfile* Profile = Save ? Save->GetActiveProfile() : nullptr;
	if (Profile && Profile->RunsPlayed > 0)
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
	DrawMenuButton(CX - BackW * 0.5f, Y, BackW, U(50.f), TEXT("BACK"), EMenuAction::Back, TextDim);
}

void ASpinHUD::DrawModifierSelect(const ASpinGameMode& GM)
{
	DrawBackdrop(0.78f);

	const float CX = ScreenW * 0.5f;
	float Y = DrawScreenTitle(TEXT("MODIFIERS"), TEXT("Optional rules that make the run harder - and multiply every point you score"), CX, ScreenH * 0.1f, Purple);

	const float PanelW = FMath::Min(U(720.f), ScreenW - U(Margin) * 2.f);
	const float X = CX - PanelW * 0.5f;
	const float P = U(Pad);
	const float RowH = U(64.f);
	const int32 NumModifiers = static_cast<int32>(ESpinModifier::Count);
	const float FooterH = LineH(HeadingSize) + U(12.f);
	DrawPanel(X, Y, PanelW, P * 2.f + NumModifiers * RowH + FooterH, Purple);

	// One clickable row per modifier: a check box, its name and what it does, and its bonus.
	for (int32 i = 0; i < NumModifiers; ++i)
	{
		const FSpinModifierInfo& Info = ASpinGameMode::GetModifierInfo(static_cast<ESpinModifier>(i));
		const bool bOn = (GM.SelectedModifiers & (1 << i)) != 0;
		const float RowX = X + P;
		const float RowY = Y + P + i * RowH;
		const float RowW = PanelW - P * 2.f;

		if (IsHovered(RowX, RowY, RowW, RowH) || GM.MenuFocus == i)
		{
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.04f), RowX, RowY, RowW, RowH);
		}
		if (GM.MenuFocus == i)
		{
			DrawFocusRing(RowX, RowY, RowW, RowH);
		}
		if (i > 0)
		{
			DrawRect(PanelEdge, RowX, RowY, RowW, FMath::Max(1.f, U(1.f)));
		}

		const float Box = U(26.f);
		const float BoxX = RowX + U(12.f);
		const float BoxY = RowY + (RowH - Box) * 0.5f;
		DrawRect(bOn ? Purple : TrackColor, BoxX, BoxY, Box, Box);
		DrawOutline(BoxX, BoxY, Box, Box, bOn ? Purple : PanelEdge, FMath::Max(1.f, U(2.f)));

		const float TextX = BoxX + Box + U(16.f);
		const float NameY = RowY + (RowH - LineH(BodySize) - LineH(SmallSize)) * 0.5f;
		DrawLabel(Info.Name, TextX, NameY, BodySize, bOn ? TextMain : TextDim, ESpinTextAlign::Left, true);
		DrawLabel(Info.Description, TextX, NameY + LineH(BodySize), SmallSize, TextDim);
		DrawLabel(FormatMultiplier(Info.PointsMultiplier), RowX + RowW - U(12.f), RowY + (RowH - LineH(HeadingSize)) * 0.5f, HeadingSize,
			bOn ? Gold : TextFaint, ESpinTextAlign::Right, true);

		AddRegion(RowX, RowY, RowW, RowH, EMenuAction::ToggleModifier, i);
	}

	// The combined bonus.
	const float FooterY = Y + P + NumModifiers * RowH + U(6.f);
	const float Total = ASpinGameMode::GetModifiersMultiplier(GM.SelectedModifiers);
	DrawLabel(TEXT("POINTS MULTIPLIER"), X + P + U(12.f), FooterY + (LineH(HeadingSize) - LineH(SmallSize)) * 0.5f, SmallSize, TextDim, ESpinTextAlign::Left, true);
	DrawLabel(FormatMultiplier(Total), X + PanelW - P - U(12.f), FooterY, HeadingSize, GM.SelectedModifiers != 0 ? Gold : TextFaint, ESpinTextAlign::Right, true);
	Y += P * 2.f + NumModifiers * RowH + FooterH + U(18.f);

	// The bonus loadout this run will use.
	const int32 Equipped = GM.GetEquippedBonuses();
	DrawLabel(Equipped != 0 ? FString::Printf(TEXT("BONUSES:  %s"), *ASpinGameMode::GetBonusNames(Equipped)) : FString(TEXT("NO BONUSES ON")),
		CX, Y, SmallSize, Equipped != 0 ? Green : TextFaint, ESpinTextAlign::Center, true);
	Y += LineH(SmallSize) + U(18.f);

	const float BtnGap = U(16.f);
	const float BtnW = FMath::Min(U(230.f), (PanelW - BtnGap * 2.f) / 3.f);
	const float BtnH = U(60.f);
	const float BtnX = CX - BtnW * 1.5f - BtnGap;
	DrawMenuButton(BtnX, Y, BtnW, BtnH, TEXT("START"), EMenuAction::StartEndless, Gold, GM.GetPauseKey());
	DrawMenuButton(BtnX + BtnW + BtnGap, Y, BtnW, BtnH, TEXT("BONUSES"), EMenuAction::OpenBonuses, Green, KeyHintOf(GM, 2));
	DrawMenuButton(BtnX + (BtnW + BtnGap) * 2.f, Y, BtnW, BtnH, TEXT("BACK"), EMenuAction::Back, TextDim, KeyHintOf(GM, 1));
	for (int32 i = 0; i < 3; ++i)
	{
		if (GM.MenuFocus == NumModifiers + i)
		{
			DrawFocusRing(BtnX + (BtnW + BtnGap) * i, Y, BtnW, BtnH);
		}
	}
	DrawNavHint(GM, CX, Y + BtnH + U(16.f));
}

void ASpinHUD::DrawBonusSelect(const ASpinGameMode& GM)
{
	DrawBackdrop(0.78f);

	const float CX = ScreenW * 0.5f;
	float Y = DrawScreenTitle(TEXT("BONUSES"), TEXT("Earned by playing - they make a run more forgiving, and mark it on the leaderboard"), CX, ScreenH * 0.08f, Green);

	const float PanelW = FMath::Min(U(760.f), ScreenW - U(Margin) * 2.f);
	const float X = CX - PanelW * 0.5f;
	const float P = U(Pad);

	// Slots: one box per slot, filled when used, with the level that opens each locked one.
	const int32 Slots = GM.GetBonusSlots();
	const int32 Equipped = GM.GetEquippedBonuses();
	const int32 Used = FMath::CountBits(static_cast<uint64>(Equipped));
	const bool bFull = Used >= Slots;
	{
		const float Box = U(30.f);
		const float BoxGap = U(10.f);
		const int32 SlotLevels[] = { 0, GM.SecondSlotLevel, GM.ThirdSlotLevel };
		const float LabelW = MeasureText(TEXT("SLOTS"), SmallSize, true).X + U(14.f);
		const float RowW = LabelW + 3.f * Box + 2.f * BoxGap;
		float BoxX = CX - RowW * 0.5f;
		DrawLabel(TEXT("SLOTS"), BoxX, Y + (Box - LineH(SmallSize)) * 0.5f, SmallSize, TextDim, ESpinTextAlign::Left, true);
		BoxX += LabelW;
		for (int32 i = 0; i < 3; ++i, BoxX += Box + BoxGap)
		{
			if (i < Slots)
			{
				DrawRect(i < Used ? Green : TrackColor, BoxX, Y, Box, Box);
				DrawOutline(BoxX, Y, Box, Box, Green, FMath::Max(1.f, U(2.f)));
			}
			else
			{
				DrawOutline(BoxX, Y, Box, Box, PanelEdge, FMath::Max(1.f, U(1.f)));
				DrawLabel(FString::Printf(TEXT("%d"), SlotLevels[i]), BoxX + Box * 0.5f, Y + (Box - LineH(TinySize)) * 0.5f, TinySize, TextFaint, ESpinTextAlign::Center, true);
			}
		}
		Y += Box + U(6.f);
		const int32 NextLevel = GM.GetNextBonusSlotLevel();
		DrawLabel(NextLevel > 0 ? FString::Printf(TEXT("Reach level %d for another slot"), NextLevel) : FString(TEXT("All slots open")),
			CX, Y, TinySize, TextFaint, ESpinTextAlign::Center);
		Y += LineH(TinySize) + U(16.f);
	}

	const int32 NumBonuses = static_cast<int32>(ESpinBonus::Count);
	const float RowH = U(72.f);
	DrawPanel(X, Y, PanelW, P * 2.f + NumBonuses * RowH, Green);

	for (int32 i = 0; i < NumBonuses; ++i)
	{
		const ESpinBonus Bonus = static_cast<ESpinBonus>(i);
		const FSpinBonusInfo& Info = ASpinGameMode::GetBonusInfo(Bonus);
		const bool bUnlocked = GM.IsBonusUnlocked(Bonus);
		const bool bOn = (Equipped & (1 << i)) != 0;
		const bool bCanToggle = bUnlocked && (bOn || !bFull);
		const float RowX = X + P;
		const float RowY = Y + P + i * RowH;
		const float RowW = PanelW - P * 2.f;
		const float RightX = RowX + RowW - U(12.f);

		// Locked rows can still be highlighted, to read what they need.
		if ((bCanToggle && IsHovered(RowX, RowY, RowW, RowH)) || GM.MenuFocus == i)
		{
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.04f), RowX, RowY, RowW, RowH);
		}
		if (GM.MenuFocus == i)
		{
			DrawFocusRing(RowX, RowY, RowW, RowH);
		}
		if (i > 0)
		{
			DrawRect(PanelEdge, RowX, RowY, RowW, FMath::Max(1.f, U(1.f)));
		}

		// Check box, or an empty dimmed one while locked.
		const float Box = U(26.f);
		const float BoxX = RowX + U(12.f);
		const float BoxY = RowY + (RowH - Box) * 0.5f;
		DrawRect(bOn ? Green : TrackColor, BoxX, BoxY, Box, Box);
		DrawOutline(BoxX, BoxY, Box, Box, bOn ? Green : PanelEdge, FMath::Max(1.f, U(2.f)));

		const float TextX = BoxX + Box + U(16.f);
		const float NameY = RowY + (RowH - LineH(BodySize) - LineH(SmallSize)) * 0.5f;
		DrawLabel(Info.Name, TextX, NameY, BodySize, bOn ? TextMain : bUnlocked ? TextDim : TextFaint, ESpinTextAlign::Left, true);
		DrawLabel(Info.Description, TextX, NameY + LineH(BodySize), SmallSize, bUnlocked ? TextDim : TextFaint);

		if (bUnlocked)
		{
			const TCHAR* Status = bOn ? TEXT("ON") : bFull ? TEXT("SLOTS FULL") : TEXT("OFF");
			DrawLabel(Status, RightX, RowY + (RowH - LineH(SmallSize)) * 0.5f, SmallSize, bOn ? Green : TextFaint, ESpinTextAlign::Right, true);
		}
		else
		{
			// Locked: what it takes, and how close they are.
			int32 Current = 0, Target = 1;
			GM.GetBonusProgress(Bonus, Current, Target);
			const float BarW = U(170.f);
			const float ReqY = RowY + (RowH - LineH(TinySize) - U(6.f) - LineH(TinySize)) * 0.5f;
			DrawLabel(FString::Printf(TEXT("LOCKED  -  %s"), Info.Requirement), RightX, ReqY, TinySize, Gold, ESpinTextAlign::Right, true);
			DrawBar(RightX - BarW, ReqY + LineH(TinySize) + U(3.f), BarW, U(4.f), static_cast<float>(Current) / FMath::Max(1, Target), Fade(Gold, 0.7f));
			DrawLabel(FString::Printf(TEXT("%d / %d"), FMath::Min(Current, Target), Target), RightX, ReqY + LineH(TinySize) + U(6.f) + U(1.f), TinySize, TextDim, ESpinTextAlign::Right);
		}

		if (bCanToggle)
		{
			AddRegion(RowX, RowY, RowW, RowH, EMenuAction::ToggleBonus, i);
		}
	}
	Y += P * 2.f + NumBonuses * RowH + U(28.f);

	const float BtnW = U(260.f);
	const float BtnH = U(60.f);
	const float BtnGap = U(20.f);
	const float BtnX = CX - BtnGap * 0.5f - BtnW;
	DrawMenuButton(BtnX, Y, BtnW, BtnH, TEXT("START"), EMenuAction::StartEndless, Gold, GM.GetPauseKey());
	DrawMenuButton(BtnX + BtnW + BtnGap, Y, BtnW, BtnH, TEXT("BACK"), EMenuAction::Back, TextDim, KeyHintOf(GM, 1));
	for (int32 i = 0; i < 2; ++i)
	{
		if (GM.MenuFocus == NumBonuses + i)
		{
			DrawFocusRing(BtnX + (BtnW + BtnGap) * i, Y, BtnW, BtnH);
		}
	}
	DrawNavHint(GM, CX, Y + BtnH + U(16.f));
}

void ASpinHUD::DrawFocusRing(float X, float Y, float W, float H)
{
	const float Out = U(3.f);
	DrawOutline(X - Out, Y - Out, W + Out * 2.f, H + Out * 2.f, Fade(TextMain, 0.45f + 0.55f * Pulse01(Now, 6.f)), FMath::Max(1.f, U(2.f)));
}

void ASpinHUD::DrawNavHint(const ASpinGameMode& GM, float CenterX, float Y)
{
	const bool bPad = GM.InputStyle != ESpinInputStyle::Keyboard;
	const FString Text = FString::Printf(TEXT("%s  move        %s  select        %s  start run"),
		bPad ? TEXT("D-PAD") : TEXT("ARROWS"),
		*(bPad ? GM.GetButtonKey(0) : GM.GetButtonKey(0) + TEXT(" / ENTER")),
		*GM.GetPauseKey());
	DrawLabel(Text, CenterX, Y, TinySize, TextFaint, ESpinTextAlign::Center, true);
}

void ASpinHUD::DrawSettings(const ASpinGameMode& GM)
{
	DrawBackdrop(0.75f);

	const float CX = ScreenW * 0.5f;
	float Y = DrawScreenTitle(TEXT("SETTINGS"), FString(), CX, ScreenH * 0.15f, Cyan) + U(10.f);

	const float PanelW = FMath::Min(U(640.f), ScreenW - U(Margin) * 2.f);
	const float X = CX - PanelW * 0.5f;
	const float RowH = U(76.f);
	const int32 NumRows = 4;
	const float P = U(Pad);
	DrawPanel(X, Y, PanelW, NumRows * RowH + P * 2.f, Cyan);

	auto DrawRow = [&](int32 Index, const TCHAR* Label, const FString& Value, EMenuAction Prev, EMenuAction Next)
	{
		const float RowY = Y + P + Index * RowH;
		if (Index > 0)
		{
			DrawRect(PanelEdge, X + P, RowY, PanelW - P * 2.f, FMath::Max(1.f, U(1.f)));
		}

		DrawLabel(Label, X + U(24.f), RowY + (RowH - LineH(SmallSize)) * 0.5f, SmallSize, TextDim, ESpinTextAlign::Left, true);

		// [<]  VALUE  [>] on the right.
		const float Arrow = U(44.f);
		const float ValueW = U(200.f);
		const float NextX = X + PanelW - U(24.f) - Arrow;
		const float PrevX = NextX - ValueW - Arrow;
		const float ArrowY = RowY + (RowH - Arrow) * 0.5f;
		DrawMenuButton(PrevX, ArrowY, Arrow, Arrow, TEXT("<"), Prev, Cyan);
		DrawLabel(Value, PrevX + Arrow + ValueW * 0.5f, RowY + (RowH - LineH(22.f)) * 0.5f, 22.f, TextMain, ESpinTextAlign::Center, true);
		DrawMenuButton(NextX, ArrowY, Arrow, Arrow, TEXT(">"), Next, Cyan);
	};

	DrawRow(0, TEXT("GRAPHICS QUALITY"), GM.GetQualityLabel(), EMenuAction::QualityPrev, EMenuAction::QualityNext);
	DrawRow(1, TEXT("RESOLUTION"), GM.GetResolutionLabel(), EMenuAction::ResolutionPrev, EMenuAction::ResolutionNext);
	DrawRow(2, TEXT("VOLUME"), FString::Printf(TEXT("%d%%"), GM.VolumePercent), EMenuAction::VolumeDown, EMenuAction::VolumeUp);
	// Only two choices, so both arrows just flip it.
	DrawRow(3, TEXT("CONTROLLER ICONS"), GM.GetControllerIconsLabel(), EMenuAction::ToggleControllerIcons, EMenuAction::ToggleControllerIcons);

	const float BackW = U(220.f);
	DrawMenuButton(CX - BackW * 0.5f, Y + NumRows * RowH + P * 2.f + U(32.f), BackW, U(50.f), TEXT("BACK"), EMenuAction::Back, TextDim);
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
		DrawLabel(Heading, X + P, RowY, HeadingSize, Accent, ESpinTextAlign::Left, true);
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
				DrawRect(Fade(Green, 0.12f), X + U(6.f), ThisRowY, PanelW - U(12.f), RowH);
				DrawRect(Green, X + U(6.f), ThisRowY, FMath::Max(2.f, U(3.f)), RowH);
			}

			const float TextY = ThisRowY + (RowH - LineH(SmallSize)) * 0.5f;
			const FString Level = FString::Printf(TEXT("%d"), Run.Level);
			const FString Points = FormatPoints(Run.Points);
			DrawLabel(FString::FromInt(i + 1), RankX, TextY, SmallSize, i == 0 ? Gold : TextDim, ESpinTextAlign::Left, true);
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
	DrawMenuButton(CX - BackW * 0.5f, Y, BackW, U(50.f), TEXT("BACK"), EMenuAction::Back, TextDim, KeyHintOf(GM, 1));
}

void ASpinHUD::DrawProfileSelect(const ASpinGameMode& GM)
{
	DrawBackdrop(0.82f);

	const float CX = ScreenW * 0.5f;
	float Y = DrawScreenTitle(TEXT("WHO'S PLAYING?"), TEXT("Pick a player, or add a new one"), CX, ScreenH * 0.12f, Cyan);

	const USpinSaveGame* Save = GM.GetSaveData();
	const int32 NumProfiles = Save ? Save->Profiles.Num() : 0;

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
			// Confirmation in place of the row - deleting also takes their runs off the boards.
			DrawPanel(X, Y, TotalW, RowH, Red);
			DrawLabel(FString::Printf(TEXT("Delete %s and all their scores?"), *Profile.Name), X + U(20.f), Y + (RowH - LineH(BodySize)) * 0.5f, BodySize, TextMain, ESpinTextAlign::Left, true);
			const float BtnW = U(96.f);
			const float BtnH = RowH - U(16.f);
			DrawMenuButton(X + TotalW - U(10.f) - BtnW * 2.f - U(8.f), Y + U(8.f), BtnW, BtnH, TEXT("YES"), EMenuAction::ConfirmDeleteProfile, Red);
			DrawMenuButton(X + TotalW - U(10.f) - BtnW, Y + U(8.f), BtnW, BtnH, TEXT("NO"), EMenuAction::CancelDeleteProfile, TextDim);
			continue;
		}

		// The whole row picks the player; the current one is lit.
		const bool bActive = Save->ActiveProfile == i;
		DrawPanel(X, Y, RowW, RowH, bActive ? Cyan : TextFaint);
		if (IsHovered(X, Y, RowW, RowH))
		{
			DrawOutline(X, Y, RowW, RowH, Cyan, FMath::Max(1.f, U(2.f)));
		}
		AddRegion(X, Y, RowW, RowH, EMenuAction::SelectProfile, i);

		const float NameSize = 24.f;
		const float NameW = DrawLabel(Profile.Name, X + U(20.f), Y + (RowH - LineH(NameSize)) * 0.5f, NameSize, bActive ? Cyan : TextMain, ESpinTextAlign::Left, true);
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
		const bool bDelHover = IsHovered(DelX, Y, DeleteW, RowH);
		DrawRect(Fade(Red, bDelHover ? 0.3f : 0.1f), DelX, Y, DeleteW, RowH);
		DrawOutline(DelX, Y, DeleteW, RowH, Fade(Red, bDelHover ? 1.f : 0.45f), FMath::Max(1.f, U(bDelHover ? 2.f : 1.f)));
		DrawLabel(TEXT("X"), DelX + DeleteW * 0.5f, Y + (RowH - LineH(HeadingSize)) * 0.5f, HeadingSize, Red, ESpinTextAlign::Center, true);
		AddRegion(DelX, Y, DeleteW, RowH, EMenuAction::DeleteProfile, i);
	}

	Y += U(6.f);
	if (NumProfiles < USpinSaveGame::MaxProfiles)
	{
		DrawMenuButton(X, Y, TotalW, RowH, TEXT("+   NEW PLAYER"), EMenuAction::NewProfile, Green);
	}
	else
	{
		DrawLabel(FString::Printf(TEXT("%d players max - delete one to add another"), USpinSaveGame::MaxProfiles), CX, Y + (RowH - LineH(SmallSize)) * 0.5f, SmallSize, TextDim, ESpinTextAlign::Center);
	}
	Y += RowH + U(28.f);

	const float BackW = U(220.f);
	DrawMenuButton(CX - BackW * 0.5f, Y, BackW, U(50.f), TEXT("BACK"), EMenuAction::Back, TextDim, KeyHintOf(GM, 1));
}

void ASpinHUD::DrawNameEntry(const ASpinGameMode& GM)
{
	DrawBackdrop(0.86f);

	const float CX = ScreenW * 0.5f;
	float Y = DrawScreenTitle(TEXT("WHAT'S YOUR NAME?"), TEXT("It goes on the leaderboard next to your runs"), CX, ScreenH * 0.2f, Cyan) + U(10.f);

	// The text box, with a blinking cursor.
	const float BoxW = FMath::Min(U(540.f), ScreenW - U(Margin) * 2.f);
	const float BoxH = U(84.f);
	const float BoxX = CX - BoxW * 0.5f;
	DrawPanel(BoxX, Y, BoxW, BoxH, Cyan);
	DrawOutline(BoxX, Y, BoxW, BoxH, Fade(Cyan, 0.6f), FMath::Max(1.f, U(2.f)));

	const bool bCursorOn = FMath::Fmod(Now, 1.f) < 0.55f;
	const bool bFull = GM.NameEntryText.Len() >= USpinSaveGame::MaxNameLength;
	const float TextY = Y + (BoxH - LineH(TitleSize)) * 0.5f;
	if (GM.NameEntryText.IsEmpty())
	{
		DrawLabel(TEXT("TYPE A NAME"), CX, TextY, TitleSize, Fade(TextFaint, bCursorOn ? 1.f : 0.6f), ESpinTextAlign::Center, true);
	}
	else
	{
		DrawLabel(GM.NameEntryText + ((bCursorOn && !bFull) ? TEXT("_") : TEXT(" ")), CX, TextY, TitleSize, TextMain, ESpinTextAlign::Center, true);
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
		DrawLabel(GM.NameEntryError, CX, Y, SmallSize, Red, ESpinTextAlign::Center, true);
	}
	Y += LineH(SmallSize) + U(16.f);

	const float BtnW = U(200.f);
	const float BtnH = U(56.f);
	const float Gap = U(20.f);
	DrawMenuButton(CX - Gap * 0.5f - BtnW, Y, BtnW, BtnH, TEXT("OK"), EMenuAction::ConfirmName, Green);
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
	DrawLabel(TEXT("GAME OVER"), CX, Y, GameOverSize, Red, ESpinTextAlign::Center, true);
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
		DrawRect(bNewBest ? Fade(Gold, 0.08f) : FLinearColor(1.f, 1.f, 1.f, 0.03f), TileX, Y, TileW, TileH);
		DrawOutline(TileX, Y, TileW, TileH, bNewBest ? Fade(Gold, 0.8f) : PanelEdge, FMath::Max(1.f, U(bNewBest ? 2.f : 1.f)));

		const float TileCX = TileX + TileW * 0.5f;
		DrawLabel(Label, TileCX, Y + U(14.f), TinySize, TextDim, ESpinTextAlign::Center, true);
		DrawLabel(Value, TileCX, Y + U(14.f) + LineH(TinySize), 48.f, bNewBest ? Gold : TextMain, ESpinTextAlign::Center, true);
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
		const float Glow = Pulse01(Now, 4.f);
		DrawRect(Fade(Green, 0.08f + 0.06f * Glow), StripX, Y, StripW, StripH);
		DrawOutline(StripX, Y, StripW, StripH, Fade(Green, 0.6f + 0.4f * Glow), FMath::Max(1.f, U(2.f)));

		float StripY = Y + UnlockPad;
		DrawLabel(TEXT("UNLOCKED"), CX, StripY, TinySize, Green, ESpinTextAlign::Center, true);
		StripY += LineH(TinySize);
		DrawLabel(Unlocks, CX, StripY, HeadingSize, TextMain, ESpinTextAlign::Center, true);
		StripY += LineH(HeadingSize);
		DrawLabel(TEXT("Set up your bonuses from the modifier screen before a run"), CX, StripY, TinySize, TextDim, ESpinTextAlign::Center);
		Y += StripH;
	}
	Y += U(28.f);

	// Shown straight away but only clickable after the short delay, so a press meant for the hand
	// can't land on one by accident.
	const bool bReady = GM.IsGameOverMenuReady();
	const float BtnGap = U(16.f);
	const float BtnW = (CardW - P * 2.f - BtnGap * 2.f) / 3.f;
	DrawMenuButton(X + P, Y, BtnW, BtnH, TEXT("RETRY"), EMenuAction::RetryGame, Gold, KeyHintOf(GM, 0), bReady);
	DrawMenuButton(X + P + BtnW + BtnGap, Y, BtnW, BtnH, TEXT("MAIN MENU"), EMenuAction::ReturnToMainMenu, Cyan, KeyHintOf(GM, 1), bReady);
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
	DrawLabel(TEXT("PAUSED"), CX, Y, TitleSize, Gold, ESpinTextAlign::Center, true);
	Y += LineH(TitleSize) + U(24.f);

	// Key hints match ASpinGameMode::HandleButtonPressed: the first button resumes (the second does
	// too, as "back"), the third quits. Restart has no key - it's click-only, so it can't be hit by accident.
	const float BtnW = CardW - P * 2.f;
	DrawMenuButton(X + P, Y, BtnW, BtnH, TEXT("RESUME"), EMenuAction::ResumeGame, Gold, KeyHintOf(GM, 0));
	Y += BtnH + BtnGap;
	DrawMenuButton(X + P, Y, BtnW, BtnH, TEXT("RESTART"), EMenuAction::RestartRun, Cyan, FString());
	Y += BtnH + BtnGap;
	DrawMenuButton(X + P, Y, BtnW, BtnH, TEXT("MAIN MENU"), EMenuAction::QuitRunToMenu, Red, KeyHintOf(GM, 2));
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

	DrawLabel(Eyebrow, CX, Y, TinySize, Cyan, ESpinTextAlign::Center, true);
	Y += LineH(TinySize) + U(6.f);
	DrawLabel(Title, CX, Y, TitleSize, Gold, ESpinTextAlign::Center, true);
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

	DrawKeyPrompt(ConfirmVerb, ConfirmKey, TEXT("to continue"), CX, Y, PromptSize, ConfirmColor);
}
