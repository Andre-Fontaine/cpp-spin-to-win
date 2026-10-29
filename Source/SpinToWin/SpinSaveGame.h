// Spin To Win - player profiles and the local leaderboard, saved between sessions.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SpinSaveGame.generated.h"

/** One finished Endless run. */
USTRUCT(BlueprintType)
struct FSpinRunRecord
{
	GENERATED_BODY()

	/** Name of the profile that played it. Empty for runs saved before profiles existed. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Spin")
	FString PlayerName;

	/** Level the run reached. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Spin")
	int32 Level = 1;

	/** Points scored (precision x combo). */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Spin")
	int32 Points = 0;

	/** Successful hits. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Spin")
	int32 Hits = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Spin")
	FDateTime Date;
};

/** One player. Their personal bests live here rather than being read off the shared boards, so
 *  they survive being pushed off the top-N by someone else's runs. */
USTRUCT(BlueprintType)
struct FSpinProfile
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Spin")
	FString Name;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Spin")
	int32 BestLevel = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Spin")
	int32 BestPoints = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Spin")
	int32 RunsPlayed = 0;
};

/**
 * Player profiles, plus two shared top-N boards over everyone's runs: one ranks by how far you got
 * (level, ties broken by points), the other purely by points - so a run that played the multiplier
 * well can top the score board without having gone the furthest.
 */
UCLASS()
class SPINTOWIN_API USpinSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static const FString SlotName;

	/** Longest allowed profile name. */
	static constexpr int32 MaxNameLength = 12;

	/** Most profiles at once - as many as the "who's playing?" screen fits. */
	static constexpr int32 MaxProfiles = 6;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Spin")
	TArray<FSpinProfile> Profiles;

	/** Index into Profiles of whoever played last, so they're picked again next session. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Spin")
	int32 ActiveProfile = INDEX_NONE;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Spin")
	TArray<FSpinRunRecord> LevelBoard;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Spin")
	TArray<FSpinRunRecord> ScoreBoard;

	/** Adds a run to both boards (each kept sorted and trimmed to MaxEntries) and to its player's
	 *  personal bests. Returns its 1-based rank on each board through the out params, or 0 if it
	 *  didn't make that board. */
	void AddRun(const FSpinRunRecord& Run, int32 MaxEntries, int32& OutLevelRank, int32& OutScoreRank);

	/** The active profile, or null if nobody has been picked/created yet. */
	const FSpinProfile* GetActiveProfile() const;

	/** Index of the profile with this name (case-insensitive), or INDEX_NONE. */
	int32 FindProfile(const FString& Name) const;

	/** Creates a profile and makes it active. Returns its index, or INDEX_NONE if the name is
	 *  empty or taken, or there are already MaxProfiles. The very first profile also adopts any runs saved before profiles existed. */
	int32 AddProfile(const FString& Name);

	/** Removes a profile and all of its runs from the boards. */
	void DeleteProfile(int32 Index);
};
