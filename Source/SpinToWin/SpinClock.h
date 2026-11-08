// Spin To Win - the physical dial in the world: dial, hand, target zone, obstacles, camera and screen shake.
//
// The game mode owns the rules and the numbers (hand angle, zone, state). This actor
// presents them as 3D meshes, and owns the physical side: lifting the hand and obstacles.
//
// World layout: the dial lies flat in the XY plane, Z is up.
//   0 degrees = +X = "12 o'clock" (the top of the screen), 90 degrees = +Y = screen right.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpinClock.generated.h"

class UAudioComponent;
class UCameraComponent;
class USoundBase;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMesh;
class USceneComponent;
class UStaticMeshComponent;
class ASpinGameMode;
class ASpinCityFX;

/** Plain (non-reflected) bookkeeping for one obstacle slot. */
enum class EObstaclePhase : uint8
{
	Inactive,
	FadingIn,
	Waiting,
	FadingOut
};

struct FObstacleState
{
	EObstaclePhase Phase = EObstaclePhase::Inactive;
	float Elapsed = 0.f;

	/** Where it stands on the dial plane, relative to the clock actor. */
	FVector2D Current = FVector2D::ZeroVector;

	/** How strongly it's glitching in or out (0 once settled), and the seed for this jump of the glitch. */
	float Glitch = 0.f;
	int32 GlitchSeed = 0;

	/** Degrees the hand still has to travel to reach this obstacle (negative once it is beyond it). */
	float RemainingDeg = 0.f;

	/** Once RemainingDeg drops below minus this, the hand has fully cleared the obstacle. */
	float PassMarginDeg = 0.f;

	/** True once the hand has fully passed this obstacle. */
	bool bPassed = false;

	/** A high post: it floats above the dial, so the hand passes under it - unless it's lifted. */
	bool bHigh = false;

	/** The second (high) post of a Twin Posts pair: passing it means the pair was cleared. */
	bool bTwin = false;

	/** Ghost Posts: still too far from the hand to be seen. (It's there all the same.) */
	bool bGhostHidden = false;
};

/** An obstacle glitching in or out, for the HUD to draw its pixel blocks over. */
struct FSpinObstacleGlitch
{
	FVector WorldCenter = FVector::ZeroVector;
	float WorldRadius = 0.f;
	float Strength = 0.f;
	int32 Seed = 0;
};

/** One step of the dial growing as the camera pulls back (see ASpinClock::DialGrowthSteps). */
USTRUCT(BlueprintType)
struct FSpinDialGrowthStep
{
	GENERATED_BODY()

	FSpinDialGrowthStep() = default;
	FSpinDialGrowthStep(float InCameraBlend, float InScale) : CameraBlend(InCameraBlend), Scale(InScale) {}

	/** How far the camera has moved from the close-up to the wide shot: 0 to 1. */
	UPROPERTY(EditAnywhere, Category = "Clock")
	float CameraBlend = 0.f;

	/** The dial's size from there on, as a multiple of its normal size. */
	UPROPERTY(EditAnywhere, Category = "Clock")
	float Scale = 1.f;
};

UCLASS()
class SPINTOWIN_API ASpinClock : public AActor
{
	GENERATED_BODY()

public:
	ASpinClock();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Shake the camera. Intensity is 0..1 (1 = biggest). Overlapping shakes keep the stronger one. */
	void PlayShake(float Intensity);

	/** World-space point on the dial at the target zone's angle. RadiusFraction 0 = centre, 1 = rim. */
	FVector GetZoneWorldLocation(float RadiusFraction) const;

	/** World-space point on the dial at any angle (degrees, 0 = 12 o'clock, clockwise). */
	FVector GetDialWorldLocation(float Angle, float RadiusFraction) const;

	// ---------------- Lift ----------------

	/** Tell the clock the lift button was pressed or released. bValidPress says lifting is allowed
	 *  right now (playing, not jammed). Only valid presses raise the hand, and it drops by itself
	 *  after MaxLiftTravelDeg of hand travel (the button must be pressed again to lift again). */
	void SetLiftHeld(bool bHeld, bool bValidPress);

	/** True while the lift is in effect: held from a valid press and not yet timed out. */
	bool IsLiftActive() const;

	/** True while a (low) obstacle is out and the hand hasn't passed it yet, i.e. there's something to lift over. */
	bool IsLiftAvailable() const;

	/** True when the hand is lifted clearly off the dial (it turns cyan). Hitting zones works at any height. */
	bool IsHandRaised() const;

	/** 1 as the dial starts glitching to a new size, falling to 0 once it has locked in. For the
	 *  HUD's pixel blocks, along with a seed that changes with each jump of the glitch. */
	float GetDialGlitch() const { return DialGlitch; }
	int32 GetDialGlitchSeed() const { return DialGlitchSeed; }

	// ---------------- Obstacles ----------------

