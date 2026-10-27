// Spin To Win - the physical dial in the world.

#include "SpinClock.h"
#include "SpinGameMode.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// The engine's built-in BasicShapeMaterial exposes a vector parameter called "Color".
	const FName ColorParam(TEXT("Color"));
}

ASpinClock::ASpinClock()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Built-in engine shapes, so no imported assets are needed.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	// Emissive neon material from the level's city dressing, used only for the thin ring highlights.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> NeonMaterialFinder(TEXT("/Game/Materials/Neon/M_Neon.M_Neon"));

	UStaticMesh* CylinderMesh = CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr;
	UStaticMesh* CubeMesh = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
	BaseMaterial = MaterialFinder.Succeeded() ? MaterialFinder.Object : nullptr;
	NeonMaterial = NeonMaterialFinder.Succeeded() ? NeonMaterialFinder.Object : nullptr;

	auto MakeMesh = [this](FName Name, UStaticMesh* Mesh, USceneComponent* Parent) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Comp = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Comp->SetupAttachment(Parent);
		Comp->SetMobility(EComponentMobility::Movable);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision); // collisions are computed in code, see IsHandHittingObstacle
		if (Mesh)
		{
			Comp->SetStaticMesh(Mesh);
		}
		if (BaseMaterial)
		{
			Comp->SetMaterial(0, BaseMaterial);
		}
		return Comp;
	};

	Rim = MakeMesh(TEXT("Rim"), CylinderMesh, Root);
	Dial = MakeMesh(TEXT("Dial"), CylinderMesh, Root);
	Hub = MakeMesh(TEXT("Hub"), CylinderMesh, Root);

	HandPivot = CreateDefaultSubobject<USceneComponent>(TEXT("HandPivot"));
	HandPivot->SetupAttachment(Root);
	Hand = MakeMesh(TEXT("Hand"), CubeMesh, HandPivot);

	for (int32 i = 0; i < NumZoneSegments; ++i)
	{
		UStaticMeshComponent* Segment = MakeMesh(*FString::Printf(TEXT("ZoneSegment%d"), i), CubeMesh, Root);
		Segment->SetCastShadow(false);
		ZoneSegments.Add(Segment);
	}

	for (int32 i = 0; i < NumRingSegments; ++i)
	{
		UStaticMeshComponent* Segment = MakeMesh(*FString::Printf(TEXT("OuterRing%d"), i), CubeMesh, Root);
		Segment->SetCastShadow(false);
		OuterRingSegments.Add(Segment);
	}

	for (int32 i = 0; i < NumRingSegments; ++i)
	{
		UStaticMeshComponent* Segment = MakeMesh(*FString::Printf(TEXT("InnerRing%d"), i), CubeMesh, Root);
		Segment->SetCastShadow(false);
		InnerRingSegments.Add(Segment);
	}

	for (int32 i = 0; i < NumObstacleSlots; ++i)
	{
		ObstacleMeshes.Add(MakeMesh(*FString::Printf(TEXT("Obstacle%d"), i), CylinderMesh, Root));
	}
	ObstacleStates.SetNum(NumObstacleSlots);

	for (int32 i = 0; i < NumSparks; ++i)
	{
		UStaticMeshComponent* Spark = MakeMesh(*FString::Printf(TEXT("Spark%d"), i), CubeMesh, Root);
		Spark->SetCastShadow(false);
		SparkMeshes.Add(Spark);
	}
	Sparks.SetNum(NumSparks);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->bConstrainAspectRatio = false; // no black bars on non-16:9 windows

	FillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(Root);
	FillLight->SetMobility(EComponentMobility::Movable);
	FillLight->SetCastShadows(false);
}

void ASpinClock::BeginPlay()
{
	Super::BeginPlay();

	GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ASpinGameMode>() : nullptr;
	if (const ASpinGameMode* GM = GameMode.Get())
	{
		PrevHandAngle = GM->HandAngle;
	}
	BuildVisuals();
}

