// Spin To Win - the procedural neon city around the dial.
//
// Builds the whole city in code (in the editor too, so it can be seen while its settings are tuned)
// from instanced engine cubes and cylinders: a dark floor, ring roads and avenues radiating from the
// dial like a clock face, districts of towers that grow taller with distance, parks with neon trees
// and fountains, street lamps, an elevated maglev ring, and a few enormous landmarks on the horizon.
// Every glowing surface uses M_Neon, so the city reveal (see ASpinCityFX) lights it up from the dial
// outward. Place one at the origin; change Seed for a different city.
//
// Layout follows the dial: 0 degrees = +X = straight ahead of the camera (which sits on the -X side).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpinCityBuilder.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class ASpinGameMode;

/** How the districts repeat around the dial. (Roads, the plaza and the landmarks are always symmetric.) */
UENUM()
enum class ESpinCitySymmetry : uint8
{
	/** Every part of the city is different. */
	None,
	/** The left half of the view mirrors the right; each wedge between avenues is still different. */
	Mirror,
	/** One wedge, mirrored and repeated all the way round, like a kaleidoscope. */
	Kaleidoscope
};

UCLASS()
class SPINTOWIN_API ASpinCityBuilder : public AActor
{
	GENERATED_BODY()

public:
	ASpinCityBuilder();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Distance from the centre to the farthest structure (the reveal grows out to here). */
	float GetCityRadius() const;

	/** Rebuild the city now (also happens automatically whenever a setting changes). */
	UFUNCTION(CallInEditor, Category = "City")
	void Rebuild();

	// ---------------- Layout ----------------

	/** Change for a different city with the same overall shape. */
	UPROPERTY(EditAnywhere, Category = "City|Layout")
	int32 Seed = 1337;

	UPROPERTY(EditAnywhere, Category = "City|Layout")
	ESpinCitySymmetry Symmetry = ESpinCitySymmetry::None;

	/** Open plaza around the dial: only low details inside this radius. */
	UPROPERTY(EditAnywhere, Category = "City|Layout")
	float PlazaRadius = 2600.f;

	/** Nothing stands within this distance of the dial - no buildings, parks or flying-car lanes -
	 *  so the area around it stays open. (Only the flat floor markings and the river come closer.) */
	UPROPERTY(EditAnywhere, Category = "City|Layout")
	float DialClearRadius = 5000.f;

	/** Ordinary buildings stand out to here. */
	UPROPERTY(EditAnywhere, Category = "City|Layout")
	float CityRadius = 42000.f;

	/** The giant landmarks stand around this distance. */
	UPROPERTY(EditAnywhere, Category = "City|Layout")
	float LandmarkRadius = 52000.f;

	/** Avenues radiating from the dial (12 = one per hour of the clock). */
	UPROPERTY(EditAnywhere, Category = "City|Layout", meta = (ClampMin = "0", ClampMax = "48"))
	int32 NumAvenues = 12;

	UPROPERTY(EditAnywhere, Category = "City|Layout")
	float AvenueWidth = 900.f;

	/** Ring roads circle the dial at this spacing. */
	UPROPERTY(EditAnywhere, Category = "City|Layout")
	float RingRoadSpacing = 6000.f;

	UPROPERTY(EditAnywhere, Category = "City|Layout")
	float RingRoadWidth = 700.f;

	/** Size of one city block (one building or park per block at most). */
	UPROPERTY(EditAnywhere, Category = "City|Layout", meta = (ClampMin = "200"))
	float BlockSize = 1400.f;

	/** Chance a block has something on it. */
	UPROPERTY(EditAnywhere, Category = "City|Layout", meta = (ClampMin = "0", ClampMax = "1"))
	float BuildingDensity = 0.9f;

	/** Typical building height near the plaza and at the city's edge; heights rise along a curve between. */
	UPROPERTY(EditAnywhere, Category = "City|Layout")
	float MinHeight = 500.f;

	UPROPERTY(EditAnywhere, Category = "City|Layout")
	float MaxHeight = 11000.f;

	/** Above 1, the city stays low near the dial and rises sharply toward the edge, like a bowl. */
	UPROPERTY(EditAnywhere, Category = "City|Layout", meta = (ClampMin = "0.2"))
	float HeightCurve = 1.4f;

	/** The camera never looks behind itself, so beyond this radius only the part in front of it is built. */
	UPROPERTY(EditAnywhere, Category = "City|Layout")
	float FullCircleRadius = 7000.f;

