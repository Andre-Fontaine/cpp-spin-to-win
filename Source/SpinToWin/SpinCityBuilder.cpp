// Spin To Win - the procedural neon city around the dial.

#include "SpinCityBuilder.h"
#include "SpinGameMode.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FName ColorParam(TEXT("Color"));
	const FName IntensityParam(TEXT("Intensity"));

	// The engine's cube and cylinder are 100 units across, centred on their pivot.
	constexpr float MeshSize = 100.f;

	// Lines lying flat on the floor are seen at a shallow angle, so they look far thinner than
	// upright ones; below this width they break up into flickering dashes.
	constexpr float MinFloorLineWidth = 14.f;
}

ASpinCityBuilder::ASpinCityBuilder()
{
	PrimaryActorTick.bCanEverTick = true;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BaseFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> NeonFinder(TEXT("/Game/Materials/Neon/M_Neon.M_Neon"));

	UStaticMesh* Cube = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
	UStaticMesh* Cylinder = CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr;
	BaseMaterial = BaseFinder.Succeeded() ? BaseFinder.Object : nullptr;
	NeonMaterial = NeonFinder.Succeeded() ? NeonFinder.Object : nullptr;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WaterFinder(TEXT("/Game/Materials/Neon/M_Water.M_Water"));
	WaterMaterial = WaterFinder.Succeeded() ? WaterFinder.Object : nullptr;

	auto Setup = [this](UPrimitiveComponent* Comp)
	{
		Comp->SetupAttachment(GetRootComponent());
		Comp->SetMobility(EComponentMobility::Movable);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetCastShadow(false);
		Comp->SetCanEverAffectNavigation(false);
	};
	auto MakeISM = [this, &Setup](const TCHAR* Name, UStaticMesh* Mesh) -> UInstancedStaticMeshComponent*
	{
		UInstancedStaticMeshComponent* ISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Setup(ISM);
		if (Mesh)
		{
			ISM->SetStaticMesh(Mesh);
		}
		return ISM;
	};

	Floor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Floor"));
	Setup(Floor);
	if (Cube)
	{
		Floor->SetStaticMesh(Cube);
	}

	Water = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Water"));
	Setup(Water);
	if (Cube)
	{
		Water->SetStaticMesh(Cube);
	}

	for (int32 i = 0; i < NumHullBands; ++i)
	{
		HullBoxes.Add(MakeISM(*FString::Printf(TEXT("HullBoxes%d"), i), Cube));
		HullCylinders.Add(MakeISM(*FString::Printf(TEXT("HullCylinders%d"), i), Cylinder));
	}
	CarBodies = MakeISM(TEXT("CarBodies"), Cube);
	CarHeadlights = MakeISM(TEXT("CarHeadlights"), Cube);
	CarTaillights = MakeISM(TEXT("CarTaillights"), Cube);
	for (int32 i = 0; i < NumSlots; ++i)
	{
		NeonBoxes.Add(MakeISM(*FString::Printf(TEXT("NeonBoxes%d"), i), Cube));
		NeonDiscs.Add(MakeISM(*FString::Printf(TEXT("NeonDiscs%d"), i), Cylinder));
	}
	RingRoadLines = MakeISM(TEXT("RingRoadLines"), Cube);
	AvenueLines = MakeISM(TEXT("AvenueLines"), Cube);
	TrafficLines = MakeISM(TEXT("TrafficLines"), Cube);
	Beacons = MakeISM(TEXT("Beacons"), Cube);

	for (int32 i = 0; i < NumWindowGroups * 2; ++i)
	{
		WindowGroups.Add(MakeISM(*FString::Printf(TEXT("Windows%d"), i), Cube));
	}
	for (int32 i = 0; i < 2; ++i)
	{
		FlickerSigns.Add(MakeISM(*FString::Printf(TEXT("FlickerSigns%d"), i), Cube));
	}
	Motes = MakeISM(TEXT("Motes"), Cube);
}

void ASpinCityBuilder::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetupMaterials();
	Build();
}

void ASpinCityBuilder::Rebuild()
{
	SetupMaterials();
	Build();
}

void ASpinCityBuilder::BeginPlay()
{
	Super::BeginPlay();

	GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ASpinGameMode>() : nullptr;
	// The dynamic materials are transient, so a play session needs its own (the instances themselves are saved).
	SetupMaterials();
	InitLife();
}

float ASpinCityBuilder::GetCityRadius() const
{
	return BuiltRadius > 0.f ? BuiltRadius : LandmarkRadius;
}

// ---------------------------------------------------------------------------------------------
// Materials and the city's "life": hit flares, the floor reacting to speed, blinking beacons.
// ---------------------------------------------------------------------------------------------

void ASpinCityBuilder::SetupMaterials()
{
	if (!BaseMaterial || NeonBoxes.Num() < NumSlots || NeonDiscs.Num() < NumSlots
		|| WindowGroups.Num() < NumWindowGroups * 2 || FlickerSigns.Num() < 2
		|| HullBoxes.Num() < NumHullBands || HullCylinders.Num() < NumHullBands)
	{
		return;
	}
	UMaterialInterface* Glow = NeonMaterial ? NeonMaterial.Get() : BaseMaterial.Get();

	auto MakeMID = [this](UMaterialInterface* Parent, const FLinearColor& Color, float Intensity) -> UMaterialInstanceDynamic*
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, this);
		MID->SetVectorParameterValue(ColorParam, Color);
		MID->SetScalarParameterValue(IntensityParam, Intensity);
		return MID;
	};

	// Tower bodies: each distance band a step further from dark hull toward the haze colour. They glow
	// faintly (on M_Neon, so they also wait for the city reveal like everything else).
	for (int32 Band = 0; Band < NumHullBands; ++Band)
	{
		const float T = static_cast<float>(Band) / FMath::Max(1, NumHullBands - 1);
		UMaterialInstanceDynamic* HullMID = NeonMaterial
			? MakeMID(NeonMaterial, FMath::Lerp(HullColor, DepthHazeColor, T), DepthHazeIntensity)
			: MakeMID(BaseMaterial, HullColor, 0.f);
		HullBoxes[Band]->SetMaterial(0, HullMID);
		HullCylinders[Band]->SetMaterial(0, HullMID);
	}
	Floor->SetMaterial(0, MakeMID(BaseMaterial, FloorColor, 0.f));
	Water->SetMaterial(0, WaterMaterial ? WaterMaterial.Get() : Floor->GetMaterial(0));

	CarBodies->SetMaterial(0, MakeMID(BaseMaterial, HullColor, 0.f));
	CarHeadlights->SetMaterial(0, MakeMID(Glow, HeadlightColor, NeonIntensity * 1.5f));
	CarTaillights->SetMaterial(0, MakeMID(Glow, TaillightColor, NeonIntensity * 1.5f));

	NeonMIDs.Reset();
	for (int32 i = 0; i < NumSlots; ++i)
	{
		FLinearColor Color = FoliageColor;
		if (i == WaterSlot)
		{
			Color = WaterColor;
		}
		else if (i < NumPalette)
		{
			Color = Palette.Num() > 0 ? Palette[i % Palette.Num()] : FLinearColor::White;
		}
		UMaterialInstanceDynamic* MID = MakeMID(Glow, Color, NeonIntensity);
		NeonBoxes[i]->SetMaterial(0, MID);
		NeonDiscs[i]->SetMaterial(0, MID);
		NeonMIDs.Add(MID);
	}

	FloorLineMIDs.Reset();
	FloorLineMIDs.Add(MakeMID(Glow, RingRoadColor, FloorLineIntensity));
	FloorLineMIDs.Add(MakeMID(Glow, AvenueColor, FloorLineIntensity));
	FloorLineMIDs.Add(MakeMID(Glow, TrafficColor, FloorLineIntensity));
	RingRoadLines->SetMaterial(0, FloorLineMIDs[0]);
	AvenueLines->SetMaterial(0, FloorLineMIDs[1]);
	TrafficLines->SetMaterial(0, FloorLineMIDs[2]);

	BeaconMID = MakeMID(Glow, BeaconColor, NeonIntensity * 2.f);
	Beacons->SetMaterial(0, BeaconMID);

	// Windows: blue batches then amber ones, each with its own brightness so they switch independently.
	auto PaletteColor = [this](int32 Index)
	{
		return Palette.Num() > 0 ? Palette[Index % Palette.Num()] : FLinearColor::White;
	};
	WindowMIDs.Reset();
	for (int32 i = 0; i < WindowGroups.Num(); ++i)
	{
		UMaterialInstanceDynamic* MID = MakeMID(Glow, PaletteColor(i < NumWindowGroups ? 1 : 4), NeonIntensity);
		WindowGroups[i]->SetMaterial(0, MID);
		WindowMIDs.Add(MID);
	}

	// Faulty signs' lettering: cyan, then magenta.
	FlickerMIDs.Reset();
	for (int32 i = 0; i < FlickerSigns.Num(); ++i)
	{
		UMaterialInstanceDynamic* MID = MakeMID(Glow, PaletteColor(i == 0 ? 0 : 2), NeonIntensity);
		FlickerSigns[i]->SetMaterial(0, MID);
		FlickerMIDs.Add(MID);
	}

	MoteMID = MakeMID(Glow, MoteColor, MoteIntensity);
	Motes->SetMaterial(0, MoteMID);

	// Every light group the jam glitch recolours, remembering its normal colour.
	GlitchTargets.Reset();
	auto AddGlitchTarget = [this](UMaterialInstanceDynamic* MID)
	{
		if (!MID)
		{
			return;
		}
		FGlitchTarget Target;
		Target.MID = MID;
		MID->GetVectorParameterValue(FHashedMaterialParameterInfo(ColorParam), Target.BaseColor);
		GlitchTargets.Add(Target);
	};
	for (UMaterialInstanceDynamic* MID : NeonMIDs) { AddGlitchTarget(MID); }
	for (UMaterialInstanceDynamic* MID : FloorLineMIDs) { AddGlitchTarget(MID); }
	for (UMaterialInstanceDynamic* MID : WindowMIDs) { AddGlitchTarget(MID); }
	for (UMaterialInstanceDynamic* MID : FlickerMIDs) { AddGlitchTarget(MID); }
	AddGlitchTarget(BeaconMID);
	AddGlitchTarget(MoteMID);
}

float ASpinCityBuilder::UpdateJamGlitch(float DeltaSeconds)
{
	const ASpinGameMode* GM = GameMode.Get();
	const bool bJammed = GM && GM->State == ESpinState::Playing && GM->JamPhase == EJamPhase::Jammed;

	const float Before = JamGlitch;
	JamGlitch = FMath::FInterpConstantTo(JamGlitch, bJammed ? 1.f : 0.f, DeltaSeconds, 1.f / FMath::Max(0.01f, JamGlitchFadeTime));
	if (JamGlitch <= 0.f && Before <= 0.f)
	{
		return 1.f; // nothing to do (and the colours are already back to normal)
	}

	for (FGlitchTarget& Target : GlitchTargets)
	{
		UMaterialInstanceDynamic* MID = Target.MID.Get();
		if (!MID)
		{
			continue;
		}
		if (bJammed)
		{
			Target.Timer -= DeltaSeconds;
			if (Target.Timer <= 0.f)
			{
				// A new random, fully saturated colour - sometimes bright, sometimes barely there.
				Target.Timer = FMath::FRandRange(JamGlitchMinInterval, FMath::Max(JamGlitchMinInterval, JamGlitchMaxInterval));
				Target.GlitchColor = FLinearColor::MakeFromHSV8(static_cast<uint8>(FMath::RandRange(0, 255)), 255, 255) * FMath::FRandRange(0.3f, 1.6f);
			}
		}
		MID->SetVectorParameterValue(ColorParam, FMath::Lerp(Target.BaseColor, Target.GlitchColor, JamGlitch));
	}

	return FMath::Lerp(1.f, JamGlitchBrightness, JamGlitch);
}

