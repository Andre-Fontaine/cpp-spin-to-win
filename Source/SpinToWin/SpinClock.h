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

class UCameraComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;
class ASpinGameMode;

/** Plain (non-reflected) bookkeeping for one obstacle slot. */
enum class EObstaclePhase : uint8
{
	Inactive,
	SlidingIn,
	Waiting,
	SlidingOut
};

struct FObstacleState
{
	EObstaclePhase Phase = EObstaclePhase::Inactive;
	float Elapsed = 0.f;

	/** Positions on the dial plane, relative to the clock actor. */
	FVector2D Start = FVector2D::ZeroVector;   // off the side of the dial
	FVector2D Target = FVector2D::ZeroVector;  // resting place on the dial
	FVector2D From = FVector2D::ZeroVector;    // where a slide-out began
	FVector2D Current = FVector2D::ZeroVector;

	/** Degrees the hand still has to travel to reach this obstacle (negative once it is beyond it). */
	float RemainingDeg = 0.f;

	/** Once RemainingDeg drops below minus this, the hand has fully cleared the obstacle. */
	float PassMarginDeg = 0.f;

	/** True once the hand has fully passed this obstacle. The lift switches off. */
	bool bPassed = false;
};

UCLASS()
class SPINTOWIN_API ASpinClock : public AActor
{
	GENERATED_BODY()

public:
	ASpinClock();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Shake the camera. Intensity is 0..1 (1 = biggest). Overlapping shakes keep the stronger one. */
	void PlayShake(float Intensity);

	/** World-space point on the dial at the target zone's angle. RadiusFraction 0 = centre, 1 = rim. */
	FVector GetZoneWorldLocation(float RadiusFraction) const;

	// ---------------- Lift ----------------

	/** Tell the clock the lift button was pressed or released. bValidPress says the press began
	 *  while an obstacle was out. Only valid presses raise the hand, and it drops by itself once
	 *  the obstacle has been passed (the button must be pressed again for the next one). */
	void SetLiftHeld(bool bHeld, bool bValidPress);

	/** True while an obstacle is out and the hand hasn't passed it yet, i.e. lifting is allowed. */
	bool IsLiftAvailable() const;

	/** True when the hand is lifted clearly off the dial (it turns cyan). Hitting zones works at any height. */
	bool IsHandRaised() const;

	// ---------------- Obstacles ----------------

	/** Slide an obstacle in from the side so it comes to rest at this angle / radius on the dial. */
	void SpawnObstacle(float AngleDeg, float RadiusFraction, float AheadDeg);
	// AheadDeg = how many degrees ahead of the hand (in its direction of travel) the obstacle is.

	/** Slide every current obstacle back out of the dial. */
	void RetireObstacles();

	/** True if the hand, at the given angle and its current height, is touching an obstacle. */
	bool IsHandHittingObstacle(float HandAngleDeg) const;

	// ---------------- Jam ----------------

	/** Sparks shower off the whole hand (and it flickers hot orange) as a jam warning. */
	void SetSparking(bool bSparking);

	/** A jammed hand tilts and turns orange-brown ("broken") while it keeps spinning. */
	void SetJammed(bool bJammed);

	// ---------------- Layout ----------------

	/** Radius of the dial in world units. Everything else scales from this. */
	UPROPERTY(EditDefaultsOnly, Category = "Clock|Layout")
	float DialRadius = 600.f;

	UPROPERTY(EditDefaultsOnly, Category = "Clock|Layout")
	float DialThickness = 20.f;

	/** Hand length as a fraction of the dial radius. */
	UPROPERTY(EditDefaultsOnly, Category = "Clock|Layout")
	float HandLengthFraction = 0.9f;

	UPROPERTY(EditDefaultsOnly, Category = "Clock|Layout")
	float HandWidth = 28.f;

	UPROPERTY(EditDefaultsOnly, Category = "Clock|Layout")
	float HandHeight = 12.f;

	/** Where the target zone sits, as a fraction of the dial radius (centre of the band). */
	UPROPERTY(EditDefaultsOnly, Category = "Clock|Layout")
	float ZoneRadiusFraction = 0.78f;

	/** Thickness of the target zone band as a fraction of the dial radius. */
	UPROPERTY(EditDefaultsOnly, Category = "Clock|Layout")
	float ZoneThicknessFraction = 0.2f;

	// ---------------- Neon ring highlights ----------------

	/** Outer ring sits just inside the dial's edge and always shows the currently-needed button's
	 *  colour (same colour as the target zone), so the whole rim reads as a glowing accent. */
	UPROPERTY(EditDefaultsOnly, Category = "Clock|Neon")
	float OuterRingRadiusFraction = 0.99f;

	/** Inner ring frames the hub with a fixed accent colour. */
	UPROPERTY(EditDefaultsOnly, Category = "Clock|Neon")
	float InnerRingRadiusFraction = 0.16f;

	/** Thickness of both rings, as a fraction of the dial radius. Kept small for a thin glowing line. */
	UPROPERTY(EditDefaultsOnly, Category = "Clock|Neon")
	float RingThicknessFraction = 0.012f;