void ASpinClock::BuildVisuals()
{
	const float R = DialRadius;

	// Sizes: the engine's cylinder and cube are 100 units across, so scale = size / 100.
	const float RimThickness = 20.f;
	const float HubRadius = R * 0.06f;
	const float HubHeight = 14.f;
	const float HandLength = R * HandLengthFraction;

	Rim->SetRelativeScale3D(FVector(R * 2.14f / 100.f, R * 2.14f / 100.f, RimThickness / 100.f));
	Rim->SetRelativeLocation(FVector(0.f, 0.f, -4.f - RimThickness * 0.5f));

	Dial->SetRelativeScale3D(FVector(R * 2.f / 100.f, R * 2.f / 100.f, DialThickness / 100.f));
	Dial->SetRelativeLocation(FVector(0.f, 0.f, -DialThickness * 0.5f));

	Hub->SetRelativeScale3D(FVector(HubRadius * 2.f / 100.f, HubRadius * 2.f / 100.f, HubHeight / 100.f));
	Hub->SetRelativeLocation(FVector(0.f, 0.f, HubHeight * 0.5f));

	// The hand extends along the pivot's +X, and sits just above the zone band.
	Hand->SetRelativeScale3D(FVector(HandLength / 100.f, HandWidth / 100.f, HandHeight / 100.f));
	Hand->SetRelativeLocation(FVector(HandLength * 0.5f, 0.f, 3.f + HandHeight * 0.5f));

	// Obstacles: round posts, hidden until one is spawned.
	for (UStaticMeshComponent* Post : ObstacleMeshes)
	{
		Post->SetRelativeScale3D(FVector(ObstacleRadius * 2.f / 100.f, ObstacleRadius * 2.f / 100.f, ObstacleHeight / 100.f));
		Post->SetVisibility(false);
	}

	// Sparks: tiny glowing blocks, hidden until needed.
	for (UStaticMeshComponent* Spark : SparkMeshes)
	{
		Spark->SetVisibility(false);
	}

	// Colours.
	auto MakeMID = [](UStaticMeshComponent* Comp, const FLinearColor& Color) -> UMaterialInstanceDynamic*
	{
		UMaterialInstanceDynamic* MID = Comp ? Comp->CreateAndSetMaterialInstanceDynamic(0) : nullptr;
		if (MID)
		{
			MID->SetVectorParameterValue(ColorParam, Color);
		}
		return MID;
	};

	MakeMID(Rim, FLinearColor(0.12f, 0.13f, 0.17f));
	MakeMID(Dial, FLinearColor(0.03f, 0.04f, 0.07f));
	MakeMID(Hub, FLinearColor(0.9f, 0.9f, 0.9f));
	HandMID = MakeMID(Hand, FLinearColor::White);

	if (BaseMaterial)
	{
		ZoneMID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		for (UStaticMeshComponent* Segment : ZoneSegments)
		{
			Segment->SetMaterial(0, ZoneMID);
		}

		SparkMID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		if (SparkMID)
		{
			SparkMID->SetVectorParameterValue(ColorParam, FLinearColor(1.f, 0.85f, 0.2f));
		}
		for (UStaticMeshComponent* Spark : SparkMeshes)
		{
			Spark->SetMaterial(0, SparkMID);
		}

		ObstacleMID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		if (ObstacleMID)
		{
			ObstacleMID->SetVectorParameterValue(ColorParam, FLinearColor(0.95f, 0.3f, 0.1f));
		}
		for (UStaticMeshComponent* Post : ObstacleMeshes)
		{
			Post->SetMaterial(0, ObstacleMID);
		}
	}

	// Thin neon ring highlights: built the same way as the zone band (a ring of small blocks),
	// but as a full 360-degree circle that never moves - only its colour changes.
	if (NeonMaterial)
	{
		auto BuildRing = [this](TArray<TObjectPtr<UStaticMeshComponent>>& Segments, float RadiusFraction, UMaterialInstanceDynamic* MID)
		{
			const int32 Count = Segments.Num();
			const float Rho = DialRadius * RadiusFraction;
			const float StepDeg = 360.f / Count;
			const float TangentLength = FMath::DegreesToRadians(StepDeg) * Rho * 1.15f;
			const float RadialLength = DialRadius * RingThicknessFraction;

			for (int32 i = 0; i < Count; ++i)
			{
				const float AngleDeg = (i + 0.5f) * StepDeg;
				const float Rad = FMath::DegreesToRadians(AngleDeg);

				UStaticMeshComponent* Segment = Segments[i];
				Segment->SetRelativeLocation(FVector(Rho * FMath::Cos(Rad), Rho * FMath::Sin(Rad), 2.f));
				Segment->SetRelativeRotation(FRotator(0.f, AngleDeg, 0.f));
				Segment->SetRelativeScale3D(FVector(RadialLength / 100.f, TangentLength / 100.f, 2.5f / 100.f));
				Segment->SetMaterial(0, MID);
			}
		};

		OuterRingMID = UMaterialInstanceDynamic::Create(NeonMaterial, this);
		BuildRing(OuterRingSegments, OuterRingRadiusFraction, OuterRingMID);

		InnerRingMID = UMaterialInstanceDynamic::Create(NeonMaterial, this);
		if (InnerRingMID)
		{
			InnerRingMID->SetVectorParameterValue(ColorParam, InnerRingColor);
		}
		BuildRing(InnerRingSegments, InnerRingRadiusFraction, InnerRingMID);
	}

	// Camera: fixed, looking down at the dial from the "6 o'clock" side.
	const float Dist = R * CameraDistanceFactor;
	const float PitchRad = FMath::DegreesToRadians(CameraPitch);
	CameraBaseLocation = FVector(-Dist * FMath::Cos(PitchRad), 0.f, Dist * FMath::Sin(PitchRad));
	CameraBaseRotation = FRotator(-CameraPitch, 0.f, 0.f);
	Camera->SetRelativeLocation(CameraBaseLocation);
	Camera->SetRelativeRotation(CameraBaseRotation);
	Camera->SetFieldOfView(CameraFOV);

	// Fill light so the scene is visible even in an unlit level.
	FillLight->SetRelativeLocation(FVector(0.f, 0.f, R * 2.5f));
	FillLight->SetIntensity(FillLightIntensity);
	FillLight->SetAttenuationRadius(R * 12.f);
}