void ASpinCityBuilder::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	float Pulse = 0.f;
	float SpeedFactor = 0.f;
	if (const ASpinGameMode* GM = GameMode.Get())
	{
		Pulse = GM->GetHitPulse();
		const float Range = FMath::Max(1.f, GM->MaxSpeed - GM->StartSpeed);
		SpeedFactor = FMath::Clamp((GM->CurrentSpeed - GM->StartSpeed) / Range, 0.f, 1.f);
	}
	CurrentFloorSpeed = FMath::FInterpTo(CurrentFloorSpeed, SpeedFactor, DeltaSeconds, 1.5f);

	// While the hand is jammed, everything dims and flashes random colours (see UpdateJamGlitch).
	const float GlitchDim = UpdateJamGlitch(DeltaSeconds);

	const float Flare = (1.f + HitPulseBoost * Pulse) * GlitchDim;
	for (UMaterialInstanceDynamic* MID : NeonMIDs)
	{
		MID->SetScalarParameterValue(IntensityParam, NeonIntensity * Flare);
	}

	const float FloorGlow = FloorLineIntensity * (1.f + FloorSpeedBoost * CurrentFloorSpeed) * Flare;
	for (UMaterialInstanceDynamic* MID : FloorLineMIDs)
	{
		MID->SetScalarParameterValue(IntensityParam, FloorGlow);
	}

	if (BeaconMID)
	{
		// A short flash each period, like aircraft warning lights.
		const float Period = FMath::Max(0.1f, BeaconBlinkPeriod);
		const float Phase = FMath::Fmod(World->GetTimeSeconds(), Period) / Period;
		BeaconMID->SetScalarParameterValue(IntensityParam, (Phase < 0.2f ? NeonIntensity * 3.f : NeonIntensity * 0.08f) * GlitchDim);
	}

	UpdateWindows(DeltaSeconds, Flare);
	UpdateSignFlicker(DeltaSeconds, Flare);
	UpdateMotes(DeltaSeconds, World->GetTimeSeconds());
	UpdateFlyingCars(DeltaSeconds);
}

void ASpinCityBuilder::InitLife()
{
	FRandomStream Rand(Seed + 101);

	// Windows start in a random mix of on and off, each batch on its own timer.
	const int32 Groups = WindowGroups.Num();
	WindowLevel.SetNum(Groups);
	WindowTarget.SetNum(Groups);
	WindowTimer.SetNum(Groups);
	for (int32 i = 0; i < Groups; ++i)
	{
		WindowTarget[i] = WindowLevel[i] = Rand.FRand() < WindowsOnShare ? 1.f : 0.f;
		WindowTimer[i] = Rand.FRandRange(0.f, WindowSwitchMaxInterval);
	}
	for (int32 i = 0; i < 2; ++i)
	{
		FlickerWait[i] = Rand.FRandRange(1.f, 5.f);
		FlickerBurst[i] = 0.f;
		bFlickerLit[i] = true;
	}

	// Scatter the motes through their box of air.
	MoteState.Reset();
	MoteTransforms.Reset();
	if (Motes)
	{
		Motes->ClearInstances();
	}
	for (int32 i = 0; i < NumMotes; ++i)
	{
		FMote Mote;
		Mote.Position = FVector(Rand.FRandRange(MoteFieldMinX, MoteFieldMaxX), Rand.FRandRange(-MoteFieldHalfY, MoteFieldHalfY), Rand.FRandRange(MoteFieldMinZ, MoteFieldMaxZ));
		Mote.Phase = Rand.FRandRange(0.f, 2.f * PI);
		Mote.Size = Rand.FRandRange(MoteMinSize, MoteMaxSize);
		Mote.Spin = Rand.FRandRange(40.f, 160.f);
		Mote.bFlake = Rand.FRand() < 0.35f;
		MoteState.Add(Mote);
		MoteTransforms.Add(FTransform(FRotator::ZeroRotator, Mote.Position, FVector(Mote.Size) / MeshSize));
	}
	if (Motes && MoteTransforms.Num() > 0)
	{
		Motes->AddInstances(MoteTransforms, false, false);
	}

	// Flying cars. About half stream along the avenues that lead away from the camera (out and back),
	// the rest circle above the ring roads. (Avenues toward the camera are left empty, so nothing
	// flies through the shot right in front of it.)
	CarState.Reset();
	TArray<float> OpenAvenues;
	for (int32 i = 0; i < NumAvenues; ++i)
	{
		const float Angle = 2.f * PI * i / NumAvenues;
		if (FMath::Cos(Angle) > 0.2f)
		{
			OpenAvenues.Add(Angle);
		}
	}
	const int32 Rings = FMath::Max(1, FMath::FloorToInt((CityRadius - PlazaRadius) / RingRoadSpacing));
	for (int32 i = 0; i < NumFlyingCars; ++i)
	{
		FFlyingCar Car;
		Car.Height = Rand.FRandRange(FlyingCarMinHeight, FlyingCarMaxHeight);
		Car.Speed = Rand.FRandRange(FlyingCarMinSpeed, FlyingCarMaxSpeed) * (Rand.FRand() < 0.5f ? 1.f : -1.f);
		Car.bRing = OpenAvenues.Num() == 0 || Rand.FRand() < 0.5f;
		if (Car.bRing)
		{
			Car.Radius = FMath::Max(PlazaRadius + RingRoadSpacing * Rand.RandRange(1, Rings) + Rand.FRandRange(-250.f, 250.f), DialClearRadius + 500.f);
			Car.Angle = Rand.FRandRange(0.f, 2.f * PI);
		}
		else
		{
			Car.Angle = OpenAvenues[Rand.RandRange(0, OpenAvenues.Num() - 1)];
			Car.Distance = Rand.FRandRange(FMath::Max(PlazaRadius + 300.f, DialClearRadius), CityRadius * 0.85f);
			// Outbound traffic keeps to one side of the avenue, inbound to the other.
			Car.Offset = (Car.Speed > 0.f ? 1.f : -1.f) * AvenueWidth * Rand.FRandRange(0.1f, 0.35f);
		}
		CarState.Add(Car);
	}

	for (UInstancedStaticMeshComponent* ISM : { CarBodies.Get(), CarHeadlights.Get(), CarTaillights.Get() })
	{
		if (ISM)
		{
			ISM->ClearInstances();
		}
	}
	CarBodyTransforms.Init(FTransform::Identity, CarState.Num());
	CarHeadTransforms.Init(FTransform::Identity, CarState.Num());
	CarTailTransforms.Init(FTransform::Identity, CarState.Num());
	UpdateFlyingCars(0.f); // place them before adding the instances
	if (CarState.Num() > 0 && CarBodies && CarHeadlights && CarTaillights)
	{
		CarBodies->AddInstances(CarBodyTransforms, false, false);
		CarHeadlights->AddInstances(CarHeadTransforms, false, false);
		CarTaillights->AddInstances(CarTailTransforms, false, false);
	}
}

void ASpinCityBuilder::UpdateFlyingCars(float DeltaSeconds)
{
	if (CarState.Num() == 0 || CarBodyTransforms.Num() != CarState.Num())
	{
		return;
	}

	const float Start = FMath::Max(PlazaRadius + 300.f, DialClearRadius);
	const float End = CityRadius * 0.85f;

	for (int32 i = 0; i < CarState.Num(); ++i)
	{
		FFlyingCar& Car = CarState[i];
		FVector Position;
		FVector Forward;

		if (Car.bRing)
		{
			Car.Angle += Car.Speed / FMath::Max(1.f, Car.Radius) * DeltaSeconds;
			const FVector Out(FMath::Cos(Car.Angle), FMath::Sin(Car.Angle), 0.f);
			Position = Out * Car.Radius + FVector(0.f, 0.f, Car.Height);
			Forward = FVector(-Out.Y, Out.X, 0.f) * FMath::Sign(Car.Speed);
		}
		else
		{
			// Run the length of the avenue, then come round again from the other end.
			Car.Distance += Car.Speed * DeltaSeconds;
			if (Car.Distance > End)
			{
				Car.Distance -= End - Start;
			}
			else if (Car.Distance < Start)
			{
				Car.Distance += End - Start;
			}
			const FVector Dir(FMath::Cos(Car.Angle), FMath::Sin(Car.Angle), 0.f);
			const FVector Side(-Dir.Y, Dir.X, 0.f);
			Position = Dir * Car.Distance + Side * Car.Offset + FVector(0.f, 0.f, Car.Height);
			Forward = Dir * FMath::Sign(Car.Speed);
		}

		// Farther cars are drawn a little bigger, so they still read as more than a speck.
		const float FromCamera = (FVector2D(Position) - FVector2D(-CameraBackDistance, 0.f)).Size();
		const float Scale = FMath::Clamp(FMath::Sqrt(FromCamera / FMath::Max(1.f, CameraBackDistance + PlazaRadius)), 1.f, 3.f);
		const FRotator Rot = Forward.Rotation();

		CarBodyTransforms[i] = FTransform(Rot, Position, FVector(130.f, 52.f, 30.f) * Scale / MeshSize);
		CarHeadTransforms[i] = FTransform(Rot, Position + Forward * (66.f * Scale), FVector(6.f, 46.f, 12.f) * Scale / MeshSize);
		CarTailTransforms[i] = FTransform(Rot, Position - Forward * (66.f * Scale), FVector(6.f, 48.f, 9.f) * Scale / MeshSize);
	}

	if (DeltaSeconds > 0.f && CarBodies && CarHeadlights && CarTaillights)
	{
		CarBodies->BatchUpdateInstancesTransforms(0, CarBodyTransforms, false, true);
		CarHeadlights->BatchUpdateInstancesTransforms(0, CarHeadTransforms, false, true);
		CarTaillights->BatchUpdateInstancesTransforms(0, CarTailTransforms, false, true);
	}
}

void ASpinCityBuilder::UpdateWindows(float DeltaSeconds, float Flare)
{
	// Every so often a batch decides whether to switch, then snaps over in a quick fade - like a
	// room's lights going on or off. Each batch is scattered all over the city, so it reads as single
	// windows changing here and there rather than whole blocks.
	for (int32 i = 0; i < WindowMIDs.Num() && i < WindowLevel.Num(); ++i)
	{
		WindowTimer[i] -= DeltaSeconds;
		if (WindowTimer[i] <= 0.f)
		{
			WindowTimer[i] = FMath::FRandRange(WindowSwitchMinInterval, WindowSwitchMaxInterval);
			WindowTarget[i] = FMath::FRand() < WindowsOnShare ? 1.f : 0.f;
		}
		WindowLevel[i] = FMath::FInterpConstantTo(WindowLevel[i], WindowTarget[i], DeltaSeconds, 6.f);
		WindowMIDs[i]->SetScalarParameterValue(IntensityParam, NeonIntensity * FMath::Lerp(0.04f, 1.f, WindowLevel[i]) * Flare);
	}
}

