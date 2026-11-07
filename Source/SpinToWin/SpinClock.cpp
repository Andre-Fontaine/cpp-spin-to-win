// Spin To Win - the physical dial in the world.

#include "SpinClock.h"
#include "SpinGameMode.h"
#include "SpinCityFX.h"
#include "SpinCityBuilder.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "EngineUtils.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// The engine's built-in BasicShapeMaterial exposes a vector parameter called "Color".
	const FName ColorParam(TEXT("Color"));

	// The colours the dial and obstacles flash through while glitching (above 1, so they bloom).
	const FLinearColor GlitchPalette[] =
	{
		FLinearColor(0.1f, 3.f, 3.5f), // cyan
		FLinearColor(4.f, 0.2f, 2.8f), // magenta
		FLinearColor(3.5f, 3.f, 0.3f), // yellow
		FLinearColor(3.f, 3.f, 3.f),   // white
	};

	// Thickness of the neon plates on an obstacle, and how far they overhang its hull.
	constexpr float ObstacleGlowThickness = 5.f;
	constexpr float ObstacleGlowOverhang = 1.08f;
}

/** How far into its close-up -> wide-shot move the camera should be (0..1), given how far the city's
 *  light has spread: the least pulled-back pose whose view still reaches past the light's edge. */
static float GetRevealCameraBlend(const ASpinClock& Clock)
{
	const ASpinCityFX* FX = nullptr;
	for (TActorIterator<ASpinCityFX> It(Clock.GetWorld()); It; ++It)
	{
		FX = *It;
		break;
	}
	if (!FX)
	{
		return 0.f;
	}

	// How far the camera needs to see: past the light's outer edge (including the band where lights
	// are still fading in), with a buffer, so newly lit buildings come on in the shot.
	const float Fraction = FX->GetRevealFraction();
	const float Wanted = FX->GetRevealOuterRadius() * Clock.RevealCameraLead + Clock.RevealCameraBuffer;

	// How far ahead of the dial the top of the frame meets the ground, for a given blend.
	const UCameraComponent* Camera = Clock.FindComponentByClass<UCameraComponent>();
	const float Aspect = Camera ? FMath::Max(0.5f, Camera->AspectRatio) : 16.f / 9.f;
	auto Reach = [&Clock, Aspect](float Blend) -> float
	{
		const float FOV = FMath::Lerp(Clock.CameraFOV, Clock.RevealedCameraFOV, Blend);
		const float HalfVerticalFOV = FMath::RadiansToDegrees(FMath::Atan(FMath::Tan(FMath::DegreesToRadians(FOV * 0.5f)) / Aspect));
		const float Pitch = FMath::Lerp(Clock.CameraPitch, Clock.RevealedCameraPitch, Blend);
		const float Dist = Clock.DialRadius * FMath::Lerp(Clock.CameraDistanceFactor, Clock.RevealedCameraDistanceFactor, Blend);
		const float ViewPitch = Pitch - Clock.RevealedCameraLookUp * Blend;
		const float TopEdgeDown = ViewPitch - HalfVerticalFOV; // degrees below the horizon
		if (TopEdgeDown < 0.5f)
		{
			return BIG_NUMBER; // the top of the frame is at or above the horizon
		}
		const float PitchRad = FMath::DegreesToRadians(Pitch);
		const float Height = Dist * FMath::Sin(PitchRad) + Clock.GetActorLocation().Z;
		return Height / FMath::Tan(FMath::DegreesToRadians(TopEdgeDown)) - Dist * FMath::Cos(PitchRad);
	};

	// The smallest blend that sees far enough (the reach grows steadily with the blend).
	float Blend = 1.f;
	if (Reach(0.f) >= Wanted)
	{
		Blend = 0.f;
	}
	else if (Reach(1.f) >= Wanted)
	{
		float Lo = 0.f;
		float Hi = 1.f;
		for (int32 i = 0; i < 14; ++i)
		{
			const float Mid = (Lo + Hi) * 0.5f;
			(Reach(Mid) >= Wanted ? Hi : Lo) = Mid;
		}
		Blend = Hi;
	}

	// Either way, finish on the full wide shot once the whole city is lit.
	return FMath::Max(Blend, FMath::SmoothStep(0.f, 1.f, Fraction));
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
	RoundPostMesh = CylinderMesh;
	SquarePostMesh = CubeMesh;
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

	// Everything on the dial hangs off this, so scaling it grows the whole dial at once. All the
	// gameplay works in the dial's own units underneath, so its size is purely visual.
	DialPivot = CreateDefaultSubobject<USceneComponent>(TEXT("DialPivot"));
	DialPivot->SetupAttachment(Root);

	Rim = MakeMesh(TEXT("Rim"), CylinderMesh, DialPivot);
	Dial = MakeMesh(TEXT("Dial"), CylinderMesh, DialPivot);
	Hub = MakeMesh(TEXT("Hub"), CylinderMesh, DialPivot);

	HandPivot = CreateDefaultSubobject<USceneComponent>(TEXT("HandPivot"));
	HandPivot->SetupAttachment(DialPivot);
	Hand = MakeMesh(TEXT("Hand"), CubeMesh, HandPivot);
	HandOuterPivot = CreateDefaultSubobject<USceneComponent>(TEXT("HandOuterPivot"));
	HandOuterPivot->SetupAttachment(HandPivot);
	HandOuterBody = MakeMesh(TEXT("HandOuterBody"), CubeMesh, HandOuterPivot);
	for (int32 i = 0; i < NumHandNeon; ++i)
	{
		USceneComponent* Parent = i < NumInnerHandNeon ? HandPivot.Get() : HandOuterPivot.Get();
		UStaticMeshComponent* Line = MakeMesh(*FString::Printf(TEXT("HandNeon%d"), i), CubeMesh, Parent);
		Line->SetCastShadow(false);
		HandNeon.Add(Line);
	}
	HandSpindle = MakeMesh(TEXT("HandSpindle"), CylinderMesh, DialPivot);
	HandSpindleBand = MakeMesh(TEXT("HandSpindleBand"), CylinderMesh, DialPivot);
	HandSpindleBand->SetCastShadow(false);
	HandCollar = MakeMesh(TEXT("HandCollar"), CylinderMesh, HandPivot);
	HandCollar->SetCastShadow(false);

	for (int32 i = 0; i < NumZoneSegments; ++i)
	{
		UStaticMeshComponent* Segment = MakeMesh(*FString::Printf(TEXT("ZoneSegment%d"), i), CubeMesh, DialPivot);
		Segment->SetCastShadow(false);
		ZoneSegments.Add(Segment);
	}

	for (int32 i = 0; i < NumZoneMarks; ++i)
	{
		UStaticMeshComponent* Mark = MakeMesh(*FString::Printf(TEXT("ZoneMark%d"), i), CubeMesh, DialPivot);
		Mark->SetCastShadow(false);
		ZoneMarks.Add(Mark);
	}

	for (int32 i = 0; i < NumRingSegments; ++i)
	{
		UStaticMeshComponent* Segment = MakeMesh(*FString::Printf(TEXT("OuterRing%d"), i), CubeMesh, DialPivot);
		Segment->SetCastShadow(false);
		OuterRingSegments.Add(Segment);
	}

	for (int32 i = 0; i < NumRingSegments; ++i)
	{
		UStaticMeshComponent* Segment = MakeMesh(*FString::Printf(TEXT("InnerRing%d"), i), CubeMesh, DialPivot);
		Segment->SetCastShadow(false);
		InnerRingSegments.Add(Segment);
	}

	for (int32 i = 0; i < NumTicks; ++i)
	{
		UStaticMeshComponent* Tick = MakeMesh(*FString::Printf(TEXT("Tick%d"), i), CubeMesh, DialPivot);
		Tick->SetCastShadow(false);
		TickMarks.Add(Tick);
	}

	for (int32 i = 0; i < NumTrackSegments; ++i)
	{
		UStaticMeshComponent* Inner = MakeMesh(*FString::Printf(TEXT("TrackInner%d"), i), CubeMesh, DialPivot);
		Inner->SetCastShadow(false);
		TrackInnerSegments.Add(Inner);

		UStaticMeshComponent* Outer = MakeMesh(*FString::Printf(TEXT("TrackOuter%d"), i), CubeMesh, DialPivot);
		Outer->SetCastShadow(false);
		TrackOuterSegments.Add(Outer);
	}

	for (int32 i = 0; i < NumObstacleSlots; ++i)
	{
		ObstacleMeshes.Add(MakeMesh(*FString::Printf(TEXT("Obstacle%d"), i), CylinderMesh, DialPivot));
		UStaticMeshComponent* Shadow = MakeMesh(*FString::Printf(TEXT("ObstacleShadow%d"), i), CylinderMesh, DialPivot);
		Shadow->SetCastShadow(false);
		ObstacleShadows.Add(Shadow);

		UStaticMeshComponent* TopGlow = MakeMesh(*FString::Printf(TEXT("ObstacleTopGlow%d"), i), CylinderMesh, DialPivot);
		TopGlow->SetCastShadow(false);
		ObstacleTopGlows.Add(TopGlow);

		UStaticMeshComponent* BaseGlow = MakeMesh(*FString::Printf(TEXT("ObstacleBaseGlow%d"), i), CylinderMesh, DialPivot);
		BaseGlow->SetCastShadow(false);
		ObstacleBaseGlows.Add(BaseGlow);

		// Its neon sign: a dark board, and the tubes of its arrow and frame.
		ObstacleSignBoards.Add(MakeMesh(*FString::Printf(TEXT("ObstacleSignBoard%d"), i), CubeMesh, DialPivot));
		for (int32 t = 0; t < NumSignNeon; ++t)
		{
			UStaticMeshComponent* Tube = MakeMesh(*FString::Printf(TEXT("ObstacleSignNeon%d_%d"), i, t), CubeMesh, DialPivot);
			Tube->SetCastShadow(false);
			ObstacleSignNeon.Add(Tube);
		}
	}
	ObstacleStates.SetNum(NumObstacleSlots);

	for (int32 i = 0; i < NumSparks; ++i)
	{
		UStaticMeshComponent* Spark = MakeMesh(*FString::Printf(TEXT("Spark%d"), i), CubeMesh, DialPivot);
		Spark->SetCastShadow(false);
		SparkMeshes.Add(Spark);
	}
	Sparks.SetNum(NumSparks);

	DialGrowthSteps.Add(FSpinDialGrowthStep(0.35f, 1.4f)); // out to the city's 900 floor ring
	DialGrowthSteps.Add(FSpinDialGrowthStep(0.7f, 2.f));   // out to the 1300 ring

	// Music: flat 2D stereo, started by UpdateMusic. Counting as UI sound keeps it going through a pause.
	auto MakeMusicPlayer = [this](FName Name) -> UAudioComponent*
	{
		UAudioComponent* Player = CreateDefaultSubobject<UAudioComponent>(Name);
		Player->SetupAttachment(Root);
		Player->bAutoActivate = false;
		Player->bAllowSpatialization = false;
		Player->bIsUISound = true;
		return Player;
	};
	MenuMusicPlayer = MakeMusicPlayer(TEXT("MenuMusicPlayer"));
	RunMusicPlayer = MakeMusicPlayer(TEXT("RunMusicPlayer"));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->bConstrainAspectRatio = false; // no black bars on non-16:9 windows

	FillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(Root);
	FillLight->SetMobility(EComponentMobility::Movable);
	FillLight->SetCastShadows(false);
}

