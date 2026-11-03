// Spin To Win - ambient city effects: a speed-reactive floor grid, chasing "data" pulses along
// circuit traces, and flickering motherboard-style node lights. Purely cosmetic, drives dynamic
// material instances on level-placed city dressing (the floor grid, circuit traces and nodes),
// found at BeginPlay by actor tag. Place one instance of this actor anywhere in the level.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpinCityFX.generated.h"

class UMaterialInstanceDynamic;
class UMaterialParameterCollection;
class ASpinGameMode;

/** One circuit trace: an ordered chain of segments a pulse chases along. */
USTRUCT()
struct FSpinTraceChain
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Segments;

	UPROPERTY()
	float PhaseOffset = 0.f;
};

USTRUCT()
struct FSpinFlickerNode
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MID;

	UPROPERTY()
	float NextFlashTime = 0.f;

	UPROPERTY()
	float FlashEndTime = -1.f;
};

UCLASS()
class SPINTOWIN_API ASpinCityFX : public AActor
{
	GENERATED_BODY()

public:
	ASpinCityFX();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** How much of the city is lit right now, 0..1 (eased, as shown - not the target). The dial's
	 *  camera follows it, pulling back as the city lights up. */
	float GetRevealFraction() const;

	// ---------------- Grid brightness (reacts to hand speed) ----------------

	/** Actor tag used to find the floor grid lines to drive. */
	UPROPERTY(EditAnywhere, Category = "CityFX|Grid")
	FName GridTag = TEXT("CityGrid");

	/** Grid glow while the hand is at its starting speed. */
	UPROPERTY(EditAnywhere, Category = "CityFX|Grid")
	float GridMinIntensity = 1.4f;

	/** Grid glow once the hand reaches max speed. The specified ceiling the grid ramps up to. */
	UPROPERTY(EditAnywhere, Category = "CityFX|Grid")
	float GridMaxIntensity = 7.f;

	/** How quickly displayed grid brightness chases the target value, so speed-ups ramp smoothly. */
	UPROPERTY(EditAnywhere, Category = "CityFX|Grid")
	float GridResponseSpeed = 1.5f;

	// ---------------- Circuit trace chase ----------------

	/** Actor tag used to find circuit-trace segments (small pulsing "data" lights). Each segment
	 *  also needs an order tag of the form "T<chain>_<index>" (e.g. "T0_007") so its chain and
	 *  position within that chain can be recovered. */
	UPROPERTY(EditAnywhere, Category = "CityFX|Traces")
	FName TraceTag = TEXT("CityTrace");

	/** How many segments long each pulse's bright "head" is. */
	UPROPERTY(EditAnywhere, Category = "CityFX|Traces")
	float ChaseWidth = 2.2f;

	/** Segments advanced per second. */
	UPROPERTY(EditAnywhere, Category = "CityFX|Traces")
	float ChaseSpeed = 5.f;

	UPROPERTY(EditAnywhere, Category = "CityFX|Traces")
	float ChaseDimIntensity = 0.6f;

	UPROPERTY(EditAnywhere, Category = "CityFX|Traces")
	float ChaseBrightIntensity = 9.f;

	// ---------------- Flickering nodes ----------------

	/** Actor tag used to find the small flickering "motherboard" node lights. */
	UPROPERTY(EditAnywhere, Category = "CityFX|Nodes")
	FName NodeTag = TEXT("CityNode");

	UPROPERTY(EditAnywhere, Category = "CityFX|Nodes")
	float NodeDimIntensity = 0.4f;

	UPROPERTY(EditAnywhere, Category = "CityFX|Nodes")
	float NodeBrightIntensity = 8.f;

	/** Minimum / maximum seconds between flickers for a given node. */
	UPROPERTY(EditAnywhere, Category = "CityFX|Nodes")
	float NodeFlickerMinInterval = 1.2f;

	UPROPERTY(EditAnywhere, Category = "CityFX|Nodes")
	float NodeFlickerMaxInterval = 4.5f;

	UPROPERTY(EditAnywhere, Category = "CityFX|Nodes")
	float NodeFlashDuration = 0.18f;

	// ---------------- Hit pulse ----------------