	/** Glitch an obstacle in at this angle / radius on the dial. A high one floats above the dial
	 *  instead of standing on it (see FObstacleState::bHigh). */
	void SpawnObstacle(float AngleDeg, float RadiusFraction, float AheadDeg, bool bHigh = false, bool bTwin = false);
	// AheadDeg = how many degrees ahead of the hand (in its direction of travel) the obstacle is.

	/** Glitch every current obstacle back out. */
	void RetireObstacles();

	/** The obstacles glitching in or out right now, for the HUD. */
	void GetObstacleGlitches(TArray<FSpinObstacleGlitch>& OutGlitches) const;

	/** True if the hand, at the given angle and its current height, is touching an obstacle: a low
	 *  one while not lifted clear of it, or a high one while lifted at all. bOutHigh says which. */
	bool IsHandHittingObstacle(float HandAngleDeg, bool* bOutHigh = nullptr) const;

	// ---------------- Jam ----------------

	/** Jam warning: the gears in the hub pillar catch, throwing bursts of sparks and making it glow
	 *  hot, and the hand flickers hot orange. */
	void SetSparking(bool bSparking);

	/** A jammed hand tilts and turns orange-brown ("broken") while it keeps spinning. */
	void SetJammed(bool bJammed);

	// ---------------- Music ----------------

	/** Dips the music while the pause menu is up (it keeps playing, unlike the paused world). */
	void SetMusicDucked(bool bDucked);

	/** Starts a different random track from RunMusic. */
	void StartNextRunTrack();

	/** Plays behind the menus. Any empty slot just stays silent. Turn on Looping on the sound itself. */
	UPROPERTY(EditAnywhere, Category = "Clock|Music")
	TObjectPtr<USoundBase> MenuMusic;

	/** Played during a run, tutorial included: a random one each run, never the same one twice in a
	 *  row. Leave Looping off on these, so a long run moves on to another when one ends. */
	UPROPERTY(EditAnywhere, Category = "Clock|Music")
	TArray<TObjectPtr<USoundBase>> RunMusic;

	/** Plays once when a run ends, as the run music fades out. No music follows it until the player
	 *  leaves the game-over screen. */
	UPROPERTY(EditAnywhere, Category = "Clock|Music")
	TObjectPtr<USoundBase> GameOverSting;

	/** Seconds to crossfade between the menu and run music. */
	UPROPERTY(EditAnywhere, Category = "Clock|Music", meta = (ClampMin = "0"))
	float MusicFadeTime = 1.5f;

	/** Music volume (0..1) while the pause menu is up. */
	UPROPERTY(EditAnywhere, Category = "Clock|Music", meta = (ClampMin = "0", ClampMax = "1"))
	float MusicPauseVolume = 0.4f;

	// ---------------- Layout ----------------

	/** Radius of the dial in world units. Everything else scales from this. */
	UPROPERTY(EditAnywhere, Category = "Clock|Layout")
	float DialRadius = 600.f;

	UPROPERTY(EditAnywhere, Category = "Clock|Layout")
	float DialThickness = 20.f;

	/** The dial steps up in size as the camera pulls back, so it stays readable from the wide shot,
	 *  spreading out over the rings on the city floor. It shrinks again as the camera swoops back in.
	 *  Purely visual: hit detection doesn't change. */
	UPROPERTY(EditAnywhere, Category = "Clock|Growth")
	TArray<FSpinDialGrowthStep> DialGrowthSteps;

	/** Seconds the dial glitches for on its way to a bigger size, and how hard it shakes the camera
	 *  (0..1) as it locks in during a run. */
	UPROPERTY(EditAnywhere, Category = "Clock|Growth", meta = (ClampMin = "0.01"))
	float DialGrowTime = 0.8f;

	UPROPERTY(EditAnywhere, Category = "Clock|Growth", meta = (ClampMin = "0", ClampMax = "1"))
	float DialGrowShake = 0.2f;

	/** Seconds to glitch back down when the camera returns. */
	UPROPERTY(EditAnywhere, Category = "Clock|Growth", meta = (ClampMin = "0.01"))
	float DialShrinkTime = 0.5f;

	/** How many times a second the glitch jumps to a new size, position and colour. */
	UPROPERTY(EditAnywhere, Category = "Clock|Growth", meta = (ClampMin = "1"))
	float DialGlitchRate = 20.f;

	/** How far the dial jumps sideways at the start of a glitch (world units), settling to nothing. */
	UPROPERTY(EditAnywhere, Category = "Clock|Growth", meta = (ClampMin = "0"))
	float DialGlitchJitter = 30.f;

	// ---------------- Game over collapse ----------------

	/** Seconds the dial takes to break down when the run ends: it glitches, spins off its track and
	 *  collapses to nothing. It glitches back in when the game-over screen is left. */
	UPROPERTY(EditAnywhere, Category = "Clock|GameOver", meta = (ClampMin = "0.1"))
	float CollapseTime = 1.6f;

	/** How fast the dial is whirling (degrees a second) by the time it vanishes. It speeds up from nothing. */
	UPROPERTY(EditAnywhere, Category = "Clock|GameOver", meta = (ClampMin = "0"))
	float CollapseSpinSpeed = 1080.f;