void ASpinCityBuilder::UpdateSignFlicker(float DeltaSeconds, float Flare)
{
	// Faulty neon: steady most of the time, then a burst of rapid stuttering, then steady again.
	for (int32 i = 0; i < FlickerMIDs.Num() && i < 2; ++i)
	{
		if (FlickerBurst[i] > 0.f)
		{
			FlickerBurst[i] -= DeltaSeconds;
			FlickerStepTimer[i] -= DeltaSeconds;
			if (FlickerStepTimer[i] <= 0.f)
			{
				FlickerStepTimer[i] = FMath::FRandRange(0.03f, 0.12f);
				bFlickerLit[i] = FMath::FRand() < 0.45f;
			}
			if (FlickerBurst[i] <= 0.f)
			{
				bFlickerLit[i] = true;
				FlickerWait[i] = FMath::FRandRange(1.5f, 6.f);
			}
		}
		else
		{
			FlickerWait[i] -= DeltaSeconds;
			if (FlickerWait[i] <= 0.f)
			{
				FlickerBurst[i] = FMath::FRandRange(0.3f, 1.1f);
				FlickerStepTimer[i] = 0.f;
			}
		}
		FlickerMIDs[i]->SetScalarParameterValue(IntensityParam, NeonIntensity * (bFlickerLit[i] ? 1.f : 0.05f) * Flare);
	}
}

void ASpinCityBuilder::UpdateMotes(float DeltaSeconds, float Time)
{
	if (!Motes || MoteState.Num() == 0 || MoteTransforms.Num() != MoteState.Num())
	{
		return;
	}

	// A steady breeze that gusts and eases, with each mote swirling and bobbing on its own.
	const float Gust = 1.f + 0.5f * FMath::Sin(Time * 0.27f) + 0.25f * FMath::Sin(Time * 1.13f + 1.7f);
	const FVector2D Dir = WindDirection.GetSafeNormal();
	const FVector Wind(Dir.X * WindSpeed * Gust, Dir.Y * WindSpeed * Gust, 0.f);

	const FVector FieldMin(MoteFieldMinX, -MoteFieldHalfY, MoteFieldMinZ);
	const FVector FieldSize(MoteFieldMaxX - MoteFieldMinX, MoteFieldHalfY * 2.f, MoteFieldMaxZ - MoteFieldMinZ);

	for (int32 i = 0; i < MoteState.Num(); ++i)
	{
		FMote& M = MoteState[i];
		const FVector Swirl(
			FMath::Sin(Time * 0.9f + M.Phase) * 70.f,
			FMath::Cos(Time * 0.7f + M.Phase * 1.3f) * 70.f,
			FMath::Sin(Time * 1.3f + M.Phase) * 40.f - 12.f);
		M.Position += (Wind + Swirl) * DeltaSeconds;

		// Wrap around inside the box, so the air never empties.
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (M.Position[Axis] < FieldMin[Axis])
			{
				M.Position[Axis] += FieldSize[Axis];
			}
			else if (M.Position[Axis] > FieldMin[Axis] + FieldSize[Axis])
			{
				M.Position[Axis] -= FieldSize[Axis];
			}
		}

		// Specks are little tumbling cubes; flakes are flat and catch the light as they turn.
		const FRotator Spin(Time * M.Spin + M.Phase * 57.f, Time * M.Spin * 0.7f, Time * M.Spin * 0.5f);
		FVector Scale = M.bFlake ? FVector(M.Size * 1.6f, M.Size, M.Size * 0.08f) : FVector(M.Size * 0.6f);

		// Keep the air over the plaza clear: motes shrink away as they drift in toward the dial and
		// grow back as they leave.
		const float FromDial = FVector2D(M.Position).Size();
		Scale *= FMath::Clamp((FromDial - PlazaRadius) / 600.f, 0.f, 1.f);
		MoteTransforms[i] = FTransform(Spin, M.Position, Scale / MeshSize);
	}

	Motes->BatchUpdateInstancesTransforms(0, MoteTransforms, false, true);
}

// ---------------------------------------------------------------------------------------------
// Building blocks
// ---------------------------------------------------------------------------------------------

bool ASpinCityBuilder::IsInView(const FVector2D& Pos) const
{
	const float Dist = Pos.Size();
	// In front of the camera = within about 110 degrees either side of straight ahead (+X).
	return Dist <= FullCircleRadius || Pos.X / Dist > -0.34f;
}

int32 ASpinCityBuilder::HullBandAt(const FVector2D& Pos) const
{
	const float FromCamera = (Pos - FVector2D(-CameraBackDistance, 0.f)).Size();
	const float T = FMath::Clamp((FromCamera - DepthHazeStart) / FMath::Max(1.f, DepthHazeEnd - DepthHazeStart), 0.f, 1.f);
	return FMath::Clamp(FMath::RoundToInt(T * (NumHullBands - 1)), 0, NumHullBands - 1);
}

float ASpinCityBuilder::LineWidthAt(const FVector2D& Pos) const
{
	// Measured from where the camera roughly sits, relative to the front edge of the plaza.
	const float FromCamera = (Pos - FVector2D(-CameraBackDistance, 0.f)).Size();
	const float Reference = FMath::Max(1.f, CameraBackDistance + PlazaRadius);
	const float Scale = FMath::Pow(FMath::Max(FromCamera, 1.f) / Reference, LineDepthExponent);
	return LineWidth * FMath::Max(Scale, 0.35f);
}

void ASpinCityBuilder::AddHull(const FVector& Center, const FVector& Size, float Yaw, bool bRound)
{
	const int32 Band = HullBandAt(FVector2D(Center));
	Pending.FindOrAdd(bRound ? HullCylinders[Band].Get() : HullBoxes[Band].Get())
		.Add(FTransform(FRotator(0.f, Yaw, 0.f), Center, Size / MeshSize));
	BuiltRadius = FMath::Max(BuiltRadius, FVector2D(Center).Size() + FMath::Max(Size.X, Size.Y) * 0.5f);
}

void ASpinCityBuilder::AddNeonBox(int32 Slot, const FVector& Center, const FVector& Size, float Yaw)
{
	Pending.FindOrAdd(NeonBoxes[FMath::Abs(Slot) % NumSlots].Get())
		.Add(FTransform(FRotator(0.f, Yaw, 0.f), Center, Size / MeshSize));
}

void ASpinCityBuilder::AddNeonDisc(int32 Slot, const FVector& Center, float Radius, float Thickness)
{
	Pending.FindOrAdd(NeonDiscs[FMath::Abs(Slot) % NumSlots].Get())
		.Add(FTransform(FRotator::ZeroRotator, Center, FVector(Radius * 2.f, Radius * 2.f, Thickness) / MeshSize));
}

void ASpinCityBuilder::AddNeonLine(int32 Slot, const FVector& A, const FVector& B, float Width)
{
	const FVector Delta = B - A;
	const float Length = Delta.Size();
	if (Length < KINDA_SMALL_NUMBER)
	{
		return;
	}
	Pending.FindOrAdd(NeonBoxes[FMath::Abs(Slot) % NumSlots].Get())
		.Add(FTransform(Delta.Rotation(), (A + B) * 0.5f, FVector(Length + Width, Width, Width) / MeshSize));
	BuiltRadius = FMath::Max(BuiltRadius, FMath::Max(FVector2D(A).Size(), FVector2D(B).Size()));
}

void ASpinCityBuilder::AddFloorLine(UInstancedStaticMeshComponent* ISM, const FVector2D& A, const FVector2D& B, float Width)
{
	const FVector2D Delta = B - A;
	const float Length = Delta.Size();
	if (Length < KINDA_SMALL_NUMBER)
	{
		return;
	}
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
	const FVector2D Mid = (A + B) * 0.5f;
	const float FloorWidth = FMath::Max(Width, MinFloorLineWidth);
	Pending.FindOrAdd(ISM).Add(FTransform(FRotator(0.f, Yaw, 0.f), FVector(Mid.X, Mid.Y, 1.f), FVector(Length, FloorWidth, 2.f) / MeshSize));
}

void ASpinCityBuilder::AddFloorRing(UInstancedStaticMeshComponent* ISM, float Radius, float WidthScale, bool bFullCircle)
{
	const int32 Segments = FMath::Clamp(FMath::CeilToInt(2.f * PI * Radius / 900.f), 48, 900);
	const float Step = 2.f * PI / Segments;
	for (int32 i = 0; i < Segments; ++i)
	{
		const FVector2D A(Radius * FMath::Cos(i * Step), Radius * FMath::Sin(i * Step));
		const FVector2D B(Radius * FMath::Cos((i + 1) * Step), Radius * FMath::Sin((i + 1) * Step));
		const FVector2D Mid = (A + B) * 0.5f;
		if (bFullCircle || IsInView(Mid))
		{
			// Stretch each chord a touch so neighbours overlap with no gaps.
			AddFloorLine(ISM, Mid + (A - Mid) * 1.04f, Mid + (B - Mid) * 1.04f, LineWidthAt(Mid) * WidthScale);
		}
	}
}

void ASpinCityBuilder::AddBeacon(const FVector& At, float Size)
{
	Pending.FindOrAdd(Beacons.Get()).Add(FTransform(FRotator::ZeroRotator, At, FVector(Size) / MeshSize));
}

void ASpinCityBuilder::OutlineBox(int32 Slot, const FVector& Base, const FVector& Size, float Yaw, float Width, bool bVerticals)
{
	const FRotator Rot(0.f, Yaw, 0.f);
	const float HX = Size.X * 0.5f;
	const float HY = Size.Y * 0.5f;
	const float Top = Base.Z + Size.Z;

	if (bVerticals)
	{
		for (const FVector2D& Corner : { FVector2D(HX, HY), FVector2D(HX, -HY), FVector2D(-HX, HY), FVector2D(-HX, -HY) })
		{
			const FVector Offset = Rot.RotateVector(FVector(Corner.X, Corner.Y, 0.f));
			AddNeonBox(Slot, FVector(Base.X, Base.Y, Base.Z + Size.Z * 0.5f) + Offset, FVector(Width, Width, Size.Z), Yaw);
		}
	}

	// The rim at the top (also used on its own for the horizontal bands up a tower).
	AddNeonBox(Slot, FVector(Base.X, Base.Y, Top) + Rot.RotateVector(FVector(0.f, HY, 0.f)), FVector(Size.X + Width, Width, Width), Yaw);
	AddNeonBox(Slot, FVector(Base.X, Base.Y, Top) + Rot.RotateVector(FVector(0.f, -HY, 0.f)), FVector(Size.X + Width, Width, Width), Yaw);
	AddNeonBox(Slot, FVector(Base.X, Base.Y, Top) + Rot.RotateVector(FVector(HX, 0.f, 0.f)), FVector(Width, Size.Y + Width, Width), Yaw);
	AddNeonBox(Slot, FVector(Base.X, Base.Y, Top) + Rot.RotateVector(FVector(-HX, 0.f, 0.f)), FVector(Width, Size.Y + Width, Width), Yaw);
}

// ---------------------------------------------------------------------------------------------
// The city
// ---------------------------------------------------------------------------------------------

