// Spin To Win - player profiles and the local leaderboard.

#include "SpinSaveGame.h"

const FString USpinSaveGame::SlotName(TEXT("SpinToWin"));

namespace
{
	/** Inserts Run into Board, keeping it sorted by IsBetter and at most MaxEntries long.
	 *  Returns the run's 1-based rank, or 0 if it didn't make the board. */
	template <typename PredicateType>
	int32 InsertRanked(TArray<FSpinRunRecord>& Board, const FSpinRunRecord& Run, int32 MaxEntries, PredicateType IsBetter)
	{
		// Equal runs go below existing ones: the older record keeps its place.
		int32 Index = 0;
		while (Index < Board.Num() && !IsBetter(Run, Board[Index]))
		{
			++Index;
		}

		if (Index >= MaxEntries)
		{
			return 0;
		}

		Board.Insert(Run, Index);
		if (Board.Num() > MaxEntries)
		{
			Board.SetNum(MaxEntries);
		}
		return Index + 1;
	}

	void RecordPersonalBest(FSpinProfile& Profile, const FSpinRunRecord& Run)
	{
		Profile.BestLevel = FMath::Max(Profile.BestLevel, Run.Level);
		Profile.BestPoints = FMath::Max(Profile.BestPoints, Run.Points);
		++Profile.RunsPlayed;
	}
}

void USpinSaveGame::AddRun(const FSpinRunRecord& Run, int32 MaxEntries, int32& OutLevelRank, int32& OutScoreRank)
{
	MaxEntries = FMath::Max(1, MaxEntries);

	OutLevelRank = InsertRanked(LevelBoard, Run, MaxEntries, [](const FSpinRunRecord& A, const FSpinRunRecord& B)
	{
		return A.Level != B.Level ? A.Level > B.Level : A.Points > B.Points;
	});

	OutScoreRank = InsertRanked(ScoreBoard, Run, MaxEntries, [](const FSpinRunRecord& A, const FSpinRunRecord& B)
	{
		return A.Points != B.Points ? A.Points > B.Points : A.Level > B.Level;
	});

	const int32 ProfileIndex = FindProfile(Run.PlayerName);
	if (Profiles.IsValidIndex(ProfileIndex))
	{
		RecordPersonalBest(Profiles[ProfileIndex], Run);
	}
}

const FSpinProfile* USpinSaveGame::GetActiveProfile() const
{
	return Profiles.IsValidIndex(ActiveProfile) ? &Profiles[ActiveProfile] : nullptr;
}

int32 USpinSaveGame::FindProfile(const FString& Name) const
{
	return Profiles.IndexOfByPredicate([&Name](const FSpinProfile& Profile)
	{
		return Profile.Name.Equals(Name, ESearchCase::IgnoreCase);
	});
}

int32 USpinSaveGame::AddProfile(const FString& Name)
{
	const FString Trimmed = Name.TrimStartAndEnd().Left(MaxNameLength);
	if (Trimmed.IsEmpty() || FindProfile(Trimmed) != INDEX_NONE || Profiles.Num() >= MaxProfiles)
	{
		return INDEX_NONE;
	}

	FSpinProfile Profile;
	Profile.Name = Trimmed;

	// Runs from before profiles existed have no name: they belong to whoever played first.
	if (Profiles.Num() == 0)
	{
		TSet<FDateTime> Adopted;
		for (TArray<FSpinRunRecord>* Board : { &LevelBoard, &ScoreBoard })
		{
			for (FSpinRunRecord& Run : *Board)
			{
				if (Run.PlayerName.IsEmpty())
				{
					Run.PlayerName = Trimmed;
					Profile.BestLevel = FMath::Max(Profile.BestLevel, Run.Level);
					Profile.BestPoints = FMath::Max(Profile.BestPoints, Run.Points);
					Adopted.Add(Run.Date); // the same run usually sits on both boards
				}
			}
		}
		Profile.RunsPlayed = Adopted.Num();
	}

	ActiveProfile = Profiles.Add(Profile);
	return ActiveProfile;
}

void USpinSaveGame::DeleteProfile(int32 Index)
{
	if (!Profiles.IsValidIndex(Index))
	{
		return;
	}

	const FString Name = Profiles[Index].Name;
	auto IsTheirs = [&Name](const FSpinRunRecord& Run) { return Run.PlayerName.Equals(Name, ESearchCase::IgnoreCase); };
	LevelBoard.RemoveAll(IsTheirs);
	ScoreBoard.RemoveAll(IsTheirs);

	Profiles.RemoveAt(Index);

	// Keep pointing at the same player if it was someone else; nobody if it was them.
	if (ActiveProfile == Index)
	{
		ActiveProfile = INDEX_NONE;
	}
	else if (ActiveProfile > Index)
	{
		--ActiveProfile;
	}
}