void ASpinClock::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Lay the dial out in the editor too, so a placed dial shows what it will look like (and where
	// its camera points) while its settings are tuned. Re-runs whenever a setting changes.
	BuildVisuals();

	// The zone only exists during a run; outside play there's nowhere to put it.
	if (!GetWorld() || !GetWorld()->IsGameWorld())
	{
		for (UStaticMeshComponent* Segment : ZoneSegments)
		{
			Segment->SetVisibility(false);
		}
		for (UStaticMeshComponent* Mark : ZoneMarks)
		{
			Mark->SetVisibility(false);
		}
	}
}

void ASpinClock::BeginPlay()
{
	Super::BeginPlay();

	GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ASpinGameMode>() : nullptr;
	for (TActorIterator<ASpinCityFX> It(GetWorld()); It; ++It)
	{
		CityFX = *It;
		break;
	}
	if (const ASpinGameMode* GM = GameMode.Get())
	{
		PrevHandAngle = GM->HandAngle;
	}

	// A dial saved into a level keeps the attachments it was saved with, which can be from before
	// DialPivot existed - so make sure everything on the dial really hangs off it.
	const TArray<TObjectPtr<USceneComponent>> Attached = Root->GetAttachChildren();
	for (USceneComponent* Child : Attached)
	{
		if (Child->IsA<UStaticMeshComponent>() || Child == HandPivot)
		{
			Child->AttachToComponent(DialPivot, FAttachmentTransformRules::KeepRelativeTransform);
		}
	}

	BuildVisuals();
}