void ASpinCityBuilder::Build()
{
	if (NeonBoxes.Num() < NumSlots || NeonDiscs.Num() < NumSlots || WindowGroups.Num() < NumWindowGroups * 2 || FlickerSigns.Num() < 2)
	{
		return;
	}

	if (HullBoxes.Num() < NumHullBands || HullCylinders.Num() < NumHullBands)
	{
		return;
	}

	TArray<UInstancedStaticMeshComponent*> All = { RingRoadLines, AvenueLines, TrafficLines, Beacons };
	All.Append(HullBoxes);
	All.Append(HullCylinders);
	for (int32 i = 0; i < NumSlots; ++i)
	{
		All.Add(NeonBoxes[i]);
		All.Add(NeonDiscs[i]);
	}
	All.Append(WindowGroups);
	All.Append(FlickerSigns);
	for (UInstancedStaticMeshComponent* ISM : All)
	{
		ISM->ClearInstances();
	}

	Pending.Reset();
	BuiltRadius = 0.f;

	FRandomStream Rand(Seed);
	BuildFloor();
	BuildPlaza();
	BuildRiver();
	BuildStreetLamps();
	BuildMaglev();
	BuildDistricts(Rand);
	BuildLandmarks(Rand);

	for (TPair<UInstancedStaticMeshComponent*, TArray<FTransform>>& Pair : Pending)
	{
		Pair.Key->AddInstances(Pair.Value, false, false);
	}
	Pending.Reset();
}

void ASpinCityBuilder::BuildFloor()
{
	// One huge dark slab whose top is at Z = 0, far wider than the camera can ever see.
	Floor->SetRelativeScale3D(FVector(FloorRadius * 2.f, FloorRadius * 2.f, 10.f) / MeshSize);
	Floor->SetRelativeLocation(FVector(0.f, 0.f, -5.f));

	const float LinesTo = FloorRadius * 0.6f;

	// Ring roads: a pair of lines along each one, then wider-spaced rings out toward the horizon.
	float Spacing = RingRoadSpacing;
	for (float Road = PlazaRadius + RingRoadSpacing; Road < LinesTo; Road += Spacing)
	{
		const bool bFull = Road <= FullCircleRadius;
		AddFloorRing(RingRoadLines, Road, 1.f, bFull);
		AddFloorRing(RingRoadLines, Road + RingRoadWidth, 1.f, bFull);
		if (Road > CityRadius)
		{
			Spacing *= 1.25f;
		}
	}

	// Avenues: a line along each side, running from the plaza to the horizon (in pieces, so each can
	// widen with distance).
	for (int32 i = 0; i < NumAvenues; ++i)
	{
		if (i == 0 && bRiver)
		{
			continue; // straight ahead is the river (see BuildRiver)
		}
		const float Angle = 2.f * PI * i / NumAvenues;
		const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
		const FVector2D Side(-Dir.Y, Dir.X);
		const float Reach = IsInView(Dir * LinesTo) ? LinesTo : FullCircleRadius;

		for (const float S : { -0.5f, 0.5f })
		{
			const FVector2D Offset = Side * (AvenueWidth * S);
			for (float D = PlazaRadius; D < Reach; D += 3000.f)
			{
				const float End = FMath::Min(D + 3000.f, Reach);
				AddFloorLine(AvenueLines, Dir * D + Offset, Dir * End + Offset, LineWidthAt(Dir * ((D + End) * 0.5f) + Offset));
			}
		}
	}
}

void ASpinCityBuilder::BuildPlaza()
{
	// A giant clock face on the floor around the dial: rings, and 60 ticks with longer "hours".
	for (const float R : { 900.f, 1300.f, 1950.f })
	{
		AddFloorRing(RingRoadLines, R, 1.f, true);
	}
	AddFloorRing(AvenueLines, PlazaRadius, 1.5f, true);

	for (int32 i = 0; i < 60; ++i)
	{
		const float Angle = 2.f * PI * i / 60.f;
		const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
		const bool bHour = i % 5 == 0;
		AddFloorLine(bHour ? AvenueLines : RingRoadLines, Dir * 1350.f, Dir * (bHour ? 1900.f : 1600.f), LineWidthAt(Dir * 1600.f) * (bHour ? 2.f : 1.f));
	}

}

void ASpinCityBuilder::BuildStreetLamps()
{
	if (StreetLampSpacing <= 0.f)
	{
		return;
	}

	// Slim poles along both kerbs of every avenue, each with an arm reaching over the road and a
	// warm light at its end - a rhythm of small lights leading the eye out from the dial.
	constexpr float PoleHeight = 600.f;
	for (int32 i = 0; i < NumAvenues; ++i)
	{
		const float AngleDeg = 360.f * i / NumAvenues;
		const float Rad = FMath::DegreesToRadians(AngleDeg);
		const FVector Dir(FMath::Cos(Rad), FMath::Sin(Rad), 0.f);
		const FVector Side(-Dir.Y, Dir.X, 0.f);

		for (float D = PlazaRadius + 700.f; D < FMath::Min(StreetLampReach, CityRadius); D += StreetLampSpacing)
		{
			for (const float S : { -1.f, 1.f })
			{
				const FVector Base = Dir * D + Side * (S * (AvenueWidth * 0.5f + 110.f));
				if (!IsInView(FVector2D(Base)))
				{
					continue;
				}
				const float W = LineWidthAt(FVector2D(Base));
				AddHull(Base + FVector(0.f, 0.f, PoleHeight * 0.5f), FVector(20.f, 20.f, PoleHeight), AngleDeg);
				AddHull(Base - Side * (S * 90.f) + FVector(0.f, 0.f, PoleHeight), FVector(16.f, 190.f, 14.f), AngleDeg);
				AddNeonBox(4, Base - Side * (S * 170.f) + FVector(0.f, 0.f, PoleHeight - 14.f), FVector(FMath::Max(W * 3.f, 34.f), 120.f, 14.f), AngleDeg);
			}
		}
	}
}

void ASpinCityBuilder::BuildMaglev()
{
	if (MaglevRingRoad <= 0)
	{
		return;
	}
	const float Radius = PlazaRadius + MaglevRingRoad * RingRoadSpacing + RingRoadWidth * 0.5f;
	if (Radius > CityRadius)
	{
		return;
	}

	// A raised track circling the city above a ring road: a dark deck on pillars, two bright rails,
	// and a purple glow along its underside.
	const int32 Segments = FMath::CeilToInt(2.f * PI * Radius / 1400.f);
	for (int32 i = 0; i < Segments; ++i)
	{
		const float A0 = 2.f * PI * i / Segments;
		const float A1 = 2.f * PI * (i + 1) / Segments;
		const float AMid = (A0 + A1) * 0.5f;
		const FVector2D Mid(Radius * FMath::Cos(AMid), Radius * FMath::Sin(AMid));
		if (!IsInView(Mid))
		{
			continue;
		}

		const FVector Out0(FMath::Cos(A0), FMath::Sin(A0), 0.f);
		const FVector Out1(FMath::Cos(A1), FMath::Sin(A1), 0.f);
		const float Chord = 2.f * Radius * FMath::Sin((A1 - A0) * 0.5f);
		const float W = LineWidthAt(Mid);
		const FVector Up(0.f, 0.f, MaglevHeight);

		AddHull(FVector(Mid.X, Mid.Y, MaglevHeight), FVector(Chord * 1.02f, 260.f, 70.f), FMath::RadiansToDegrees(AMid) + 90.f);
		for (const float Rail : { -115.f, 115.f })
		{
			AddNeonLine(0, Out0 * (Radius + Rail) + Up + FVector(0.f, 0.f, 45.f), Out1 * (Radius + Rail) + Up + FVector(0.f, 0.f, 45.f), W);
		}
		AddNeonLine(3, Out0 * Radius + Up - FVector(0.f, 0.f, 40.f), Out1 * Radius + Up - FVector(0.f, 0.f, 40.f), W * 1.5f);

		if (i % 2 == 0 && !IsOnBridgeRoad(FVector2D(Out0 * Radius), 110.f))
		{
			AddHull(Out0 * Radius + FVector(0.f, 0.f, MaglevHeight * 0.5f), FVector(110.f, 110.f, MaglevHeight), FMath::RadiansToDegrees(A0));
		}
	}
}

void ASpinCityBuilder::BuildDistricts(FRandomStream& Rand)
{
	// Blocks are laid out in "half-wedges": from an avenue to the line halfway to the next one. How
	// the layouts repeat depends on Symmetry:
	//   Kaleidoscope - one half-wedge, mirrored and repeated all the way round.
	//   Mirror       - a different layout for each half-wedge on one side of the view, mirrored onto the other.
	//   None         - a different layout for every half-wedge.
	// Each copy of a block is built from the same seed, so mirrored and repeated ones match exactly.
	const int32 Wedges = NumAvenues > 0 ? NumAvenues : 12;
	const int32 HalfWedges = Wedges * 2;
	const float HalfDeg = 180.f / Wedges;
	const int32 Layouts = Symmetry == ESpinCitySymmetry::Kaleidoscope ? 1 : (Symmetry == ESpinCitySymmetry::Mirror ? Wedges : HalfWedges);

	for (float Dist = FMath::Max(PlazaRadius, DialClearRadius) + BlockSize * 0.5f; Dist < CityRadius; Dist += BlockSize)
	{
		// Slots across a half-wedge at this distance, spaced so the ones nearest its edges sit half a
		// slot in - so neighbouring half-wedges' blocks are always a slot apart.
		const int32 Slots = FMath::FloorToInt(Dist * FMath::DegreesToRadians(HalfDeg) / BlockSize);
		const float T = FMath::Clamp((Dist - PlazaRadius) / FMath::Max(1.f, CityRadius - PlazaRadius), 0.f, 1.f);

		for (int32 Layout = 0; Layout < Layouts; ++Layout)
		{
			for (int32 Slot = 0; Slot < Slots; ++Slot)
			{
				const bool bUsed = Rand.FRand() <= BuildingDensity;
				const float Local = HalfDeg * (Slot + 0.5f + Rand.FRandRange(-0.15f, 0.15f)) / Slots;
				const float R = Dist + Rand.FRandRange(-0.15f, 0.15f) * BlockSize;
				const float Footprint = BlockSize * Rand.FRandRange(0.4f, 0.72f);
				const int32 BlockSeed = Rand.RandHelper(MAX_int32);
				float Height = FMath::Lerp(MinHeight, MaxHeight, FMath::Pow(T, HeightCurve)) * Rand.FRandRange(0.55f, 1.45f);
				if (Rand.FRand() < 0.06f)
				{
					Height *= 1.8f; // the odd standout tower
				}
				if (!bUsed)
				{
					continue;
				}

				// Even half-wedges start at an avenue, odd ones end at one. (In the kaleidoscope, Local
				// is always measured from the avenue.)
				const bool bFromAvenue = Symmetry == ESpinCitySymmetry::Kaleidoscope || Layout % 2 == 0;
				const float FromAvenueDeg = bFromAvenue ? Local : HalfDeg - Local;

				// Keep clear of the avenue...
				if (NumAvenues > 0 && R * FMath::DegreesToRadians(FromAvenueDeg) < AvenueWidth * 0.5f + Footprint * 0.75f)
				{
					continue;
				}
				// ...of the half-wedge's other edge...
				if (R * FMath::DegreesToRadians(HalfDeg - FromAvenueDeg) < Footprint * 0.75f)
				{
					continue;
				}
				// ...and of the ring roads.
				const int32 RoadIndex = FMath::RoundToInt((R - PlazaRadius) / RingRoadSpacing);
				const float Road = PlazaRadius + RoadIndex * RingRoadSpacing;
				if (RoadIndex >= 1 && R + Footprint * 0.75f > Road && R - Footprint * 0.75f < Road + RingRoadWidth)
				{
					continue;
				}

				// Every angle this block appears at.
				TArray<float, TInlineAllocator<48>> Angles;
				switch (Symmetry)
				{
				case ESpinCitySymmetry::Kaleidoscope:
					for (int32 H = 0; H < HalfWedges; ++H)
					{
						Angles.Add(H * HalfDeg + (H % 2 == 0 ? Local : HalfDeg - Local));
					}
					break;
				case ESpinCitySymmetry::Mirror:
					Angles.Add(Layout * HalfDeg + Local);
					Angles.Add(-(Layout * HalfDeg + Local));
					break;
				default:
					Angles.Add(Layout * HalfDeg + Local);
					break;
				}

				for (const float AngleDeg : Angles)
				{
					const float Rad = FMath::DegreesToRadians(AngleDeg);
					const FVector2D Pos(R * FMath::Cos(Rad), R * FMath::Sin(Rad));
					if (!IsInView(Pos))
					{
						continue;
					}

					// Between the camera and the dial, stay well under the line of sight. (This lane is
					// mirrored across the view too, so left and right still match.)
					float CopyHeight = Height;
					if (Pos.X < 0.f && FMath::Abs(Pos.Y) < CameraLaneHalfWidth + Footprint * 0.5f)
					{
						CopyHeight = FMath::Min(CopyHeight, FMath::Max(80.f, -Pos.X * 0.25f));
					}

					// Everything stands on one street grid, like a real city - except in the kaleidoscope,
					// where each block turns with its wedge so the pattern repeats exactly.
					const float BlockYaw = Symmetry == ESpinCitySymmetry::Kaleidoscope ? AngleDeg : 0.f;
					FRandomStream BlockRand(BlockSeed);
					if (!IsInRiverZone(Pos, Footprint) && Pos.Size() - Footprint * 0.75f >= DialClearRadius)
					{
						BuildBlock(BlockRand, Pos, BlockYaw, Footprint, CopyHeight, R);
					}
				}
			}
		}
	}
}

