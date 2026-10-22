// Spin To Win - draws the clock face, hand, target zone and text.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SpinHUD.generated.h"

UCLASS()
class SPINTOWIN_API ASpinHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawCenteredText(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float Scale);
};