void ASpinClock::BuildVisuals()
{
	const float R = DialRadius;

	// Sizes: the engine's cylinder and cube are 100 units across, so scale = size / 100.
	const float RimThickness = 20.f;
	const float HubRadius = R * HubRadiusFraction;
	const float HandLength = R * HandLengthFraction;

	Rim->SetRelativeScale3D(FVector(R * 2.14f / 100.f, R * 2.14f / 100.f, RimThickness / 100.f));
	Rim->SetRelativeLocation(FVector(0.f, 0.f, -4.f - RimThickness * 0.5f));

	Dial->SetRelativeScale3D(FVector(R * 2.f / 100.f, R * 2.f / 100.f, DialThickness / 100.f));
	Dial->SetRelativeLocation(FVector(0.f, 0.f, -DialThickness * 0.5f));

	Hub->SetRelativeScale3D(FVector(HubRadius * 2.f / 100.f, HubRadius * 2.f / 100.f, HubHeight / 100.f));
	Hub->SetRelativeLocation(FVector(0.f, 0.f, HubHeight * 0.5f));

	// The hand extends along the pivot's +X, and sits just above the zone band. Its dark body runs
	// from a short counterweight behind the hub to where the pointed tip begins; neon traces its edges,
	// a spine runs down its middle, a chevron makes the tip, and a bar caps the tail. It's in two
	// halves, jointed in the middle (HandOuterPivot), so a jam can buckle it.
	{
		// A dial saved into a level keeps the attachments it was saved with, which can be from before
		// the joint existed - so make sure the outer half really hangs off it.
		for (int32 i = NumInnerHandNeon; i < HandNeon.Num(); ++i)
		{
			if (HandNeon[i]->GetAttachParent() != HandOuterPivot)
			{
				HandNeon[i]->AttachToComponent(HandOuterPivot, FAttachmentTransformRules::KeepRelativeTransform);
			}
		}
		for (int32 i = 0; i < NumInnerHandNeon && i < HandNeon.Num(); ++i)
		{
			if (HandNeon[i]->GetAttachParent() != HandPivot)
			{
				HandNeon[i]->AttachToComponent(HandPivot, FAttachmentTransformRules::KeepRelativeTransform);
			}
		}

		const float L = HandLength;
		const float HalfW = HandWidth * 0.4f;
		const float Top = 3.f + HandHeight + 1.f;
		const float E = FMath::Max(HandLineWidth, 1.f);
		const float TailX = -L * 0.14f;
		const float TipStartX = L * 0.86f;

		// The joint, halfway along. Inner parts are placed relative to the hub, outer ones relative to
		// the joint (hence the "- Joint" below).
		const float Joint = L * 0.5f;
		HandOuterPivot->SetRelativeLocation(FVector(Joint, 0.f, 0.f));

		Hand->SetRelativeScale3D(FVector(Joint - TailX, HandWidth * 0.8f, HandHeight) / 100.f);
		Hand->SetRelativeLocation(FVector((Joint + TailX) * 0.5f, 0.f, 3.f + HandHeight * 0.5f));
		HandOuterBody->SetRelativeScale3D(FVector(TipStartX - Joint, HandWidth * 0.8f, HandHeight) / 100.f);
		HandOuterBody->SetRelativeLocation(FVector((TipStartX - Joint) * 0.5f, 0.f, 3.f + HandHeight * 0.5f));

		auto Line = [](UStaticMeshComponent* Comp, const FVector& A, const FVector& B, float Width)
		{
			const FVector D = B - A;
			const float Length = D.Size();
			Comp->SetRelativeLocation((A + B) * 0.5f);
			Comp->SetRelativeRotation(Length > KINDA_SMALL_NUMBER ? D.Rotation() : FRotator::ZeroRotator);
			Comp->SetRelativeScale3D(FVector(Length + Width, Width, Width) / 100.f);
		};
		if (HandNeon.Num() >= NumHandNeon)
		{
			// Inner half: edges and spine up to the joint, and the tail cap.
			Line(HandNeon[0], FVector(L * 0.02f, -HalfW, Top), FVector(Joint, -HalfW, Top), E);
			Line(HandNeon[1], FVector(L * 0.02f, HalfW, Top), FVector(Joint, HalfW, Top), E);
			Line(HandNeon[2], FVector(L * 0.1f, 0.f, Top + 1.f), FVector(Joint, 0.f, Top + 1.f), E * 0.7f);
			Line(HandNeon[3], FVector(TailX, -HandWidth * 0.5f, Top), FVector(TailX, HandWidth * 0.5f, Top), E * 1.4f);
			// Outer half: edges and spine from the joint on, and the pointed tip.
			Line(HandNeon[4], FVector(0.f, -HalfW, Top), FVector(TipStartX - Joint, -HalfW, Top), E);
			Line(HandNeon[5], FVector(0.f, HalfW, Top), FVector(TipStartX - Joint, HalfW, Top), E);
			Line(HandNeon[6], FVector(0.f, 0.f, Top + 1.f), FVector(L * 0.8f - Joint, 0.f, Top + 1.f), E * 0.7f);
			Line(HandNeon[7], FVector(TipStartX - Joint, -HalfW, Top), FVector(L - Joint, 0.f, Top), E);
			Line(HandNeon[8], FVector(TipStartX - Joint, HalfW, Top), FVector(L - Joint, 0.f, Top), E);
		}
	}

	// The collar seating the hand on its spindle (the spindle itself is laid out by UpdateSpindle).
	{
		const float SpindleRadius = HubRadius * 0.45f;
		HandCollar->SetRelativeLocation(FVector(0.f, 0.f, 3.f + HandHeight * 0.5f));
		HandCollar->SetRelativeScale3D(FVector(SpindleRadius * 3.2f, SpindleRadius * 3.2f, HandHeight + 4.f) / 100.f);
		UpdateSpindle(0.f);
	}

	// Obstacles' signs: hidden until one is spawned (PlaceObstacle draws them).
	for (const TArray<TObjectPtr<UStaticMeshComponent>>* Parts : { &ObstacleSignNeon, &ObstacleSignBoards })
	{
		for (UStaticMeshComponent* Part : *Parts)
		{
			Part->SetVisibility(false);
		}
	}

	// Obstacles: round posts, hidden until one is spawned (which also sets their height).
	for (UStaticMeshComponent* Post : ObstacleMeshes)
	{
		Post->SetRelativeScale3D(FVector(ObstacleRadius * 2.f / 100.f, ObstacleRadius * 2.f / 100.f, ObstacleHeight / 100.f));
		Post->SetVisibility(false);
	}
	// High posts' shadows: a flat square lying on the dial (matching the post's shape, see SpawnObstacle).
	for (UStaticMeshComponent* Shadow : ObstacleShadows)
	{
		Shadow->SetRelativeScale3D(FVector(ObstacleRadius * 2.f / 100.f, ObstacleRadius * 2.f / 100.f, 1.f / 100.f));
		Shadow->SetVisibility(false);
	}
	// Their neon plates: thin caps a touch wider than the hull.
	const float GlowSize = ObstacleRadius * 2.f * ObstacleGlowOverhang / 100.f;
	for (const TArray<TObjectPtr<UStaticMeshComponent>>* Glows : { &ObstacleTopGlows, &ObstacleBaseGlows })
	{
		for (UStaticMeshComponent* Glow : *Glows)
		{
			Glow->SetRelativeScale3D(FVector(GlowSize, GlowSize, ObstacleGlowThickness / 100.f));
			Glow->SetVisibility(false);
		}
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

	// The face and rim are dark hull, like the city's buildings.
	MakeMID(Rim, RimColor);
	MakeMID(Dial, DialColor);

	// The hand, hub and zone glow like the city's neon (falling back to the plain lit material).
	if (NeonMaterial)
	{
		Hub->SetMaterial(0, NeonMaterial);
	}
	HubMID = MakeMID(Hub, HubIdleColor);

	// The hand: a dark body (lit material), with its neon lines sharing one glowing material whose
	// colour shows the hand's state (see UpdateColors).
	if (BaseMaterial)
	{
		Hand->SetMaterial(0, BaseMaterial); // (it was neon once; a saved level may still say so)
	}
	MakeMID(Hand, HandBodyColor);
	if (BaseMaterial)
	{
		HandSpindle->SetMaterial(0, BaseMaterial);
		HandOuterBody->SetMaterial(0, BaseMaterial);
	}
	MakeMID(HandOuterBody, HandBodyColor);
	MakeMID(HandSpindle, HandBodyColor);
	HandMID = UMaterialInstanceDynamic::Create(NeonMaterial ? NeonMaterial.Get() : BaseMaterial.Get(), this);
	if (HandMID)
	{
		HandMID->SetVectorParameterValue(ColorParam, FLinearColor::White * (NeonMaterial ? HandGlow : 1.f));
		for (UStaticMeshComponent* Line : HandNeon)
		{
			Line->SetMaterial(0, HandMID);
		}
		HandSpindleBand->SetMaterial(0, HandMID);
		HandCollar->SetMaterial(0, HandMID);
	}

	if (BaseMaterial)
	{
		ZoneMID = UMaterialInstanceDynamic::Create(NeonMaterial ? NeonMaterial.Get() : BaseMaterial.Get(), this);
		for (UStaticMeshComponent* Segment : ZoneSegments)
		{
			Segment->SetMaterial(0, ZoneMID);
		}

		// Grade lines glow: the emissive neon material when it's available, otherwise the plain lit one.
		// The outer pair (index 0 and 3) marks GOOD, the inner pair marks PERFECT, see UpdateZone.
		UMaterialInterface* MarkMaterial = NeonMaterial ? NeonMaterial.Get() : BaseMaterial.Get();
		PerfectMarkMID = UMaterialInstanceDynamic::Create(MarkMaterial, this);
		GoodMarkMID = UMaterialInstanceDynamic::Create(MarkMaterial, this);
		if (PerfectMarkMID && GoodMarkMID)
		{
			PerfectMarkMID->SetVectorParameterValue(ColorParam, PerfectMarkColor);
			GoodMarkMID->SetVectorParameterValue(ColorParam, GoodMarkColor);
		}
		for (int32 i = 0; i < ZoneMarks.Num(); ++i)
		{
			const bool bGood = i == 0 || i == ZoneMarks.Num() - 1;
			ZoneMarks[i]->SetMaterial(0, bGood ? GoodMarkMID : PerfectMarkMID);
		}

		// Sparks glow: the emissive neon material when it's available (pushed well past 1 so it
		// blooms), otherwise the plain lit one.
		SparkMID = UMaterialInstanceDynamic::Create(NeonMaterial ? NeonMaterial.Get() : BaseMaterial.Get(), this);
		if (SparkMID)
		{
			SparkMID->SetVectorParameterValue(ColorParam, NeonMaterial ? FLinearColor(6.f, 3.5f, 0.6f) : FLinearColor(1.f, 0.85f, 0.2f));
		}
		for (UStaticMeshComponent* Spark : SparkMeshes)
		{
			Spark->SetMaterial(0, SparkMID);
		}

		// The obstacles' hulls are dark, like the city's buildings, with just a hint of their neon's colour.
		ObstacleMID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		if (ObstacleMID)
		{
			ObstacleMID->SetVectorParameterValue(ColorParam, ObstacleColor * 0.03f);
		}
		for (UStaticMeshComponent* Post : ObstacleMeshes)
		{
			Post->SetMaterial(0, ObstacleMID);
		}

		HighObstacleMID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		if (HighObstacleMID)
		{
			HighObstacleMID->SetVectorParameterValue(ColorParam, HighObstacleColor * 0.03f);
		}

		// A dark tint of the high posts' colour.
		ShadowMID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		if (ShadowMID)
		{
			ShadowMID->SetVectorParameterValue(ColorParam, HighObstacleColor * 0.03f);
		}
		for (UStaticMeshComponent* Shadow : ObstacleShadows)
		{
			Shadow->SetMaterial(0, ShadowMID);
		}

		// Each obstacle's neon has a material of its own, since each fades on its own.
		ObstacleGlowMIDs.Reset();
		for (int32 i = 0; i < NumObstacleSlots; ++i)
		{
			UMaterialInstanceDynamic* GlowMID = UMaterialInstanceDynamic::Create(NeonMaterial ? NeonMaterial.Get() : BaseMaterial.Get(), this);
			ObstacleGlowMIDs.Add(GlowMID);
			ObstacleTopGlows[i]->SetMaterial(0, GlowMID);
			ObstacleBaseGlows[i]->SetMaterial(0, GlowMID);
			for (int32 t = 0; t < NumSignNeon; ++t)
			{
				ObstacleSignNeon[i * NumSignNeon + t]->SetMaterial(0, GlowMID);
			}
		}

		// The signs' boards: dark, like the city's buildings.
		for (UStaticMeshComponent* Board : ObstacleSignBoards)
		{
			Board->SetMaterial(0, BaseMaterial);
			MakeMID(Board, ObstacleSignBoardColor);
		}
	}

	// Thin neon ring highlights: built the same way as the zone band (a ring of small blocks),
	// but as a full 360-degree circle that never moves - only its colour changes.
	if (NeonMaterial)
	{
		auto BuildRing = [this](TArray<TObjectPtr<UStaticMeshComponent>>& Segments, float RadiusFraction, float ThicknessFraction, float Z, float Height, UMaterialInstanceDynamic* MID)
		{
			const int32 Count = Segments.Num();
			const float Rho = DialRadius * RadiusFraction;
			const float StepDeg = 360.f / Count;
			const float TangentLength = FMath::DegreesToRadians(StepDeg) * Rho * 1.15f;
			const float RadialLength = DialRadius * ThicknessFraction;

			for (int32 i = 0; i < Count; ++i)
			{
				const float AngleDeg = (i + 0.5f) * StepDeg;
				const float Rad = FMath::DegreesToRadians(AngleDeg);

				UStaticMeshComponent* Segment = Segments[i];
				Segment->SetRelativeLocation(FVector(Rho * FMath::Cos(Rad), Rho * FMath::Sin(Rad), Z));
				Segment->SetRelativeRotation(FRotator(0.f, AngleDeg, 0.f));
				Segment->SetRelativeScale3D(FVector(RadialLength / 100.f, TangentLength / 100.f, Height / 100.f));
				Segment->SetMaterial(0, MID);
			}
		};

		OuterRingMID = UMaterialInstanceDynamic::Create(NeonMaterial, this);
		BuildRing(OuterRingSegments, OuterRingRadiusFraction, RingThicknessFraction, 2.f, 2.5f, OuterRingMID);

		InnerRingMID = UMaterialInstanceDynamic::Create(NeonMaterial, this);
		if (InnerRingMID)
		{
			InnerRingMID->SetVectorParameterValue(ColorParam, InnerRingColor);
		}
		BuildRing(InnerRingSegments, InnerRingRadiusFraction, RingThicknessFraction, 2.f, 2.5f, InnerRingMID);

		// Faint track along both edges of the zone band. It sits low, so a zone covers it where it lies.
		TrackMID = UMaterialInstanceDynamic::Create(NeonMaterial, this);
		if (TrackMID)
		{
			TrackMID->SetVectorParameterValue(ColorParam, TrackColor);
		}
		const float TrackThickness = RingThicknessFraction * 0.5f;
		BuildRing(TrackInnerSegments, ZoneRadiusFraction - ZoneThicknessFraction * 0.5f, TrackThickness, 0.6f, 1.f, TrackMID);
		BuildRing(TrackOuterSegments, ZoneRadiusFraction + ZoneThicknessFraction * 0.5f, TrackThickness, 0.6f, 1.f, TrackMID);

		// Clock-face ticks: 60 around the edge, every fifth one longer and wider (the "hours").
		TickMID = UMaterialInstanceDynamic::Create(NeonMaterial, this);
		if (TickMID)
		{
			TickMID->SetVectorParameterValue(ColorParam, TickColor);
		}
		for (int32 i = 0; i < TickMarks.Num(); ++i)
		{
			const bool bMajor = i % 5 == 0;
			const float AngleDeg = i * 360.f / TickMarks.Num();
			const float Rad = FMath::DegreesToRadians(AngleDeg);
			const float Length = R * (bMajor ? 0.05f : 0.022f);
			const float Width = bMajor ? 6.f : 2.5f;
			const float Rho = R * TickRadiusFraction + (bMajor ? 0.f : R * 0.014f); // minor ticks hug the rim

			UStaticMeshComponent* Tick = TickMarks[i];
			Tick->SetRelativeLocation(FVector(Rho * FMath::Cos(Rad), Rho * FMath::Sin(Rad), 1.f));
			Tick->SetRelativeRotation(FRotator(0.f, AngleDeg, 0.f));
			Tick->SetRelativeScale3D(FVector(Length / 100.f, Width / 100.f, 1.5f / 100.f));
			Tick->SetMaterial(0, TickMID);
		}
	}
	else
	{
		// Without the neon material the decorative lines can't glow: hide them rather than leave
		// unscaled cubes sitting at the centre of the dial.
		for (auto* Group : { &OuterRingSegments, &InnerRingSegments, &TrackInnerSegments, &TrackOuterSegments, &TickMarks })
		{
			for (UStaticMeshComponent* Comp : *Group)
			{
				Comp->SetVisibility(false);
			}
		}
	}

	// Camera: looking down at the dial from the "6 o'clock" side (the close-up; see SetCameraBase).
	SetCameraBase(0.f);
	Camera->SetFieldOfView(CameraFOV);

	// Fill light so the scene is visible even in an unlit level.
	FillLight->SetRelativeLocation(FVector(0.f, 0.f, R * 2.5f));
	FillLight->SetIntensity(FillLightIntensity);
	FillLight->SetLightColor(FillLightColor);
	FillLight->SetAttenuationRadius(R * 12.f);
}

void ASpinClock::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// How far the hand moved since last frame. A big jump is a reset (new run), not real travel.
	float Moved = 0.f;
	if (ASpinGameMode* GM = GameMode.Get())
	{
		Moved = FMath::Abs(FMath::FindDeltaAngleDegrees(PrevHandAngle, GM->HandAngle));
		PrevHandAngle = GM->HandAngle;
		if (Moved > 45.f)
		{
			Moved = 0.f;
		}
		UpdateObstaclePassing(*GM, Moved);
	}

	// Lift: the hand rises while the button is held, whether or not an obstacle is out. It drops
	// by itself (even if still held) once it has been carried a full turn, and the button has to
	// be pressed again to lift it for the next obstacle.
	if (IsLiftActive())
	{
		LiftTravelDeg += Moved;
		if (LiftTravelDeg >= MaxLiftTravelDeg)
		{
			bLiftPressValid = false;
		}
	}
	// The hand rides a spring toward its target height rather than sliding at a fixed speed: it snaps
	// up, overshoots a touch and settles; dropping, it falls and bounces lightly off the dial. (Small
	// steps keep the spring steady however long the frame was.)
	const bool bRaise = IsLiftActive();
	const float LiftTarget = bRaise ? MaxHandLift : 0.f;
	{
		const float Stiffness = bRaise ? LiftRiseStiffness : LiftDropStiffness;
		const float Damping = 2.f * (bRaise ? LiftRiseDamping : LiftDropDamping) * FMath::Sqrt(Stiffness);
		const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaSeconds * 240.f), 1, 24);
		const float Step = DeltaSeconds / Steps;
		for (int32 i = 0; i < Steps; ++i)
		{
			HandLiftVelocity += (Stiffness * (LiftTarget - HandLift) - Damping * HandLiftVelocity) * Step;
			HandLift += HandLiftVelocity * Step;
			if (HandLift < 0.f)
			{
				HandLift = 0.f;
				HandLiftVelocity = HandLiftVelocity < 0.f ? -HandLiftVelocity * LiftLandingBounce : HandLiftVelocity;
			}
		}
		HandHoverTime = HandLift > MaxHandLift * 0.5f ? HandHoverTime + DeltaSeconds : 0.f;
	}

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
		else
		{
			// Each catch of the gears makes the hand snag for a moment (visual only).
			HandRotation.Yaw -= GM->Direction * GrindFlash * 1.2f;
			// As it lifts or drops, its tip lags behind the hub - tipping down on the way up, up on the way down.
			HandRotation.Pitch = FMath::Clamp(-HandLiftVelocity * LiftTiltPerSpeed, -4.f, 4.f);
		}
		HandPivot->SetRelativeRotation(HandRotation);

		// Held up, it hovers gently (easing in, so it doesn't jerk the moment it gets there). Looks
		// only: the hit test uses HandLift.
		const float Hover = LiftHoverBob * FMath::Sin(HandHoverTime * 5.f) * FMath::Clamp(HandHoverTime * 2.f, 0.f, 1.f);
		HandPivot->SetRelativeLocation(FVector(0.f, 0.f, HandLift + Hover));
		UpdateSpindle(HandLift + Hover);

		// A jam buckles the hand at its joint (sideways and drooping), and fixing it straightens it -
		// both happening while the hand glitches.
		HandBend = FMath::FInterpConstantTo(HandBend, bJammed ? 1.f : 0.f, DeltaSeconds, 1.f / FMath::Max(0.05f, JamGlitchTime * 0.7f));
		const float Bend = FMath::SmoothStep(0.f, 1.f, HandBend);
		HandOuterPivot->SetRelativeRotation(FRotator(-JamDroop * Bend, JamBendAngle * Bend, 0.f));

		// The glitch: the hand's parts blink out at random and it jumps about (it also flashes colours,
		// see UpdateColors). Looks only - the hand's real position is untouched.
		const bool bWasGlitching = HandGlitchTimer > 0.f;
		HandGlitchTimer = FMath::Max(0.f, HandGlitchTimer - DeltaSeconds);
		UStaticMeshComponent* HandParts[] = { Hand, HandOuterBody, HandCollar };
		if (HandGlitchTimer > 0.f)
		{
			const float Strength = FMath::Clamp(HandGlitchTimer / FMath::Max(0.01f, JamGlitchTime), 0.f, 1.f);
			HandGlitchSeed = static_cast<int32>(HashCombine(static_cast<uint32>(FMath::FloorToInt(GetWorld()->GetTimeSeconds() * DialGlitchRate)), 0x7A11u));
			const FRandomStream Noise(HandGlitchSeed);
			const float ShowChance = 0.4f + 0.6f * (1.f - Strength);
			for (UStaticMeshComponent* Part : HandParts)
			{
				Part->SetVisibility(Noise.FRand() < ShowChance);
			}
			for (UStaticMeshComponent* Line : HandNeon)
			{
				Line->SetVisibility(Noise.FRand() < ShowChance);
			}
			const FVector Jump(Noise.FRandRange(-1.f, 1.f), Noise.FRandRange(-1.f, 1.f), Noise.FRandRange(-0.4f, 0.4f));
			HandPivot->SetRelativeLocation(HandPivot->GetRelativeLocation() + Jump * 30.f * Strength);
		}
		else if (bWasGlitching)
		{
			for (UStaticMeshComponent* Part : HandParts)
			{
				Part->SetVisibility(true);
			}
			for (UStaticMeshComponent* Line : HandNeon)
			{
				Line->SetVisibility(true);
			}
		}

		InvincibleDim = FMath::FInterpConstantTo(InvincibleDim, (GM->bInvincible || GM->IsResumeCountingDown()) ? 1.f : 0.f, DeltaSeconds, 1.f / InvincibleFadeTime);

		UpdateZone(*GM);
		UpdateColors(*GM);
		UpdateMusic(*GM);
	}

	UpdateObstacles(DeltaSeconds);
	UpdateSparks(DeltaSeconds);

	// The camera pulls back toward the skyline as the city lights up. The light moves in stops and
	// starts (it chases each hit), so the camera follows on a spring rather than copying it.
	if (bCameraFollowsReveal)
	{
		const float TargetBlend = GetRevealCameraBlend(*this);
		if (CameraBlend < 0.f)
		{
			CameraBlend = TargetBlend; // first frame: no sweep from nothing
		}
		const float SmoothTime = TargetBlend < CameraBlend ? CameraReturnSmoothTime : CameraRiseSmoothTime;
		FMath::CriticallyDampedSmoothing(CameraBlend, CameraBlendRate, TargetBlend, 0.f, DeltaSeconds, SmoothTime);
		SetCameraBase(CameraBlend);
	}

	UpdateDialGrowth(DeltaSeconds);
	UpdateCameraMotion(DeltaSeconds);
}