void ASpinCityBuilder::BuildBlock(FRandomStream& Rand, const FVector2D& Pos, float Yaw, float Footprint, float Height, float Dist)
{
	// Parks thin out toward the edge, where the towers take over.
	const float T = FMath::Clamp((Dist - PlazaRadius) / FMath::Max(1.f, CityRadius - PlazaRadius), 0.f, 1.f);
	if (Rand.FRand() < ParkChance * (1.3f - T))
	{
		BuildPark(Rand, Pos, Yaw, BlockSize * 0.8f);
	}
	else
	{
		BuildTower(Rand, Pos, Yaw, Footprint, Height, Dist);
	}
}

void ASpinCityBuilder::BuildTower(FRandomStream& Rand, const FVector2D& Pos, float Yaw, float Footprint, float Height, float Dist)
{
	const float T = FMath::Clamp((Dist - PlazaRadius) / FMath::Max(1.f, CityRadius - PlazaRadius), 0.f, 1.f);
	const float W = LineWidthAt(Pos);
	const bool bDetailed = Dist < 22000.f; // floor bands are too fine to see much farther out

	// Mostly cyberpunk magenta and cyan, with purple, blue and amber mixed in - hotter farther out.
	static const int32 NearColors[] = { 0, 0, 2, 1, 3 };
	static const int32 FarColors[] = { 2, 2, 0, 3, 4, 1 };
	const int32 Color = T < 0.4f ? NearColors[Rand.RandRange(0, UE_ARRAY_COUNT(NearColors) - 1)] : FarColors[Rand.RandRange(0, UE_ARRAY_COUNT(FarColors) - 1)];
	const int32 Accent = (Color + Rand.RandRange(1, NumPalette - 1)) % NumPalette;
	const FVector Base(Pos.X, Pos.Y, 0.f);

	const float Roll = Rand.FRand();
	int32 Style = Roll < 0.4f ? 0 : Roll < 0.6f ? 1 : Roll < 0.7f ? 2 : Roll < 0.78f ? 3 : 4;
	if (Style == 3 && Height < 2000.f)
	{
		Style = 0; // spires need some height to read
	}

	float Top = Height;

	// The main mass of the tower, for hanging signs on (see BuildFacade).
	float BodyX = Footprint;
	float BodyY = Footprint;
	float BodyHeight = Height;

	switch (Style)
	{
	case 0: // Plain tower: outlined, often banded floor by floor.
	{
		const FVector Size(Footprint * Rand.FRandRange(0.6f, 1.f), Footprint * Rand.FRandRange(0.6f, 1.f), Height);
		BodyX = Size.X;
		BodyY = Size.Y;
		AddHull(Base + FVector(0.f, 0.f, Height * 0.5f), Size, Yaw);
		OutlineBox(Color, Base, Size, Yaw, W);
		if (bDetailed && Rand.FRand() < 0.6f)
		{
			const float Spacing = Rand.FRandRange(250.f, 700.f) * (1.f + T);
			const int32 Bands = FMath::Min(FMath::FloorToInt(Height / Spacing) - 1, 14);
			for (int32 b = 1; b <= Bands; ++b)
			{
				OutlineBox(b % 3 == 0 ? Accent : Color, Base, FVector(Size.X, Size.Y, b * Spacing), Yaw, W * 0.7f, false);
			}
		}
		break;
	}

	case 1: // Stepped: two or three tiers, each narrower than the one below.
	{
		const int32 Tiers = Rand.RandRange(2, 3);
		const float Fractions[3] = { 0.5f, 0.3f, 0.2f };
		float Z = 0.f;
		float Side = Footprint;
		for (int32 Tier = 0; Tier < Tiers; ++Tier)
		{
			const float TierHeight = Height * (Tiers == 2 ? (Tier == 0 ? 0.65f : 0.35f) : Fractions[Tier]);
			const FVector Size(Side, Side, TierHeight);
			if (Tier == 0)
			{
				BodyHeight = TierHeight;
			}
			AddHull(FVector(Pos.X, Pos.Y, Z + TierHeight * 0.5f), Size, Yaw);
			OutlineBox(Tier % 2 == 0 ? Color : Accent, FVector(Pos.X, Pos.Y, Z), Size, Yaw, W);
			Z += TierHeight;
			Side *= 0.68f;
		}
		break;
	}

	case 2: // Round tower ringed with glowing bands, with a lit crown.
	{
		const float Radius = Footprint * 0.45f;
		BodyX = BodyY = Radius * 2.f;
		AddHull(Base + FVector(0.f, 0.f, Height * 0.5f), FVector(Radius * 2.f, Radius * 2.f, Height), Yaw, true);
		const float Spacing = Rand.FRandRange(300.f, 800.f) * (1.f + T);
		const int32 Rings = FMath::Min(FMath::FloorToInt(Height / Spacing), bDetailed ? 16 : 4);
		// Each band is a disc just wider than the tower, so only its glowing rim shows outside the hull.
		for (int32 r = 1; r <= Rings; ++r)
		{
			AddNeonDisc(r % 4 == 0 ? Accent : Color, Base + FVector(0.f, 0.f, Height * r / (Rings + 1)), Radius + W * 0.6f, W * 1.2f);
		}
		BuildHaloRing(Accent, Base, Radius + W * 0.6f, Height, W * 1.5f, Yaw);
		break;
	}

	case 3: // Spire: a podium with a needle rising out of it.
	{
		const FVector Podium(Footprint, Footprint, Height * 0.25f);
		BodyHeight = Podium.Z;
		AddHull(Base + FVector(0.f, 0.f, Podium.Z * 0.5f), Podium, Yaw);
		OutlineBox(Color, Base, Podium, Yaw, W);

		const FVector Needle(Footprint * 0.2f, Footprint * 0.2f, Height);
		AddHull(Base + FVector(0.f, 0.f, Height * 0.5f), Needle, Yaw);
		OutlineBox(Accent, Base, Needle, Yaw, W);
		AddNeonBox(Accent, Base + FVector(0.f, 0.f, Height + Footprint * 0.15f), FVector(W * 2.f, W * 2.f, Footprint * 0.3f), Yaw);
		Top = Height + Footprint * 0.3f;
		break;
	}

	default: // Slab: broad and thin, its face to the camera striped with vertical light.
	{
		const FVector Size(Footprint * 0.45f, Footprint * 1.4f, Height);
		BodyX = Size.X;
		BodyY = Size.Y;
		AddHull(Base + FVector(0.f, 0.f, Height * 0.5f), Size, Yaw);
		OutlineBox(Color, Base, Size, Yaw, W);

		// Local -X is the broad face toward the camera (or toward the dial, in the kaleidoscope).
		const FRotator Rot(0.f, Yaw, 0.f);
		const int32 Stripes = Rand.RandRange(3, 6);
		for (int32 s = 1; s <= Stripes; ++s)
		{
			const float Across = Size.Y * (static_cast<float>(s) / (Stripes + 1) - 0.5f);
			const FVector Offset = Rot.RotateVector(FVector(-Size.X * 0.5f - W * 0.3f, Across, 0.f));
			AddNeonBox(Accent, Base + Offset + FVector(0.f, 0.f, Height * 0.5f), FVector(W * 0.7f, W * 0.7f, Height * 0.92f), Yaw);
		}
		break;
	}
	}

	if (Top > BeaconMinHeight)
	{
		AddBeacon(Base + FVector(0.f, 0.f, Top + W * 3.f), W * 4.f);
	}

	// Signs, windows and rooftop clutter (only on the street grid, where walls face the camera squarely).
	if (Yaw == 0.f)
	{
		BuildFacade(Rand, Pos, BodyX, BodyY, BodyHeight, W, bDetailed);
	}
}