	/** On every successful hit, the whole city flares up and fades back (see the game mode's
	 *  HitPulseDuration). Each value is how much is added on top of the normal brightness at the
	 *  peak of the pulse. */
	UPROPERTY(EditAnywhere, Category = "CityFX|HitPulse")
	float GridHitPulseBoost = 14.f;

	UPROPERTY(EditAnywhere, Category = "CityFX|HitPulse")
	float TraceHitPulseBoost = 10.f;

	UPROPERTY(EditAnywhere, Category = "CityFX|HitPulse")
	float NodeHitPulseBoost = 10.f;

	// ---------------- City reveal (the lights spread out from the dial as the run goes on) ----------------
	//
	// Drives MPC_CityReveal, which M_Neon reads: neon within the lit radius of the dial glows normally,
	// fades out across a soft edge, and beyond it stays almost off. Applies to every M_Neon surface,
	// so the city's lines light up partway along their length as the circle reaches them.

	UPROPERTY(EditAnywhere, Category = "CityFX|Reveal")
	bool bRevealEnabled = true;

	/** Fully lit radius (world units, from the dial's centre) at the very start of a run. Keep it
	 *  past the dial's rim (about 650) so the dial itself is always at full brightness. */
	UPROPERTY(EditAnywhere, Category = "CityFX|Reveal")
	float RevealStartRadius = 800.f;

	/** Endless level at which the whole city is lit. The radius grows a little with every hit on the way. */
	UPROPERTY(EditAnywhere, Category = "CityFX|Reveal", meta = (ClampMin = "2"))
	float RevealFullLevel = 20.f;

	/** Share of the city the tutorial lights up by its last level (0..1). */
	UPROPERTY(EditAnywhere, Category = "CityFX|Reveal", meta = (ClampMin = "0", ClampMax = "1"))
	float TutorialRevealShare = 0.2f;

	/** Width of the soft fade at the edge of the lit area, at the start of a run and once fully grown. */
	UPROPERTY(EditAnywhere, Category = "CityFX|Reveal")
	float RevealEdgeStart = 1200.f;

	UPROPERTY(EditAnywhere, Category = "CityFX|Reveal")
	float RevealEdgeFull = 3000.f;

	/** Brightness of the unlit city (0 = black, 1 = normal). */
	UPROPERTY(EditAnywhere, Category = "CityFX|Reveal", meta = (ClampMin = "0", ClampMax = "1"))
	float RevealFloor = 0.03f;

	/** How fast the lit edge spreads outward (world units per second) - slow, so it visibly creeps. */
	UPROPERTY(EditAnywhere, Category = "CityFX|Reveal")
	float RevealSpreadSpeed = 1100.f;

	/** How fast the city powers down when a new run starts (world units per second). */
	UPROPERTY(EditAnywhere, Category = "CityFX|Reveal")
	float RevealCollapseSpeed = 25000.f;

	/** Extra glow on the band of neon right at the spreading edge, while it's still moving. */
	UPROPERTY(EditAnywhere, Category = "CityFX|Reveal")
	float RevealFrontBoost = 1.5f;

	/** Light the whole city behind the menus (it powers down when a run starts). */
	UPROPERTY(EditAnywhere, Category = "CityFX|Reveal")
	bool bFullCityOnMenus = true;

private:
	void SetupGridMIDs();
	void SetupTraceChains();
	void SetupFlickerNodes();

	void UpdateGrid(float DeltaSeconds);
	void UpdateTraces();
	void UpdateFlickerNodes();
	void UpdateReveal(float DeltaSeconds);

	/** 0..1 hit pulse from the game mode (0 if there isn't one). */
	float GetHitPulse() const;

	/** How far into the reveal the current run is: 0 = just started, 1 = whole city lit.
	 *  Negative when no run is going (on the menus). */
	float GetRevealProgress() const;

	/** Distance from the reveal centre to the farthest piece of the city, found at BeginPlay. */
	void MeasureCityExtent();

	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> RevealCollection;

	float CityExtent = 20000.f;
	/** The fully lit radius as currently shown (chases the target, see RevealSpreadSpeed). */
	float CurrentRevealRadius = -1.f;

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	TWeakObjectPtr<ASpinGameMode> GameMode;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> GridMIDs;

	UPROPERTY()
	TArray<FSpinTraceChain> TraceChains;

	UPROPERTY()
	TArray<FSpinFlickerNode> FlickerNodes;

	float CurrentGridIntensity = 0.f;
};