void ASpinClock::UpdateDialGrowth(float DeltaSeconds)
{
	// The biggest step the camera has pulled back far enough for.
	float TargetScale = 1.f;
	for (const FSpinDialGrowthStep& Step : DialGrowthSteps)
	{
		if (CameraBlend >= Step.CameraBlend)
		{
			TargetScale = FMath::Max(TargetScale, Step.Scale);
		}
	}

	const float OldScale = DialScale;
	if (DialGrowTo < 0.f)
	{
		DialScale = DialGrowFrom = DialGrowTo = TargetScale; // first frame: no animation from nothing
	}
	else if (TargetScale != DialGrowTo)
	{
		DialGrowFrom = DialScale;
		DialGrowTo = TargetScale;
		DialGrowElapsed = 0.f;
	}

	const bool bGrowing = DialGrowTo > DialGrowFrom;
	const float Duration = FMath::Max(0.01f, bGrowing ? DialGrowTime : DialShrinkTime);
	const float PrevElapsed = DialGrowElapsed;
	DialGrowElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(DialGrowElapsed / Duration, 0.f, 1.f);

	// Mid-change the dial glitches: every few frames it lands on a random size, position and colour,
	// more and more likely to be the final size, until it locks in. The HUD draws its blocks from the
	// same seed, so they all flicker together.
	FVector Jitter = FVector::ZeroVector;
	if (Alpha < 1.f && DialGrowFrom != DialGrowTo)
	{
		DialGlitchSeed = static_cast<int32>(HashCombine(static_cast<uint32>(FMath::FloorToInt(DialGrowElapsed * DialGlitchRate)), 0x51A3u));
		const FRandomStream Glitch(DialGlitchSeed);

		const float Roll = Glitch.FRand();
		DialScale = Roll < Alpha ? DialGrowTo
			: Roll < Alpha + 0.25f ? FMath::Lerp(DialGrowFrom, DialGrowTo, Glitch.FRand())
			: DialGrowFrom;

		Jitter = FVector(Glitch.FRandRange(-1.f, 1.f), Glitch.FRandRange(-1.f, 1.f), 0.f) * DialGlitchJitter * (1.f - Alpha);

		DialGlitchTint = GlitchPalette[Glitch.RandHelper(UE_ARRAY_COUNT(GlitchPalette))];
		DialGlitch = 1.f - Alpha * Alpha * Alpha;
	}
	else
	{
		DialScale = DialGrowTo;
		DialGlitch = 0.f;
	}

	// Locking into a bigger size: the rings flare, and mid-run the dial lands with a thump.
	const ASpinGameMode* GM = GameMode.Get();
	if (bGrowing && PrevElapsed < Duration && DialGrowElapsed >= Duration && GM && GM->State == ESpinState::Playing)
	{
		DialGrowFlash = 1.f;
		PlayShake(DialGrowShake);
	}
	DialGrowFlash = FMath::Max(0.f, DialGrowFlash - DeltaSeconds * 3.f);

	// Only when they change: these move every mesh on the dial.
	if (DialScale != OldScale || !DialPivot->GetRelativeScale3D().Equals(FVector(DialScale)))
	{
		DialPivot->SetRelativeScale3D(FVector(DialScale));
	}
	if (!DialPivot->GetRelativeLocation().Equals(Jitter))
	{
		DialPivot->SetRelativeLocation(Jitter);
	}
}

