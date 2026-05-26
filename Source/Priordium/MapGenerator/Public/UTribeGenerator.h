// Copyright Priordium. All Rights Reserved.
//
// UTribeGenerator.h
// ActorComponent that generates tribes on the map after the landscape is built.
// Each tribe consists of a Storage, a configurable number of TribeMan actors,
// and an ATribeManager that ties them together.
// SOLID: Single Responsibility -- exclusively tribe placement logic.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UTribeGenerator.generated.h"

class UHeightmapGenerator;
class UMapGeneratorSettings;
class ULandscapeBuilder;
class ATribeManager;
class AQuestManager;

UCLASS(ClassGroup = (MapGenerator), meta = (BlueprintSpawnableComponent))
class PRIORDIUM_API UTribeGenerator : public UActorComponent
{
	GENERATED_BODY()

public:

	UTribeGenerator();

	/**
	 * The BP_ItemPrices Blueprint class.
	 * Assign BP_ItemPrices (Content/Tribes) here in the editor.
	 * Every TribeManager spawned by GenerateTribes() will have its
	 * ItemPrices property set to this class so BeginPlay can create
	 * the instance and call calculatePrices().
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generator|Tribes")
	TSubclassOf<UObject> ItemPricesClass;

	/**
	 * Generates TribeCount tribes on the map, placing them as far apart as possible
	 * using farthest-point sampling on land cells (height > SeaLevel).
	 * Requires that the landscape is already built (uses line traces for terrain height).
	 *
	 * @param Settings   Generation settings (tribe count, Blueprint classes, spawn radii)
	 * @param Heightmap  Generated heightmap used to identify land cells
	 * @return           true if at least one tribe was spawned
	 */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Tribes")
	bool GenerateTribes(const UMapGeneratorSettings* Settings, const UHeightmapGenerator* Heightmap, ULandscapeBuilder* LandscapeBuilder = nullptr);

	/** Destroys all tribe actors spawned by GenerateTribes(). */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Tribes")
	void ClearTribes();

	/** Returns all spawned TribeManager actors (read-only). */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Tribes")
	const TArray<ATribeManager*>& GetTribeManagers() const { return SpawnedTribeManagers; }

	/**
	 * Line-traces downward from high above (X, Y) to find the terrain surface Z.
	 * Falls back to direct heightmap math if the trace returns no hit.
	 * @return false only if both the trace and the heightmap fallback fail.
	 */
	static bool GetTerrainHeight(
		UWorld*                      World,
		float                        X,
		float                        Y,
		float&                       OutZ,
		const UHeightmapGenerator*   Heightmap  = nullptr,
		const UMapGeneratorSettings* Settings   = nullptr);

	/**
	 * Spawns a single building actor, snaps it to the terrain, optionally flattens
	 * the landscape under its footprint, and registers it with the TribeManager.
	 * Shared by SpawnStorage (generation time) and ATribeManager::Build (runtime).
	 *
	 * @param World            World to spawn in.
	 * @param TribeManager     Manager that will own the building (added to storages).
	 * @param BuildingClass    Actor class to spawn.
	 * @param Location         Approximate world position (Z will be corrected to terrain).
	 * @param Heightmap        Optional – used for terrain-height sampling.
	 * @param Settings         Optional – required when Heightmap is provided.
	 * @param LandscapeBuilder Optional – flattens the terrain under the footprint.
	 * @param TribeActor       Optional – sets the "Tribe" property on the spawned actor.
	 * @param FolderPath       Optional – Outliner folder for the spawned actor.
	 * @return The spawned actor, or nullptr on failure.
	 */
	static AActor* SpawnBuilding(
		UWorld*                      World,
		ATribeManager*               TribeManager,
		TSubclassOf<AActor>          BuildingClass,
		const FVector&               Location,
		const UHeightmapGenerator*   Heightmap        = nullptr,
		const UMapGeneratorSettings* Settings         = nullptr,
		ULandscapeBuilder*           LandscapeBuilder = nullptr,
		AActor*                      TribeActor       = nullptr,
		const FName&                 FolderPath       = NAME_None);

	/**
	 * Spawns Count TribeMen evenly distributed in a circle of SpawnRadius around
	 * CenterLocation and registers each one with TribeManager.
	 *
	 * Shared by GenerateTribes() (generation time, N men in a circle) and
	 * ATribeManager::CreateTribeMan() (runtime, Count=1 / Radius=0).
	 *
	 * @param World           World to spawn in.
	 * @param TribeManager    Manager that will own the spawned characters.
	 * @param CenterLocation  Centre of the spawn circle (Z corrected via Heightmap when provided).
	 * @param TribeManClass   Character class to spawn.
	 * @param Count           Number of TribeMen to spawn.
	 * @param SpawnRadius     Radius of the spawn circle in cm (0 = spawn at centre).
	 * @param TribeActor      Optional – sets the "Tribe" property on each spawned actor.
	 * @param FolderPath      Optional – Outliner folder for spawned actors.
	 * @param Heightmap       Optional – used to snap Z to the terrain surface.
	 * @param Settings        Optional – required when Heightmap is provided.
	 * @return Array of successfully spawned ACharacter pointers (may be shorter than Count on partial failure).
	 */
	static TArray<ACharacter*> SpawnTribeMen(
		UWorld*                      World,
		ATribeManager*               TribeManager,
		const FVector&               CenterLocation,
		TSubclassOf<AActor>          TribeManClass,
		int32                        Count,
		float                        SpawnRadius,
		AActor*                      TribeActor      = nullptr,
		const FName&                 FolderPath      = NAME_None,
		const UHeightmapGenerator*   Heightmap       = nullptr,
		const UMapGeneratorSettings* Settings        = nullptr);

private:

	TArray<ATribeManager*> SpawnedTribeManagers;
	TArray<AActor*>        SpawnedActors; // Storage + TribeMan actors for cleanup

	/**
	 * Selects Count positions from Candidates using farthest-point sampling (greedy maximin).
	 * Each new point maximises the minimum distance to all already-selected points.
	 * Uses squared distances to avoid unnecessary sqrt calls.
	 * Time complexity: O(Candidates.Num() * Count).
	 */
	static TArray<FVector2D> SelectTribePositions(
		const TArray<FVector2D>& Candidates,
		int32                    Count,
		FRandomStream&           Stream);

	/**
	 * Builds possible land spawn positions for tribes from the heightmap.
	 * Applies map-edge margins and subsampling for performance.
	 */
	static TArray<FVector2D> BuildLandCandidates(
		const UMapGeneratorSettings* Settings,
		const UHeightmapGenerator*   Heightmap);

	static void SetTribeColor(AActor* TribeActor, int32 TribeIndex, int32 TribeCount);
	static void SetTribeOwner(AActor* Actor, AActor* TribeActor);
	
	/**
	 * Spawns one complete tribe (TribeManager + Storage + TribeMen) centred at SpawnLocation.
	 * @param Heightmap  Passed down to SpawnStorage for footprint height sampling.
	 * @return The spawned ATribeManager, or nullptr on failure.
	 */
	ATribeManager* SpawnTribe(
		UWorld*                      World,
		const FVector&               SpawnLocation,
		const UMapGeneratorSettings* Settings,
		const UHeightmapGenerator*   Heightmap,
		ULandscapeBuilder*           LandscapeBuilder);

	/**
	 * Determines the appropriate TribeManager class (Blueprint or base C++) based on settings
	 * and spawns an instance at the given location.
	 * @param World         The world to spawn in
	 * @param Location      World location to spawn at
	 * @param Settings      Generation settings with TribeManagerClass configuration
	 * @param SpawnParams   Actor spawn parameters
	 * @param TribeFolderPath  Outliner folder path for organization
	 * @return The spawned ATribeManager, or nullptr on failure.
	 */
	ATribeManager* SpawnTribeManager(
		UWorld*                      World,
		const FVector&               Location,
		const UMapGeneratorSettings* Settings,
		const FActorSpawnParameters& SpawnParams,
		const FName&                 TribeFolderPath);

	/**
	 * Spawns the tribal reference actor (BP_Tribe) if configured, and applies visual customization.
	 * @param World          The world to spawn in
	 * @param SpawnLocation  Location to spawn at
	 * @param Settings       Generation settings with TribeClass configuration
	 * @param SpawnParams    Actor spawn parameters
	 * @param TribeIndex     Index of this tribe (for color distribution)
	 * @param TribeFolderPath Outliner folder path for organization
	 * @return The spawned TribeActor, or nullptr if TribeClass is not set
	 */
	AActor* SpawnTribeActor(
		UWorld*                      World,
		const FVector&               SpawnLocation,
		const UMapGeneratorSettings* Settings,
		const FActorSpawnParameters& SpawnParams,
		int32                        TribeIndex,
		const FName&                 TribeFolderPath);

	/**
	 * Spawns a Storage actor and registers it with the given TribeManager.
	 * Derives the footprint half-extent from the actor's own bounding box,
	 * samples the terrain at a 3x3 grid across that footprint, flattens the
	 * landscape to the lowest point, then repositions the actor to sit flush.
	 * @param World           The world to spawn in
	 * @param TribeManager    The TribeManager to register the Storage with
	 * @param CenterLocation  Center of the footprint (X, Y); Z is the approximate reference height
	 * @param Settings        Generation settings (StorageClass)
	 * @param SpawnParams     Actor spawn parameters
	 * @param Heightmap       Optional heightmap for terrain-height fallback sampling
	 * @param TribeActor      The owning tribe actor (BP_Tribe) for Tribe property references
	 * @param TribeFolderPath Outliner folder path for organization
	 */
	void SpawnStorage(
		UWorld*                      World,
		ATribeManager*               TribeManager,
		const FVector&               CenterLocation,
		const UMapGeneratorSettings* Settings,
		const FActorSpawnParameters& SpawnParams,
		const UHeightmapGenerator*   Heightmap,
		AActor*                      TribeActor,
		const FName&                 TribeFolderPath,
		ULandscapeBuilder*           LandscapeBuilder);

};