void ASpinClock::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (ASpinGameMode* GM = GameMode.Get())
	{
		UpdateObstaclePassing(*GM);
	}

	// Lift: only possible while an obstacle is out and not yet passed. The hand rises while the
	// button is held, and drops by itself (even if still held) once the obstacle is behind it.
	// A lift only counts if it was started while an obstacle was out. Once that obstacle is behind
	// the hand the lift is spent, and the button has to be pressed again for the next one.
	if (bLiftPressValid && !IsLiftAvailable())
	{
		bLiftPressValid = false;
	}
	const bool bRaise = bLiftHeld && bLiftPressValid;
	const float LiftTarget = bRaise ? MaxHandLift : 0.f;
	HandLift = FMath::FInterpConstantTo(HandLift, LiftTarget, DeltaSeconds, bRaise ? LiftRiseSpeed : LiftLowerSpeed);

	if (const ASpinGameMode* GM = GameMode.Get())
	{
		// A jammed hand is visibly bent and shudders: the pivot tilts and its heading jitters.
		FRotator HandRotation(0.f, GM->HandAngle, 0.f);
		if (bJammed)
		{
			JamWobbleTime += DeltaSeconds;
			HandRotation.Pitch = 9.f;
			HandRotation.Roll = 7.f;
			HandRotation.Yaw += FMath::Sin(JamWobbleTime * 45.f) * 1.5f;
		}
		HandPivot->SetRelativeRotation(HandRotation);
		HandPivot->SetRelativeLocation(FVector(0.f, 0.f, HandLift));

		UpdateZone(*GM);
		UpdateColors(*GM);
	}

	UpdateObstacles(DeltaSeconds);
	UpdateSparks(DeltaSeconds);
	UpdateShake(DeltaSeconds);
}

void ASpinClock::SetSparking(bool bNewSparking)
{
	bSparking = bNewSparking;
}

void ASpinClock::SetJammed(bool bNewJammed)
{
	bJammed = bNewJammed;
	if (!bJammed)
	{
		JamWobbleTime = 0.f;
	}
}

void ASpinClock::UpdateSparks(float DeltaSeconds)
{
	// Emit new sparks from the hub while sparking.
	if (bSparking)
	{
		SparkSpawnAccumulator += DeltaSeconds * 45.f; // sparks per second
		while (SparkSpawnAccumulator >= 1.f)
		{
			SparkSpawnAccumulator -= 1.f;
			for (FSpark& S : Sparks)
			{
				if (S.Life <= 0.f)
				{
					S.Age = 0.f;
					S.Life = FMath::FRandRange(0.25f, 0.55f);
					S.Location = FVector(0.f, 0.f, 18.f);
					const float Angle = FMath::FRandRange(0.f, 2.f * PI);
					const float Speed = FMath::FRandRange(80.f, 260.f);
					S.Velocity = FVector(FMath::Cos(Angle) * Speed, FMath::Sin(Angle) * Speed, FMath::FRandRange(120.f, 320.f));
					break;
				}
			}
		}
	}
	else
	{
		SparkSpawnAccumulator = 0.f;
	}

	// Move and shrink the live ones.
	for (int32 i = 0; i < Sparks.Num(); ++i)
	{
		FSpark& S = Sparks[i];
		UStaticMeshComponent* Mesh = SparkMeshes[i];
		if (S.Life <= 0.f)
		{
			continue;
		}

		S.Age += DeltaSeconds;
		if (S.Age >= S.Life)
		{
			S.Life = 0.f;
			Mesh->SetVisibility(false);
			continue;
		}

		S.Velocity.Z -= 700.f * DeltaSeconds; // gravity
		S.Location += S.Velocity * DeltaSeconds;
		if (S.Location.Z < 2.f)
		{
			S.Location.Z = 2.f;
			S.Velocity.Z = FMath::Abs(S.Velocity.Z) * 0.3f;
		}

		const float Size = 5.f * (1.f - S.Age / S.Life) + 1.f;
		Mesh->SetVisibility(true);
		Mesh->SetRelativeLocation(S.Location);
		Mesh->SetRelativeScale3D(FVector(Size / 100.f));
	}
}