	/** Between the camera and the dial buildings are kept low, so nothing blocks the view. Half-width of that lane. */
	UPROPERTY(EditAnywhere, Category = "City|Layout")
	float CameraLaneHalfWidth = 3000.f;

	/** How far the floor (and its grid) extends, so its edge is never in view. */
	UPROPERTY(EditAnywhere, Category = "City|Layout")
	float FloorRadius = 150000.f;

	// ---------------- Parks, streets and transit ----------------

	/** Chance an occupied block is a park (neon trees, sometimes a fountain) rather than a building. */
	UPROPERTY(EditAnywhere, Category = "City|Parks", meta = (ClampMin = "0", ClampMax = "1"))
	float ParkChance = 0.08f;

	/** Chance a park has a fountain in the middle (ringed with trees). */
	UPROPERTY(EditAnywhere, Category = "City|Parks", meta = (ClampMin = "0", ClampMax = "1"))
	float FountainChance = 0.45f;

	UPROPERTY(EditAnywhere, Category = "City|Parks")
	FLinearColor FoliageColor = FLinearColor(0.2f, 1.f, 0.45f);

	UPROPERTY(EditAnywhere, Category = "City|Parks")
	FLinearColor WaterColor = FLinearColor(0.35f, 0.85f, 1.f);

	/** Street lamps line both sides of every avenue, this far apart (0 = none), out to StreetLampReach. */
	UPROPERTY(EditAnywhere, Category = "City|Parks")
	float StreetLampSpacing = 0.f;

	UPROPERTY(EditAnywhere, Category = "City|Parks")
	float StreetLampReach = 26000.f;

	/** An elevated maglev ring on pillars, running above one of the ring roads (0 = none, 1 = the first). */
	UPROPERTY(EditAnywhere, Category = "City|Parks", meta = (ClampMin = "0"))
	int32 MaglevRingRoad = 2;

	UPROPERTY(EditAnywhere, Category = "City|Parks")
	float MaglevHeight = 1100.f;

	// ---------------- Look ----------------

	/** Neon colours the city picks from. The first two are used most near the dial, the rest farther out. */
	UPROPERTY(EditAnywhere, Category = "City|Look")
	TArray<FLinearColor> Palette = {
		FLinearColor(0.f, 1.f, 1.f),     // cyan
		FLinearColor(0.1f, 0.4f, 1.f),   // blue
		FLinearColor(1.f, 0.f, 0.85f),   // magenta
		FLinearColor(0.55f, 0.f, 1.f),   // purple
		FLinearColor(1.f, 0.45f, 0.f)    // amber
	};

	UPROPERTY(EditAnywhere, Category = "City|Look")
	FLinearColor HullColor = FLinearColor(0.006f, 0.008f, 0.016f);

	UPROPERTY(EditAnywhere, Category = "City|Look")
	FLinearColor FloorColor = FLinearColor(0.003f, 0.004f, 0.009f);

	UPROPERTY(EditAnywhere, Category = "City|Look")
	FLinearColor RingRoadColor = FLinearColor(0.1f, 0.5f, 1.f);

	UPROPERTY(EditAnywhere, Category = "City|Look")
	FLinearColor AvenueColor = FLinearColor(1.f, 0.f, 0.85f);

	UPROPERTY(EditAnywhere, Category = "City|Look")
	FLinearColor TrafficColor = FLinearColor(1.f, 0.55f, 0.1f);

	/** Glow of building neon, and of the floor lines (M_Neon's own default is 4). */
	UPROPERTY(EditAnywhere, Category = "City|Look")
	float NeonIntensity = 4.f;

	UPROPERTY(EditAnywhere, Category = "City|Look")
	float FloorLineIntensity = 2.5f;

	/** Width of neon lines at the front edge of the plaza (just past the dial, as seen from the camera). */
	UPROPERTY(EditAnywhere, Category = "City|Look")
	float LineWidth = 7.f;

	/** How lines widen with distance from the camera. At 1 they'd look equally thick near and far;
	 *  above 1, far lines look bolder than near ones, which reads as depth. */
	UPROPERTY(EditAnywhere, Category = "City|Look", meta = (ClampMin = "0"))
	float LineDepthExponent = 1.3f;

	/** Roughly how far behind the dial the camera sits (for judging distance from it). */
	UPROPERTY(EditAnywhere, Category = "City|Look")
	float CameraBackDistance = 4800.f;

	// ---------------- Life ----------------

