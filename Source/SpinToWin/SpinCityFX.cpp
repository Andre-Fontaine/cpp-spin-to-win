// Spin To Win - ambient city effects.

#include "SpinCityFX.h"
#include "SpinGameMode.h"
#include "SpinClock.h"
#include "SpinCityBuilder.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FName IntensityParam(TEXT("Intensity"));

	// MPC_CityReveal's parameters, read by M_Neon. Neon is fully lit up to (RevealRadius - RevealEdge)
	// from RevealCenter, fades to RevealFloor by RevealRadius, and RevealFrontBoost brightens the fade band.
	const FName RevealCenterParam(TEXT("RevealCenter"));
	const FName RevealRadiusParam(TEXT("RevealRadius"));
	const FName RevealEdgeParam(TEXT("RevealEdge"));
	const FName RevealFloorParam(TEXT("RevealFloor"));
	const FName RevealFrontBoostParam(TEXT("RevealFrontBoost"));
}

ASpinCityFX::ASpinCityFX()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> RevealFinder(TEXT("/Game/Materials/Neon/MPC_CityReveal.MPC_CityReveal"));
	RevealCollection = RevealFinder.Succeeded() ? RevealFinder.Object : nullptr;
}

void ASpinCityFX::BeginPlay()
{
	Super::BeginPlay();

	GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ASpinGameMode>() : nullptr;
	CurrentGridIntensity = GridMinIntensity;

	SetupGridMIDs();
	SetupTraceChains();
	SetupFlickerNodes();
	MeasureCityExtent();
}

void ASpinCityFX::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateGrid(DeltaSeconds);
	UpdateTraces();
	UpdateFlickerNodes();
	UpdateReveal(DeltaSeconds);
}

// ---------------------------------------------------------------------------------------------
// Setup: find the level's tagged city dressing and wrap each one's material in a MID we can drive.
// ---------------------------------------------------------------------------------------------

void ASpinCityFX::SetupGridMIDs()
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), GridTag, Found);

	GridMIDs.Reset();
	for (AActor* A : Found)
	{
		UStaticMeshComponent* Comp = A ? A->FindComponentByClass<UStaticMeshComponent>() : nullptr;
		if (UMaterialInstanceDynamic* MID = Comp ? Comp->CreateAndSetMaterialInstanceDynamic(0) : nullptr)
		{
			GridMIDs.Add(MID);
		}
	}
}

void ASpinCityFX::SetupTraceChains()
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), TraceTag, Found);

	// Each trace segment carries a second tag "T<chain>_<index>" giving its chain and position
	// within that chain. Group by chain, then sort each chain by index.
	TMap<int32, TArray<TPair<int32, TObjectPtr<UMaterialInstanceDynamic>>>> ByChain;

	for (AActor* A : Found)
	{
		if (!A)
		{
			continue;
		}

		UStaticMeshComponent* Comp = A->FindComponentByClass<UStaticMeshComponent>();
		UMaterialInstanceDynamic* MID = Comp ? Comp->CreateAndSetMaterialInstanceDynamic(0) : nullptr;
		if (!MID)
		{
			continue;
		}

		FString OrderTag;
		for (const FName& Tag : A->Tags)
		{
			const FString TagStr = Tag.ToString();
			if (TagStr.Len() > 1 && TagStr[0] == TEXT('T') && TagStr.Contains(TEXT("_")))
			{
				OrderTag = TagStr;
				break;
			}
		}

		FString ChainStr, IndexStr;
		if (OrderTag.IsEmpty() || !OrderTag.Mid(1).Split(TEXT("_"), &ChainStr, &IndexStr))
		{
			continue;
		}

		ByChain.FindOrAdd(FCString::Atoi(*ChainStr)).Add(TPair<int32, TObjectPtr<UMaterialInstanceDynamic>>(FCString::Atoi(*IndexStr), MID));
	}

	TraceChains.Reset();
	for (auto& Pair : ByChain)
	{
		Pair.Value.Sort([](const TPair<int32, TObjectPtr<UMaterialInstanceDynamic>>& A, const TPair<int32, TObjectPtr<UMaterialInstanceDynamic>>& B)
		{
			return A.Key < B.Key;
		});

		FSpinTraceChain Chain;
		Chain.PhaseOffset = FMath::FRand();
		for (const auto& Entry : Pair.Value)
		{
			Chain.Segments.Add(Entry.Value);
		}
		TraceChains.Add(Chain);
	}
}

void ASpinCityFX::SetupFlickerNodes()
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), NodeTag, Found);

	FlickerNodes.Reset();
	for (AActor* A : Found)
	{
		UStaticMeshComponent* Comp = A ? A->FindComponentByClass<UStaticMeshComponent>() : nullptr;
		UMaterialInstanceDynamic* MID = Comp ? Comp->CreateAndSetMaterialInstanceDynamic(0) : nullptr;
		if (!MID)
		{
			continue;
		}
		MID->SetScalarParameterValue(IntensityParam, NodeDimIntensity);

		FSpinFlickerNode Node;
		Node.MID = MID;
		Node.NextFlashTime = FMath::FRandRange(0.f, NodeFlickerMaxInterval);
		FlickerNodes.Add(Node);
	}
}