	/** How far (degrees) the dial tips over as it wobbles like a dropped coin. */
	UPROPERTY(EditAnywhere, Category = "Clock|GameOver", meta = (ClampMin = "0", ClampMax = "80"))
	float CollapseTilt = 28.f;

	/** How far it skids off its spot before vanishing, as a fraction of the dial radius. */
	UPROPERTY(EditAnywhere, Category = "Clock|GameOver", meta = (ClampMin = "0"))
	float CollapseDrift = 0.35f;

	/** Fraction of CollapseTime (0..1) at which it starts shrinking; before that it only glitches and spins. */
	UPROPERTY(EditAnywhere, Category = "Clock|GameOver", meta = (ClampMin = "0", ClampMax = "0.95"))
	float CollapseShrinkStart = 0.45f;

	/** Hand length as a fraction of the dial radius. */
	UPROPERTY(EditAnywhere, Category = "Clock|Layout")
	float HandLengthFraction = 0.9f;

	UPROPERTY(EditAnywhere, Category = "Clock|Layout")
	float HandWidth = 28.f;

	UPROPERTY(EditAnywhere, Category = "Clock|Layout")
	float HandHeight = 12.f;

	/** Where the target zone sits, as a fraction of the dial radius (centre of the band). */
	UPROPERTY(EditAnywhere, Category = "Clock|Layout")
	float ZoneRadiusFraction = 0.78f;

	/** Thickness of the target zone band as a fraction of the dial radius. */
	UPROPERTY(EditAnywhere, Category = "Clock|Layout")
	float ZoneThicknessFraction = 0.2f;

	/** The neon grade lines across the zone: width in world units, and the glow colour of the
	 *  PERFECT and GOOD boundaries (values above 1 bloom). */
	UPROPERTY(EditAnywhere, Category = "Clock|Layout")
	float ZoneMarkWidth = 3.f;

	UPROPERTY(EditAnywhere, Category = "Clock|Layout")
	FLinearColor PerfectMarkColor = FLinearColor(4.f, 2.8f, 0.5f);

	UPROPERTY(EditAnywhere, Category = "Clock|Layout")
	FLinearColor GoodMarkColor = FLinearColor(2.f, 2.f, 2.2f);

	// ---------------- Look (matches the city: dark hulls, neon accents) ----------------

	/** Dial face: the city's navy hull colour. */
	UPROPERTY(EditAnywhere, Category = "Clock|Look")
	FLinearColor DialColor = FLinearColor(0.008f, 0.012f, 0.022f);

	/** Rim under the dial: a touch lighter than the face so the edge still reads. */
	UPROPERTY(EditAnywhere, Category = "Clock|Look")
	FLinearColor RimColor = FLinearColor(0.025f, 0.03f, 0.045f);

	/** The hand's neon (edges, spine, pointed tip, tail) glows in its state colour multiplied by this. */
	UPROPERTY(EditAnywhere, Category = "Clock|Look")
	float HandGlow = 1.6f;

	/** The hand's body under the neon: dark, like the city's buildings. */
	UPROPERTY(EditAnywhere, Category = "Clock|Look")
	FLinearColor HandBodyColor = FLinearColor(0.01f, 0.012f, 0.022f);

	/** Width of the hand's neon lines. */
	UPROPERTY(EditAnywhere, Category = "Clock|Look")
	float HandLineWidth = 5.f;

	/** Glow of the target zone band normally, and while the hand is inside it. */
	UPROPERTY(EditAnywhere, Category = "Clock|Look")
	float ZoneGlow = 0.8f;

	UPROPERTY(EditAnywhere, Category = "Clock|Look")
	float ZoneActiveGlow = 1.6f;

	/** Hub pillar glow at rest, and at the peak of a gear catch while sparking. */
	UPROPERTY(EditAnywhere, Category = "Clock|Look")
	FLinearColor HubIdleColor = FLinearColor(0.35f, 0.5f, 0.7f);

	UPROPERTY(EditAnywhere, Category = "Clock|Look")
	FLinearColor HubHotColor = FLinearColor(6.f, 2.2f, 0.3f);

	/** Clock-face ticks just inside the outer ring (every fifth one is a longer "hour" tick). */
	UPROPERTY(EditAnywhere, Category = "Clock|Look")
	float TickRadiusFraction = 0.935f;

	UPROPERTY(EditAnywhere, Category = "Clock|Look")
	FLinearColor TickColor = FLinearColor(0.12f, 0.45f, 0.9f);

	/** Faint guide lines along both edges of the band the target zone sits in. */
	UPROPERTY(EditAnywhere, Category = "Clock|Look")
	FLinearColor TrackColor = FLinearColor(0.04f, 0.12f, 0.32f);

	// ---------------- Neon ring highlights ----------------