void ASpinClock::UpdateMusic(const ASpinGameMode& GM)
{
	const bool bGameOver = GM.State == ESpinState::GameOver;
	if (bGameOver && !bWasGameOver && GameOverSting)
	{
		UGameplayStatics::PlaySound2D(this, GameOverSting);
	}
	bWasGameOver = bGameOver;

	// The game-over screen has no music of its own: the sting has it to itself.
	UAudioComponent* Wanted = bGameOver ? nullptr : GM.IsInRun() ? RunMusicPlayer.Get() : MenuMusicPlayer.Get();
	if (Wanted == ActiveMusicPlayer)
	{
		// A run track without Looping has finished: on to another.
		if (Wanted && Wanted == RunMusicPlayer && !Wanted->IsPlaying())
		{
			StartNextRunTrack();
		}
		return;
	}

	if (ActiveMusicPlayer)
	{
		ActiveMusicPlayer->FadeOut(MusicFadeTime, 0.f);
	}
	ActiveMusicPlayer = Wanted;
	if (Wanted == RunMusicPlayer)
	{
		StartNextRunTrack();
	}
	else if (Wanted && MenuMusic)
	{
		Wanted->SetSound(MenuMusic);
		Wanted->FadeIn(MusicFadeTime);
	}
}

void ASpinClock::StartNextRunTrack()
{
	// Plain FMath rather than the run's random stream, so the music can't change the daily's rolls.
	TArray<USoundBase*> Choices;
	for (USoundBase* Track : RunMusic)
	{
		if (Track && Track != LastRunTrack)
		{
			Choices.Add(Track);
		}
	}

	// With a single track there's nothing else to pick, so it just plays again.
	USoundBase* Track = Choices.Num() > 0 ? Choices[FMath::RandHelper(Choices.Num())] : LastRunTrack.Get();
	if (Track)
	{
		LastRunTrack = Track;
		RunMusicPlayer->SetSound(Track);
		RunMusicPlayer->FadeIn(MusicFadeTime);
	}
}

void ASpinClock::SetMusicDucked(bool bDucked)
{
	if (ActiveMusicPlayer && ActiveMusicPlayer->IsPlaying())
	{
		ActiveMusicPlayer->AdjustVolume(0.25f, bDucked ? MusicPauseVolume : 1.f);
	}
}

void ASpinClock::SetSparking(bool bNewSparking)
{
	bSparking = bNewSparking;
}

void ASpinClock::SetJammed(bool bNewJammed)
{
	// The hand glitches as it breaks, and again as it's fixed.
	if (bNewJammed != bJammed)
	{
		HandGlitchTimer = JamGlitchTime;
	}
	bJammed = bNewJammed;
	if (!bJammed)
	{
		JamWobbleTime = 0.f;
		JamTrailOwed = 0.f;
	}
}