// ---------------------------------------------------------------------------------------------
// Per-frame updates.
// ---------------------------------------------------------------------------------------------

float ASpinCityFX::GetHitPulse() const
{
	const ASpinGameMode* GM = GameMode.Get();
	return GM ? GM->GetHitPulse() : 0.f;
}

void ASpinCityFX::UpdateGrid(float DeltaSeconds)
{
	if (GridMIDs.Num() == 0)
	{
		return;
	}

	float SpeedFactor = 0.f;
	if (const ASpinGameMode* GM = GameMode.Get())
	{
		const float Range = FMath::Max(1.f, GM->MaxSpeed - GM->StartSpeed);
		SpeedFactor = FMath::Clamp((GM->CurrentSpeed - GM->StartSpeed) / Range, 0.f, 1.f);
	}

	const float Target = FMath::Lerp(GridMinIntensity, GridMaxIntensity, SpeedFactor);
	CurrentGridIntensity = FMath::FInterpTo(CurrentGridIntensity, Target, DeltaSeconds, GridResponseSpeed);

	// The hit pulse rides on top of the smoothed value, so it snaps in instantly instead of easing.
	const float Intensity = CurrentGridIntensity + GridHitPulseBoost * GetHitPulse();

	for (UMaterialInstanceDynamic* MID : GridMIDs)
	{
		if (MID)
		{
			MID->SetScalarParameterValue(IntensityParam, Intensity);
		}
	}
}

void ASpinCityFX::UpdateTraces()
{
	if (TraceChains.Num() == 0 || !GetWorld())
	{
		return;
	}

	const float T = GetWorld()->GetTimeSeconds() * ChaseSpeed;
	const float PulseBoost = TraceHitPulseBoost * GetHitPulse();

	for (FSpinTraceChain& Chain : TraceChains)
	{
		const int32 Count = Chain.Segments.Num();
		if (Count == 0)
		{
			continue;
		}

		const float Head = FMath::Fmod(T + Chain.PhaseOffset * Count, static_cast<float>(Count));

		for (int32 i = 0; i < Count; ++i)
		{
			float Dist = FMath::Abs(static_cast<float>(i) - Head);
			Dist = FMath::Min(Dist, Count - Dist); // wrap-around distance, so the chase loops smoothly
			const float Falloff = FMath::Clamp(1.f - Dist / ChaseWidth, 0.f, 1.f);
			const float Intensity = FMath::Lerp(ChaseDimIntensity, ChaseBrightIntensity, Falloff) + PulseBoost;

			if (UMaterialInstanceDynamic* MID = Chain.Segments[i])
			{
				MID->SetScalarParameterValue(IntensityParam, Intensity);
			}
		}
	}
}

void ASpinCityFX::UpdateFlickerNodes()
{
	if (FlickerNodes.Num() == 0 || !GetWorld())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const float PulseBoost = NodeHitPulseBoost * GetHitPulse();

	for (FSpinFlickerNode& Node : FlickerNodes)
	{
		if (!Node.MID)
		{
			continue;
		}

		if (Node.FlashEndTime > 0.f && Now >= Node.FlashEndTime)
		{
			// Flash finished: drop back to idle and schedule the next one.
			Node.FlashEndTime = -1.f;
			Node.NextFlashTime = Now + FMath::FRandRange(NodeFlickerMinInterval, NodeFlickerMaxInterval);
		}
		else if (Node.FlashEndTime <= 0.f && Now >= Node.NextFlashTime)
		{
			// Time to flash.
			Node.FlashEndTime = Now + NodeFlashDuration;
		}

		// Set every frame (rather than only on flash changes) so the hit pulse can ride on top.
		const float Base = (Node.FlashEndTime > 0.f) ? NodeBrightIntensity : NodeDimIntensity;
		Node.MID->SetScalarParameterValue(IntensityParam, Base + PulseBoost);
	}
}

// ---------------------------------------------------------------------------------------------
// City reveal: the lit area grows out from the dial as the run progresses.
// ---------------------------------------------------------------------------------------------

void ASpinCityFX::MeasureCityExtent()
{
	// The farthest corner of any placed mesh, measured flat (XY) from the dial's centre at the origin.
	// Anything absurdly large (a leftover sky sphere, say) is skipped so it can't stretch the reveal.
	float Farthest = 0.f;
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
	{
		FVector Origin, Extent;
		It->GetActorBounds(false, Origin, Extent);
		if (Extent.GetMax() > 100000.f)
		{
			continue;
		}
		const float CornerDist = FVector2D(FMath::Abs(Origin.X) + Extent.X, FMath::Abs(Origin.Y) + Extent.Y).Size();
		Farthest = FMath::Max(Farthest, CornerDist);
	}
	// The procedural city, if there is one.
	for (TActorIterator<ASpinCityBuilder> It(GetWorld()); It; ++It)
	{
		Farthest = FMath::Max(Farthest, It->GetCityRadius());
	}

	if (Farthest > 0.f)
	{
		CityExtent = Farthest;
	}
}

