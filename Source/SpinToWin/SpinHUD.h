// Spin To Win - the 2D overlay: the in-run HUD, and every menu and pause screen.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Fonts/SlateFontInfo.h"
#include "SpinGameMode.h"
#include "SpinHUD.generated.h"

class UTexture2D;

/** Names one menu item across frames (the layout is rebuilt every frame): what it does, and which
 *  item of a list it is (e.g. which profile), or INDEX_NONE. */
struct FSpinFocusKey
{
	EMenuAction Action = EMenuAction::None;
	int32 Index = INDEX_NONE;

	bool operator==(const FSpinFocusKey& Other) const { return Action == Other.Action && Index == Other.Index; }
	friend uint32 GetTypeHash(const FSpinFocusKey& Key) { return HashCombine(::GetTypeHash(static_cast<uint8>(Key.Action)), ::GetTypeHash(Key.Index)); }
};

/** A screen-space rectangle for one menu item, rebuilt fresh every DrawHUD() call since layout
 *  depends on the current viewport size. The player controller hit-tests mouse clicks against
 *  these, and the controller highlight moves between them. */
struct FSpinMenuRegion
{
	FBox2D Rect = FBox2D(ForceInit);
	EMenuAction Action = EMenuAction::None;

	/** Which item of a list the action applies to (e.g. which profile), or INDEX_NONE. */
	int32 Index = INDEX_NONE;

	/** Left / right on the controller while this item is highlighted (a settings value): fired
	 *  instead of moving the highlight. None = left / right move the highlight as usual. */
	EMenuAction LeftAction = EMenuAction::None;
	EMenuAction RightAction = EMenuAction::None;

	/** Can be highlighted (controller or mouse hover). Click-only parts, like the < > arrows on a
	 *  settings row, aren't - the row they sit on is. */
	bool bFocusable = true;

	/** Responds to mouse clicks. */
	bool bClickable = true;

	FSpinFocusKey GetKey() const { return { Action, Index }; }
};

/** One spark of the HUD's particle effects, in screen pixels. */
struct FSpinSpark
{
	FVector2D Pos = FVector2D::ZeroVector;
	FVector2D Vel = FVector2D::ZeroVector;
	float Age = 0.f;
	float Life = 1.f;
	float Size = 2.f;
	FLinearColor Color = FLinearColor::White;
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
 * The look is neon on near-black, to match the city: panels with cut corners and glowing edges, a
 * CRT-style reveal whenever a screen opens, and ambient life around the edges of the menus (sparks,
 * electric arcs, blinking status lights, scanlines, a scrolling grid). Glows are drawn additively
 * with a few small textures generated at startup, so none of it needs any Content assets.
 *
 * Menus: every item is a region (see FSpinMenuRegion). One of them is highlighted at a time. The
 * controller's D-pad / stick (or the arrow keys) moves the highlight to the nearest item in that
 * direction, and the mouse moves it by hovering. Selecting and going back are handled by the
 * player controller.
 *
 * In-run layout: two anchored modules along the top (run/score, level), health columns either side
 * of the dial, the jam repair panel along the bottom, one callout slot above the dial for messages, and
 * transient feedback (hit grades, countdown) near the centre.
 */
UCLASS()
class SPINTOWIN_API ASpinHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** This frame's menu items, for the player controller to hit-test mouse clicks against. */
	const TArray<FSpinMenuRegion>& GetMenuRegions() const { return MenuRegions; }

	/** The highlighted menu item, or null if the screen has none. */
	const FSpinMenuRegion* GetFocusedRegion() const;

	/** The topmost item under a screen point that can be highlighted (bFocusable) or clicked (!bFocusable). */
	const FSpinMenuRegion* FindRegionAt(const FVector2D& Point, bool bFocusable) const;

	/** Moves the highlight to the nearest item in that direction (DY -1 = up), wrapping around at the ends. */
	void MoveFocus(int32 DX, int32 DY);

	void SetFocus(const FSpinFocusKey& Key);