	/** Red warning beacons on the tallest towers blink with this period (seconds). */
	UPROPERTY(EditAnywhere, Category = "City|Life")
	float BeaconBlinkPeriod = 1.6f;

	UPROPERTY(EditAnywhere, Category = "City|Life")
	FLinearColor BeaconColor = FLinearColor(1.f, 0.08f, 0.03f);

	/** Towers taller than this get a beacon. */
	UPROPERTY(EditAnywhere, Category = "City|Life")
	float BeaconMinHeight = 4000.f;

	/** On each successful hit the city's neon flares: brightness is multiplied by 1 + this at the peak. */
	UPROPERTY(EditAnywhere, Category = "City|Life")
	float HitPulseBoost = 1.5f;

	/** The floor lines brighten as the hand speeds up: multiplied by 1 + this at max speed. */
	UPROPERTY(EditAnywhere, Category = "City|Life")
	float FloorSpeedBoost = 1.5f;

	/** Lit windows switch on and off around the city: each batch of them waits between these many
	 *  seconds before deciding whether to switch. */
	UPROPERTY(EditAnywhere, Category = "City|Life")
	float WindowSwitchMinInterval = 1.5f;

	UPROPERTY(EditAnywhere, Category = "City|Life")
	float WindowSwitchMaxInterval = 7.f;

	/** Share of windows lit at any one time (roughly). */
	UPROPERTY(EditAnywhere, Category = "City|Life", meta = (ClampMin = "0", ClampMax = "1"))
	float WindowsOnShare = 0.7f;

	/** Chance a cyan or magenta sign is a faulty one that stutters now and then. */
	UPROPERTY(EditAnywhere, Category = "City|Life", meta = (ClampMin = "0", ClampMax = "1"))
	float SignFlickerChance = 0.2f;

	/** While the hand is jammed (the mashing phase), the city's lights dim to this share of their
	 *  brightness and flash random colours, until the jam is fixed. */
	UPROPERTY(EditAnywhere, Category = "City|Life", meta = (ClampMin = "0", ClampMax = "1"))
	float JamGlitchBrightness = 0.45f;

	/** Each group of lights holds a random colour for between these many seconds before changing. */
	UPROPERTY(EditAnywhere, Category = "City|Life")
	float JamGlitchMinInterval = 0.06f;

	UPROPERTY(EditAnywhere, Category = "City|Life")
	float JamGlitchMaxInterval = 0.25f;

	/** Seconds to fade into the glitch when a jam starts, and back out when it's fixed. */
	UPROPERTY(EditAnywhere, Category = "City|Life", meta = (ClampMin = "0.01"))
	float JamGlitchFadeTime = 0.25f;

	// ---------------- Wind ----------------

	/** Glowing specks and tumbling flakes carried through the air around the dial (0 = none). */
	UPROPERTY(EditAnywhere, Category = "City|Wind", meta = (ClampMin = "0", ClampMax = "3000"))
	int32 NumMotes = 500;

	/** Average wind speed (world units per second); it gusts above and eases below this. */
	UPROPERTY(EditAnywhere, Category = "City|Wind")
	float WindSpeed = 260.f;

	/** Which way the wind blows, flat across the city (X = away from the camera, Y = to the right). */
	UPROPERTY(EditAnywhere, Category = "City|Wind")
	FVector2D WindDirection = FVector2D(0.3f, 1.f);

	UPROPERTY(EditAnywhere, Category = "City|Wind")
	FLinearColor MoteColor = FLinearColor(0.75f, 0.85f, 1.f);

	UPROPERTY(EditAnywhere, Category = "City|Wind")
	float MoteIntensity = 2.5f;

	UPROPERTY(EditAnywhere, Category = "City|Wind")
	float MoteMinSize = 14.f;

	UPROPERTY(EditAnywhere, Category = "City|Wind")
	float MoteMaxSize = 34.f;

	// ---------------- Flying traffic ----------------

	/** Flying cars streaming along the avenues and circling above the ring roads (0 = none). */
	UPROPERTY(EditAnywhere, Category = "City|Traffic", meta = (ClampMin = "0", ClampMax = "1000"))
	int32 NumFlyingCars = 140;

	UPROPERTY(EditAnywhere, Category = "City|Traffic")
	float FlyingCarMinSpeed = 900.f;

	UPROPERTY(EditAnywhere, Category = "City|Traffic")
	float FlyingCarMaxSpeed = 1900.f;

	/** The air lanes are spread between these heights. */
	UPROPERTY(EditAnywhere, Category = "City|Traffic")
	float FlyingCarMinHeight = 1300.f;

