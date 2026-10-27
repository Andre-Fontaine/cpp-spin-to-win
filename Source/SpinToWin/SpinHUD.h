// Spin To Win - draws the clock face, hand, target zone and text.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SpinGameMode.h"
#include "SpinHUD.generated.h"

/** A screen-space rectangle the player controller hit-tests mouse clicks against, rebuilt fresh
 *  every DrawHUD() call since layout depends on the current viewport size. */
struct FSpinMenuRegion
{
	FBox2D Rect = FBox2D(ForceInit);
	EMenuAction Action = EMenuAction::None;
};

UCLASS()
class SPINTOWIN_API ASpinHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** This frame's clickable regions, for the player controller to hit-test mouse clicks against. */
	const TArray<FSpinMenuRegion>& GetMenuRegions() const { return MenuRegions; }

private:
	void DrawCenteredText(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float Scale);

	/** Draws a clickable button: a background rect (brighter if hovered) plus centered label text(s),
	 *  and registers its region for click hit-testing. */
	void DrawButton(float X, float Y, float W, float H, EMenuAction Action, const FLinearColor& BgColor);

	bool IsHovered(float X, float Y, float W, float H) const;

	/** Draws the big, centered, "in your face" invincibility countdown: each whole second punches
	 *  in huge and quickly shrinks and fades, rather than lingering as a static number. */
	void DrawInvincibilityCountdown(float TimeRemaining, float CenterX, float CenterY);

	TArray<FSpinMenuRegion> MenuRegions;
	FVector2D MousePos = FVector2D::ZeroVector;
	bool bHaveMouse = false;
};