	UPROPERTY(EditDefaultsOnly, Category = "Clock|Neon")
	FLinearColor InnerRingColor = FLinearColor(0.1f, 0.9f, 1.f);

	/** At the peak of a successful hit's pulse, both rings' glow is multiplied by 1 + this. */
	UPROPERTY(EditDefaultsOnly, Category = "Clock|Neon")
	float RingHitPulseBoost = 6.f;

	/** At the peak of a successful hit's pulse, the fill light is multiplied by 1 + this. */
	UPROPERTY(EditDefaultsOnly, Category = "Clock|Neon")
	float FillLightHitPulseBoost = 1.5f;

	// ---------------- Lift tuning ----------------

	/** How high the hand rises while the lift button is held (world units). Must be above ObstacleHeight. */
	UPROPERTY(EditDefaultsOnly, Category = "Clock|Lift")
	float MaxHandLift = 150.f;

	/** Rise speed, world units per second. */
	UPROPERTY(EditDefaultsOnly, Category = "Clock|Lift")
	float LiftRiseSpeed = 1400.f;

	/** Lowering speed, world units per second. */
	UPROPERTY(EditDefaultsOnly, Category = "Clock|Lift")
	float LiftLowerSpeed = 1000.f;

	/** Above this height the hand counts as "raised" (only changes its colour). */
	UPROPERTY(EditDefaultsOnly, Category = "Clock|Lift")
	float RaisedThreshold = 40.f;

	// ---------------- Obstacle tuning ----------------

	UPROPERTY(EditDefaultsOnly, Category = "Clock|Obstacles")
	float ObstacleRadius = 72.f;

	/** The hand clears an obstacle once it is lifted at least this high. */
	UPROPERTY(EditDefaultsOnly, Category = "Clock|Obstacles")
	float ObstacleHeight = 90.f;

	UPROPERTY(EditDefaultsOnly, Category = "Clock|Obstacles")
	float ObstacleSlideInTime = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category = "Clock|Obstacles")
	float ObstacleSlideOutTime = 0.3f;

	// ---------------- Camera ----------------

	/** Camera distance from the dial centre, as a multiple of the dial radius. */
	UPROPERTY(EditDefaultsOnly, Category = "Clock|Camera")
	float CameraDistanceFactor = 8.0f;

	/** Degrees the camera looks down. 90 = straight top-down, lower = more side-on. */
	UPROPERTY(EditDefaultsOnly, Category = "Clock|Camera")
	float CameraPitch = 42.f;

	UPROPERTY(EditDefaultsOnly, Category = "Clock|Camera")
	float CameraFOV = 68.f;

	/** Biggest camera offset (world units) at full shake intensity. */
	UPROPERTY(EditDefaultsOnly, Category = "Clock|Camera")
	float MaxShakeOffset = 91.f;

	/** How fast a shake dies down (intensity units per second). */
	UPROPERTY(EditDefaultsOnly, Category = "Clock|Camera")
	float ShakeDecayPerSecond = 2.5f;

	/** Brightness of the light over the dial. Lower it if the scene looks washed out. */
	UPROPERTY(EditDefaultsOnly, Category = "Clock|Camera")
	float FillLightIntensity = 400000.f;

	// ---------------- State ----------------

	/** How far the hand is lifted above the dial (world units). 0 = resting on the dial. */
	UPROPERTY(BlueprintReadOnly, Category = "Clock|State")
	float HandLift = 0.f;

private:
	void BuildVisuals();
	void UpdateZone(const ASpinGameMode& GM);
	void UpdateColors(const ASpinGameMode& GM);
	void UpdateObstacles(float DeltaSeconds);
	void UpdateObstaclePassing(ASpinGameMode& GM);
	void UpdateShake(float DeltaSeconds);
	void UpdateSparks(float DeltaSeconds);

	static constexpr int32 NumZoneSegments = 48;
	static constexpr int32 NumObstacleSlots = 4;
	static constexpr int32 NumSparks = 80;
	static constexpr int32 NumRingSegments = 90;

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

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

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ZoneSegments;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> OuterRingSegments;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> InnerRingSegments;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ObstacleMeshes;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SparkMeshes;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SparkMID;

	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> FillLight;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	/** Emissive neon material shared with the city's level dressing, used for the ring highlights. */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> NeonMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ZoneMID;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> OuterRingMID;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> InnerRingMID;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HandMID;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ObstacleMID;

	TWeakObjectPtr<ASpinGameMode> GameMode;

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
	float SparkSpawnAccumulator = 0.f;
	bool bSparking = false;
	bool bJammed = false;
	float JamWobbleTime = 0.f;

	FVector CameraBaseLocation = FVector::ZeroVector;
	FRotator CameraBaseRotation = FRotator::ZeroRotator;

	bool bLiftHeld = false;
	bool bLiftPressValid = false;
	float PrevHandAngle = 0.f;
	float ShakeAmount = 0.f;
	float LastZoneCenter = -1000.f;
	float LastZoneHalfWidth = -1000.f;
	bool bZonesVisible = true;
};