	/** Outer ring sits just inside the dial's edge and always shows the currently-needed button's
	 *  colour (same colour as the target zone), so the whole rim reads as a glowing accent. */
	UPROPERTY(EditAnywhere, Category = "Clock|Neon")
	float OuterRingRadiusFraction = 0.99f;

	/** Inner ring frames the hub with a fixed accent colour. */
	UPROPERTY(EditAnywhere, Category = "Clock|Neon")
	float InnerRingRadiusFraction = 0.16f;

	/** Thickness of both rings, as a fraction of the dial radius. Kept small for a thin glowing line. */
	UPROPERTY(EditAnywhere, Category = "Clock|Neon")
	float RingThicknessFraction = 0.012f;

	UPROPERTY(EditAnywhere, Category = "Clock|Neon")
	FLinearColor InnerRingColor = FLinearColor(0.1f, 0.9f, 1.f);

	/** At the peak of a successful hit's pulse, both rings' glow is multiplied by 1 + this. */
	UPROPERTY(EditAnywhere, Category = "Clock|Neon")
	float RingHitPulseBoost = 6.f;

	/** At the peak of a successful hit's pulse, the fill light is multiplied by 1 + this. */
	UPROPERTY(EditAnywhere, Category = "Clock|Neon")
	float FillLightHitPulseBoost = 1.5f;

	/** During invincibility the dial's glow (zone, grade lines, rings) and the fill light dim to
	 *  this fraction and gently breathe, so it's obvious nothing counts yet. The hand stays bright. */
	UPROPERTY(EditAnywhere, Category = "Clock|Neon", meta = (ClampMin = "0", ClampMax = "1"))
	float InvincibleBrightness = 0.4f;

	/** Seconds to dim down when invincibility starts, and to brighten back up when it ends. */
	UPROPERTY(EditAnywhere, Category = "Clock|Neon", meta = (ClampMin = "0.01"))
	float InvincibleFadeTime = 0.3f;

	// ---------------- Lift tuning ----------------

	/** How high the hand rises while the lift button is held (world units). Must be above ObstacleHeight. */
	UPROPERTY(EditAnywhere, Category = "Clock|Lift")
	float MaxHandLift = 150.f;

	/** The hand moves on a spring, not at a fixed speed. Stiffness sets how snappy it is; damping
	 *  how much it overshoots and wobbles before settling (1 = none, lower = more). Rising: */
	UPROPERTY(EditAnywhere, Category = "Clock|Lift", meta = (ClampMin = "10"))
	float LiftRiseStiffness = 650.f;

	UPROPERTY(EditAnywhere, Category = "Clock|Lift", meta = (ClampMin = "0.05", ClampMax = "2"))
	float LiftRiseDamping = 0.55f;

	/** ...and dropping back down. */
	UPROPERTY(EditAnywhere, Category = "Clock|Lift", meta = (ClampMin = "10"))
	float LiftDropStiffness = 420.f;

	UPROPERTY(EditAnywhere, Category = "Clock|Lift", meta = (ClampMin = "0.05", ClampMax = "2"))
	float LiftDropDamping = 0.45f;

	/** When the hand lands on the dial it bounces back up with this share of its speed. */
	UPROPERTY(EditAnywhere, Category = "Clock|Lift", meta = (ClampMin = "0", ClampMax = "0.9"))
	float LiftLandingBounce = 0.3f;

	/** The hand tips as it moves (its tip lagging behind): degrees of tilt per unit/second of speed. */
	UPROPERTY(EditAnywhere, Category = "Clock|Lift")
	float LiftTiltPerSpeed = 0.004f;

	/** While held up, the hand gently hovers up and down by this much (world units; looks only). */
	UPROPERTY(EditAnywhere, Category = "Clock|Lift")
	float LiftHoverBob = 3.f;

	// ---------------- Jam look ----------------

	/** When the hand jams its outer half buckles at the joint: this far sideways (degrees) and this
	 *  far down. It straightens again once fixed. */
	UPROPERTY(EditAnywhere, Category = "Clock|Jam")
	float JamBendAngle = 28.f;

	UPROPERTY(EditAnywhere, Category = "Clock|Jam")
	float JamDroop = 10.f;

	/** Seconds the whole hand glitches for when it jams, and again when it's fixed. */
	UPROPERTY(EditAnywhere, Category = "Clock|Jam", meta = (ClampMin = "0.01"))
	float JamGlitchTime = 0.45f;

	/** Sparks per second streaming off the jammed hand, trailing behind it. */
	UPROPERTY(EditAnywhere, Category = "Clock|Jam", meta = (ClampMin = "0"))
	float JamTrailSparksPerSecond = 70.f;

	/** One lift lasts at most this many degrees of hand travel (360 = one full turn), then the
	 *  hand drops by itself even if the button is still held. */
	UPROPERTY(EditAnywhere, Category = "Clock|Lift", meta = (ClampMin = "1"))
	float MaxLiftTravelDeg = 360.f;

	/** Above this height the hand counts as "raised" (only changes its colour). */
	UPROPERTY(EditAnywhere, Category = "Clock|Lift")
	float RaisedThreshold = 40.f;