void ASpinClock::UpdateZone(const ASpinGameMode& GM)
{
	const bool bShow = GM.State != ESpinState::WaitingToStart;
	if (bShow != bZonesVisible)
	{
		bZonesVisible = bShow;
		for (UStaticMeshComponent* Segment : ZoneSegments)
		{
			Segment->SetVisibility(bShow);
		}
	}

	if (!bShow || ZoneSegments.Num() == 0)
	{
		return;
	}

	if (FMath::IsNearlyEqual(GM.ZoneCenter, LastZoneCenter, 0.01f)
		&& FMath::IsNearlyEqual(GM.ZoneHalfWidth, LastZoneHalfWidth, 0.01f))
	{
		return;
	}
	LastZoneCenter = GM.ZoneCenter;
	LastZoneHalfWidth = GM.ZoneHalfWidth;

	// The zone is an arc built from a row of small blocks laid along the dial.
	const int32 Count = ZoneSegments.Num();
	const float Rho = DialRadius * ZoneRadiusFraction;
	const float StepDeg = (GM.ZoneHalfWidth * 2.f) / Count;
	const float TangentLength = FMath::DegreesToRadians(StepDeg) * Rho * 1.15f; // slight overlap, no gaps
	const float RadialLength = DialRadius * ZoneThicknessFraction;

	for (int32 i = 0; i < Count; ++i)
	{
		const float AngleDeg = GM.ZoneCenter - GM.ZoneHalfWidth + (i + 0.5f) * StepDeg;
		const float Rad = FMath::DegreesToRadians(AngleDeg);

		UStaticMeshComponent* Segment = ZoneSegments[i];
		Segment->SetRelativeLocation(FVector(Rho * FMath::Cos(Rad), Rho * FMath::Sin(Rad), 1.5f));
		Segment->SetRelativeRotation(FRotator(0.f, AngleDeg, 0.f));
		Segment->SetRelativeScale3D(FVector(RadialLength / 100.f, TangentLength / 100.f, 3.f / 100.f));
	}
}

void ASpinClock::UpdateColors(const ASpinGameMode& GM)
{
	const FLinearColor ZoneBase = GM.Prompts.IsValidIndex(GM.ActivePromptIndex)
		? GM.Prompts[GM.ActivePromptIndex].Color
		: FLinearColor(0.1f, 0.8f, 0.2f);

	// The zone brightens while the hand is inside it.
	const bool bActive = GM.State == ESpinState::Playing && GM.IsHandInZone();
	const FLinearColor ZoneColor = bActive ? (ZoneBase + (FLinearColor::White - ZoneBase) * 0.45f) : ZoneBase;

	if (ZoneMID)
	{
		ZoneMID->SetVectorParameterValue(ColorParam, ZoneColor);
	}

	if (OuterRingMID)
	{
		// The whole rim glows the colour of whichever button the next zone needs.
		OuterRingMID->SetVectorParameterValue(ColorParam, ZoneBase);
	}

	if (HandMID)
	{
		// White normally, cyan while raised (can't hit), red when the run is over.
		FLinearColor HandColor = FLinearColor::White;
		if (GM.State == ESpinState::GameOver)
		{
			HandColor = FLinearColor::Red;
		}
		else if (bJammed)
		{
			HandColor = FLinearColor(0.75f, 0.3f, 0.08f); // broken: orange-brown
		}
		else if (IsHandRaised())
		{
			HandColor = FLinearColor(0.4f, 0.9f, 1.f);
		}
		HandMID->SetVectorParameterValue(ColorParam, HandColor);
	}
}

