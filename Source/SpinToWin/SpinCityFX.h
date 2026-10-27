// Spin To Win - ambient city effects: a speed-reactive floor grid, chasing "data" pulses along
// circuit traces, and flickering motherboard-style node lights. Purely cosmetic, drives dynamic
// material instances on level-placed city dressing (the floor grid, circuit traces and nodes),
// found at BeginPlay by actor tag. Place one instance of this actor anywhere in the level.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpinCityFX.generated.h"

class UMaterialInstanceDynamic;
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

	// ---------------- Grid brightness (reacts to hand speed) ----------------

	/** Actor tag used to find the floor grid lines to drive. */
	UPROPERTY(EditDefaultsOnly, Category = "CityFX|Grid")
	FName GridTag = TEXT("CityGrid");

	/** Grid glow while the hand is at its starting speed. */
	UPROPERTY(EditDefaultsOnly, Category = "CityFX|Grid")
	float GridMinIntensity = 1.4f;

	/** Grid glow once the hand reaches max speed. The specified ceiling the grid ramps up to. */
	UPROPERTY(EditDefaultsOnly, Category = "CityFX|Grid")
	float GridMaxIntensity = 7.f;

	/** How quickly displayed grid brightness chases the target value, so speed-ups ramp smoothly. */
	UPROPERTY(EditDefaultsOnly, Category = "CityFX|Grid")
	float GridResponseSpeed = 1.5f;

	// ---------------- Circuit trace chase ----------------

	/** Actor tag used to find circuit-trace segments (small pulsing "data" lights). Each segment
	 *  also needs an order tag of the form "T<chain>_<index>" (e.g. "T0_007") so its chain and
	 *  position within that chain can be recovered. */
	UPROPERTY(EditDefaultsOnly, Category = "CityFX|Traces")
	FName TraceTag = TEXT("CityTrace");

	/** How many segments long each pulse's bright "head" is. */
	UPROPERTY(EditDefaultsOnly, Category = "CityFX|Traces")
	float ChaseWidth = 2.2f;

	/** Segments advanced per second. */
	UPROPERTY(EditDefaultsOnly, Category = "CityFX|Traces")
	float ChaseSpeed = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "CityFX|Traces")
	float ChaseDimIntensity = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "CityFX|Traces")
	float ChaseBrightIntensity = 9.f;

	// ---------------- Flickering nodes ----------------

	/** Actor tag used to find the small flickering "motherboard" node lights. */
	UPROPERTY(EditDefaultsOnly, Category = "CityFX|Nodes")
	FName NodeTag = TEXT("CityNode");

	UPROPERTY(EditDefaultsOnly, Category = "CityFX|Nodes")
	float NodeDimIntensity = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category = "CityFX|Nodes")
	float NodeBrightIntensity = 8.f;

	/** Minimum / maximum seconds between flickers for a given node. */
	UPROPERTY(EditDefaultsOnly, Category = "CityFX|Nodes")
	float NodeFlickerMinInterval = 1.2f;

	UPROPERTY(EditDefaultsOnly, Category = "CityFX|Nodes")
	float NodeFlickerMaxInterval = 4.5f;

	UPROPERTY(EditDefaultsOnly, Category = "CityFX|Nodes")
	float NodeFlashDuration = 0.18f;

private:
	void SetupGridMIDs();
	void SetupTraceChains();
	void SetupFlickerNodes();

	void UpdateGrid(float DeltaSeconds);
	void UpdateTraces();
	void UpdateFlickerNodes();

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