	UPROPERTY(EditAnywhere, Category = "City|Traffic")
	float FlyingCarMaxHeight = 5500.f;

	UPROPERTY(EditAnywhere, Category = "City|Traffic")
	FLinearColor HeadlightColor = FLinearColor(1.f, 0.95f, 0.85f);

	UPROPERTY(EditAnywhere, Category = "City|Traffic")
	FLinearColor TaillightColor = FLinearColor(1.f, 0.06f, 0.04f);

	// ---------------- Depth ----------------

	/** Tower bodies fade from HullColor near the camera to this faint haze glow far away, so each
	 *  layer of buildings stands out as a slightly lighter silhouette against the ones in front. */
	UPROPERTY(EditAnywhere, Category = "City|Depth")
	FLinearColor DepthHazeColor = FLinearColor(0.05f, 0.02f, 0.085f);

	/** Distance from the camera where the haze starts, and where it's at full strength. */
	UPROPERTY(EditAnywhere, Category = "City|Depth")
	float DepthHazeStart = 5000.f;

	UPROPERTY(EditAnywhere, Category = "City|Depth")
	float DepthHazeEnd = 55000.f;

	/** Glow strength of the haze on tower bodies. */
	UPROPERTY(EditAnywhere, Category = "City|Depth")
	float DepthHazeIntensity = 5.f;

	// ---------------- River and bridges ----------------

	/** A river runs straight ahead from the plaza to the ziggurat, splitting the city in two, with a
	 *  single grand suspension bridge joining the two sides. */
	UPROPERTY(EditAnywhere, Category = "City|River")
	bool bRiver = true;

	UPROPERTY(EditAnywhere, Category = "City|River")
	float RiverWidth = 2600.f;

	/** How far out (from the dial) the grand bridge crosses the river. */
	UPROPERTY(EditAnywhere, Category = "City|River")
	float GrandBridgeDistance = 12000.f;

	UPROPERTY(EditAnywhere, Category = "City|River")
	float GrandBridgeTowerHeight = 4200.f;

	/** Distance between the bridge's two towers (they stand on land beyond the banks if this is
	 *  wider than the river). The deck runs on past each tower to the shore. */
	UPROPERTY(EditAnywhere, Category = "City|River")
	float GrandBridgeSpan = 7000.f;

	/** How far the main cables dip between the towers: 0 = taut from tower to tower, 1 = all the way
	 *  down to the deck at mid-span. */
	UPROPERTY(EditAnywhere, Category = "City|River", meta = (ClampMin = "0", ClampMax = "1"))
	float GrandBridgeCableSag = 0.45f;

	/** Height of the bridge's road deck above the water. */
	UPROPERTY(EditAnywhere, Category = "City|River")
	float BridgeDeckHeight = 900.f;

	/** Highways carry on from both ends of the bridge, straight out across the city. */
	UPROPERTY(EditAnywhere, Category = "City|River")
	bool bBridgeRoads = true;

	/** How far each highway runs, measured from the river's centre line. */
	UPROPERTY(EditAnywhere, Category = "City|River")
	float BridgeRoadReach = 70000.f;

	/** The highways' height above the ground (0 = at street level). If it differs from the bridge
	 *  deck, each one ramps from the deck down (or up) to it over BridgeRoadRampLength. */
	UPROPERTY(EditAnywhere, Category = "City|River", meta = (ClampMin = "0"))
	float BridgeRoadHeight = 900.f;

	UPROPERTY(EditAnywhere, Category = "City|River", meta = (ClampMin = "100"))
	float BridgeRoadRampLength = 4000.f;

	UPROPERTY(EditAnywhere, Category = "City|River", meta = (ClampMin = "100"))
	float BridgeRoadWidth = 700.f;

private:
	/** Neon colour slots: the five palette colours, then foliage and water. */
	static constexpr int32 NumPalette = 5;
	static constexpr int32 FoliageSlot = 5;
	static constexpr int32 WaterSlot = 6;
	static constexpr int32 NumSlots = 7;

	/** Windows come in this many independently switched batches per colour (blue, amber). */
	static constexpr int32 NumWindowGroups = 8;

	/** Tower bodies are split into this many distance bands, each a step further into the haze. */
	static constexpr int32 NumHullBands = 6;