	// ---------------- Obstacle tuning ----------------

	UPROPERTY(EditAnywhere, Category = "Clock|Obstacles")
	float ObstacleRadius = 72.f;

	/** The hand clears an obstacle once it is lifted at least this high. */
	UPROPERTY(EditAnywhere, Category = "Clock|Obstacles")
	float ObstacleHeight = 90.f;

	/** The neon on a low post (values above 1 bloom). Its dark hull is a faint tint of this. */
	UPROPERTY(EditAnywhere, Category = "Clock|Obstacles")
	FLinearColor ObstacleColor = FLinearColor(4.5f, 0.2f, 0.18f);

	/** High posts float this far above the dial (world units). Lifting the hand at all reaches
	 *  them; a resting hand passes underneath. */
	UPROPERTY(EditAnywhere, Category = "Clock|Obstacles")
	float HighObstacleBottom = 45.f;

	/** High posts are flat square slabs (low ones are round), so this is the slab's thickness. */
	UPROPERTY(EditAnywhere, Category = "Clock|Obstacles")
	float HighObstacleHeight = 16.f;

	/** The neon on a high post. */
	UPROPERTY(EditAnywhere, Category = "Clock|Obstacles")
	FLinearColor HighObstacleColor = FLinearColor(3.f, 0.4f, 4.5f);

	/** Seconds an obstacle takes to glitch in and out. It can be hit from the moment it appears. */
	UPROPERTY(EditAnywhere, Category = "Clock|Obstacles", meta = (ClampMin = "0.01"))
	float ObstacleFadeInTime = 0.3f;

	UPROPERTY(EditAnywhere, Category = "Clock|Obstacles", meta = (ClampMin = "0.01"))
	float ObstacleFadeOutTime = 0.3f;

	/** Obstacles are neon signs: a dark board with a neon frame and an arrow bent from neon tube - an
	 *  up arrow standing on the dial for a low one (lift over it), a down arrow floating above it for a
	 *  high one (keep the hand down). The arrow's height, and the thickness of its tube. */
	UPROPERTY(EditAnywhere, Category = "Clock|Obstacles")
	float ObstacleArrowHeight = 110.f;

	UPROPERTY(EditAnywhere, Category = "Clock|Obstacles")
	float ObstacleArrowThickness = 7.f;

	/** How far a floating (high) sign hovers up and down. Signs standing on the dial stay put. */
	UPROPERTY(EditAnywhere, Category = "Clock|Obstacles")
	float ObstacleArrowBob = 8.f;

	/** Space between the arrow and the edge of its board. */
	UPROPERTY(EditAnywhere, Category = "Clock|Obstacles")
	float ObstacleSignMargin = 14.f;

	UPROPERTY(EditAnywhere, Category = "Clock|Obstacles")
	FLinearColor ObstacleSignBoardColor = FLinearColor(0.008f, 0.01f, 0.018f);

	// ---------------- Camera ----------------

	/** Camera distance from the dial centre, as a multiple of the dial radius. */
	UPROPERTY(EditAnywhere, Category = "Clock|Camera")
	float CameraDistanceFactor = 8.0f;

	/** Degrees the camera looks down. 90 = straight top-down, lower = more side-on. */
	UPROPERTY(EditAnywhere, Category = "Clock|Camera")
	float CameraPitch = 35.f;

	UPROPERTY(EditAnywhere, Category = "Clock|Camera")
	float CameraFOV = 68.f;

	/** As the city lights up (see ASpinCityFX), the camera eases back and lifts its gaze toward the
	 *  skyline, reaching these values once the whole city is lit. It swoops back in when a run starts. */
	UPROPERTY(EditAnywhere, Category = "Clock|Camera")
	bool bCameraFollowsReveal = true;

	UPROPERTY(EditAnywhere, Category = "Clock|Camera")
	float RevealedCameraPitch = 24.f;

	UPROPERTY(EditAnywhere, Category = "Clock|Camera")
	float RevealedCameraDistanceFactor = 10.f;

	/** Once the city is lit, the camera also tilts its view up by this many degrees (without moving),
	 *  so the dial drops toward the bottom of the frame and the skyline and sky rise into view. */
	UPROPERTY(EditAnywhere, Category = "Clock|Camera")
	float RevealedCameraLookUp = 18.f;

	/** The field of view widens to this as the camera pulls back, fitting in the skyline and the
	 *  sky above it while keeping the dial in the shot. */
	UPROPERTY(EditAnywhere, Category = "Clock|Camera")
	float RevealedCameraFOV = 82.f;

	/** The camera pulls back far enough to see past the light's outer edge (where lights are still
	 *  fading in) by this multiple of its distance, plus RevealCameraBuffer - so lights always come
	 *  on inside the shot, never just out of it. Higher = the camera stays further ahead. */
	UPROPERTY(EditAnywhere, Category = "Clock|Camera", meta = (ClampMin = "1"))
	float RevealCameraLead = 1.25f;

