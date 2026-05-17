
#include "ResourceGatheringTask.h"
#include "GameFramework/Character.h"

ResourceGatheringTask::ResourceGatheringTask(ACharacter* InTribeMan, AActor* InResource)
    : TribeTask(InTribeMan)
    , Resource(InResource)
{
}

bool ResourceGatheringTask::Execute()
{
    if (!IsValid(Resource.Get()))
    {
        return false;
    }

    if (!IsValid(TribeMan.Get()))
    {
        return false;
    }

    UFunction* CollectResourceFunc = TribeMan->FindFunction(FName("CollectResource"));
    if (!CollectResourceFunc)
    {
        UE_LOG(LogTemp, Warning, TEXT("ResourceGatheringTask: TribeMan does not have CollectResource function."));
        return false;
    }

    struct FCollectResourceParams
    {
        AActor* Resource = nullptr;
    };

    FCollectResourceParams Params;
    Params.Resource = Resource.Get();
    TribeMan->ProcessEvent(CollectResourceFunc, &Params);

    return true;
}