	/** The box of air the motes drift through (they wrap around inside it). */
	static constexpr float MoteFieldMinX = -3500.f;
	static constexpr float MoteFieldMaxX = 10000.f;
	static constexpr float MoteFieldHalfY = 8000.f;
	static constexpr float MoteFieldMinZ = 80.f;
	static constexpr float MoteFieldMaxZ = 2800.f;

	void Build();
	void SetupMaterials();

	// Things that move or change while playing.
	void InitLife();
	void UpdateWindows(float DeltaSeconds, float Flare);
	void UpdateSignFlicker(float DeltaSeconds, float Flare);
	void UpdateMotes(float DeltaSeconds, float Time);
	void UpdateFlyingCars(float DeltaSeconds);

	/** The jam glitch: eases in or out with the jam, and while in, gives every light group a new
	 *  random colour now and then. Returns the brightness multiplier to apply this frame. */
	float UpdateJamGlitch(float DeltaSeconds);

	/** Which haze band something at this spot belongs to (0 = nearest the camera). */
	int32 HullBandAt(const FVector2D& Pos) const;

	/** A lit window on a wall facing along X (FaceYaw 0/180) or Y (90/-90), in one of the switched batches. */
	void AddWindow(FRandomStream& Rand, bool bAmber, const FVector& Center, const FVector& Size, float FaceYaw);

	// Building blocks. All positions are relative to the actor; Yaw is in degrees.
	void AddHull(const FVector& Center, const FVector& Size, float Yaw, bool bRound = false);
	void AddNeonBox(int32 Slot, const FVector& Center, const FVector& Size, float Yaw);
	void AddNeonDisc(int32 Slot, const FVector& Center, float Radius, float Thickness);
	void AddNeonLine(int32 Slot, const FVector& A, const FVector& B, float Width);
	void AddFloorLine(UInstancedStaticMeshComponent* ISM, const FVector2D& A, const FVector2D& B, float Width);
	/** A ring of floor line; each segment's width is WidthScale x the depth-scaled width at that spot. */
	void AddFloorRing(UInstancedStaticMeshComponent* ISM, float Radius, float WidthScale, bool bFullCircle);
	void AddBeacon(const FVector& At, float Size);

	/** Neon outline of a box: the four vertical edges and the top rim. */
	void OutlineBox(int32 Slot, const FVector& Base, const FVector& Size, float Yaw, float Width, bool bVerticals = true);

	// Districts and pieces.
	void BuildFloor();
	void BuildPlaza();
	void BuildDistricts(FRandomStream& Rand);
	void BuildBlock(FRandomStream& Rand, const FVector2D& Pos, float Yaw, float Footprint, float Height, float Dist);
	void BuildTower(FRandomStream& Rand, const FVector2D& Pos, float Yaw, float Footprint, float Height, float Dist);

	/** Street-level clutter on a tower whose main body is BodyX x BodyY x BodyHeight (axis-aligned):
	 *  neon signs and lit windows on the faces the camera sees, antennas and billboards on the roof. */
	void BuildFacade(FRandomStream& Rand, const FVector2D& Pos, float BodyX, float BodyY, float BodyHeight, float Width, bool bDetailed);

	/** A framed neon sign filled with glyph-like strokes, flat against a wall. Center is on the wall's
	 *  surface; FaceYaw points out of the wall (0 = +X). Long side vertical if Height > SignWidth. */
	void AddSign(FRandomStream& Rand, int32 Slot, const FVector& Center, float FaceYaw, float SignWidth, float SignHeight, float Width);
	void BuildPark(FRandomStream& Rand, const FVector2D& Pos, float Yaw, float Size);
	void BuildTree(const FVector2D& Pos, float Height, float Width);
	void BuildFountain(const FVector2D& Pos, float Radius, float Width, float Yaw);
	void BuildStreetLamps();
	void BuildMaglev();
	void BuildLandmarks(FRandomStream& Rand);
	void BuildRiver();
	void BuildSuspensionBridge(float X);

	/** The highways leading away from both ends of the bridge (whose deck ends DeckEnd from the
	 *  centre line, at height Deck). */
	void BuildBridgeRoads(float X, float DeckEnd, float Deck);

	/** How far either side of the river's centre line the bridge deck reaches. */
	float GetBridgeDeckEnd() const;

	/** True if something this far across (Footprint) at this spot would stand on a bridge highway. */
	bool IsOnBridgeRoad(const FVector2D& Pos, float Footprint) const;

	/** Far end of the river (it stops at the ziggurat's steps). */
	float GetRiverEnd() const;