	/** The flash and burst of sparks when an item is pressed. */
	void NotifyActivated(const FSpinFocusKey& Key, const FBox2D& Rect);

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
		ESpinTextAlign Align = ESpinTextAlign::Left, bool bBold = false, bool bShadow = true);

	/** Bold text with a neon glow behind it, and optionally split into magenta / cyan copies either
	 *  side (chromatic aberration) by Aberration pixels. Returns its width. */
	float DrawNeonText(const FString& Text, float X, float Y, float Size, const FLinearColor& Color, const FLinearColor& GlowColor,
		float Glow, ESpinTextAlign Align = ESpinTextAlign::Center, float Aberration = 0.f);

	/** Hollow letters traced in a neon tube: a soft halo hugging the letter shapes, the coloured tube,
	 *  its white-hot inner edge, and a dark fill. Brightness 0..1 dims the tube (for flicker). */
	void DrawNeonOutlineText(const FString& Text, float X, float Y, float Size, const FLinearColor& Tube, float Brightness,
		ESpinTextAlign Align = ESpinTextAlign::Left);

	/** Greedy word wrap to fit MaxWidth (screen pixels). */
	TArray<FString> WrapText(const FString& Text, float Size, bool bBold, float MaxWidth) const;

	/** Draws wrapped text, one centred line after another, and returns the Y below the last line. */
	float DrawWrapped(const FString& Text, float CenterX, float Y, float Size, const FLinearColor& Color, float MaxWidth, bool bBold = false);

	/** Huge animated text (countdown, GO!) centred on a point: rasterized once at a large size and
	 *  scaled from there, so it stays sharp at every size the animation passes through. Scale is a
	 *  multiple of 10pt at 1080p. */
	void DrawBigText(const FString& Text, const FLinearColor& Color, float Scale, float CenterX, float CenterY);

	// Glow and shape drawing.

	/** Creates the glow / gradient / scanline textures the first time they're needed. */
	void EnsureTextures();

	/** Everything drawn is faded by this (0..1) - lets a whole screen fade in at once (the game-over
	 *  screen, see ASpinGameMode::GameOverFadeTime). Applied in the primitives below. */
	float DrawOpacity = 1.f;

	/** A colour with DrawOpacity applied: to the alpha, or for additive drawing (which ignores alpha)
	 *  to the brightness. */
	FLinearColor WithOpacity(const FLinearColor& Color, bool bAdditive = false) const;

	/** Hides AHUD::DrawRect so plain rectangles fade with DrawOpacity too. */
	void DrawRect(FLinearColor RectColor, float RectX, float RectY, float RectW, float RectH);

	/** A textured tile, optionally additive (for glows: then the colour's RGB is the brightness and
	 *  its alpha is ignored) and rotated about its centre. */
	void DrawTex(UTexture2D* Texture, float X, float Y, float W, float H, const FLinearColor& Color, bool bAdditive,
		const FVector2D& UV0 = FVector2D(0.f, 0.f), const FVector2D& UV1 = FVector2D(1.f, 1.f), float AngleDeg = 0.f);

	/** A soft round glow centred on a point. */
	void DrawGlowSprite(float CenterX, float CenterY, float W, float H, const FLinearColor& Color, float Intensity);

	/** A soft glowing strip along a line (the halo of a neon tube). */
	void DrawBeam(const FVector2D& A, const FVector2D& B, float Width, const FLinearColor& Color, float Intensity);

	/** A neon tube: a crisp core line with a glow around it. */
	void DrawNeonLine(const FVector2D& A, const FVector2D& B, const FLinearColor& Color, float Thickness, float Glow);

	/** Fills a convex polygon with a top-to-bottom gradient. */
	void FillPolygon(const FVector2D* Points, int32 NumPoints, const FLinearColor& Top, const FLinearColor& Bottom, bool bAdditive = false);

	/** The six corners of a rectangle with its top-left and bottom-right corners cut off, clockwise
	 *  from the top edge. Cut is clamped to fit. */
	static void ChamferRect(float X, float Y, float W, float H, float Cut, FVector2D (&Out)[6]);

	/** The standard panel: dark gradient fill with cut corners, a neon edge with an accent bar
	 *  along the top, bracketed corners, and a light that runs around its edge. Opacity fades the
	 *  whole thing. */
	void DrawPanel(float X, float Y, float W, float H, const FLinearColor& Accent, float Opacity = 1.f);
	void DrawOutline(float X, float Y, float W, float H, const FLinearColor& Color, float Thickness);
	void DrawCornerBrackets(float X, float Y, float W, float H, const FLinearColor& Color, float Length, float Thickness, float Glow = 0.f);

	/** A progress bar on a dark track: the fill glows and carries a bright head. */
	void DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Fill);

	/** A bar split into Count slanted segments, the first Lit of them filled. */
	void DrawSegments(float X, float Y, float W, float H, int32 Count, int32 Lit, const FLinearColor& On);

	/** A check box: neon outline, lit centre when on. */
	void DrawCheckBox(float X, float Y, float Size, bool bOn, const FLinearColor& Color);

	/** A small padlock, for things still to unlock. */
	void DrawLockIcon(float X, float Y, float Size, const FLinearColor& Color);

	/** A keyboard-key style box ("W", "LMB / R2"). Lit = filled with the colour. Returns its width. */
	float DrawKeyCap(const FString& Key, float X, float Y, float Size, const FLinearColor& Color, bool bLit);
	float KeyCapWidth(const FString& Key, float Size) const;
	float KeyCapHeight(float Size) const;

	/** "Press [W] to continue", centred on CenterX. */
	void DrawKeyPrompt(const FString& Before, const FString& Key, const FString& After, float CenterX, float Y, float Size, const FLinearColor& KeyColor);

	bool IsHovered(float X, float Y, float W, float H) const;

	/** Registers a menu item for this frame (see FSpinMenuRegion). */
	void AddRegion(float X, float Y, float W, float H, EMenuAction Action, int32 Index = INDEX_NONE,
		EMenuAction LeftAction = EMenuAction::None, EMenuAction RightAction = EMenuAction::None, bool bFocusable = true, bool bClickable = true);

	/** The item just added is the one highlighted when the screen opens (otherwise it's the first). */
	void MarkDefaultFocus();

	/** 0..1, how highlighted an item is: eases in and out as the highlight arrives and leaves. */
	float FocusAmount(const FSpinFocusKey& Key) const;

	/** 1 the moment an item is pressed, fading to 0. */
	float ActivateFlash(const FSpinFocusKey& Key) const;

	/** A clickable menu button: cut-corner panel, accent strip on the left, centred label, and an
	 *  optional key cap on the right showing its keyboard/gamepad shortcut. Lights up, glows and
	 *  shows chevrons while highlighted. Disabled buttons are dimmed and not selectable. */
	void DrawMenuButton(float X, float Y, float W, float H, const FString& Label, EMenuAction Action,
		const FLinearColor& Accent, const FString& KeyHint = FString(), bool bEnabled = true, int32 Index = INDEX_NONE);

	/** The highlight on a list row or card: target brackets that close in around it, a soft glow, and
	 *  a flash when it's pressed. */
	void DrawFocusFrame(float X, float Y, float W, float H, const FLinearColor& Accent, const FSpinFocusKey& Key);

	/** A settings row's < or > button (click-only - the row is what's highlighted). */
	void DrawArrowButton(float X, float Y, float Size, bool bRight, EMenuAction Action, float RowFocus);

	/** The leaderboard's mark for a run that used bonuses: a small green "+" box, left edge at X. */
	void DrawBonusBadge(float X, float CenterY);

	/** Full-screen dimming behind a menu or pause screen, with its ambient animation: a scrolling
	 *  grid floor, a slow scan band, scanlines and a vignette. */
	void DrawBackdrop(float Opacity = 0.8f);

	/** A screen's title (neon, with an underline that draws itself in) and an optional subtitle.
	 *  Returns the Y below it. */
	float DrawScreenTitle(const FString& Title, const FString& Subtitle, float CenterX, float Y, const FLinearColor& Accent);

	// ---------------- Ambient effects ----------------

	/** The chrome around every menu screen: neon corner brackets, status read-outs, blinking
	 *  lights, the odd electric arc along an edge, and sparks falling from the corners. */
	void DrawScreenFrame(const ASpinGameMode& GM);

	/** The quieter version during a run: corner brackets that flash with each hit. */
	void DrawRunChrome(const ASpinGameMode& GM);

	/** What the controls do on this screen, along the bottom ("D-PAD move   A select   B back"). */
	void DrawNavLegend(const ASpinGameMode& GM);

	/** The screen switching on: it opens out from a bright line across the middle, like an old CRT. */
	void DrawScreenReveal();

	/** Throws sparks out from a point. AngleDeg is the direction in screen space (0 = right, 90 = down). */
	void EmitSparks(const FVector2D& Pos, int32 Count, const FLinearColor& Color, float Speed, float AngleDeg, float SpreadDeg);

	/** Moves, ages and draws every live spark (real time, so they keep going while paused). */
	void UpdateAndDrawSparks();

	// ---------------- Focus ----------------

	void OnScreenChanged(int32 NewScreen);

	/** After a screen is drawn: if the highlighted item isn't on it, highlight its default item. */
	void ValidateFocus();

	void UpdateFocusAnimations();

	// ---------------- In-run HUD ----------------

	/** Everything shown while the hand is in play. */
	void DrawRunHUD(const ASpinGameMode& GM);

	/** The key the current zone needs, floating over the zone itself (lit while the hand is in it). */
	void DrawZoneLabel(const ASpinGameMode& GM);

	/** The pixel blocks over the dial while it glitches to a new size, and over obstacles glitching in or out. */
	void DrawGlitches(const ASpinGameMode& GM);

	/** Random chunky pixels on a grid inside an ellipse, plus torn scanlines past its edges. Seed
	 *  picks the pattern; Strength 1 fills MaxDensity of the tiles, thinning to nothing at 0. */
	void DrawGlitchBlocks(float CenterX, float CenterY, float HalfW, float HalfH, float Strength, int32 Seed, float Tile, float MaxDensity, int32 NumTears);

	/** Top-left (Endless): score, combo multiplier and perfect streak, and the best to beat. */
	void DrawRunModule(const ASpinGameMode& GM, float X, float Y);

	/** Top-centre: level (or tutorial stage) progress, best level, and what the next level unlocks. */
	void DrawLevelModule(const ASpinGameMode& GM, float CenterX, float Y);

	/** Beside the dial: obstacle lives in a vertical pip column on its left, miss lives on its right,
	 *  with -1 / +1 feedback. */
	void DrawHealthColumns(const ASpinGameMode& GM);

	/** Bottom: the tutorial stage's one-line reminder - or, while jammed, the repair key and timer
	 *  (drawn just above the dial; BottomY is only the fallback without one). */
	void DrawControlStrip(const ASpinGameMode& GM, float CenterX, float BottomY);
	void DrawRepairPanel(const ASpinGameMode& GM, float CenterX, float BottomY);

	/** The single message slot above the dial: jam warnings, notices, and level-up / unlock banners,
	 *  highest priority first, so they never pile on top of each other. */
	void DrawCallout(const ASpinGameMode& GM, float CenterX, float Y);

	/** "PERFECT! +300" after each hit, and "COMBO LOST". */
	void DrawHitPopup(const ASpinGameMode& GM, float CenterX, float Y);


	/** Each whole second of the invincibility window punches in huge and quickly shrinks and fades. */
	void DrawInvincibilityCountdown(float TimeRemaining, float CenterX, float CenterY);

	/** The "GO!" that punches in right after the countdown runs out. */
	void DrawGo(float TimeSinceEnd, float CenterX, float CenterY);

	// ---------------- Screens ----------------

	void DrawMainMenu(const ASpinGameMode& GM);
	void DrawModeSelect(const ASpinGameMode& GM);
	/** Before an Endless run: modifiers on the left, bonuses on the right, then START. Locked ones
	 *  stay hidden behind what unlocks them and a progress bar. */
	void DrawRunSetup(const ASpinGameMode& GM);
	void DrawSettings(const ASpinGameMode& GM);
	void DrawLeaderboard(const ASpinGameMode& GM);
	void DrawProfileSelect(const ASpinGameMode& GM);
	void DrawNameEntry(const ASpinGameMode& GM);
	void DrawGameOver(const ASpinGameMode& GM);
	/** Drawn over the frozen run HUD while the game is paused. */
	void DrawPauseMenu(const ASpinGameMode& GM);

	/** The mid-run upgrade pick: the offered cards side by side, and what the run already has. */
	void DrawUpgradePick(const ASpinGameMode& GM);

	/** The explanation screens (new mechanic unlocks, tutorial stages): eyebrow tag, title, lines,
	 *  and what to press to continue. */
	void DrawBriefing(const FString& Eyebrow, const FString& Title, const TArray<FSpinBriefLine>& Lines,
		const FString& ConfirmVerb, const FString& ConfirmKey, const FLinearColor& ConfirmColor);

	// ---------------- Frame state ----------------

	/** UI scale for this frame: screen height / 1080. */
	float S = 1.f;
	float ScreenW = 1920.f;
	float ScreenH = 1080.f;

	/** World time: matches the game mode's timestamps (hits, notices), and stops while paused. */
	float Now = 0.f;

	/** Real time and its frame delta, for UI animation that keeps running while paused. */
	float UiTime = 0.f;
	float UiDelta = 0.f;

	/** How lively the ambient panel effects are: full on menus, toned down during a run. */
	float AmbientLevel = 1.f;

	TArray<FSpinMenuRegion> MenuRegions;
	FVector2D MousePos = FVector2D::ZeroVector;
	FVector2D LastMousePos = FVector2D::ZeroVector;
	bool bHaveMouse = false;
	bool bHadMouse = false;

	// ---------------- Focus state ----------------

	/** The screen being shown (an ESpinState, or PauseScreen), to notice when it changes. */
	int32 CurrentScreen = -1;
	static constexpr int32 PauseScreen = 1000;
	float ScreenEnterTime = -100.f;

	FSpinFocusKey Focus;
	bool bHasFocus = false;
	FSpinFocusKey DefaultFocus;
	bool bHasDefaultFocus = false;

	/** Where the highlight was when each menu was left, so backing out returns to the same item. */
	TMap<int32, FSpinFocusKey> RememberedFocus;

	/** Each item's highlight animation (see FocusAmount). */
	TMap<FSpinFocusKey, float> FocusAnim;

	FSpinFocusKey LastActivated;
	float ActivatedTime = -100.f;

	// ---------------- Effects state ----------------

	TArray<FSpinSpark> Sparks;
	float CornerSparkTimer = 0.5f;
	float FocusSparkTimer = 0.5f;
	int32 LastArcSlot = -1;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> GlowTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> BeamTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> FadeRightTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> FadeDownTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ScanTexture;
};
