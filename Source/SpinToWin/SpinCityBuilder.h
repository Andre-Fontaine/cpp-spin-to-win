// Spin To Win - the procedural neon city around the dial.
//
// Builds the whole city in code (in the editor too, so it can be seen while its settings are tuned)
// from instanced engine cubes and cylinders: a dark floor, ring roads and avenues radiating from the
// dial like a clock face, districts of towers that grow taller with distance, open water around the dial
// feeding a river under a suspension bridge, and enormous landmarks on the horizon.
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

	/** Nothing stands within this distance of the dial - no buildings or flying-car lanes -
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

	/** Size of one city block (one building per block at most). */
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

	// ---------------- Streets and transit ----------------

	/** Street lamps line both sides of every avenue, this far apart (0 = none), out to StreetLampReach. */
	UPROPERTY(EditAnywhere, Category = "City|Transit")
	float StreetLampSpacing = 0.f;

	UPROPERTY(EditAnywhere, Category = "City|Transit")
	float StreetLampReach = 26000.f;

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

	// ---------------- Ripples ----------------

	/** Neon ripple rings spreading across the lake and river, like rain on a lit-up surface. They
	 *  come more often as the hand speeds up, and every hit sends a shockwave out from the dial. They
	 *  use the neon material, so they only show on water the city's light has reached. */
	UPROPERTY(EditAnywhere, Category = "City|Ripples")
	bool bRipples = true;

	/** Ripples started per second at the starting speed, and at max speed. */
	UPROPERTY(EditAnywhere, Category = "City|Ripples", meta = (ClampMin = "0"))
	float RippleRateMin = 3.f;

	UPROPERTY(EditAnywhere, Category = "City|Ripples", meta = (ClampMin = "0"))
	float RippleRateMax = 8.f;

	/** Seconds a ripple takes to spread out and fade away. */
	UPROPERTY(EditAnywhere, Category = "City|Ripples", meta = (ClampMin = "0.2"))
	float RippleLifetime = 2.6f;

	/** How far a ripple spreads before it's gone (world units), before scaling up with distance so
	 *  far-off ones still read from the camera. */
	UPROPERTY(EditAnywhere, Category = "City|Ripples", meta = (ClampMin = "10"))
	float RippleSize = 420.f;

	/** Brightness of a ripple as it starts (it fades from there). */
	UPROPERTY(EditAnywhere, Category = "City|Ripples", meta = (ClampMin = "0"))
	float RippleIntensity = 3.5f;

	/** Ring thickness, as a multiple of the city's neon line width at that spot. */
	UPROPERTY(EditAnywhere, Category = "City|Ripples", meta = (ClampMin = "0.1"))
	float RippleWidthScale = 1.6f;

	/** Ripples start no nearer the dial than this, and no farther out than this (or the light's edge). */
	UPROPERTY(EditAnywhere, Category = "City|Ripples", meta = (ClampMin = "0"))
	float RippleMinDistance = 1100.f;

	UPROPERTY(EditAnywhere, Category = "City|Ripples", meta = (ClampMin = "0"))
	float RippleMaxDistance = 16000.f;

	/** The shockwave each hit sends across the water: how far it spreads (as a multiple of the dial's
	 *  radius), how long it takes, and how bright it starts. Gold for a PERFECT, cyan for GOOD, purple for OK. */
	UPROPERTY(EditAnywhere, Category = "City|Ripples", meta = (ClampMin = "1"))
	float HitRippleReach = 5.f;

	UPROPERTY(EditAnywhere, Category = "City|Ripples", meta = (ClampMin = "0.2"))
	float HitRippleTime = 1.6f;

	UPROPERTY(EditAnywhere, Category = "City|Ripples", meta = (ClampMin = "0"))
	float HitRippleIntensity = 7.f;

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

	// ---------------- Night sky ----------------

	/** A cloudy night sky over the city (M_NightSky): a purple city glow at the horizon, drifting
	 *  clouds underlit by the neon, and a big moon with a thin holographic ring. */
	UPROPERTY(EditAnywhere, Category = "City|Sky")
	bool bNightSky = true;

	/** Where the moon is: degrees round from straight ahead (positive = to the right), and above the horizon. */
	UPROPERTY(EditAnywhere, Category = "City|Sky")
	float MoonAzimuth = 20.f;

	UPROPERTY(EditAnywhere, Category = "City|Sky")
	float MoonElevation = 12.f;

	/** The moon's radius, in degrees of sky. */
	UPROPERTY(EditAnywhere, Category = "City|Sky", meta = (ClampMin = "0.2", ClampMax = "30"))
	float MoonSize = 4.9f;

	UPROPERTY(EditAnywhere, Category = "City|Sky")
	FLinearColor MoonColor = FLinearColor(2.6f, 2.4f, 2.9f);

	/** The thin holographic ring around the moon (black = none). */
	UPROPERTY(EditAnywhere, Category = "City|Sky")
	FLinearColor MoonRingColor = FLinearColor(0.2f, 1.2f, 1.4f);

	/** How much of the sky is cloud (0 = overcast, 1 = clear). */
	UPROPERTY(EditAnywhere, Category = "City|Sky", meta = (ClampMin = "0", ClampMax = "1"))
	float CloudCover = 0.45f;

	UPROPERTY(EditAnywhere, Category = "City|Sky", meta = (ClampMin = "0", ClampMax = "1"))
	float CloudOpacity = 0.9f;

	/** How fast the clouds drift (1 = a slow crawl). */
	UPROPERTY(EditAnywhere, Category = "City|Sky")
	float CloudSpeed = 1.f;

	/** Clouds near the horizon glow this colour, lit from below by the city. */
	UPROPERTY(EditAnywhere, Category = "City|Sky")
	FLinearColor CloudGlowColor = FLinearColor(0.4f, 0.09f, 0.5f);

	UPROPERTY(EditAnywhere, Category = "City|Sky")
	float SkyBrightness = 1.f;

	// ---------------- Rolling fog ----------------

	/** Soft banks of fog drifting between the buildings: a stack of wide translucent layers (M_Fog)
	 *  whose patchy clouds roll along with the wind. They fade out near the camera and around the
	 *  dial, and glow brighter as the city lights up. */
	UPROPERTY(EditAnywhere, Category = "City|Fog")
	bool bFogLayers = true;

	UPROPERTY(EditAnywhere, Category = "City|Fog", meta = (ClampMin = "0", ClampMax = "8"))
	int32 NumFogLayers = 5;

	/** Heights of the lowest and highest layers (the rest are spread between, closer together low down). */
	UPROPERTY(EditAnywhere, Category = "City|Fog")
	float FogLowest = 180.f;

	UPROPERTY(EditAnywhere, Category = "City|Fog")
	float FogHighest = 2600.f;

	UPROPERTY(EditAnywhere, Category = "City|Fog")
	FLinearColor FogColor = FLinearColor(0.3f, 0.1f, 0.5f);

	UPROPERTY(EditAnywhere, Category = "City|Fog")
	float FogBrightness = 3.f;

	/** How thick the lowest layer is (0..1); higher layers are thinner, so the fog pools near the ground. */
	UPROPERTY(EditAnywhere, Category = "City|Fog", meta = (ClampMin = "0", ClampMax = "1"))
	float FogOpacity = 0.8f;

	/** How patchy the fog is: higher leaves bigger gaps between the banks (0 = an even blanket). */
	UPROPERTY(EditAnywhere, Category = "City|Fog", meta = (ClampMin = "0", ClampMax = "0.95"))
	float FogPatchiness = 0.3f;

	/** Size of the fog banks: smaller numbers make bigger, slower-looking clouds. */
	UPROPERTY(EditAnywhere, Category = "City|Fog")
	float FogNoiseScale = 0.00018f;

	/** How fast the fog drifts, as a share of WindSpeed (it rolls along in WindDirection). */
	UPROPERTY(EditAnywhere, Category = "City|Fog")
	float FogDriftShare = 0.7f;

	// ---------------- Lake ----------------

	/** The dial sits out on open water - the same dark mirror as the river - that covers the whole near
	 *  side of the level, edge to edge, up to a straight shoreline just in front of the bridge. The city
	 *  stands on the far shore, and the river flows out through the shoreline and under the bridge. */
	UPROPERTY(EditAnywhere, Category = "City|Lake")
	bool bLake = true;

	/** How far in front of the bridge the shoreline runs (the river crosses this stretch of land before
	 *  passing under the bridge). */
	UPROPERTY(EditAnywhere, Category = "City|Lake", meta = (ClampMin = "0"))
	float LakeEndsBeforeBridge = 900.f;

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
	/** Neon colour slots: one per palette colour. */
	static constexpr int32 NumPalette = 5;
	static constexpr int32 NumSlots = NumPalette;

	/** Windows come in this many independently switched batches per colour (blue, amber). */
	static constexpr int32 NumWindowGroups = 8;

	/** Tower bodies are split into this many distance bands, each a step further into the haze. */
	static constexpr int32 NumHullBands = 6;

	/** Fog layer components made up front (NumFogLayers of them are used). */
	static constexpr int32 MaxFogLayers = 8;

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

	/** The water ripples (see bRipples). */
	static constexpr int32 MaxRipples = 40;
	static constexpr int32 NumRippleSegments = 64;
	void InitRipples();
	void UpdateRipples(float DeltaSeconds, float Flare);
	/** Starts a ripple in a free slot (or the oldest one). Delay holds it back that many seconds. */
	void StartRipple(const FVector2D& Center, float StartRadius, float EndRadius, float Duration, float Width,
		const FLinearColor& Color, float Intensity, float Delay = 0.f);
	/** True if this spot is open water (the lake or the river). */
	bool IsOnWater(const FVector2D& Pos) const;
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
	void BuildStreetLamps();
	void BuildLandmarks(FRandomStream& Rand);
	void BuildFog();
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

	void BuildLake();

	/** Where the shoreline runs across the level (X; 0 when there's no lake). */
	float GetShoreX() const;

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

	/** The open water around the dial, edge to edge up to the shoreline (a flat slab of the same water). */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> LakeWater;

	/** Height floor lines are drawn at: just above the floor, or above the lake's surface for the
	 *  clock-face markings around the dial (so they float on the water). */
	float FloorLineZ = 1.f;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> WaterMaterial;

	/** The rolling fog: flat layers stacked over the city (see NeonBoxes for why the list is Transient). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> FogLayers;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> FogMaterial;

	/** A huge sphere around everything, wearing the night sky. */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> SkyDome;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> SkyMaterial;

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

	/** One ISM of ring segments per ripple slot, each with its own material so it can fade alone. */
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> RippleRings;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> RippleMIDs;

	struct FRipple
	{
		bool bActive = false;
		/** Seconds since it started (negative while it's held back by a delay). */
		float Age = 0.f;
		float Duration = 1.f;
		FVector2D Center = FVector2D::ZeroVector;
		float StartRadius = 0.f;
		float EndRadius = 100.f;
		float Width = 10.f;
		float Intensity = 1.f;
		float Seed = 0.f;
	};
	TArray<FRipple> RippleState;
	TArray<FTransform> RippleTransforms;
	float RippleOwed = 0.f;
	float LastRippleHitTime = -1000.f;
	TWeakObjectPtr<class ASpinCityFX> CityFX;

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