void ASpinClock::UpdateSparks(float DeltaSeconds)
{
	// Everything is sized relative to the dial, so the sparks stay readable from the far-off camera
	// however big the dial is (at the original 1-6 unit size they were about a pixel on screen).
	const float SizeScale = DialRadius / 600.f;
	const ASpinGameMode* GM = GameMode.Get();
	const float SpinSign = GM ? GM->Direction : 1.f;
	const float HubRadius = DialRadius * HubRadiusFraction;

	// Gears catching inside the hub pillar: irregular bursts of streaks thrown off one point of its
	// rim, tangentially (the way a grinding edge throws them), rather than a constant shower.
	if (bSparking)
	{
		GrindBurstTimer -= DeltaSeconds;
		if (GrindBurstTimer <= 0.f)
		{
			GrindBurstTimer = FMath::FRandRange(0.12f, 0.45f);
			GrindFlash = 1.f;

			const float ContactAngle = FMath::FRandRange(0.f, 2.f * PI);
			const int32 Count = FMath::RandRange(5, 10);
			int32 Spawned = 0;
			for (FSpark& S : Sparks)
			{
				if (Spawned >= Count)
				{
					break;
				}
				if (S.Life > 0.f)
				{
					continue;
				}
				++Spawned;

				const float A = ContactAngle + FMath::FRandRange(-0.35f, 0.35f);
				const FVector Radial(FMath::Cos(A), FMath::Sin(A), 0.f);
				const FVector Tangent = FVector(-Radial.Y, Radial.X, 0.f) * SpinSign;
				const FVector Dir = (Tangent + Radial * FMath::FRandRange(0.1f, 0.7f)).GetSafeNormal();

				S.Age = 0.f;
				S.Life = FMath::FRandRange(0.18f, 0.4f);
				S.Location = Radial * HubRadius + FVector(0.f, 0.f, FMath::FRandRange(4.f, 14.f));
				S.Velocity = Dir * FMath::FRandRange(280.f, 600.f) * SizeScale + FVector(0.f, 0.f, FMath::FRandRange(60.f, 240.f) * SizeScale);
			}
		}
	}
	else
	{
		GrindBurstTimer = 0.f;
	}
	GrindFlash = FMath::Max(0.f, GrindFlash - DeltaSeconds * 6.f);

	// A jammed hand throws a steady trail of sparks off its buckled outer half, flung out behind it
	// as it turns.
	if (bJammed && GM && HandOuterPivot)
	{
		JamTrailOwed += DeltaSeconds * JamTrailSparksPerSecond;
		const float Heading = FMath::DegreesToRadians(GM->HandAngle);
		const FVector Behind = -FVector(-FMath::Sin(Heading), FMath::Cos(Heading), 0.f) * SpinSign; // opposite to its travel
		const FTransform DialToWorld = DialPivot->GetComponentTransform();
		const FTransform OuterToWorld = HandOuterPivot->GetComponentTransform();
		const float OuterLength = DialRadius * HandLengthFraction * 0.5f;

		for (FSpark& S : Sparks)
		{
			if (JamTrailOwed < 1.f)
			{
				break;
			}
			if (S.Life > 0.f)
			{
				continue;
			}
			JamTrailOwed -= 1.f;

			// Somewhere along the outer half, favouring the tip.
			const float Along = OuterLength * FMath::Sqrt(FMath::FRand());
			const FVector World = OuterToWorld.TransformPosition(FVector(Along, FMath::FRandRange(-0.4f, 0.4f) * HandWidth, 3.f + HandHeight));
			S.Age = 0.f;
			S.Life = FMath::FRandRange(0.25f, 0.5f);
			S.Location = DialToWorld.InverseTransformPosition(World);
			S.Velocity = (Behind * FMath::FRandRange(150.f, 420.f) + FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(80.f, 260.f))) * SizeScale;
		}
		JamTrailOwed = FMath::Min(JamTrailOwed, 4.f); // don't build up a backlog if the pool is full
	}

	// The pillar glows hot with each catch, then cools.
	if (HubMID)
	{
		HubMID->SetVectorParameterValue(ColorParam, FMath::Lerp(HubIdleColor, HubHotColor, GrindFlash));
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

		S.Velocity.Z -= 1400.f * SizeScale * DeltaSeconds; // gravity
		S.Location += S.Velocity * DeltaSeconds;
		if (S.Location.Z < 2.f)
		{
			S.Location.Z = 2.f;
			S.Velocity.Z = FMath::Abs(S.Velocity.Z) * 0.3f;
		}

		// Thin streaks pointing along their flight, shrinking as they cool - not tumbling glitter.
		const float Fade = 1.f - S.Age / S.Life;
		const float Length = (26.f * Fade + 6.f) * SizeScale;
		const float Thickness = (4.f * Fade + 2.f) * SizeScale;
		Mesh->SetVisibility(true);
		Mesh->SetRelativeLocation(S.Location);
		Mesh->SetRelativeRotation(S.Velocity.Rotation());
		Mesh->SetRelativeScale3D(FVector(Length, Thickness, Thickness) / 100.f);
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
		for (UStaticMeshComponent* Mark : ZoneMarks)
		{
			Mark->SetVisibility(bShow);
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

	// Neon lines across the band where the hit grades change: inside the inner pair is PERFECT,
	// out to the outer pair is GOOD, beyond that is OK. Raised just above the band so they show.
	const float MarkFractions[NumZoneMarks] = { -GM.GoodFraction, -GM.PerfectFraction, GM.PerfectFraction, GM.GoodFraction };
	for (int32 i = 0; i < ZoneMarks.Num() && i < NumZoneMarks; ++i)
	{
		const float AngleDeg = GM.ZoneCenter + MarkFractions[i] * GM.ZoneHalfWidth;
		const float Rad = FMath::DegreesToRadians(AngleDeg);

		UStaticMeshComponent* Mark = ZoneMarks[i];
		Mark->SetRelativeLocation(FVector(Rho * FMath::Cos(Rad), Rho * FMath::Sin(Rad), 3.2f));
		Mark->SetRelativeRotation(FRotator(0.f, AngleDeg, 0.f));
		Mark->SetRelativeScale3D(FVector(RadialLength * 0.92f / 100.f, ZoneMarkWidth / 100.f, 0.5f / 100.f));
	}
}

void ASpinClock::UpdateColors(const ASpinGameMode& GM)
{
	const FLinearColor ZoneBase = GM.Prompts.IsValidIndex(GM.ActivePromptIndex)
		? GM.Prompts[GM.ActivePromptIndex].Color
		: FLinearColor(0.1f, 0.8f, 0.2f);

	// The zone brightens while the hand is inside it.
	const bool bActive = GM.State == ESpinState::Playing && GM.IsHandInZone();
	FLinearColor ZoneColor = bActive ? (ZoneBase + (FLinearColor::White - ZoneBase) * 0.45f) : ZoneBase;
	if (NeonMaterial)
	{
		ZoneColor *= bActive ? ZoneActiveGlow : ZoneGlow;
	}

	// Invincibility: everything but the hand dims and slowly breathes until play is live again.
	const float Breath = 0.1f * FMath::Sin(GetWorld()->GetTimeSeconds() * 4.f);
	const float Dim = FMath::Lerp(1.f, InvincibleBrightness + Breath, InvincibleDim);

	if (ZoneMID)
	{
		ZoneMID->SetVectorParameterValue(ColorParam, ZoneColor * Dim);
	}

	if (PerfectMarkMID && GoodMarkMID)
	{
		PerfectMarkMID->SetVectorParameterValue(ColorParam, PerfectMarkColor * Dim);
		GoodMarkMID->SetVectorParameterValue(ColorParam, GoodMarkColor * Dim);
	}

	// Successful hits flare the rings and the fill light, then fade back (the city does the same,
	// see ASpinCityFX).
	const float Pulse = FMath::Max(GM.GetHitPulse(), DialGrowFlash);
	const float RingGlow = (1.f + RingHitPulseBoost * Pulse) * Dim;

	if (OuterRingMID)
	{
		// The whole rim glows the colour of whichever button the next zone needs.
		OuterRingMID->SetVectorParameterValue(ColorParam, DialGlitch > 0.f ? DialGlitchTint : ZoneBase * RingGlow);
	}

	if (InnerRingMID)
	{
		InnerRingMID->SetVectorParameterValue(ColorParam, DialGlitch > 0.f ? DialGlitchTint : InnerRingColor * RingGlow);
	}

	// Ticks and track flare too, but more gently than the rings so they stay in the background.
	const float DetailGlow = (1.f + RingHitPulseBoost * 0.5f * Pulse) * Dim;
	if (TickMID)
	{
		TickMID->SetVectorParameterValue(ColorParam, DialGlitch > 0.f ? DialGlitchTint : TickColor * DetailGlow);
	}
	if (TrackMID)
	{
		TrackMID->SetVectorParameterValue(ColorParam, DialGlitch > 0.f ? DialGlitchTint : TrackColor * DetailGlow);
	}

	if (FillLight)
	{
		FillLight->SetIntensity(FillLightIntensity * (1.f + FillLightHitPulseBoost * Pulse) * Dim);
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
		else if (bSparking)
		{
			// Overheating: flickers between white and hot orange, faster than any other hand state.
			const float Flicker = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * 30.f);
			HandColor = FMath::Lerp(FLinearColor::White, FLinearColor(1.f, 0.35f, 0.05f), Flicker);
		}
		else if (IsHandRaised())
		{
			HandColor = FLinearColor(0.4f, 0.9f, 1.f);
		}

		// Glitching (as it jams, and as it's fixed): flashes of random colour.
		if (HandGlitchTimer > 0.f)
		{
			const FRandomStream Noise(HandGlitchSeed + 7);
			if (Noise.FRand() < 0.6f)
			{
				HandColor = GlitchPalette[Noise.RandHelper(UE_ARRAY_COUNT(GlitchPalette))];
			}
		}
		HandMID->SetVectorParameterValue(ColorParam, NeonMaterial ? HandColor * HandGlow : HandColor);
	}
}

void ASpinClock::SetLiftHeld(bool bHeld, bool bValidPress)
{
	// A fresh press starts a fresh lift; releasing (or holding on) doesn't.
	if (bHeld && !bLiftHeld)
	{
		LiftTravelDeg = 0.f;
	}
	bLiftHeld = bHeld;
	bLiftPressValid = bHeld && bValidPress;
}

bool ASpinClock::IsLiftActive() const
{
	return bLiftHeld && bLiftPressValid;
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
		const bool bOut = S.Phase == EObstaclePhase::FadingIn || S.Phase == EObstaclePhase::Waiting;
		if (bOut && !S.bPassed && !S.bHigh)
		{
			return true;
		}
	}

	return false;
}

float ASpinClock::GetObstacleCenterZ(const FObstacleState& S) const
{
	return S.bHigh ? HighObstacleBottom + HighObstacleHeight * 0.5f : ObstacleHeight * 0.5f;
}

void ASpinClock::UpdateObstaclePassing(ASpinGameMode& GM, float Moved)
{
	if (GM.State != ESpinState::Playing || bJammed || Moved <= 0.f)
	{
		return;
	}

	for (FObstacleState& S : ObstacleStates)
	{
		const bool bOut = S.Phase == EObstaclePhase::FadingIn || S.Phase == EObstaclePhase::Waiting;
		if (!bOut || S.bPassed)
		{
			continue;
		}

		S.RemainingDeg -= Moved;
		if (S.RemainingDeg <= -S.PassMarginDeg)
		{
			S.bPassed = true; // the hand is fully beyond it
			if (!S.bHigh)
			{
				GM.OnObstacleCleared(); // it got over the post, which counts towards a life back (passing under a high one doesn't)
			}
		}
	}
}