void ASpinCityBuilder::BuildFacade(FRandomStream& Rand, const FVector2D& Pos, float BodyX, float BodyY, float BodyHeight, float Width, bool bDetailed)
{
	// Signs come in the classic hot neon colours: magenta, cyan, amber, purple.
	static const int32 SignSlots[] = { 2, 0, 4, 3, 2 };
	auto PickSign = [&Rand]() { return SignSlots[Rand.RandRange(0, UE_ARRAY_COUNT(SignSlots) - 1)]; };

	const float HalfX = BodyX * 0.5f;
	const float HalfY = BodyY * 0.5f;
	const float FrontX = Pos.X - HalfX; // the wall facing the camera

	// A tall vertical sign, the classic cyberpunk shop sign.
	if (BodyHeight > 500.f && Rand.FRand() < 0.6f)
	{
		const float SignWidth = FMath::Clamp(BodyY * 0.2f, 70.f, 320.f);
		const float SignHeight = FMath::Min(BodyHeight * Rand.FRandRange(0.25f, 0.6f), SignWidth * 7.f);
		const float Z = SignHeight * 0.5f + Rand.FRandRange(0.05f, 0.9f) * (BodyHeight - SignHeight);
		AddSign(Rand, PickSign(), FVector(FrontX, Pos.Y + Rand.FRandRange(-0.32f, 0.32f) * BodyY, Z), 180.f, SignWidth, SignHeight, Width);
	}

	// A big billboard across the front of larger towers.
	if (BodyHeight > 1200.f && BodyY > 500.f && Rand.FRand() < 0.35f)
	{
		const float BoardWidth = BodyY * 0.75f;
		const float BoardHeight = BoardWidth * Rand.FRandRange(0.35f, 0.55f);
		const float Z = FMath::Min(BodyHeight * Rand.FRandRange(0.5f, 0.85f), BodyHeight - BoardHeight * 0.6f);
		AddSign(Rand, PickSign(), FVector(FrontX, Pos.Y, Z), 180.f, BoardWidth, BoardHeight, Width);
	}

	// Lit windows scattered over the front wall (and the inner side wall), switching on and off as
	// the city lives (see UpdateWindows). Bigger, fewer ones far away, where small ones wouldn't show.
	{
		const int32 Windows = bDetailed ? Rand.RandRange(8, 22) : Rand.RandRange(3, 8);
		const float WindowHeight = FMath::Max(Width * 1.2f, 10.f);
		for (int32 i = 0; i < Windows; ++i)
		{
			const bool bAmber = Rand.FRand() < 0.35f;
			const float Length = Rand.FRandRange(50.f, 200.f) * (bDetailed ? 1.f : 2.f);
			const float Z = Rand.FRandRange(0.05f, 0.95f) * BodyHeight;
			if (Rand.FRand() < 0.75f || FMath::Abs(Pos.Y) < HalfY + 200.f)
			{
				const FVector At(FrontX - Width * 0.3f, Pos.Y + Rand.FRandRange(-0.42f, 0.42f) * BodyY, Z);
				AddWindow(Rand, bAmber, At, FVector(Width * 0.5f, FMath::Min(Length, BodyY * 0.6f), WindowHeight), 180.f);
			}
			else
			{
				const float Toward = Pos.Y > 0.f ? -1.f : 1.f;
				const FVector At(Pos.X + Rand.FRandRange(-0.42f, 0.42f) * BodyX, Pos.Y + Toward * (HalfY + Width * 0.3f), Z);
				AddWindow(Rand, bAmber, At, FVector(Width * 0.5f, FMath::Min(Length, BodyX * 0.6f), WindowHeight), Toward > 0.f ? 90.f : -90.f);
			}
		}
	}

	// A sign on the side wall that faces the camera's line of sight (towers off to either side).
	if (FMath::Abs(Pos.Y) > HalfY + 200.f && BodyHeight > 600.f && Rand.FRand() < 0.45f)
	{
		const float Toward = Pos.Y > 0.f ? -1.f : 1.f;
		const float SignWidth = FMath::Clamp(BodyX * 0.2f, 70.f, 300.f);
		const float SignHeight = FMath::Min(BodyHeight * Rand.FRandRange(0.25f, 0.55f), SignWidth * 7.f);
		const float Z = SignHeight * 0.5f + Rand.FRandRange(0.05f, 0.9f) * (BodyHeight - SignHeight);
		AddSign(Rand, PickSign(), FVector(Pos.X + Rand.FRandRange(-0.3f, 0.3f) * BodyX, Pos.Y + Toward * HalfY, Z), Toward > 0.f ? 90.f : -90.f, SignWidth, SignHeight, Width);
	}

	// Rooftop clutter: antennas, and now and then a billboard on legs.
	const int32 Antennas = Rand.RandRange(0, 3);
	for (int32 i = 0; i < Antennas; ++i)
	{
		const float AntennaHeight = Rand.FRandRange(120.f, 500.f);
		const FVector At(Pos.X + Rand.FRandRange(-0.35f, 0.35f) * BodyX, Pos.Y + Rand.FRandRange(-0.35f, 0.35f) * BodyY, BodyHeight + AntennaHeight * 0.5f);
		AddNeonBox(Rand.FRand() < 0.5f ? 0 : 2, At, FVector(Width, Width, AntennaHeight), 0.f);
	}
	if (BodyY > 400.f && Rand.FRand() < 0.2f)
	{
		const float BoardWidth = BodyY * 0.6f;
		const float BoardHeight = BoardWidth * 0.4f;
		const float Lift = 60.f;
		const FVector At(Pos.X - HalfX * 0.2f, Pos.Y, BodyHeight + Lift + BoardHeight * 0.5f);
		for (const float S : { -0.4f, 0.4f })
		{
			AddHull(FVector(At.X, Pos.Y + S * BoardWidth, BodyHeight + Lift * 0.5f), FVector(16.f, 16.f, Lift), 0.f);
		}
		AddSign(Rand, PickSign(), At, 180.f, BoardWidth, BoardHeight, Width);
	}
}

void ASpinCityBuilder::AddWindow(FRandomStream& Rand, bool bAmber, const FVector& Center, const FVector& Size, float FaceYaw)
{
	const int32 Group = (bAmber ? NumWindowGroups : 0) + Rand.RandRange(0, NumWindowGroups - 1);
	Pending.FindOrAdd(WindowGroups[Group].Get()).Add(FTransform(FRotator(0.f, FaceYaw, 0.f), Center, Size / MeshSize));
}

