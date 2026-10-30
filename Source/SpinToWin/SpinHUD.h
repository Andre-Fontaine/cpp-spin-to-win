// Spin To Win - the 2D overlay: the in-run HUD, and every menu and pause screen.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Fonts/SlateFontInfo.h"
#include "SpinGameMode.h"
#include "SpinHUD.generated.h"

/** A screen-space rectangle the player controller hit-tests mouse clicks against, rebuilt fresh
 *  every DrawHUD() call since layout depends on the current viewport size. */
struct FSpinMenuRegion
{
	FBox2D Rect = FBox2D(ForceInit);
	EMenuAction Action = EMenuAction::None;

	/** Which item of a list the action applies to (e.g. which profile), or INDEX_NONE. */
	int32 Index = INDEX_NONE;
};

enum class ESpinTextAlign : uint8
{
	Left,
	Center,
	Right
};

/** One line of a briefing card (the explanation screens): text and its colour. */
struct FSpinBriefLine
{
	FString Text;
	FLinearColor Color = FLinearColor::White;
};

/**
 * Everything is laid out for a 1080p screen and scaled by S (screen height / 1080), so the HUD keeps
 * its proportions at any resolution. Conventions used by all the helpers below:
 *  - positions and sizes passed in are final screen pixels (callers scale design values with U());
 *  - font sizes are in design points and are scaled internally;
 *  - text is positioned by the top of its line.
 *
 * In-run layout: three anchored modules along the top (run/score, level, health), a control strip
 * along the bottom, one callout slot above the dial for messages, and transient feedback (hit
 * grades, countdown) near the centre.
 */
UCLASS()
class SPINTOWIN_API ASpinHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** This frame's clickable regions, for the player controller to hit-test mouse clicks against. */
	const TArray<FSpinMenuRegion>& GetMenuRegions() const { return MenuRegions; }