void ASpinClock::SpawnObstacle(float AngleDeg, float RadiusFraction, float AheadDeg, bool bHigh)
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
		S.Current = FVector2D(Rho * FMath::Cos(Rad), Rho * FMath::Sin(Rad));
		S.Elapsed = 0.f;
		S.Phase = EObstaclePhase::FadingIn;

		// Track how far the hand still has to go, so passing it can be detected.
		const float Reach = ObstacleRadius + HandWidth * 0.5f;
		S.RemainingDeg = AheadDeg;
		S.PassMarginDeg = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Reach / FMath::Max(Rho, 1.f), 0.f, 1.f))) + 3.f;
		S.bPassed = false;
		S.bHigh = bHigh;

		// High posts are flat squares, so they read differently from the round low posts at a glance.
		UStaticMesh* Shape = bHigh ? SquarePostMesh.Get() : RoundPostMesh.Get();
		UStaticMeshComponent* Post = ObstacleMeshes[i];
		if (Shape)
		{
			Post->SetStaticMesh(Shape);
			ObstacleShadows[i]->SetStaticMesh(Shape);
			ObstacleTopGlows[i]->SetStaticMesh(Shape);
			ObstacleBaseGlows[i]->SetStaticMesh(Shape);
		}
		Post->SetMaterial(0, bHigh ? HighObstacleMID.Get() : ObstacleMID.Get());
		Post->SetRelativeScale3D(FVector(ObstacleRadius * 2.f / 100.f, ObstacleRadius * 2.f / 100.f, (bHigh ? HighObstacleHeight : ObstacleHeight) / 100.f));
		PlaceObstacle(i, S.Current);
		return;
	}
}

void ASpinClock::RetireObstacles()
{
	for (FObstacleState& S : ObstacleStates)
	{
		if (S.Phase == EObstaclePhase::FadingIn || S.Phase == EObstaclePhase::Waiting)
		{
			// One still fading in leaves from how far it had got, rather than popping to full first.
			const float Presence = S.Phase == EObstaclePhase::FadingIn ? FMath::Clamp(S.Elapsed / ObstacleFadeInTime, 0.f, 1.f) : 1.f;
			S.Elapsed = ObstacleFadeOutTime * (1.f - Presence);
			S.Phase = EObstaclePhase::FadingOut;
		}
	}
}

void ASpinClock::GetObstacleGlitches(TArray<FSpinObstacleGlitch>& OutGlitches) const
{
	for (const FObstacleState& S : ObstacleStates)
	{
		if (S.Phase != EObstaclePhase::Inactive && S.Glitch > 0.f)
		{
			FSpinObstacleGlitch& Glitch = OutGlitches.AddDefaulted_GetRef();
			Glitch.WorldCenter = DialPivot->GetComponentTransform().TransformPosition(FVector(S.Current.X, S.Current.Y, GetObstacleCenterZ(S)));
			Glitch.WorldRadius = ObstacleRadius * 1.5f * DialScale;
			Glitch.Strength = S.Glitch;
			Glitch.Seed = S.GlitchSeed;
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

		// How much of it is there (0..1), and how hard it's glitching.
		float Presence = 1.f;
		S.Glitch = 0.f;
		if (S.Phase == EObstaclePhase::FadingIn)
		{
			Presence = FMath::Clamp(S.Elapsed / ObstacleFadeInTime, 0.f, 1.f);
			S.Glitch = 1.f - Presence * Presence;
			if (Presence >= 1.f)
			{
				S.Phase = EObstaclePhase::Waiting;
			}
		}
		else if (S.Phase == EObstaclePhase::FadingOut)
		{
			const float Alpha = FMath::Clamp(S.Elapsed / ObstacleFadeOutTime, 0.f, 1.f);
			Presence = 1.f - Alpha;
			S.Glitch = FMath::Sin(Alpha * PI);
			if (Alpha >= 1.f)
			{
				S.Phase = EObstaclePhase::Inactive;
				S.Glitch = 0.f;
				ShowObstacle(i, false);
				continue;
			}
		}

		// The dark hull barely shows, so the neon's brightness is what reads as the fade. Glitching
		// on top of that, it blinks, jumps about and flashes colours - seen, not felt: the hit test
		// uses its true position (see IsHandHittingObstacle).
		bool bShown = true;
		FVector2D Jitter = FVector2D::ZeroVector;
		FLinearColor Glow = (S.bHigh ? HighObstacleColor : ObstacleColor) * Presence;

		// Like real neon, the sign hums: a slow, gentle pulse, and now and then a quick buzz where it dips.
		{
			float Hum = 0.88f + 0.12f * FMath::Sin(S.Elapsed * 3.f + i * 1.7f);
			const FRandomStream Buzz(static_cast<int32>(HashCombine(static_cast<uint32>(FMath::FloorToInt(S.Elapsed * 16.f)), static_cast<uint32>(i + 101))));
			if (Buzz.FRand() < 0.04f)
			{
				Hum *= 0.3f;
			}
			Glow *= Hum;
		}
		if (S.Glitch > 0.f)
		{
			S.GlitchSeed = static_cast<int32>(HashCombine(static_cast<uint32>(FMath::FloorToInt(S.Elapsed * DialGlitchRate)), static_cast<uint32>(i + 1)));
			const FRandomStream Noise(S.GlitchSeed);
			bShown = Noise.FRand() < 0.35f + 0.65f * Presence;
			Jitter = FVector2D(Noise.FRandRange(-1.f, 1.f), Noise.FRandRange(-1.f, 1.f)) * ObstacleRadius * 0.2f * (1.f - Presence);
			if (Noise.FRand() < 0.35f)
			{
				Glow = GlitchPalette[Noise.RandHelper(UE_ARRAY_COUNT(GlitchPalette))];
			}
		}

		ShowObstacle(i, bShown);
		PlaceObstacle(i, S.Current + Jitter);
		if (ObstacleGlowMIDs.IsValidIndex(i) && ObstacleGlowMIDs[i])
		{
			ObstacleGlowMIDs[i]->SetVectorParameterValue(ColorParam, Glow);
		}
	}
}

void ASpinClock::UpdateSpindle(float HandZ)
{
	// From the top of the hub up into the middle of the hand, wherever the hand is now - a piston
	// that extends as it lifts. The neon band shows once there's enough of it to see.
	const float Radius = DialRadius * HubRadiusFraction * 0.45f;
	const float Top = HandZ + 3.f + HandHeight * 0.5f;
	const float Length = FMath::Max(Top - HubHeight, 2.f);
	HandSpindle->SetRelativeLocation(FVector(0.f, 0.f, HubHeight + Length * 0.5f));
	HandSpindle->SetRelativeScale3D(FVector(Radius * 2.f, Radius * 2.f, Length) / 100.f);

	HandSpindleBand->SetVisibility(Length > 30.f);
	HandSpindleBand->SetRelativeLocation(FVector(0.f, 0.f, HubHeight + Length * 0.45f));
	HandSpindleBand->SetRelativeScale3D(FVector(Radius * 2.f + 5.f, Radius * 2.f + 5.f, 5.f) / 100.f);
}

void ASpinClock::ShowObstacle(int32 Index, bool bShown)
{
	// Obstacles are neon arrows now (see PlaceObstacle). The old post and its top plate stay hidden;
	// a low one keeps its glowing footprint on the dial, a high one its shadow beneath it.
	const bool bHigh = ObstacleStates[Index].bHigh;
	ObstacleMeshes[Index]->SetVisibility(false);
	ObstacleTopGlows[Index]->SetVisibility(false);
	ObstacleBaseGlows[Index]->SetVisibility(bShown && !bHigh);
	ObstacleShadows[Index]->SetVisibility(bShown && bHigh);
	ObstacleSignBoards[Index]->SetVisibility(bShown);
	for (int32 t = 0; t < NumSignNeon; ++t)
	{
		ObstacleSignNeon[Index * NumSignNeon + t]->SetVisibility(bShown);
	}
}

void ASpinClock::PlaceObstacle(int32 Index, const FVector2D& Position)
{
	const FObstacleState& S = ObstacleStates[Index];
	const float Bottom = S.bHigh ? HighObstacleBottom : 3.f;
	const float Top = S.bHigh ? HighObstacleBottom + HighObstacleHeight : ObstacleHeight;

	ObstacleMeshes[Index]->SetRelativeLocation(FVector(Position.X, Position.Y, GetObstacleCenterZ(S)));
	ObstacleShadows[Index]->SetRelativeLocation(FVector(Position.X, Position.Y, 0.6f));
	ObstacleTopGlows[Index]->SetRelativeLocation(FVector(Position.X, Position.Y, Top));
	ObstacleBaseGlows[Index]->SetRelativeLocation(FVector(Position.X, Position.Y, Bottom));

	// Squares turn to face the hub, so their leading edge lies along the hand as it sweeps in -
	// which keeps what you see in line with the hit test (IsHandHittingObstacle).
	const FRotator Facing(0.f, FMath::RadiansToDegrees(FMath::Atan2(S.Current.Y, S.Current.X)), 0.f);
	ObstacleMeshes[Index]->SetRelativeRotation(Facing);
	ObstacleShadows[Index]->SetRelativeRotation(Facing);
	ObstacleTopGlows[Index]->SetRelativeRotation(Facing);
	ObstacleBaseGlows[Index]->SetRelativeRotation(Facing);

	// The sign, flat-on to the camera (which always looks along +X): a dark board, a neon frame round
	// its edge, and an arrow bent from neon tube - its outline traced as one closed shape. A low
	// obstacle's sign stands on the dial with an up arrow (lift over it); a high one's floats, hovering
	// gently, with a down arrow - its bottom edge never dipping into the gap the hand passes under.
	const float ArrowWidth = ObstacleRadius * 1.7f;
	const float HeadHeight = ArrowWidth * 0.5f;
	const float ShaftHalf = ArrowWidth * 0.17f;
	const float Height = FMath::Max(ObstacleArrowHeight, HeadHeight * 1.6f);
	const float T = FMath::Max(ObstacleArrowThickness, 1.f);
	const float M = FMath::Max(ObstacleSignMargin, T);

	// The board's bottom edge, then everything stacks up from it.
	const float Hover = S.bHigh ? ObstacleArrowBob * (0.5f + 0.5f * FMath::Sin(S.Elapsed * 2.5f)) : 0.f;
	const float BoardBottom = S.bHigh ? HighObstacleBottom + Hover : 3.f;
	const float ArrowBottom = BoardBottom + M;
	const float ArrowTop = ArrowBottom + Height;
	const float BoardTop = ArrowTop + M;
	const float HalfWidth = ArrowWidth * 0.5f + M;

	auto Point = [&Position](float Across, float Z) { return FVector(Position.X, Position.Y + Across, Z); };
	auto Tube = [T](UStaticMeshComponent* Comp, const FVector& A, const FVector& B, float Width)
	{
		const FVector D = B - A;
		const float Length = D.Size();
		Comp->SetRelativeLocation((A + B) * 0.5f);
		Comp->SetRelativeRotation(Length > KINDA_SMALL_NUMBER ? D.Rotation() : FRotator::ZeroRotator);
		Comp->SetRelativeScale3D(FVector(Length + Width, Width, Width) / 100.f);
	};

	// The board sits just behind the tubes.
	UStaticMeshComponent* Board = ObstacleSignBoards[Index];
	Board->SetRelativeLocation(FVector(Position.X + T, Position.Y, (BoardBottom + BoardTop) * 0.5f));
	Board->SetRelativeRotation(FRotator::ZeroRotator);
	Board->SetRelativeScale3D(FVector(T, HalfWidth * 2.f, BoardTop - BoardBottom) / 100.f);

	// The arrow's outline, as one closed loop of tube. Up arrow: shaft at the bottom, head on top;
	// a down arrow is the same shape flipped.
	const float Neck = S.bHigh ? ArrowBottom + HeadHeight : ArrowTop - HeadHeight; // where shaft meets head
	const float TipZ = S.bHigh ? ArrowBottom : ArrowTop;
	const float TailZ = S.bHigh ? ArrowTop : ArrowBottom;
	const FVector Outline[NumSignArrowTubes] = {
		Point(-ShaftHalf, TailZ),
		Point(-ShaftHalf, Neck),
		Point(-ArrowWidth * 0.5f, Neck),
		Point(0.f, TipZ),
		Point(ArrowWidth * 0.5f, Neck),
		Point(ShaftHalf, Neck),
		Point(ShaftHalf, TailZ),
	};
	const TObjectPtr<UStaticMeshComponent>* Tubes = &ObstacleSignNeon[Index * NumSignNeon];
	for (int32 t = 0; t < NumSignArrowTubes; ++t)
	{
		Tube(Tubes[t], Outline[t], Outline[(t + 1) % NumSignArrowTubes], T);
	}

	// The frame round the board's edge: thinner tube.
	const FVector Corners[4] = {
		Point(-HalfWidth, BoardBottom), Point(-HalfWidth, BoardTop), Point(HalfWidth, BoardTop), Point(HalfWidth, BoardBottom) };
	for (int32 c = 0; c < 4; ++c)
	{
		Tube(Tubes[NumSignArrowTubes + c], Corners[c], Corners[(c + 1) % 4], T * 0.6f);
	}
}

bool ASpinClock::IsHandHittingObstacle(float HandAngleDeg, bool* bOutHigh) const
{
	// A hand lifted above the low posts passes over them; a hand whose top stays under the high
	// posts passes beneath them (the hand sits 3 units up, see BuildVisuals).
	const bool bClearsLow = HandLift >= ObstacleHeight;
	const bool bClearsHigh = HandLift + 3.f + HandHeight <= HighObstacleBottom;

	const float Rad = FMath::DegreesToRadians(HandAngleDeg);
	const float CosA = FMath::Cos(Rad);
	const float SinA = FMath::Sin(Rad);
	const float HandLength = DialRadius * HandLengthFraction;
	const float Reach = ObstacleRadius + HandWidth * 0.5f;

	for (const FObstacleState& S : ObstacleStates)
	{
		// Obstacles that are leaving are harmless: the hand reverses right after a hit and would
		// otherwise run into the post it just passed through no fault of the player.
		if (S.Phase == EObstaclePhase::Inactive || S.Phase == EObstaclePhase::FadingOut)
		{
			continue;
		}
		if (S.bHigh ? bClearsHigh : bClearsLow)
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
			if (bOutHigh)
			{
				*bOutHigh = S.bHigh;
			}
			return true;
		}
	}

	return false;
}