void ASpinClock::SetLiftHeld(bool bHeld, bool bValidPress)
{
	bLiftHeld = bHeld;
	bLiftPressValid = bHeld && bValidPress;
}

bool ASpinClock::IsHandRaised() const
{
	return HandLift > RaisedThreshold;
}

bool ASpinClock::IsLiftAvailable() const
{
	const ASpinGameMode* GM = GameMode.Get();
	if (!GM || GM->State != ESpinState::Playing || bJammed)
	{
		return false;
	}

	for (const FObstacleState& S : ObstacleStates)
	{
		const bool bOut = S.Phase == EObstaclePhase::SlidingIn || S.Phase == EObstaclePhase::Waiting;
		if (bOut && !S.bPassed)
		{
			return true;
		}
	}

	return false;
}

void ASpinClock::UpdateObstaclePassing(ASpinGameMode& GM)
{
	// How far the hand moved since last frame. A big jump is a reset (new run), not real travel.
	float Moved = FMath::Abs(FMath::FindDeltaAngleDegrees(PrevHandAngle, GM.HandAngle));
	PrevHandAngle = GM.HandAngle;

	if (GM.State != ESpinState::Playing || bJammed || Moved > 45.f)
	{
		return;
	}

	for (FObstacleState& S : ObstacleStates)
	{
		const bool bOut = S.Phase == EObstaclePhase::SlidingIn || S.Phase == EObstaclePhase::Waiting;
		if (!bOut || S.bPassed)
		{
			continue;
		}

		S.RemainingDeg -= Moved;
		if (S.RemainingDeg <= -S.PassMarginDeg)
		{
			S.bPassed = true; // the hand is fully beyond it: the lift switches off
			GM.OnObstacleCleared(); // it got over the obstacle, so the player earns a life back
		}
	}
}

void ASpinClock::SpawnObstacle(float AngleDeg, float RadiusFraction, float AheadDeg)
{
	for (int32 i = 0; i < ObstacleStates.Num(); ++i)
	{
		FObstacleState& S = ObstacleStates[i];
		if (S.Phase != EObstaclePhase::Inactive)
		{
			continue;
		}

		const float Rad = FMath::DegreesToRadians(AngleDeg);
		const float Rho = DialRadius * RadiusFraction;
		S.Target = FVector2D(Rho * FMath::Cos(Rad), Rho * FMath::Sin(Rad));

		// It enters from whichever side of the dial it will rest on (random if it's near the middle).
		const float Side = (FMath::Abs(S.Target.Y) < DialRadius * 0.1f)
			? (FMath::RandBool() ? 1.f : -1.f)
			: FMath::Sign(S.Target.Y);
		S.Start = FVector2D(S.Target.X, Side * DialRadius * 1.35f);

		S.Current = S.Start;
		S.Elapsed = 0.f;
		S.Phase = EObstaclePhase::SlidingIn;

		// Track how far the hand still has to go, so the lift can switch off once it has passed.
		const float Reach = ObstacleRadius + HandWidth * 0.5f;
		S.RemainingDeg = AheadDeg;
		S.PassMarginDeg = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Reach / FMath::Max(Rho, 1.f), 0.f, 1.f))) + 3.f;
		S.bPassed = false;

		UStaticMeshComponent* Post = ObstacleMeshes[i];
		Post->SetVisibility(true);
		Post->SetRelativeLocation(FVector(S.Current.X, S.Current.Y, ObstacleHeight * 0.5f));
		return;
	}
}

void ASpinClock::RetireObstacles()
{
	for (FObstacleState& S : ObstacleStates)
	{
		if (S.Phase == EObstaclePhase::SlidingIn || S.Phase == EObstaclePhase::Waiting)
		{
			S.From = S.Current;
			S.Elapsed = 0.f;
			S.Phase = EObstaclePhase::SlidingOut;
		}
	}
}