private:
	// ---------------- Primitives ----------------

	/** Design pixels (1080p) to screen pixels. */
	float U(float DesignPixels) const { return DesignPixels * S; }

	FSlateFontInfo MakeFont(float Size, bool bBold) const;
	FVector2D MeasureText(const FString& Text, float Size, bool bBold = false) const;

	/** Height of one line of text at this size, in screen pixels. */
	float LineH(float Size) const;

	/** Draws a line of text (with a soft drop shadow so it reads over the scene) and returns its width. */
	float DrawLabel(const FString& Text, float X, float Y, float Size, const FLinearColor& Color,
		ESpinTextAlign Align = ESpinTextAlign::Left, bool bBold = false);

	/** Greedy word wrap to fit MaxWidth (screen pixels). */
	TArray<FString> WrapText(const FString& Text, float Size, bool bBold, float MaxWidth) const;

	/** Draws wrapped text, one centred line after another, and returns the Y below the last line. */
	float DrawWrapped(const FString& Text, float CenterX, float Y, float Size, const FLinearColor& Color, float MaxWidth, bool bBold = false);

	/** Huge animated text (countdown, GO!) centred on a point: rasterized once at a large size and
	 *  scaled from there, so it stays sharp at every size the animation passes through. Scale is a
	 *  multiple of 10pt at 1080p. */
	void DrawBigText(const FString& Text, const FLinearColor& Color, float Scale, float CenterX, float CenterY);

	/** The standard panel: dark translucent fill, faint edge, accent bar along the top and neon
	 *  corner brackets. Opacity fades the whole thing. */
	void DrawPanel(float X, float Y, float W, float H, const FLinearColor& Accent, float Opacity = 1.f);
	void DrawOutline(float X, float Y, float W, float H, const FLinearColor& Color, float Thickness);
	void DrawCornerBrackets(float X, float Y, float W, float H, const FLinearColor& Color, float Length, float Thickness);

	/** A plain progress bar on a dark track. */
	void DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Fill);

	/** A bar split into Count segments, the first Lit of them filled. */
	void DrawSegments(float X, float Y, float W, float H, int32 Count, int32 Lit, const FLinearColor& On);

	/** A keyboard-key style box ("W", "LMB / R2"). Lit = filled with the colour. Returns its width. */
	float DrawKeyCap(const FString& Key, float X, float Y, float Size, const FLinearColor& Color, bool bLit);
	float KeyCapWidth(const FString& Key, float Size) const;
	float KeyCapHeight(float Size) const;

	/** "Press [W] to continue", centred on CenterX. */
	void DrawKeyPrompt(const FString& Before, const FString& Key, const FString& After, float CenterX, float Y, float Size, const FLinearColor& KeyColor);

	bool IsHovered(float X, float Y, float W, float H) const;
	void AddRegion(float X, float Y, float W, float H, EMenuAction Action, int32 Index = INDEX_NONE);

	/** A clickable menu button: panel, accent strip on the left, centred label, and an optional key
	 *  cap on the right showing its keyboard/gamepad shortcut. Disabled buttons are dimmed and not clickable. */
	void DrawMenuButton(float X, float Y, float W, float H, const FString& Label, EMenuAction Action,
		const FLinearColor& Accent, const FString& KeyHint = FString(), bool bEnabled = true, int32 Index = INDEX_NONE);

	/** Full-screen dimming behind a menu or pause screen. */
	void DrawBackdrop(float Opacity = 0.8f);

	/** A screen's title with an accent underline, and an optional subtitle. Returns the Y below it. */
	float DrawScreenTitle(const FString& Title, const FString& Subtitle, float CenterX, float Y, const FLinearColor& Accent);

	// ---------------- In-run HUD ----------------

	/** Everything shown while the hand is in play. */
	void DrawRunHUD(const ASpinGameMode& GM);

	/** The key the current zone needs, floating over the zone itself (lit while the hand is in it). */
	void DrawZoneLabel(const ASpinGameMode& GM);

	/** Top-left (Endless): score, combo multiplier and perfect streak, and the best to beat. */
	void DrawRunModule(const ASpinGameMode& GM, float X, float Y);

	/** Top-centre: level (or tutorial stage) progress, best level, and what the next level unlocks. */
	void DrawLevelModule(const ASpinGameMode& GM, float CenterX, float Y);

	/** Top-right: miss lives and obstacle lives as labelled pip rows, with -1 / +1 feedback. */
	void DrawHealthModule(const ASpinGameMode& GM, float RightX, float Y);

	/** Bottom: the keys in play (the one this zone needs lit) and the lift - or, while jammed, the repair panel. */
	void DrawControlStrip(const ASpinGameMode& GM, float CenterX, float BottomY);
	void DrawRepairPanel(const ASpinGameMode& GM, float CenterX, float BottomY);

	/** The single message slot above the dial: jam warnings, notices, and level-up / unlock banners,
	 *  highest priority first, so they never pile on top of each other. */
	void DrawCallout(const ASpinGameMode& GM, float CenterX, float Y);

	/** "PERFECT! +300" after each hit, and "COMBO LOST". */
	void DrawHitPopup(const ASpinGameMode& GM, float CenterX, float Y);

	/** Pulsing glow around the screen edges (jam warning / jammed). */
	void DrawEdgeGlow(const FLinearColor& Color, float Speed);

	/** Each whole second of the invincibility window punches in huge and quickly shrinks and fades. */
	void DrawInvincibilityCountdown(float TimeRemaining, float CenterX, float CenterY);

	/** The "GO!" that punches in right after the countdown runs out. */
	void DrawGo(float TimeSinceEnd, float CenterX, float CenterY);

	// ---------------- Screens ----------------

	void DrawMainMenu(const ASpinGameMode& GM);
	void DrawModeSelect(const ASpinGameMode& GM);
	void DrawModifierSelect(const ASpinGameMode& GM);
	void DrawSettings(const ASpinGameMode& GM);
	void DrawLeaderboard(const ASpinGameMode& GM);
	void DrawProfileSelect(const ASpinGameMode& GM);
	void DrawNameEntry(const ASpinGameMode& GM);
	void DrawGameOver(const ASpinGameMode& GM);
	void DrawTutorialComplete(const ASpinGameMode& GM);

	/** The explanation screens (new mechanic unlocks, tutorial stages): eyebrow tag, title, lines,
	 *  and what to press to continue. */
	void DrawBriefing(const FString& Eyebrow, const FString& Title, const TArray<FSpinBriefLine>& Lines,
		const FString& ConfirmVerb, const FString& ConfirmKey, const FLinearColor& ConfirmColor);

	// ---------------- Frame state ----------------

	/** UI scale for this frame: screen height / 1080. */
	float S = 1.f;
	float ScreenW = 1920.f;
	float ScreenH = 1080.f;
	float Now = 0.f;

	TArray<FSpinMenuRegion> MenuRegions;
	FVector2D MousePos = FVector2D::ZeroVector;
	bool bHaveMouse = false;
};