void ASpinClock::SetCameraBase(float Blend)
{
	const float Pitch = FMath::Lerp(CameraPitch, RevealedCameraPitch, Blend);
	const float Dist = DialRadius * FMath::Lerp(CameraDistanceFactor, RevealedCameraDistanceFactor, Blend);
	const float PitchRad = FMath::DegreesToRadians(Pitch);
	CameraBaseLocation = FVector(-Dist * FMath::Cos(PitchRad), 0.f, Dist * FMath::Sin(PitchRad));
	CameraBaseRotation = FRotator(-(Pitch - RevealedCameraLookUp * Blend), 0.f, 0.f);
	Camera->SetFieldOfView(FMath::Lerp(CameraFOV, RevealedCameraFOV, Blend));

	// In play, UpdateCameraMotion then places the camera around this base.
	Camera->SetRelativeLocation(CameraBaseLocation);
	Camera->SetRelativeRotation(CameraBaseRotation);
}

void ASpinClock::PlayShake(float Intensity)
{
	ShakeAmount = FMath::Max(ShakeAmount, FMath::Clamp(Intensity, 0.f, 1.f));
}

void ASpinClock::UpdateCameraMotion(float DeltaSeconds)
{
	// The idle sway: two slow waves per axis at unrelated speeds, so the drift never visibly repeats.
	SwayTime += DeltaSeconds * CameraSwaySpeed;
	auto Sway = [this](float Phase) -> float
	{
		return 0.6f * FMath::Sin(SwayTime * 0.7f + Phase) + 0.4f * FMath::Sin(SwayTime * 1.13f + Phase * 2.3f);
	};
	FVector Offset(Sway(0.f), Sway(1.7f), Sway(3.1f));
	Offset *= CameraSwayOffset;
	FRotator Tilt(Sway(4.3f) * CameraSwayTilt, Sway(5.9f) * CameraSwayTilt, Sway(7.2f) * CameraSwayTilt * 0.5f);

	ShakeAmount = FMath::Max(0.f, ShakeAmount - ShakeDecayPerSecond * DeltaSeconds);
	if (ShakeAmount > 0.f)
	{
		// Squared falloff so the shake snaps hard and settles smoothly.
		const float Mag = ShakeAmount * ShakeAmount * MaxShakeOffset;
		Offset += FVector(0.f, FMath::FRandRange(-Mag, Mag), FMath::FRandRange(-Mag, Mag));
		Tilt += FRotator(
			FMath::FRandRange(-Mag, Mag) * 0.08f,
			FMath::FRandRange(-Mag, Mag) * 0.08f,
			FMath::FRandRange(-Mag, Mag) * 0.08f);
	}

	Camera->SetRelativeLocation(CameraBaseLocation + Offset);
	Camera->SetRelativeRotation(CameraBaseRotation + Tilt);
}

FVector ASpinClock::GetZoneWorldLocation(float RadiusFraction) const
{
	const ASpinGameMode* GM = GameMode.Get();
	return GetDialWorldLocation(GM ? GM->ZoneCenter : 0.f, RadiusFraction);
}

FVector ASpinClock::GetDialWorldLocation(float Angle, float RadiusFraction) const
{
	const float Rad = FMath::DegreesToRadians(Angle);
	const FVector Direction(FMath::Cos(Rad), FMath::Sin(Rad), 0.f);
	return GetActorLocation() + Direction * (DialRadius * RadiusFraction * DialScale) + FVector(0.f, 0.f, 12.f);
}