	/** Extra distance (world units) the camera keeps in view beyond the light's outer edge. */
	UPROPERTY(EditAnywhere, Category = "Clock|Camera", meta = (ClampMin = "0"))
	float RevealCameraBuffer = 5000.f;

	/** Seconds the camera lags behind the city's light as it pulls back. Higher = floatier. */
	UPROPERTY(EditAnywhere, Category = "Clock|Camera", meta = (ClampMin = "0.05"))
	float CameraRiseSmoothTime = 1.8f;

	/** The same for swooping back in when a run starts (quicker, so play is framed in time). */
	UPROPERTY(EditAnywhere, Category = "Clock|Camera", meta = (ClampMin = "0.05"))
	float CameraReturnSmoothTime = 0.7f;

	/** The camera is never quite still: it slowly drifts by up to this many world units... */
	UPROPERTY(EditAnywhere, Category = "Clock|Camera", meta = (ClampMin = "0"))
	float CameraSwayOffset = 22.f;

	/** ...and tilts by up to this many degrees. */
	UPROPERTY(EditAnywhere, Category = "Clock|Camera", meta = (ClampMin = "0"))
	float CameraSwayTilt = 0.25f;

	/** How fast the sway drifts (1 = one slow wave about every 9 seconds). */
	UPROPERTY(EditAnywhere, Category = "Clock|Camera", meta = (ClampMin = "0"))
	float CameraSwaySpeed = 1.f;

	// The menu shot: on the menus (and game over) the camera drops down almost to the ground, so the
	// dial runs along the bottom of the screen with the skyline above it. It swoops up into the
	// play camera when a run starts, and back down when the run ends.

	/** Menu shot: how far back from the dial centre the camera sits, as a multiple of the dial radius. */
	UPROPERTY(EditAnywhere, Category = "Clock|Camera|Menu", meta = (ClampMin = "0"))
	float MenuCameraDistanceFactor = 2.4f;

	/** Menu shot: camera height above the dial, as a multiple of the dial radius. */
	UPROPERTY(EditAnywhere, Category = "Clock|Camera|Menu", meta = (ClampMin = "0"))
	float MenuCameraHeightFactor = 0.4f;

	/** Menu shot: degrees the camera looks up (negative = down). Slightly up keeps the dial low in
	 *  the frame and the skyline in view. */
	UPROPERTY(EditAnywhere, Category = "Clock|Camera|Menu")
	float MenuCameraPitch = 3.f;

	UPROPERTY(EditAnywhere, Category = "Clock|Camera|Menu")
	float MenuCameraFOV = 75.f;

	/** Menu shot: the camera slowly swings around the dial by up to this many degrees either way. */
	UPROPERTY(EditAnywhere, Category = "Clock|Camera|Menu", meta = (ClampMin = "0"))
	float MenuCameraSwing = 18.f;

	/** Seconds for one full swing back and forth. */
	UPROPERTY(EditAnywhere, Category = "Clock|Camera|Menu", meta = (ClampMin = "1"))
	float MenuCameraSwingPeriod = 70.f;

	/** Seconds (roughly) the camera takes to swoop between the menu shot and the play camera. */
	UPROPERTY(EditAnywhere, Category = "Clock|Camera|Menu", meta = (ClampMin = "0.05"))
	float MenuCameraTransitionTime = 0.6f;

	/** Biggest camera offset (world units) at full shake intensity. */
	UPROPERTY(EditAnywhere, Category = "Clock|Camera")
	float MaxShakeOffset = 91.f;

	/** How fast a shake dies down (intensity units per second). */
	UPROPERTY(EditAnywhere, Category = "Clock|Camera")
	float ShakeDecayPerSecond = 2.5f;

	/** Brightness of the light over the dial. Lower it if the scene looks washed out. */
	UPROPERTY(EditAnywhere, Category = "Clock|Camera")
	float FillLightIntensity = 400000.f;

	/** Slightly cool, like the city's blue-cyan light. */
	UPROPERTY(EditAnywhere, Category = "Clock|Camera")
	FLinearColor FillLightColor = FLinearColor(0.8f, 0.88f, 1.f);

	// ---------------- State ----------------

	/** How far the hand is lifted above the dial (world units). 0 = resting on the dial. */
	UPROPERTY(BlueprintReadOnly, Category = "Clock|State")
	float HandLift = 0.f;

private:
	void BuildVisuals();
	void UpdateZone(const ASpinGameMode& GM);
	/** Lays a zone's arc and grade lines out around Center. */
	void LayoutZone(const TArray<TObjectPtr<UStaticMeshComponent>>& Segments, const TArray<TObjectPtr<UStaticMeshComponent>>& Marks,
		float Center, float HalfWidth, const ASpinGameMode& GM);
	void UpdateColors(const ASpinGameMode& GM);
	void UpdateMusic(const ASpinGameMode& GM);
	void UpdateDialGrowth(float DeltaSeconds);
	/** The game-over breakdown (see CollapseTime). Returns true while it owns the dial's transform. */
	bool UpdateCollapse(float DeltaSeconds, float TargetScale);
	void UpdateObstacles(float DeltaSeconds);
	void UpdateObstaclePassing(ASpinGameMode& GM, float Moved);