void ASpinClock::UpdateObstacles(float DeltaSeconds)
{
	for (int32 i = 0; i < ObstacleStates.Num(); ++i)
	{
		FObstacleState& S = ObstacleStates[i];
		if (S.Phase == EObstaclePhase::Inactive)
		{
			continue;
		}

		S.Elapsed += DeltaSeconds;

		switch (S.Phase)
		{
		case EObstaclePhase::SlidingIn:
		{
			const float Alpha = FMath::Clamp(S.Elapsed / FMath::Max(0.01f, ObstacleSlideInTime), 0.f, 1.f);
			const float Eased = 1.f - FMath::Pow(1.f - Alpha, 3.f); // fast start, soft stop
			S.Current = S.Start + (S.Target - S.Start) * Eased;
			if (Alpha >= 1.f)
			{
				S.Phase = EObstaclePhase::Waiting;
			}
			break;
		}

		case EObstaclePhase::Waiting:
			S.Current = S.Target;
			break;

		case EObstaclePhase::SlidingOut:
		{
			const float Alpha = FMath::Clamp(S.Elapsed / FMath::Max(0.01f, ObstacleSlideOutTime), 0.f, 1.f);
			const float Eased = Alpha * Alpha; // slow start, fast exit
			S.Current = S.From + (S.Start - S.From) * Eased;
			if (Alpha >= 1.f)
			{
				S.Phase = EObstaclePhase::Inactive;
				ObstacleMeshes[i]->SetVisibility(false);
			}
			break;
		}

		default:
			break;
		}

		ObstacleMeshes[i]->SetRelativeLocation(FVector(S.Current.X, S.Current.Y, ObstacleHeight * 0.5f));
	}
}

bool ASpinClock::IsHandHittingObstacle(float HandAngleDeg) const
{
	// A hand lifted above the obstacles passes over them.
	if (HandLift >= ObstacleHeight)
	{
		return false;
	}

	const float Rad = FMath::DegreesToRadians(HandAngleDeg);
	const float CosA = FMath::Cos(Rad);
	const float SinA = FMath::Sin(Rad);
	const float HandLength = DialRadius * HandLengthFraction;
	const float Reach = ObstacleRadius + HandWidth * 0.5f;

	for (const FObstacleState& S : ObstacleStates)
	{
		// Obstacles that are leaving are harmless: the hand reverses right after a hit and would
		// otherwise run into the post it just passed through no fault of the player.
		if (S.Phase == EObstaclePhase::Inactive || S.Phase == EObstaclePhase::SlidingOut)
		{
			continue;
		}

		// Obstacle position in the hand's own frame: "Along" runs down the hand, "Across" is sideways.
		const float Along = S.Current.X * CosA + S.Current.Y * SinA;
		const float Across = -S.Current.X * SinA + S.Current.Y * CosA;

		// Distance from the obstacle's centre to the hand (a line segment from the hub to the tip).
		const float AlongOnHand = FMath::Clamp(Along, 0.f, HandLength);
		const float DX = Along - AlongOnHand;
		if (DX * DX + Across * Across <= Reach * Reach)
		{
			return true;
		}
	}

	return false;
}

void ASpinClock::PlayShake(float Intensity)
{
	ShakeAmount = FMath::Max(ShakeAmount, FMath::Clamp(Intensity, 0.f, 1.f));
}

void ASpinClock::UpdateShake(float DeltaSeconds)
{
	if (ShakeAmount <= 0.f)
	{
		return;
	}

	ShakeAmount = FMath::Max(0.f, ShakeAmount - ShakeDecayPerSecond * DeltaSeconds);

	if (ShakeAmount <= 0.f)
	{
		// Shake finished: put the camera exactly back where it belongs.
		Camera->SetRelativeLocation(CameraBaseLocation);
		Camera->SetRelativeRotation(CameraBaseRotation);
		return;
	}

	// Squared falloff so the shake snaps hard and settles smoothly.
	const float Mag = ShakeAmount * ShakeAmount * MaxShakeOffset;
	const FVector Offset(0.f, FMath::FRandRange(-Mag, Mag), FMath::FRandRange(-Mag, Mag));
	const FRotator Tilt(
		FMath::FRandRange(-Mag, Mag) * 0.08f,
		FMath::FRandRange(-Mag, Mag) * 0.08f,
		FMath::FRandRange(-Mag, Mag) * 0.08f);

	Camera->SetRelativeLocation(CameraBaseLocation + Offset);
	Camera->SetRelativeRotation(CameraBaseRotation + Tilt);
}

FVector ASpinClock::GetZoneWorldLocation(float RadiusFraction) const
{
	const ASpinGameMode* GM = GameMode.Get();
	const float Rad = GM ? FMath::DegreesToRadians(GM->ZoneCenter) : 0.f;
	const FVector Direction(FMath::Cos(Rad), FMath::Sin(Rad), 0.f);
	return GetActorLocation() + Direction * (DialRadius * RadiusFraction) + FVector(0.f, 0.f, 12.f);
}
