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

	const TCHAR* LiftKey = TEXT("LMB / R2");

	FLinearColor Fade(FLinearColor Color, float Alpha)
	{
		Color.A *= FMath::Clamp(Alpha, 0.f, 1.f);
		return Color;
	}

	FString GetMistakeMessage(ESpinMistake Mistake)
	{
		switch (Mistake)
		{
		case ESpinMistake::MissedZone:  return TEXT("You missed the zone!");
		case ESpinMistake::WrongButton: return TEXT("Wrong button, or too early!");
		case ESpinMistake::HitObstacle: return TEXT("You hit an obstacle!");
		case ESpinMistake::HandBroke:   return TEXT("The hand broke free!");
		default:                        return FString();
		}
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

	/** Full hint for a button ("W / A / X"), for instructions. */
	FString KeyHintOf(const ASpinGameMode& GM, int32 Index)
	{
		return GM.Prompts.IsValidIndex(Index) ? GM.Prompts[Index].Hint : FString();
	}

	/** Short label for a button ("W"), for the key caps on menu buttons. */
	FString KeyLabelOf(const ASpinGameMode& GM, int32 Index)
	{
		return GM.Prompts.IsValidIndex(Index) ? GM.Prompts[Index].Label : FString();
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
	case ESpinState::SettingsMenu:     DrawSettings(*GM); break;
	case ESpinState::Leaderboard:      DrawLeaderboard(*GM); break;
	case ESpinState::ProfileSelect:    DrawProfileSelect(*GM); break;
	case ESpinState::NameEntry:        DrawNameEntry(*GM); break;
	case ESpinState::Playing:          DrawRunHUD(*GM); break;
	case ESpinState::MissPause:        DrawMissPause(*GM); break;
	case ESpinState::GameOver:         DrawGameOver(*GM); break;
	case ESpinState::TutorialComplete: DrawTutorialComplete(*GM); break;

	case ESpinState::NewButtonIntro:
		if (GM->Prompts.IsValidIndex(1))
		{
			const FSpinPrompt& First = GM->Prompts[0];
			const FSpinPrompt& Second = GM->Prompts[1];
			DrawBriefing(TEXT("NEW MECHANIC"), TEXT("SECOND BUTTON"),
				{
					{ TEXT("From now on each zone asks for one of two buttons, at random. Match the zone's colour:"), TextMain },
					{ First.Hint, First.Color },
					{ Second.Hint, Second.Color },
					{ TEXT("The hand has slowed down and the zones are wide again, so you can get used to it."), TextDim },
				},
				TEXT("Press"), Second.Hint, Second.Color);
		}
		break;

	case ESpinState::ObstacleIntro:
		DrawBriefing(TEXT("NEW MECHANIC"), TEXT("OBSTACLES"),
			{
				{ TEXT("Posts will slide onto the dial. Don't let the hand touch them."), TextMain },
				{ FString::Printf(TEXT("HOLD %s to raise the hand over them."), LiftKey), Cyan },
				{ TEXT("It only works while an obstacle is out, and drops by itself once you're past it."), TextDim },
				{ FString::Printf(TEXT("You have %d lives. Raising the hand with nothing to clear costs one; hopping an obstacle earns one back."), GM->MaxLives), FLinearColor(1.f, 0.65f, 0.65f) },
			},
			TEXT("Hold"), LiftKey, Cyan);
		break;

	case ESpinState::JamIntro:
	{
		const FString Key = KeyHintOf(*GM, 0);
		DrawBriefing(TEXT("NEW MECHANIC"), TEXT("JAMS"),
			{
				{ TEXT("From now on the hand can overheat after a hit."), TextMain },
				{ FString::Printf(TEXT("1.  WARNING  -  sparks and an orange flicker for %.0f seconds."), GM->JamSparkTime), Orange },
				{ TEXT("2.  JAMMED  -  the hand spins out of control; zones and obstacles stop counting."), Red },
				{ FString::Printf(TEXT("3.  REPAIR  -  mash %s %d times within %.0f seconds."), *Key, GM->JamPressesRequired, GM->JamTimeLimit), KeyColorOf(*GM, 0) },
				{ TEXT("Too slow and the hand breaks - that costs a miss life."), TextDim },
			},
			TEXT("Press"), Key, KeyColorOf(*GM, 0));
		break;
	}

	case ESpinState::TutorialIntro:
		if (const FTutorialLevel* Level = GM->GetTutorialLevel())
		{
			TArray<FSpinBriefLine> Lines;
			for (const FString& Line : Level->IntroLines)
			{
				Lines.Add({ Line, TextMain });
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
			case ETutorialConfirm::Lift:
				DrawBriefing(Eyebrow, Level->Title, Lines, TEXT("Hold"), LiftKey, Cyan);
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

	const float LabelSize = H >= U(60.f) ? 24.f : 20.f;
	DrawLabel(Label, X + W * 0.5f, Y + (H - LineH(LabelSize)) * 0.5f, LabelSize, Fade(TextMain, Opacity), ESpinTextAlign::Center, true);

	if (!KeyHint.IsEmpty())
	{
		const float KeySize = 13.f;
		const float KeyW = KeyCapWidth(KeyHint, KeySize);
		DrawKeyCap(KeyHint, X + W - U(12.f) - KeyW, Y + (H - KeyCapHeight(KeySize)) * 0.5f, KeySize, Fade(Accent, Opacity), false);
	}

	if (bEnabled)
	{
		AddRegion(X, Y, W, H, Action, Index);
	}
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

	DrawZoneLabel(GM);

	// The three top modules: run (score) on the left, level in the middle, health on the right.
	if (!GM.IsTutorial())
	{
		DrawRunModule(GM, U(Margin), Top);
	}
	DrawLevelModule(GM, CX, Top);
	DrawHealthModule(GM, ScreenW - U(Margin), Top);

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

	// Invincibility countdown, then "GO!" - skipped after an obstacle hit, whose own feedback is enough.
	if (GM.LastMistake != ESpinMistake::HitObstacle)
	{
		if (GM.bInvincible)
		{
			DrawInvincibilityCountdown(GM.InvincibleTimeRemaining, CX, CY);
		}
		else
		{
			DrawGo(Now - GM.InvincibilityEndTime, CX, CY);
		}
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
	const FVector Screen = Project(Clock->GetZoneWorldLocation(0.5f));
	const float Size = 20.f;
	const float W = KeyCapWidth(Prompt.Label, Size);
	const float H = KeyCapHeight(Size);
	DrawKeyCap(Prompt.Label, Screen.X - W * 0.5f, Screen.Y - H * 0.5f, Size, Prompt.Color, GM.IsHandInZone());
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

	// Header: what this is, and whose run it is.
	DrawLabel(TEXT("SCORE"), X + P, HeaderY, TinySize, TextDim, ESpinTextAlign::Left, true);
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
			DrawLabel(FString::Printf(TEXT("BEST  %s"), *FormatPoints(GM.BestPoints)), X + P, BestY, SmallSize, TextDim, ESpinTextAlign::Left, true);
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

void ASpinHUD::DrawHealthModule(const ASpinGameMode& GM, float RightX, float Y)
{
	const bool bMisses = !GM.IsTutorial();
	const bool bLives = GM.ObstaclesEnabled();
	if (!bMisses && !bLives)
	{
		return;
	}

	const float W = U(300.f);
	const float X = RightX - W;
	const float P = U(Pad);
	const float RowH = LineH(SmallSize) + LineH(TinySize);
	const float RowGap = U(10.f);
	const int32 Rows = (bMisses ? 1 : 0) + (bLives ? 1 : 0);

	DrawPanel(X, Y, W, P * 2.f + Rows * RowH + (Rows - 1) * RowGap, Red);

	float RowY = Y + P;
	auto DrawRow = [&](const TCHAR* Label, const TCHAR* Sub, int32 Count, int32 Max, const FLinearColor& Color, float LostTime, float GainedTime)
	{
		// The row flashes when it loses one.
		const float SinceLost = Now - LostTime;
		if (SinceLost >= 0.f && SinceLost < 0.6f)
		{
			DrawRect(Fade(Color, 0.25f * (1.f - SinceLost / 0.6f)), X + U(4.f), RowY - U(4.f), W - U(8.f), RowH + U(8.f));
		}

		DrawLabel(Label, X + P, RowY, SmallSize, Color, ESpinTextAlign::Left, true);
		DrawLabel(Sub, X + P, RowY + LineH(SmallSize), TinySize, TextFaint);

		const float Pip = U(18.f);
		const float Gap = U(6.f);
		const float PipX = X + W - P - (Max * Pip + (Max - 1) * Gap);
		const float PipY = RowY + (RowH - Pip) * 0.5f;
		for (int32 i = 0; i < Max; ++i)
		{
			const float PX = PipX + i * (Pip + Gap);
			if (i < Count)
			{
				DrawRect(Color, PX, PipY, Pip, Pip);
			}
			else
			{
				DrawRect(TrackColor, PX, PipY, Pip, Pip);
				DrawOutline(PX, PipY, Pip, Pip, Fade(Color, 0.35f), FMath::Max(1.f, U(1.f)));
			}
		}

		// "-1" / "+1" rises off the pip that just changed.
		auto Float = [&](const TCHAR* Text, int32 PipIndex, float Since, const FLinearColor& FloatColor)
		{
			if (Since >= 0.f && Since < 1.2f && PipIndex >= 0 && PipIndex < Max)
			{
				const float T = Since / 1.2f;
				DrawLabel(Text, PipX + PipIndex * (Pip + Gap) + Pip * 0.5f, PipY + (Pip - LineH(SmallSize)) * 0.5f - U(18.f) * T,
					SmallSize, Fade(FloatColor, 1.f - T * T), ESpinTextAlign::Center, true);
			}
		};
		Float(TEXT("-1"), Count, SinceLost, Color);
		Float(TEXT("+1"), Count - 1, Now - GainedTime, Green);

		RowY += RowH + RowGap;
	};

	if (bMisses)
	{
		DrawRow(TEXT("MISSES"), TEXT("no refills"), GM.MissLives, GM.MaxMissLives, Orange, GM.MissLifeLostTime, -1000.f);
	}
	if (bLives)
	{
		DrawRow(TEXT("LIVES"), GM.IsTutorial() ? TEXT("practice - can't run out") : TEXT("+1 per obstacle hopped"),
			GM.Lives, GM.MaxLives, Red, GM.LifeLostTime, GM.LifeGainedTime);
	}
}

void ASpinHUD::DrawControlStrip(const ASpinGameMode& GM, float CenterX, float BottomY)
{
	if (GM.JamPhase == EJamPhase::Jammed)
	{
		DrawRepairPanel(GM, CenterX, BottomY);
		return;
	}

	struct FItem
	{
		FString Key;
		FString Caption;
		FLinearColor Color;
		FLinearColor CaptionColor;
		bool bLit;
	};
	TArray<FItem> Items;

	// The buttons in play, in their zone colours. The one this zone wants is lit.
	for (int32 i = 0; i < GM.GetNumActiveButtons(); ++i)
	{
		if (GM.Prompts.IsValidIndex(i))
		{
			const bool bThisZone = i == GM.ActivePromptIndex && !GM.bInvincible;
			Items.Add({ GM.Prompts[i].Hint, bThisZone ? TEXT("THIS ZONE") : TEXT(""), GM.Prompts[i].Color, GM.Prompts[i].Color, bThisZone });
		}
	}

	// The lift, once obstacles are in play: lit and pulsing while there's something to hop.
	if (GM.ObstaclesEnabled())
	{
		const ASpinClock* Clock = GM.GetClock();
		const bool bLiftNow = Clock && Clock->IsLiftAvailable() && !GM.bInvincible;
		Items.Add({ FString::Printf(TEXT("HOLD  %s"), LiftKey),
			bLiftNow ? TEXT("LIFT NOW!") : TEXT("LIFT  -  only over obstacles"),
			bLiftNow ? Cyan : Fade(Cyan, 0.6f),
			bLiftNow ? Fade(Cyan, FMath::Lerp(0.6f, 1.f, Pulse01(Now, 12.f))) : TextFaint,
			bLiftNow });
	}

	const FTutorialLevel* Level = GM.IsTutorial() ? GM.GetTutorialLevel() : nullptr;
	const FString Instruction = Level ? Level->Instruction : FString();

	const float P = U(Pad);
	const float CapSize = 16.f;
	const float ItemGap = U(28.f);
	const float CapH = KeyCapHeight(CapSize);

	TArray<float> ItemWidths;
	float ContentW = 0.f;
	for (const FItem& Item : Items)
	{
		const float ItemW = FMath::Max(KeyCapWidth(Item.Key, CapSize), MeasureText(Item.Caption, TinySize, true).X);
		ItemWidths.Add(ItemW);
		ContentW += ItemW;
	}
	ContentW += ItemGap * FMath::Max(0, Items.Num() - 1);

	const float InstructionH = Instruction.IsEmpty() ? 0.f : LineH(SmallSize) + U(10.f);
	const float PanelW = FMath::Max(ContentW, MeasureText(Instruction, SmallSize).X) + P * 2.f;
	const float PanelH = P + InstructionH + CapH + U(4.f) + LineH(TinySize) + P;
	const float X = CenterX - PanelW * 0.5f;
	const float Y = BottomY - PanelH;

	DrawPanel(X, Y, PanelW, PanelH, KeyColorOf(GM, GM.ActivePromptIndex));

	float RowY = Y + P;
	if (!Instruction.IsEmpty())
	{
		DrawLabel(Instruction, CenterX, RowY, SmallSize, TextMain, ESpinTextAlign::Center);
		RowY += InstructionH;
	}

	float ItemX = CenterX - ContentW * 0.5f;
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		const FItem& Item = Items[i];
		const float ItemW = ItemWidths[i];
		const float CapW = KeyCapWidth(Item.Key, CapSize);
		DrawKeyCap(Item.Key, ItemX + (ItemW - CapW) * 0.5f, RowY, CapSize, Item.Color, Item.bLit);
		DrawLabel(Item.Caption, ItemX + ItemW * 0.5f, RowY + CapH + U(4.f), TinySize, Item.CaptionColor, ESpinTextAlign::Center, true);
		ItemX += ItemW + ItemGap;
	}
}

void ASpinHUD::DrawRepairPanel(const ASpinGameMode& GM, float CenterX, float BottomY)
{
	const FString Key = KeyHintOf(GM, 0);
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
	DrawLabel(FString::Printf(TEXT("%d / %d"), GM.JamPresses, GM.JamPressesRequired), X + W - P, Y + RepairLabelY, TinySize, TextMain, ESpinTextAlign::Right, true);
	DrawSegments(X + P, Y + SegY, W - P * 2.f, SegH, GM.JamPressesRequired, GM.JamPresses, Green);

	const float TimeFraction = GM.GetJamTimeFraction();
	DrawBar(X + P, Y + TimeY, W - P * 2.f, TimeH, TimeFraction, Red);
	DrawLabel(TEXT("BEFORE THE HAND BREAKS"), X + P, Y + TimeLabelY, TinySize, Red, ESpinTextAlign::Left, true);
	DrawLabel(FString::Printf(TEXT("%.1fs"), GM.JamTimeLimit * TimeFraction), X + W - P, Y + TimeLabelY, TinySize, Red, ESpinTextAlign::Right, true);

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
	const FString RepairKey = KeyHintOf(GM, 0);
	if (GM.JamPhase == EJamPhase::Sparking)
	{
		Title = TEXT("OVERHEATING!");
		Color = FMath::Lerp(Orange, Gold, Pulse01(Now, 14.f));
		Subs.Add({ FString::Printf(TEXT("The hand jams in %.1fs - get ready to mash %s"), FMath::Max(0.f, GM.JamSparkTime - GM.JamTimer), *RepairKey), TextMain });
	}
	else if (GM.JamPhase == EJamPhase::Jammed)
	{
		Title = TEXT("JAMMED!");
		Color = Red;
		Subs.Add({ FString::Printf(TEXT("Mash %s to repair it before it breaks"), *RepairKey), TextMain });
	}
	else if (GM.IsShowingNotice())
	{
		Title = GM.NoticeText;
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
	DrawMenuButton(BtnX, Y, BtnW, BtnH, TEXT("PLAY"), EMenuAction::Play, Gold, KeyLabelOf(GM, 0));
	Y += BtnH + Gap;
	DrawMenuButton(BtnX, Y, BtnW, BtnH, TEXT("LEADERBOARD"), EMenuAction::OpenLeaderboard, Cyan);
	Y += BtnH + Gap;
	DrawMenuButton(BtnX, Y, BtnW, BtnH, TEXT("SETTINGS"), EMenuAction::OpenSettings, TextDim, KeyLabelOf(GM, 1));
}

void ASpinHUD::DrawModeSelect(const ASpinGameMode& GM)
{
	DrawBackdrop(0.75f);

	const float CX = ScreenW * 0.5f;
	const FString Name = GM.GetPlayerName();
	float Y = DrawScreenTitle(TEXT("CHOOSE A MODE"), Name.IsEmpty() ? FString() : FString::Printf(TEXT("Playing as %s"), *Name), CX, ScreenH * 0.15f, Cyan);
	Y += U(10.f);

	const float Gap = U(32.f);
	const float CardW = FMath::Min(U(400.f), (ScreenW - U(Margin) * 2.f - Gap) * 0.5f);
	const float CardH = U(260.f);

	auto DrawCard = [&](float X, const FString& Title, const FLinearColor& Accent, const FString& Description, const FString& Extra, const FString& Key, EMenuAction Action)
	{
		DrawPanel(X, Y, CardW, CardH, Accent);
		if (IsHovered(X, Y, CardW, CardH))
		{
			DrawOutline(X, Y, CardW, CardH, Accent, FMath::Max(1.f, U(2.f)));
		}

		float LineY = Y + U(26.f);
		DrawLabel(Title, X + CardW * 0.5f, LineY, TitleSize, Accent, ESpinTextAlign::Center, true);
		LineY += LineH(TitleSize) + U(8.f);
		LineY = DrawWrapped(Description, X + CardW * 0.5f, LineY, BodySize, TextDim, CardW - U(48.f));
		if (!Extra.IsEmpty())
		{
			DrawLabel(Extra, X + CardW * 0.5f, LineY + U(10.f), SmallSize, Gold, ESpinTextAlign::Center, true);
		}

		DrawKeyPrompt(TEXT("Click or press"), Key, FString(), X + CardW * 0.5f, Y + CardH - U(22.f) - KeyCapHeight(SmallSize), SmallSize, Accent);
		AddRegion(X, Y, CardW, CardH, Action);
	};

	FString BestLine;
	const USpinSaveGame* Save = GM.GetSaveData();
	if (const FSpinProfile* Profile = Save ? Save->GetActiveProfile() : nullptr)
	{
		if (Profile->RunsPlayed > 0)
		{
			BestLine = FString::Printf(TEXT("YOUR BEST:  LV %d  /  %s"), Profile->BestLevel, *FormatPoints(Profile->BestPoints));
		}
	}

	DrawCard(CX - Gap * 0.5f - CardW, TEXT("TUTORIAL"), KeyColorOf(GM, 0),
		FString::Printf(TEXT("Learn the ropes in %d short stages. Nothing can go wrong here."), GM.TutorialLevels.Num()),
		FString(), KeyHintOf(GM, 0), EMenuAction::StartTutorial);
	DrawCard(CX + Gap * 0.5f, TEXT("ENDLESS"), KeyColorOf(GM, 1),
		TEXT("The real run. The hand speeds up, new mechanics unlock, and mistakes cost lives."),
		BestLine, KeyHintOf(GM, 1), EMenuAction::StartEndless);

	const float BackW = U(220.f);
	DrawMenuButton(CX - BackW * 0.5f, Y + CardH + U(32.f), BackW, U(50.f), TEXT("BACK"), EMenuAction::Back, TextDim);
}

void ASpinHUD::DrawSettings(const ASpinGameMode& GM)
{
	DrawBackdrop(0.75f);

	const float CX = ScreenW * 0.5f;
	float Y = DrawScreenTitle(TEXT("SETTINGS"), FString(), CX, ScreenH * 0.15f, Cyan) + U(10.f);

	const float PanelW = FMath::Min(U(640.f), ScreenW - U(Margin) * 2.f);
	const float X = CX - PanelW * 0.5f;
	const float RowH = U(76.f);
	const int32 NumRows = 3;
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

	const float Gap = U(32.f);
	const float PanelW = FMath::Min(U(600.f), (ScreenW - U(Margin) * 2.f - Gap) * 0.5f);
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
			DrawLabel(Run.PlayerName.IsEmpty() ? FString(TEXT("???")) : Run.PlayerName, NameX, TextY, SmallSize, bMine ? Cyan : TextMain, ESpinTextAlign::Left, bMine);
			DrawLabel(bByLevel ? Level : Points, PrimaryRight, TextY, SmallSize, i == 0 ? Gold : TextMain, ESpinTextAlign::Right, true);
			DrawLabel(bByLevel ? Points : Level, SecondaryRight, TextY, SmallSize, TextDim, ESpinTextAlign::Right);
		}
	};

	DrawBoard(CX - Gap * 0.5f - PanelW, TEXT("HIGHEST LEVEL"), Cyan, Save ? &Save->LevelBoard : nullptr, GM.LastRunLevelRank, true);
	DrawBoard(CX + Gap * 0.5f, TEXT("HIGHEST SCORE"), Gold, Save ? &Save->ScoreBoard : nullptr, GM.LastRunScoreRank, false);

	Y += PanelH + U(14.f);
	DrawLabel(TEXT("Green row = your last run      Cyan name = you"), CX, Y, TinySize, TextFaint, ESpinTextAlign::Center);
	Y += LineH(TinySize) + U(20.f);

	const float BackW = U(220.f);
	DrawMenuButton(CX - BackW * 0.5f, Y, BackW, U(50.f), TEXT("BACK"), EMenuAction::Back, TextDim, KeyLabelOf(GM, 1));
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
	DrawMenuButton(CX - BackW * 0.5f, Y, BackW, U(50.f), TEXT("BACK"), EMenuAction::Back, TextDim, KeyLabelOf(GM, 1));
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

void ASpinHUD::DrawMissPause(const ASpinGameMode& GM)
{
	DrawBackdrop(0.78f);

	const float CX = ScreenW * 0.5f;
	const float CardW = FMath::Min(U(660.f), ScreenW - U(Margin) * 2.f);
	const float P = U(32.f);

	// Which pool paid for it.
	const bool bObstacle = GM.LastMistake == ESpinMistake::HitObstacle;
	const int32 Left = bObstacle ? GM.Lives : GM.MissLives;
	const int32 Max = bObstacle ? GM.MaxLives : GM.MaxMissLives;
	const FLinearColor PoolColor = bObstacle ? Red : Orange;
	const TCHAR* PoolName = bObstacle ? TEXT("LIVES") : TEXT("MISSES");
	const TCHAR* PoolNote = bObstacle ? TEXT("+1 per obstacle hopped") : TEXT("no refills");

	const TArray<FString> InfoLines = WrapText(FString::Printf(TEXT("Continue for %.0f seconds of invincibility to get back in rhythm."), GM.InvincibilityDuration),
		SmallSize, false, CardW - P * 2.f);

	const float Pip = U(28.f);
	const float BtnH = U(62.f);
	const float CardH = P + LineH(TitleSize) + U(6.f) + LineH(HeadingSize) + U(26.f) + Pip + U(10.f) + LineH(SmallSize) + U(18.f)
		+ InfoLines.Num() * LineH(SmallSize) + U(24.f) + BtnH + P;
	const float X = CX - CardW * 0.5f;
	float Y = (ScreenH - CardH) * 0.5f;
	DrawPanel(X, Y, CardW, CardH, Red);
	Y += P;

	DrawLabel(TEXT("LIFE LOST"), CX, Y, TitleSize, Red, ESpinTextAlign::Center, true);
	Y += LineH(TitleSize) + U(6.f);
	DrawLabel(GetMistakeMessage(GM.LastMistake), CX, Y, HeadingSize, TextMain, ESpinTextAlign::Center);
	Y += LineH(HeadingSize) + U(26.f);

	// The pool, big: "MISSES  [#][#][ ]".
	const float PipGap = U(8.f);
	const float LabelW = MeasureText(PoolName, BodySize, true).X + U(18.f);
	const float PipsW = Max * Pip + (Max - 1) * PipGap;
	const float RowX = CX - (LabelW + PipsW) * 0.5f;
	DrawLabel(PoolName, RowX, Y + (Pip - LineH(BodySize)) * 0.5f, BodySize, PoolColor, ESpinTextAlign::Left, true);
	for (int32 i = 0; i < Max; ++i)
	{
		const float PX = RowX + LabelW + i * (Pip + PipGap);
		if (i < Left)
		{
			DrawRect(PoolColor, PX, Y, Pip, Pip);
		}
		else
		{
			DrawRect(TrackColor, PX, Y, Pip, Pip);
			DrawOutline(PX, Y, Pip, Pip, Fade(PoolColor, 0.4f), FMath::Max(1.f, U(1.f)));
		}
	}
	Y += Pip + U(10.f);
	DrawLabel(FString::Printf(TEXT("%d / %d left  -  %s"), Left, Max, PoolNote), CX, Y, SmallSize, TextDim, ESpinTextAlign::Center);
	Y += LineH(SmallSize) + U(18.f);

	for (const FString& Line : InfoLines)
	{
		DrawLabel(Line, CX, Y, SmallSize, TextDim, ESpinTextAlign::Center);
		Y += LineH(SmallSize);
	}
	Y += U(24.f);

	const float BtnW = FMath::Min(U(320.f), CardW - P * 2.f);
	DrawMenuButton(CX - BtnW * 0.5f, Y, BtnW, BtnH, TEXT("CONTINUE"), EMenuAction::ContinueAfterMiss, Green, KeyLabelOf(GM, 0));
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

	const float CardH = P + LineH(TinySize) + U(2.f) + LineH(GameOverSize) + U(4.f) + LineH(BodySize) + U(26.f) + TileH + U(28.f) + BtnH + P;
	const float X = CX - CardW * 0.5f;
	float Y = (ScreenH - CardH) * 0.5f;
	DrawPanel(X, Y, CardW, CardH, Red);
	Y += P;

	DrawLabel(Name, CX, Y, TinySize, Cyan, ESpinTextAlign::Center, true);
	Y += LineH(TinySize) + U(2.f);
	DrawLabel(TEXT("GAME OVER"), CX, Y, GameOverSize, Red, ESpinTextAlign::Center, true);
	Y += LineH(GameOverSize) + U(4.f);
	DrawLabel(Reason, CX, Y, BodySize, TextMain, ESpinTextAlign::Center);
	Y += LineH(BodySize) + U(26.f);

	// The two results the leaderboards rank by, as tiles - each with how it stacks up.
	// BestLevel / BestPoints are the player's bests from before this run.
	const int32 Level = GM.GetLevel();
	const bool bNewBestLevel = GM.Hits > 0 && Level > GM.BestLevel;
	const bool bNewBestPoints = GM.Hits > 0 && GM.Points > GM.BestPoints;

	FString LevelStatus;
	if (bNewBestLevel)
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
		PointsStatus = TEXT("NEW BEST!");
	}
	else if (GM.BestPoints > 0)
	{
		const FString Short = FString::Printf(TEXT("%s short of your best"), *FormatPoints(GM.BestPoints - GM.Points));
		PointsStatus = GM.LastRunScoreRank > 0 ? FString::Printf(TEXT("#%d  -  %s"), GM.LastRunScoreRank, *Short) : Short;
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
	Y += TileH + U(28.f);

	// Shown straight away but only clickable after the short delay, so a press meant for the hand
	// can't land on one by accident.
	const bool bReady = GM.IsGameOverMenuReady();
	const float BtnGap = U(16.f);
	const float BtnW = (CardW - P * 2.f - BtnGap * 2.f) / 3.f;
	DrawMenuButton(X + P, Y, BtnW, BtnH, TEXT("RETRY"), EMenuAction::RetryGame, Gold, KeyLabelOf(GM, 0), bReady);
	DrawMenuButton(X + P + BtnW + BtnGap, Y, BtnW, BtnH, TEXT("MAIN MENU"), EMenuAction::ReturnToMainMenu, Cyan, KeyLabelOf(GM, 1), bReady);
	DrawMenuButton(X + P + (BtnW + BtnGap) * 2.f, Y, BtnW, BtnH, TEXT("QUIT"), EMenuAction::QuitGame, Red, FString(), bReady);
}

void ASpinHUD::DrawTutorialComplete(const ASpinGameMode& GM)
{
	DrawBackdrop(0.8f);

	const float CX = ScreenW * 0.5f;
	const float CardW = FMath::Min(U(700.f), ScreenW - U(Margin) * 2.f);
	const float P = U(32.f);
	const float BtnH = U(60.f);

	const TArray<FString> Lines = WrapText(TEXT("From here on mistakes cost lives, and a jam you can't fix ends the run."), BodySize, false, CardW - P * 2.f);
	const float CardH = P + LineH(TinySize) + U(6.f) + LineH(TitleSize) + U(16.f) + Lines.Num() * LineH(BodySize) + U(28.f) + BtnH + P;
	const float X = CX - CardW * 0.5f;
	float Y = (ScreenH - CardH) * 0.5f;
	DrawPanel(X, Y, CardW, CardH, Green);
	Y += P;

	DrawLabel(TEXT("TUTORIAL"), CX, Y, TinySize, Green, ESpinTextAlign::Center, true);
	Y += LineH(TinySize) + U(6.f);
	DrawLabel(TEXT("ALL STAGES CLEARED!"), CX, Y, TitleSize, Gold, ESpinTextAlign::Center, true);
	Y += LineH(TitleSize) + U(16.f);
	for (const FString& Line : Lines)
	{
		DrawLabel(Line, CX, Y, BodySize, TextMain, ESpinTextAlign::Center);
		Y += LineH(BodySize);
	}
	Y += U(28.f);

	const float BtnGap = U(16.f);
	const float BtnW = (CardW - P * 2.f - BtnGap) * 0.5f;
	DrawMenuButton(X + P, Y, BtnW, BtnH, TEXT("START ENDLESS"), EMenuAction::StartEndless, Gold, KeyLabelOf(GM, 0));
	DrawMenuButton(X + P + BtnW + BtnGap, Y, BtnW, BtnH, TEXT("MAIN MENU"), EMenuAction::ReturnToMainMenu, Cyan, KeyLabelOf(GM, 1));
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