void ASpinCityBuilder::AddSign(FRandomStream& Rand, int32 Slot, const FVector& Center, float FaceYaw, float SignWidth, float SignHeight, float Width)
{
	// Now and then a cyan or magenta sign is faulty: its lettering stutters (see UpdateSignFlicker).
	UInstancedStaticMeshComponent* FaultyLettering = nullptr;
	if ((Slot == 0 || Slot == 2) && Rand.FRand() < SignFlickerChance)
	{
		FaultyLettering = FlickerSigns[Slot == 0 ? 0 : 1];
	}

	const FRotator Rot(0.f, FaceYaw, 0.f);
	const FVector Out = Rot.Vector();                              // out of the wall
	const FVector Along = Rot.RotateVector(FVector(0.f, 1.f, 0.f)); // across the wall
	const FVector Up(0.f, 0.f, 1.f);
	const FVector C = Center + Out * Width;
	const float Depth = Width * 0.5f;

	// A dark backing panel so the sign reads against any wall, then a frame in a contrasting colour.
	AddHull(C - Out * (Width * 0.5f), FVector(Depth, SignWidth, SignHeight), FaceYaw);
	const int32 FrameSlot = Slot == 0 ? 2 : 0;
	AddNeonBox(FrameSlot, C + Up * (SignHeight * 0.5f), FVector(Depth, SignWidth + Width, Width), FaceYaw);
	AddNeonBox(FrameSlot, C - Up * (SignHeight * 0.5f), FVector(Depth, SignWidth + Width, Width), FaceYaw);
	AddNeonBox(FrameSlot, C + Along * (SignWidth * 0.5f), FVector(Depth, Width, SignHeight), FaceYaw);
	AddNeonBox(FrameSlot, C - Along * (SignWidth * 0.5f), FVector(Depth, Width, SignHeight), FaceYaw);

	// "Lettering": a row of glyphs down (or across) the sign, each a few random strokes.
	const bool bVertical = SignHeight >= SignWidth;
	const float Short = bVertical ? SignWidth : SignHeight;
	const float Long = bVertical ? SignHeight : SignWidth;
	const float Cell = Short * 0.62f;
	const int32 Cells = FMath::Max(1, FMath::FloorToInt((Long - Short * 0.3f) / (Cell * 1.25f)));
	const float Step = Long / Cells;
	const float Stroke = FMath::Max(Width * 0.8f, Cell * 0.09f);
	const FVector LongAxis = bVertical ? Up : Along;

	for (int32 c = 0; c < Cells; ++c)
	{
		const FVector CellCenter = C + LongAxis * (-Long * 0.5f + Step * (c + 0.5f)) + Out * (Width * 0.3f);
		const int32 Strokes = Rand.RandRange(2, 4);
		for (int32 s = 0; s < Strokes; ++s)
		{
			const float Length = Cell * Rand.FRandRange(0.45f, 1.f);
			const float Shift = Cell * Rand.FRandRange(-0.4f, 0.4f);
			const bool bHorizontal = Rand.FRand() < 0.5f;
			const FVector At = bHorizontal ? CellCenter + Up * Shift : CellCenter + Along * Shift;
			const FVector Size = bHorizontal ? FVector(Depth, Length, Stroke) : FVector(Depth, Stroke, Length);
			if (FaultyLettering)
			{
				Pending.FindOrAdd(FaultyLettering).Add(FTransform(FRotator(0.f, FaceYaw, 0.f), At, Size / MeshSize));
			}
			else
			{
				AddNeonBox(Slot, At, Size, FaceYaw);
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Parks: neon trees, fountains
// ---------------------------------------------------------------------------------------------

void ASpinCityBuilder::BuildPark(FRandomStream& Rand, const FVector2D& Pos, float Yaw, float Size)
{
	const float W = LineWidthAt(Pos);
	const FRotator Rot(0.f, Yaw, 0.f);

	// A glowing green border marks out the park.
	OutlineBox(FoliageSlot, FVector(Pos.X, Pos.Y, 0.f), FVector(Size, Size, 3.f), Yaw, W * 0.8f, false);

	if (Rand.FRand() < FountainChance)
	{
		// A fountain in the middle, ringed by eight trees of one height.
		BuildFountain(Pos, Size * 0.26f, W, Yaw);
		const float TreeHeight = Rand.FRandRange(260.f, 420.f);
		for (int32 i = 0; i < 8; ++i)
		{
			const float A = FMath::DegreesToRadians(Yaw + 22.5f + i * 45.f);
			BuildTree(Pos + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (Size * 0.4f), TreeHeight, W);
		}
	}
	else
	{
		// A grove: trees in pairs mirrored across the park, plus sometimes one in the middle.
		const int32 Pairs = Rand.RandRange(2, 3);
		for (int32 i = 0; i < Pairs; ++i)
		{
			const float Along = Rand.FRandRange(-0.36f, 0.36f) * Size;
			const float Across = Rand.FRandRange(0.12f, 0.36f) * Size;
			const float TreeHeight = Rand.FRandRange(220.f, 560.f);
			for (const float S : { -1.f, 1.f })
			{
				const FVector Offset = Rot.RotateVector(FVector(Along, S * Across, 0.f));
				BuildTree(Pos + FVector2D(Offset), TreeHeight, W);
			}
		}
		if (Rand.FRand() < 0.5f)
		{
			BuildTree(Pos, Rand.FRandRange(450.f, 700.f), W);
		}
	}
}

void ASpinCityBuilder::BuildTree(const FVector2D& Pos, float Height, float Width)
{
	// A dark trunk carrying a stack of glowing discs that narrow toward the top: a neon pine.
	const FVector Base(Pos.X, Pos.Y, 0.f);
	AddHull(Base + FVector(0.f, 0.f, Height * 0.25f), FVector(Height * 0.07f, Height * 0.07f, Height * 0.5f), 0.f, true);
	for (int32 Tier = 0; Tier < 4; ++Tier)
	{
		AddNeonDisc(FoliageSlot, Base + FVector(0.f, 0.f, Height * (0.38f + 0.17f * Tier)), Height * (0.3f - 0.06f * Tier), FMath::Max(Width * 1.2f, 6.f));
	}
	AddNeonBox(FoliageSlot, Base + FVector(0.f, 0.f, Height * 0.97f), FVector(Width * 1.5f, Width * 1.5f, Height * 0.12f), 0.f);
}

void ASpinCityBuilder::BuildFountain(const FVector2D& Pos, float Radius, float Width, float Yaw)
{
	// A round basin with a glowing pool, a column with a smaller bowl, and eight arcing jets of water.
	const FVector Base(Pos.X, Pos.Y, 0.f);
	constexpr float BasinHeight = 60.f;
	AddHull(Base + FVector(0.f, 0.f, BasinHeight * 0.5f), FVector(Radius * 2.f, Radius * 2.f, BasinHeight), Yaw, true);
	BuildHaloRing(2, Base, Radius + Width * 0.5f, BasinHeight + 2.f, Width, Yaw);
	AddNeonDisc(WaterSlot, Base + FVector(0.f, 0.f, BasinHeight + 1.f), Radius * 0.9f, 2.f);

	const float ColumnTop = BasinHeight + Radius;
	AddHull(Base + FVector(0.f, 0.f, ColumnTop * 0.5f), FVector(Radius * 0.2f, Radius * 0.2f, ColumnTop), Yaw, true);
	const float BowlZ = BasinHeight + Radius * 0.55f;
	AddHull(Base + FVector(0.f, 0.f, BowlZ), FVector(Radius * 0.76f, Radius * 0.76f, Radius * 0.08f), Yaw, true);
	AddNeonDisc(WaterSlot, Base + FVector(0.f, 0.f, BowlZ + Radius * 0.04f + 1.f), Radius * 0.34f, 2.f);

	constexpr int32 Jets = 8;
	constexpr int32 Steps = 6;
	for (int32 j = 0; j < Jets; ++j)
	{
		const float A = FMath::DegreesToRadians(Yaw + j * 360.f / Jets);
		const FVector Out(FMath::Cos(A), FMath::Sin(A), 0.f);
		FVector Prev = Base + FVector(0.f, 0.f, ColumnTop);
		for (int32 s = 1; s <= Steps; ++s)
		{
			const float T = static_cast<float>(s) / Steps;
			const float Z = ColumnTop + (BasinHeight + 2.f - ColumnTop) * T + 4.f * Radius * 0.35f * T * (1.f - T);
			const FVector Next = Base + Out * (Radius * 0.8f * T) + FVector(0.f, 0.f, Z);
			AddNeonLine(WaterSlot, Prev, Next, Width * 0.7f);
			Prev = Next;
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Landmarks
// ---------------------------------------------------------------------------------------------

void ASpinCityBuilder::BuildArch(int32 Slot, const FVector& Center, const FVector& Across, float Radius, float Width)
{
	// A tall half-ellipse of light standing across an avenue.
	constexpr int32 Segments = 28;
	FVector Prev = Center + Across * Radius;
	for (int32 i = 1; i <= Segments; ++i)
	{
		const float A = PI * i / Segments;
		const FVector Next = Center + Across * (Radius * FMath::Cos(A)) + FVector(0.f, 0.f, Radius * 1.6f * FMath::Sin(A));
		AddNeonLine(Slot, Prev, Next, Width);
		Prev = Next;
	}
}

void ASpinCityBuilder::BuildHaloRing(int32 Slot, const FVector& Center, float Radius, float Height, float Width, float StartDeg, bool bDepthScaled)
{
	// A horizontal ring of light: a tower's crown, a fountain's rim, or a colossal halo over the city.
	const int32 Segments = FMath::Clamp(FMath::CeilToInt(2.f * PI * Radius / 1500.f), 16, 400);
	for (int32 i = 0; i < Segments; ++i)
	{
		const float A0 = FMath::DegreesToRadians(StartDeg) + 2.f * PI * i / Segments;
		const float A1 = FMath::DegreesToRadians(StartDeg) + 2.f * PI * (i + 1) / Segments;
		const FVector P0 = Center + FVector(Radius * FMath::Cos(A0), Radius * FMath::Sin(A0), Height);
		const FVector P1 = Center + FVector(Radius * FMath::Cos(A1), Radius * FMath::Sin(A1), Height);
		const FVector2D Mid((P0 + P1) * 0.5f);
		if (IsInView(Mid))
		{
			AddNeonLine(Slot, P0, P1, bDepthScaled ? LineWidthAt(Mid) * Width : Width);
		}
	}
}

void ASpinCityBuilder::BuildLandmarks(FRandomStream& Rand)
{
	// Halo rings floating over the city: a smaller amber one nearer in, a vast purple one beyond.
	BuildHaloRing(4, FVector::ZeroVector, CityRadius * 0.4f, 5200.f, 3.f, 0.f, true);
	BuildHaloRing(3, FVector::ZeroVector, CityRadius * 0.72f, 9000.f, 3.5f, 0.f, true);

	// Gateway arches spanning the avenues that lead away from the camera, twice along each.
	for (int32 i = 0; i < NumAvenues; ++i)
	{
		const float Angle = 2.f * PI * i / NumAvenues;
		const FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
		if (Dir.X < 0.3f || (i == 0 && bRiver)) // the river has its own bridges
		{
			continue;
		}
		// Colour by distance round from straight ahead, so mirrored avenues get matching arches.
		const int32 Symmetric = FMath::Min(i, NumAvenues - i);
		const FVector Across(-Dir.Y, Dir.X, 0.f);
		for (const float Fraction : { 0.45f, 0.8f })
		{
			const float D = CityRadius * Fraction;
			BuildArch(Symmetric % NumPalette, Dir * D, Across, AvenueWidth * (1.4f + Fraction * 1.6f), LineWidthAt(FVector2D(Dir * D)) * 2.5f);
		}
	}

	// Colossal spires scattered along the horizon, each its own height, size and colour.
	constexpr int32 NumSpires = 6;
	for (int32 i = 0; i < NumSpires; ++i)
	{
		float AngleDeg = FMath::Lerp(-80.f, 80.f, (i + 0.5f) / NumSpires) + Rand.FRandRange(-9.f, 9.f);
		if (FMath::Abs(AngleDeg) < 10.f)
		{
			AngleDeg += AngleDeg >= 0.f ? 10.f : -10.f; // the centre of the skyline belongs to the ziggurat
		}
		const float Dist = LandmarkRadius * Rand.FRandRange(0.9f, 1.12f);
		const float Height = Rand.FRandRange(15000.f, 27000.f);
		const float BaseSide = Rand.FRandRange(2400.f, 3800.f);
		const int32 Color = Rand.RandRange(0, NumPalette - 1);

		// Swing it in toward the middle of the view until it's clear of the bridge highways.
		for (int32 Try = 0; Try < 30; ++Try)
		{
			const float Rad = FMath::DegreesToRadians(AngleDeg);
			if (!IsOnBridgeRoad(FVector2D(Dist * FMath::Cos(Rad), Dist * FMath::Sin(Rad)), BaseSide * 1.4f))
			{
				break;
			}
			AngleDeg -= FMath::Sign(AngleDeg) * 2.f;
		}

		{
			const float Rad = FMath::DegreesToRadians(AngleDeg);
			const FVector Base(Dist * FMath::Cos(Rad), Dist * FMath::Sin(Rad), 0.f);
			const float W = LineWidthAt(FVector2D(Base)) * 1.6f;

			// Four tapering tiers, banded every so often, with a needle and beacon on top.
			float Z = 0.f;
			float Side = BaseSide;
			for (int32 Tier = 0; Tier < 4; ++Tier)
			{
				const float TierHeight = Height * (0.4f - Tier * 0.08f);
				const FVector Size(Side, Side, TierHeight);
				AddHull(FVector(Base.X, Base.Y, Z + TierHeight * 0.5f), Size, AngleDeg);
				OutlineBox(Color, FVector(Base.X, Base.Y, Z), Size, AngleDeg, W);
				for (float Band = 1200.f; Band < TierHeight - 300.f; Band += 1200.f)
				{
					OutlineBox((Color + 2) % NumPalette, FVector(Base.X, Base.Y, Z), FVector(Size.X, Size.Y, Band), AngleDeg, W * 0.6f, false);
				}
				Z += TierHeight;
				Side *= 0.7f;
			}
			AddNeonBox(Color, Base + FVector(0.f, 0.f, Z + 1500.f), FVector(W * 1.5f, W * 1.5f, 3000.f), AngleDeg);
			AddBeacon(Base + FVector(0.f, 0.f, Z + 3100.f), W * 5.f);
		}
	}

	// The centrepiece, dead ahead on the horizon: a vast stepped ziggurat crowned with a spire.
	{
		const float Dist = LandmarkRadius * 1.15f;
		const FVector Base(Dist, 0.f, 0.f);
		const float W = LineWidthAt(FVector2D(Base)) * 1.8f;
		float Z = 0.f;
		float Side = 11000.f;
		for (int32 Tier = 0; Tier < 7; ++Tier)
		{
			const float TierHeight = 1700.f;
			const FVector Size(Side, Side, TierHeight);
			AddHull(FVector(Base.X, Base.Y, Z + TierHeight * 0.5f), Size, 0.f);
			OutlineBox(Tier % 2 == 0 ? 2 : 4, FVector(Base.X, Base.Y, Z), Size, 0.f, W);
			Z += TierHeight;
			Side *= 0.78f;
		}
		const FVector Needle(Side * 0.25f, Side * 0.25f, 9000.f);
		AddHull(Base + FVector(0.f, 0.f, Z + Needle.Z * 0.5f), Needle, 0.f);
		OutlineBox(0, FVector(Base.X, Base.Y, Z), Needle, 0.f, W);
		BuildHaloRing(0, Base, Side * 0.9f, Z + Needle.Z * 0.6f, W * 1.5f);
		BuildHaloRing(2, Base, Side * 0.6f, Z + Needle.Z * 0.8f, W * 1.5f);
		AddBeacon(Base + FVector(0.f, 0.f, Z + Needle.Z + W * 4.f), W * 6.f);
	}
}

// ---------------------------------------------------------------------------------------------
// The river and its bridges
// ---------------------------------------------------------------------------------------------

float ASpinCityBuilder::GetRiverEnd() const
{
	// It runs up to the foot of the ziggurat (whose lowest tier is 11000 across, see BuildLandmarks).
	return LandmarkRadius * 1.15f - 5800.f;
}

bool ASpinCityBuilder::IsInRiverZone(const FVector2D& Pos, float Footprint) const
{
	if (!bRiver)
	{
		return false;
	}
	const float Reach = Footprint * 0.75f;
	const float Half = RiverWidth * 0.5f;

	// The river and its embankments.
	if (Pos.X > PlazaRadius - 500.f && Pos.X < GetRiverEnd() + 500.f && FMath::Abs(Pos.Y) < Half + 400.f + Reach)
	{
		return true;
	}

	// The bridge and the land at either end of it, and the highways leading on from it.
	if (FMath::Abs(Pos.X - GrandBridgeDistance) < 700.f + Reach && FMath::Abs(Pos.Y) < GetBridgeDeckEnd() + 600.f + Reach)
	{
		return true;
	}
	return IsOnBridgeRoad(Pos, Footprint);
}

float ASpinCityBuilder::GetBridgeDeckEnd() const
{
	// The towers stand GrandBridgeSpan apart (or just inside the banks, if that's wider), and the
	// deck runs on 2200 past each one.
	return FMath::Max(GrandBridgeSpan * 0.5f, RiverWidth * 0.5f + 250.f) + 2200.f;
}

bool ASpinCityBuilder::IsOnBridgeRoad(const FVector2D& Pos, float Footprint) const
{
	if (!bRiver || !bBridgeRoads)
	{
		return false;
	}
	const float Reach = Footprint * 0.75f;
	return FMath::Abs(Pos.X - GrandBridgeDistance) < BridgeRoadWidth * 0.5f + 400.f + Reach
		&& FMath::Abs(Pos.Y) < BridgeRoadReach + Reach;
}

void ASpinCityBuilder::BuildBridgeRoads(float X, float DeckEnd, float Deck)
{
	if (!bBridgeRoads || BridgeRoadReach <= DeckEnd)
	{
		return;
	}

	// Each highway: a ramp from the deck to its own height (if they differ), then straight on to
	// BridgeRoadReach. Elevated stretches stand on pillars, with a purple glow along the underside.
	const float RoadZ = FMath::Max(BridgeRoadHeight, 0.f);
	const float Ramp = FMath::Abs(Deck - RoadZ) > 1.f ? BridgeRoadRampLength : 0.f;
	const float HalfWidth = BridgeRoadWidth * 0.5f;
	auto HeightAt = [&](float Y) -> float
	{
		if (Ramp <= 0.f || Y >= DeckEnd + Ramp)
		{
			return RoadZ;
		}
		const float T = FMath::Clamp((Y - DeckEnd) / Ramp, 0.f, 1.f);
		return FMath::Lerp(Deck, RoadZ, FMath::SmoothStep(0.f, 1.f, T));
	};

	constexpr float PillarSpacing = 1800.f;
	for (const float S : { -1.f, 1.f })
	{
		// Short pieces along the ramp so it curves smoothly, longer ones after.
		TArray<float> Breaks;
		for (float Y = DeckEnd; Y < DeckEnd + Ramp; Y += 500.f)
		{
			Breaks.Add(Y);
		}
		for (float Y = DeckEnd + Ramp; Y < BridgeRoadReach; Y += 3000.f)
		{
			Breaks.Add(Y);
		}
		Breaks.Add(BridgeRoadReach);

		for (int32 i = 0; i + 1 < Breaks.Num(); ++i)
		{
			const float Y0 = Breaks[i];
			const float Y1 = Breaks[i + 1];
			const FVector2D Mid(X, S * (Y0 + Y1) * 0.5f);
			if (!IsInView(Mid))
			{
				continue;
			}
			const float Z0 = HeightAt(Y0);
			const float Z1 = HeightAt(Y1);
			const bool bElevated = FMath::Max(Z0, Z1) > 60.f;
			const float W = LineWidthAt(Mid);

			// The road surface: a slab tilted to follow the ramp (or a thin strip at street level).
			const float Thickness = bElevated ? 80.f : 20.f;
			const float LowZ = bElevated ? 0.f : Thickness * 0.5f;
			const FVector A(X, S * Y0, FMath::Max(Z0, LowZ));
			const FVector B(X, S * Y1, FMath::Max(Z1, LowZ));
			const float Length = FVector::Dist(A, B);
			const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(B.Z - A.Z, Y1 - Y0));
			const FRotator Rot(Pitch, S > 0.f ? 90.f : -90.f, 0.f);
			const int32 Band = HullBandAt(Mid);
			Pending.FindOrAdd(HullBoxes[Band].Get()).Add(FTransform(Rot, (A + B) * 0.5f, FVector(Length + 20.f, BridgeRoadWidth, Thickness) / MeshSize));
			BuiltRadius = FMath::Max(BuiltRadius, FMath::Max(FVector2D(A).Size(), FVector2D(B).Size()));

			// Lit edges (just above the surface) and, when raised, the underglow.
			const FVector Up(0.f, 0.f, Thickness * 0.5f + 5.f);
			for (const float Edge : { -HalfWidth, HalfWidth })
			{
				AddNeonLine(0, A + FVector(Edge, 0.f, 0.f) + Up, B + FVector(Edge, 0.f, 0.f) + Up, W);
			}
			if (bElevated)
			{
				AddNeonLine(3, A - Up, B - Up, W * 1.5f);
			}

			// Pillars under the raised stretches.
			for (float Y = FMath::CeilToFloat(Y0 / PillarSpacing) * PillarSpacing; Y < Y1; Y += PillarSpacing)
			{
				const float Z = HeightAt(Y) - Thickness * 0.5f;
				if (Z > 120.f && Y > DeckEnd + 200.f)
				{
					AddHull(FVector(X, S * Y, Z * 0.5f), FVector(140.f, 140.f, Z), 0.f);
				}
			}
		}
	}
}

void ASpinCityBuilder::BuildRiver()
{
	Water->SetVisibility(bRiver);
	if (!bRiver)
	{
		return;
	}

	const float Start = PlazaRadius + 300.f;
	const float End = GetRiverEnd();
	const float Half = RiverWidth * 0.5f;

	// The water: a dark mirror just above the floor (high enough to hide floor lines beneath it),
	// so the neon city reflects in it.
	Water->SetRelativeLocation(FVector((Start + End) * 0.5f, 0.f, 1.5f));
	Water->SetRelativeScale3D(FVector(End - Start, RiverWidth, 3.f) / MeshSize);

	// Embankment walls along both banks, with a neon edge along the top and a glowing waterline.
	constexpr float WallHeight = 120.f;
	for (float X = Start; X < End; X += 3000.f)
	{
		const float X1 = FMath::Min(X + 3000.f, End);
		const float W = LineWidthAt(FVector2D((X + X1) * 0.5f, Half));
		for (const float S : { -1.f, 1.f })
		{
			AddHull(FVector((X + X1) * 0.5f, S * (Half + 60.f), WallHeight * 0.5f), FVector(X1 - X, 120.f, WallHeight), 0.f);
			AddNeonLine(0, FVector(X, S * Half, WallHeight + 2.f), FVector(X1, S * Half, WallHeight + 2.f), W);
			AddNeonLine(2, FVector(X, S * (Half - 4.f), 6.f), FVector(X1, S * (Half - 4.f), 6.f), W * 0.8f);
		}
	}

	// Walls closing off each end: at the plaza, and at the foot of the ziggurat.
	for (const float X : { Start - 60.f, End + 60.f })
	{
		const float W = LineWidthAt(FVector2D(X, 0.f));
		AddHull(FVector(X, 0.f, WallHeight * 0.5f), FVector(120.f, RiverWidth + 240.f, WallHeight), 0.f);
		AddNeonLine(0, FVector(X, -Half, WallHeight + 2.f), FVector(X, Half, WallHeight + 2.f), W);
	}

	BuildSuspensionBridge(GrandBridgeDistance);
}

void ASpinCityBuilder::BuildSuspensionBridge(float X)
{
	// The grand bridge: a road deck across the river on two tall towers, slung from sweeping main
	// cables by rows of hangers - seen broadside from the camera, the centrepiece of the river.
	const float Half = RiverWidth * 0.5f;
	const float W = LineWidthAt(FVector2D(X, 0.f));
	const float Deck = BridgeDeckHeight;
	const float TowerY = FMath::Max(GrandBridgeSpan * 0.5f, Half + 250.f); // the towers, either side of the centre line
	const float DeckEnd = GetBridgeDeckEnd(); // the deck runs on past each tower to the shore
	const float TowerTop = FMath::Max(GrandBridgeTowerHeight, Deck + 1000.f);
	// The lowest point of the main cables, mid-span: GrandBridgeCableSag of the way from the tower
	// tops down to just above the deck.
	const float CableLow = TowerTop - FMath::Clamp(GrandBridgeCableSag, 0.f, 1.f) * (TowerTop - (Deck + 160.f));
	constexpr float DeckWidth = 700.f;
	const float EdgeX = DeckWidth * 0.5f;

	// Deck, with lit edges and a purple glow along its underside, and solid abutments at each end.
	AddHull(FVector(X, 0.f, Deck), FVector(DeckWidth, DeckEnd * 2.f, 80.f), 0.f);
	for (const float S : { -1.f, 1.f })
	{
		AddNeonLine(0, FVector(X + S * EdgeX, -DeckEnd, Deck + 45.f), FVector(X + S * EdgeX, DeckEnd, Deck + 45.f), W);
		AddHull(FVector(X, S * DeckEnd, Deck * 0.5f), FVector(900.f, 500.f, Deck), 0.f);
		OutlineBox(2, FVector(X, S * DeckEnd, 0.f), FVector(900.f, 500.f, Deck), 0.f, W, false);
	}
	AddNeonLine(3, FVector(X, -DeckEnd, Deck - 45.f), FVector(X, DeckEnd, Deck - 45.f), W * 1.5f);

	// Two towers, each a pair of legs tied by crossbeams, outlined in magenta and topped with a beacon.
	for (const float S : { -1.f, 1.f })
	{
		const float Y = S * TowerY;
		for (const float Leg : { -1.f, 1.f })
		{
			const FVector Base(X + Leg * (EdgeX + 40.f), Y, 0.f);
			const FVector Size(160.f, 160.f, TowerTop);
			AddHull(Base + FVector(0.f, 0.f, TowerTop * 0.5f), Size, 0.f);
			OutlineBox(2, Base, Size, 0.f, W);
		}
		for (const float Beam : { Deck - 200.f, TowerTop * 0.66f, TowerTop - 60.f })
		{
			const FVector BeamSize(DeckWidth + 240.f, 140.f, 120.f);
			AddHull(FVector(X, Y, Beam), BeamSize, 0.f);
			OutlineBox(2, FVector(X, Y, Beam - 60.f), BeamSize, 0.f, W * 0.8f, false);
		}
		AddBeacon(FVector(X, Y, TowerTop + W * 4.f), W * 5.f);
	}

	// Main cables along both edges of the deck: a parabola between the towers, then a sweep down to
	// each end of the deck. Hangers drop from them to the deck, and lights bead the main span.
	auto CableZ = [&](float Y) -> float
	{
		const float A = FMath::Abs(Y);
		if (A <= TowerY)
		{
			const float T = A / TowerY;
			return CableLow + (TowerTop - CableLow) * T * T;
		}
		const float T = (A - TowerY) / (DeckEnd - TowerY); // 0 at the tower, 1 at the deck end
		return FMath::Lerp(TowerTop, Deck + 60.f, T) - 250.f * T * (1.f - T);
	};

	constexpr int32 Segments = 60;
	for (const float Side : { -1.f, 1.f })
	{
		const float CX = X + Side * EdgeX;
		FVector Prev(CX, -DeckEnd, CableZ(-DeckEnd));
		for (int32 i = 1; i <= Segments; ++i)
		{
			const float Y = FMath::Lerp(-DeckEnd, DeckEnd, static_cast<float>(i) / Segments);
			const FVector Next(CX, Y, CableZ(Y));
			AddNeonLine(0, Prev, Next, W * 1.4f);
			if (i % 2 == 0 && FMath::Abs(Y) < TowerY)
			{
				AddNeonBox(0, Next, FVector(W * 3.f), 0.f); // a light on the cable
			}
			Prev = Next;
		}

		for (float Y = -DeckEnd + 175.f; Y < DeckEnd; Y += 350.f)
		{
			if (FMath::Abs(FMath::Abs(Y) - TowerY) < 150.f)
			{
				continue; // the towers themselves
			}
			AddNeonLine(1, FVector(CX, Y, Deck + 45.f), FVector(CX, Y, CableZ(Y)), W * 0.6f);
		}
	}

	BuildBridgeRoads(X, DeckEnd, Deck);
}
