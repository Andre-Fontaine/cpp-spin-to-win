// Spin To Win - ambient city effects.

#include "SpinCityFX.h"
#include "SpinGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	const FName IntensityParam(TEXT("Intensity"));
}

ASpinCityFX::ASpinCityFX()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ASpinCityFX::BeginPlay()
{
	Super::BeginPlay();

	GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ASpinGameMode>() : nullptr;
	CurrentGridIntensity = GridMinIntensity;

	SetupGridMIDs();
	SetupTraceChains();
	SetupFlickerNodes();
}

void ASpinCityFX::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateGrid(DeltaSeconds);
	UpdateTraces();
	UpdateFlickerNodes();
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

	for (UMaterialInstanceDynamic* MID : GridMIDs)
	{
		if (MID)
		{
			MID->SetScalarParameterValue(IntensityParam, CurrentGridIntensity);
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
			const float Intensity = FMath::Lerp(ChaseDimIntensity, ChaseBrightIntensity, Falloff);

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

	for (FSpinFlickerNode& Node : FlickerNodes)
	{
		if (!Node.MID)
		{
			continue;
		}

		if (Node.FlashEndTime > 0.f && Now >= Node.FlashEndTime)
		{
			// Flash finished: drop back to idle and schedule the next one.
			Node.MID->SetScalarParameterValue(IntensityParam, NodeDimIntensity);
			Node.FlashEndTime = -1.f;
			Node.NextFlashTime = Now + FMath::FRandRange(NodeFlickerMinInterval, NodeFlickerMaxInterval);
		}
		else if (Node.FlashEndTime <= 0.f && Now >= Node.NextFlashTime)
		{
			// Time to flash.
			Node.MID->SetScalarParameterValue(IntensityParam, NodeBrightIntensity);
			Node.FlashEndTime = Now + NodeFlashDuration;
		}
	}
}