	/** True if a footprint this big at this spot would stand in the river, on its banks, or on the
	 *  approach to one of its bridges. */
	bool IsInRiverZone(const FVector2D& Pos, float Footprint) const;
	void BuildArch(int32 Slot, const FVector& Center, const FVector& Across, float Radius, float Width);
	/** A horizontal ring of light. With bDepthScaled, Width is a multiplier on the depth-scaled width. */
	void BuildHaloRing(int32 Slot, const FVector& Center, float Radius, float Height, float Width, float StartDeg = 0.f, bool bDepthScaled = false);

	/** Should anything be built at this spot? (False behind the camera, beyond FullCircleRadius.) */
	bool IsInView(const FVector2D& Pos) const;

	/** Neon line width for something at this spot: thin near the camera, bolder far away. */
	float LineWidthAt(const FVector2D& Pos) const;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Floor;

	/** The river's surface (a dark mirror, M_Water). */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Water;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> WaterMaterial;

	/** Dark tower bodies, one component per haze band (see NeonBoxes for why these lists are Transient). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> HullBoxes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> HullCylinders;

	/** Flying cars: dark bodies, white headlights, red tail lights. */
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> CarBodies;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> CarHeadlights;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> CarTaillights;

	// Filled in the constructor and never saved: the components themselves are saved by name, and a
	// saved list would overwrite this one on load (and be the wrong length if NumSlots ever changes).
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> NeonBoxes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> NeonDiscs;

	/** Lit windows: NumWindowGroups blue batches, then NumWindowGroups amber ones (see NeonBoxes for why Transient). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> WindowGroups;

	/** The lettering of faulty signs: cyan, then magenta. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> FlickerSigns;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Motes;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> RingRoadLines;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> AvenueLines;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> TrafficLines;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Beacons;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> NeonMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> NeonMIDs;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> FloorLineMIDs;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BeaconMID;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> WindowMIDs;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> FlickerMIDs;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MoteMID;

	/** Each window batch: current brightness (0..1), where it's heading, and seconds until it may switch. */
	TArray<float> WindowLevel;
	TArray<float> WindowTarget;
	TArray<float> WindowTimer;

	/** Each faulty-sign batch: seconds until its next stutter, seconds left in it, and whether it's lit. */
	float FlickerWait[2] = { 0.f, 0.f };
	float FlickerBurst[2] = { 0.f, 0.f };
	float FlickerStepTimer[2] = { 0.f, 0.f };
	bool bFlickerLit[2] = { true, true };

	struct FMote
	{
		FVector Position = FVector::ZeroVector;
		float Phase = 0.f;
		float Size = 10.f;
		float Spin = 60.f;
		bool bFlake = false;
	};
	TArray<FMote> MoteState;
	TArray<FTransform> MoteTransforms;

	struct FFlyingCar
	{
		/** Avenue lanes run straight out along Angle; ring lanes circle at Radius. */
		bool bRing = false;
		float Angle = 0.f;      // radians: the avenue's direction, or the car's place round its ring
		float Radius = 0.f;     // ring lanes only
		float Distance = 0.f;   // avenue lanes only: how far out from the dial
		float Offset = 0.f;     // avenue lanes only: sideways from the avenue's centre line
		float Height = 0.f;
		float Speed = 0.f;      // world units per second, signed (negative = inbound / anticlockwise)
	};
	/** Every light group the jam glitch recolours, with its normal colour to return to. */
	struct FGlitchTarget
	{
		TWeakObjectPtr<UMaterialInstanceDynamic> MID;
		FLinearColor BaseColor = FLinearColor::White;
		FLinearColor GlitchColor = FLinearColor::White;
		float Timer = 0.f;
	};
	TArray<FGlitchTarget> GlitchTargets;
	/** 0 = normal, 1 = fully glitched (eased, see JamGlitchFadeTime). */
	float JamGlitch = 0.f;

	TArray<FFlyingCar> CarState;
	TArray<FTransform> CarBodyTransforms;
	TArray<FTransform> CarHeadTransforms;
	TArray<FTransform> CarTailTransforms;

	/** Per-component instance transforms, gathered during Build and handed over in one batch each. */
	TMap<UInstancedStaticMeshComponent*, TArray<FTransform>> Pending;

	TWeakObjectPtr<ASpinGameMode> GameMode;

	/** Distance to the farthest structure, measured while building (saved, so play sessions have it). */
	UPROPERTY()
	float BuiltRadius = 0.f;

	float CurrentFloorSpeed = 0.f;
};