	/** Places the camera for this frame: its base, plus the idle sway and any shake. */
	void UpdateCameraMotion(float DeltaSeconds);

	/** Where the camera rests (before shake): 0 = the close-up from CameraPitch / CameraDistanceFactor,
	 *  1 = the wide shot from the Revealed values. */
	void SetCameraBase(float Blend);

	/** Blends the camera base toward the low menu shot while a menu is up, and back out of it when
	 *  a run starts. Runs after SetCameraBase, before UpdateCameraMotion. */
	void ApplyMenuShot(float DeltaSeconds);
	void UpdateSparks(float DeltaSeconds);

	static constexpr int32 NumZoneSegments = 48;
	/** PERFECT and GOOD boundaries, either side of the zone's centre. */
	static constexpr int32 NumZoneMarks = 4;
	static constexpr int32 NumObstacleSlots = 4;
	static constexpr int32 NumSparks = 80;
	/** Radius of the hub pillar at the centre, as a fraction of the dial radius. */
	static constexpr float HubRadiusFraction = 0.06f;
	/** Height of the hub pillar's top above the dial. */
	static constexpr float HubHeight = 14.f;

	/** Reach the spindle up from the hub to the hand, at its current height (see HandSpindle). */
	void UpdateSpindle(float HandZ);
	static constexpr int32 NumRingSegments = 90;
	static constexpr int32 NumTicks = 60;
	static constexpr int32 NumTrackSegments = 72;

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	/** Everything on the dial hangs off this, and it's what scales (see DialGrowthSteps). */
	UPROPERTY()
	TObjectPtr<USceneComponent> DialPivot;

	UPROPERTY()
	TObjectPtr<USceneComponent> HandPivot;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Rim;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Dial;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Hub;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Hand;

	/** The shaft the hand rides on: rises out of the hub and stretches with the lift, so the hand is
	 *  always mounted on something. A neon band runs round it, and a neon collar seats the hand on it. */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> HandSpindle;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> HandSpindleBand;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> HandCollar;

	/** The hand's joint, halfway along: everything beyond it hangs off this, so a jam can buckle the
	 *  outer half (see JamBendAngle). The outer half of the dark body hangs off it too. */
	UPROPERTY()
	TObjectPtr<USceneComponent> HandOuterPivot;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> HandOuterBody;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ZoneSegments;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ZoneMarks;

	/** The second zone of a double-zone pair (see ESpinLevelEvent::DoubleZone), and its grade lines. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SecondZoneSegments;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SecondZoneMarks;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> OuterRingSegments;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> InnerRingSegments;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> TickMarks;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> TrackInnerSegments;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> TrackOuterSegments;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ObstacleMeshes;

	/** A dark disc on the dial under each high post, so it reads as floating over that spot. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ObstacleShadows;

	/** Glowing neon plates capping each obstacle's dark hull, top and bottom. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ObstacleTopGlows;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ObstacleBaseGlows;

	/** Each obstacle's sign: NumSignNeon neon tubes per obstacle (the arrow's outline, then the frame),
	 *  and one dark board each. Filled in the constructor and not saved (a saved list would replace
	 *  this one when the level loads). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ObstacleSignNeon;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ObstacleSignBoards;

	/** Tubes in one sign: 7 for the arrow's outline, 4 for the frame. */
	static constexpr int32 NumSignArrowTubes = 7;
	static constexpr int32 NumSignNeon = NumSignArrowTubes + 4;

	/** The hand's neon: edge rails, centre spine, the two strokes of its pointed tip, and its tail. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> HandNeon;

	/** One per obstacle, since each fades on its own. */
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ObstacleGlowMIDs;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SparkMeshes;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SparkMID;

	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY()
	TObjectPtr<UAudioComponent> MenuMusicPlayer;

	UPROPERTY()
	TObjectPtr<UAudioComponent> RunMusicPlayer;

	/** Whichever of the two is playing, or null before the first frame and on the game-over screen. */
	UPROPERTY()
	TObjectPtr<UAudioComponent> ActiveMusicPlayer;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> FillLight;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	/** Low posts are round; high posts are flat squares (and so are their shadows). */
	UPROPERTY()
	TObjectPtr<UStaticMesh> RoundPostMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SquarePostMesh;

	/** Emissive neon material shared with the city's level dressing: rings, ticks, track, hand, hub and zone. */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> NeonMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ZoneMID;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SecondZoneMID;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PerfectMarkMID;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GoodMarkMID;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> OuterRingMID;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> InnerRingMID;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> TickMID;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> TrackMID;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HandMID;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HubMID;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ObstacleMID;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HighObstacleMID;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ShadowMID;