float ASpinCityFX::GetRevealFraction() const
{
	if (!bRevealEnabled || !RevealCollection)
	{
		return 1.f;
	}
	const float Span = FMath::Max(1.f, CityExtent - RevealStartRadius);
	return FMath::Clamp((CurrentRevealRadius - RevealStartRadius) / Span, 0.f, 1.f);
}

float ASpinCityFX::GetRevealOuterRadius() const
{
	if (!bRevealEnabled || !RevealCollection)
	{
		return CityExtent;
	}
	return FMath::Max(CurrentRevealRadius, 0.f) + CurrentRevealEdge;
}

float ASpinCityFX::GetRevealProgress() const
{
	const ASpinGameMode* GM = GameMode.Get();
	if (!GM)
	{
		return -1.f;
	}

	switch (GM->State)
	{
	case ESpinState::Playing:
	case ESpinState::GameOver: // stays where the run got to, so you can see how far you made it
	case ESpinState::NewButtonIntro:
	case ESpinState::ObstacleIntro:
	case ESpinState::JamIntro:
	case ESpinState::HighObstacleIntro:
	case ESpinState::TutorialIntro:
	case ESpinState::EndlessIntro:
		break;
	default:
		return -1.f; // a menu
	}

	if (GM->IsTutorial())
	{
		// The tutorial lights up a small part of the city, a step per tutorial level and a little per hit.
		const int32 NumLevels = FMath::Max(1, GM->TutorialLevels.Num());
		const FTutorialLevel* Level = GM->GetTutorialLevel();
		const float Within = Level ? FMath::Clamp(static_cast<float>(GM->TutorialHits) / FMath::Max(1, Level->Hits), 0.f, 1.f) : 0.f;
		return TutorialRevealShare * (GM->TutorialIndex + Within) / NumLevels;
	}

	// Endless: a step per level, plus the hits made into the current one, so every hit pushes it out a bit.
	const int32 Level = GM->GetLevel();
	const int32 LevelHits = FMath::Max(1, GM->GetHitsForLevel(Level));
	const int32 LevelStart = GM->GetLevelTarget() - LevelHits;
	const float Within = FMath::Clamp(static_cast<float>(GM->Hits - LevelStart) / LevelHits, 0.f, 1.f);
	return FMath::Clamp((Level - 1 + Within) / (RevealFullLevel - 1.f), 0.f, 1.f);
}

void ASpinCityFX::UpdateReveal(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!RevealCollection || !World)
	{
		return;
	}

	const float Progress = bRevealEnabled ? GetRevealProgress() : 1.f;
	const bool bOnMenu = Progress < 0.f;

	float TargetRadius;
	float Edge;
	if (bOnMenu)
	{
		TargetRadius = bFullCityOnMenus ? CityExtent : RevealStartRadius;
		Edge = RevealEdgeFull;
	}
	else
	{
		TargetRadius = FMath::Lerp(RevealStartRadius, CityExtent, Progress);
		Edge = FMath::Lerp(RevealEdgeStart, RevealEdgeFull, Progress);
	}

	if (CurrentRevealRadius < 0.f)
	{
		CurrentRevealRadius = TargetRadius; // first frame: no sweep from nothing
	}

	// Spreading out during a run is slow so it visibly creeps; powering down for a new run, or
	// lighting back up behind the menus, is quick.
	const bool bGrowing = TargetRadius > CurrentRevealRadius;
	const float Speed = (bGrowing && !bOnMenu) ? RevealSpreadSpeed : RevealCollapseSpeed;
	CurrentRevealRadius = FMath::FInterpConstantTo(CurrentRevealRadius, TargetRadius, DeltaSeconds, Speed);
	CurrentRevealEdge = Edge;

	// The leading edge glows extra while it's still travelling outward, then settles.
	const float Travel = bGrowing ? FMath::Clamp((TargetRadius - CurrentRevealRadius) / FMath::Max(1.f, Edge * 0.5f), 0.f, 1.f) : 0.f;

	const ASpinGameMode* GM = GameMode.Get();
	const ASpinClock* Clock = GM ? GM->GetClock() : nullptr;
	const FVector Center = Clock ? Clock->GetActorLocation() : FVector::ZeroVector;

	UKismetMaterialLibrary::SetVectorParameterValue(World, RevealCollection, RevealCenterParam, FLinearColor(Center.X, Center.Y, 0.f, 0.f));
	UKismetMaterialLibrary::SetScalarParameterValue(World, RevealCollection, RevealRadiusParam, CurrentRevealRadius + Edge);
	UKismetMaterialLibrary::SetScalarParameterValue(World, RevealCollection, RevealEdgeParam, Edge);
	UKismetMaterialLibrary::SetScalarParameterValue(World, RevealCollection, RevealFloorParam, RevealFloor);
	UKismetMaterialLibrary::SetScalarParameterValue(World, RevealCollection, RevealFrontBoostParam, RevealFrontBoost * Travel);
}
