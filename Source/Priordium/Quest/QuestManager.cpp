// Copyright Priordium. All Rights Reserved.

#include "QuestManager.h"

AQuestManager::AQuestManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AQuestManager::BeginPlay()
{
	Super::BeginPlay();
	
	for (int32 i = 0; i < 20; ++i)
	{
		CreateRandomQuest();
	}
}

UQuest* AQuestManager::CreateRandomQuest()
{
	if (PossibleResourceTypes.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AQuestManager::CreateRandomQuest: PossibleResourceTypes is empty – cannot generate a quest."));
		return nullptr;
	}

	// Shuffle a copy of the pool so we can pick the first N without repetition.
	TArray<FName> Pool = PossibleResourceTypes;
	const int32 PoolSize = Pool.Num();
	FRandomStream Stream(FMath::Rand());
	for (int32 i = PoolSize - 1; i > 0; --i)
	{
		const int32 j = Stream.RandRange(0, i);
		Pool.Swap(i, j);
	}

	const int32 ClampedMin = FMath::Max(1, MinRequirementCount);
	const int32 ClampedMax = FMath::Clamp(MaxRequirementCount, ClampedMin, PoolSize);
	const int32 Count      = Stream.RandRange(ClampedMin, ClampedMax);

	UQuest* Quest = NewObject<UQuest>(this);
	for (int32 i = 0; i < Count; ++i)
	{
		const int32 Amount = Stream.RandRange(FMath::Max(1, MinAmount), FMath::Max(1, MaxAmount));
		Quest->Requirements.Add({ Pool[i], Amount * 100 });
	}

	Quests.Add(Quest);
	return Quest;
}

void AQuestManager::AddQuest(UQuest* Quest)
{
	if (!Quest) return;
	Quests.AddUnique(Quest);
}

void AQuestManager::RemoveQuest(UQuest* Quest)
{
	if (!Quest) return;
	Quests.Remove(Quest);
}

void AQuestManager::RegisterCompletion(AActor* TribeActor, UQuest* Quest)
{
	if (!IsValid(TribeActor) || !IsValid(Quest)) return;

	int32& Count = CompletedCounts.FindOrAdd(TribeActor, 0);
	++Count;
	Quests.Remove(Quest);
	CreateRandomQuest();
}

int32 AQuestManager::GetCompletedCount(AActor* TribeActor) const
{
	if (!IsValid(TribeActor)) return 0;

	const int32* Count = CompletedCounts.Find(TribeActor);
	return Count ? *Count : 0;
}