	/** Where an obstacle's mesh sits: its centre height, standing on the dial or floating. */
	float GetObstacleCenterZ(const FObstacleState& S) const;

	/** Moves an obstacle's parts to this spot, turned to face the hub (which only shows on the
	 *  square high posts). */
	void PlaceObstacle(int32 Index, const FVector2D& Position);

	/** Shows or hides an obstacle's parts. */
	void ShowObstacle(int32 Index, bool bShown);

	TWeakObjectPtr<ASpinGameMode> GameMode;

	/** The city effects, whose reveal the camera follows (see bCameraFollowsReveal). */
	TWeakObjectPtr<ASpinCityFX> CityFX;

	TArray<FObstacleState> ObstacleStates;

	/** Plain bookkeeping for one spark particle. */
	struct FSpark
	{
		FVector Velocity = FVector::ZeroVector;
		FVector Location = FVector::ZeroVector;
		float Age = 0.f;
		float Life = 0.f; // 0 = unused
	};
	TArray<FSpark> Sparks;
	/** Seconds until the gears next catch and throw a burst of sparks. */
	float GrindBurstTimer = 0.f;
	/** 1 at a catch, fading to 0: drives the pillar's hot glow and the hand's snag. */
	float GrindFlash = 0.f;
	bool bSparking = false;
	bool bJammed = false;
	float JamWobbleTime = 0.f;

	/** The lift spring's speed (world units per second, + = rising), and time spent held up (for the hover). */
	float HandLiftVelocity = 0.f;
	float HandHoverTime = 0.f;

	/** Neon hand parts. The first four are on the inner half (edges, spine, tail cap), the rest on the
	 *  outer half beyond the joint (edges, spine, and the two strokes of the tip). */
	static constexpr int32 NumHandNeon = 9;
	static constexpr int32 NumInnerHandNeon = 4;

	/** How buckled the hand is (0 = straight, 1 = fully bent by a jam), eased. */
	float HandBend = 0.f;

	/** Seconds left of the hand's glitch (when it jams, and when it's fixed), and this frame's noise seed. */
	float HandGlitchTimer = 0.f;
	int32 HandGlitchSeed = 0;

	/** Fractional sparks owed to the jammed hand's trail, carried between frames. */
	float JamTrailOwed = 0.f;

	FVector CameraBaseLocation = FVector::ZeroVector;
	FRotator CameraBaseRotation = FRotator::ZeroRotator;

	/** The camera's smoothed place between close-up and wide shot, and its speed (see SetCameraBase). Below 0 = not started. */
	float CameraBlend = -1.f;
	float CameraBlendRate = 0.f;
	float SwayTime = 0.f;

	/** 1 = the menu shot, 0 = the play camera, smoothed (see ApplyMenuShot). Below 0 = not started. */
	float MenuBlend = -1.f;
	float MenuBlendRate = 0.f;
	float MenuSwingTime = 0.f;

	/** The dial's current size, and the animation towards DialGrowTo (below 0 = not started). */
	float DialScale = 1.f;
	float DialGrowFrom = 1.f;
	float DialGrowTo = -1.f;
	float DialGrowElapsed = 0.f;
	/** 1 as a step outward lands, fading to 0: flares the rings. */
	float DialGrowFlash = 0.f;
	/** How strong the current glitch is (see GetDialGlitch), its colour, and its seed. */
	float DialGlitch = 0.f;
	FLinearColor DialGlitchTint = FLinearColor::White;
	int32 DialGlitchSeed = 0;

	/** The game-over collapse: running (or finished and hidden) while the run is over. */
	bool bCollapsing = false;
	float CollapseElapsed = 0.f;
	float CollapseYaw = 0.f;
	float CollapseSpinSign = 1.f;
	/** The dial's size when the run ended, and the direction it skids off in. */
	float CollapseFromScale = 1.f;
	FVector CollapseDriftDir = FVector::ForwardVector;

	/** The track from RunMusic played last, so the next one can be a different one. */
	UPROPERTY()
	TObjectPtr<USoundBase> LastRunTrack;

	/** The game was on its game-over screen last frame, to catch the moment a run ends. */
	bool bWasGameOver = false;

	bool bLiftHeld = false;
	bool bLiftPressValid = false;
	/** Degrees the hand has travelled during the current lift. */
	float LiftTravelDeg = 0.f;
	float PrevHandAngle = 0.f;
	float ShakeAmount = 0.f;
	/** 0 = full brightness, 1 = fully dimmed for invincibility (eased, see InvincibleFadeTime). */
	float InvincibleDim = 0.f;
	float LastZoneCenter = -1000.f;
	float LastZoneHalfWidth = -1000.f;
	bool bZonesVisible = true;
	/** The PERFECT band the grade lines were last laid out for (Sweet Spot can widen it mid-zone). */
	float LastPerfectFraction = -1.f;
	float LastSecondZoneCenter = -1000.f;
	float LastSecondZoneHalfWidth = -1000.f;
	bool bSecondZoneVisible = true;
};
