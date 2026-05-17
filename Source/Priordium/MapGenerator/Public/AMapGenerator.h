// Copyright Priordium. All Rights Reserved.
//
// AMapGenerator.h
// Top-level Actor that orchestrates the entire map generation pipeline.
// SOLID: Single Responsibility -- exclusively pipeline orchestration.
//        Open/Closed -- new pipeline steps can be added without modifying existing ones.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AMapGenerator.generated.h"

class UHeightmapGenerator;
class UBiomeManager;
class ULandscapeBuilder;
class UWaterSystemBuilder;
class UClimateZoneManager;
class UResourceDistributor;
class UTribeGenerator;
class UMapGeneratorSettings;

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

/** Broadcast when GenerateWorld() completes successfully. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGenerationCompleted);

/** Broadcast when GenerateWorld() fails (invalid state, null settings, step error). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGenerationFailed, FString, ErrorMessage);

/** Broadcast after each pipeline step completes. StepIndex is 0-based. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStepCompleted, int32, StepIndex);

/**
 * AMapGenerator
 * Owns all map generator components and runs the 6-step generation pipeline.
 *
 * Pipeline steps (ExecuteGenerationStep):
 *   0 - HeightmapGenerator::Generate()
 *   1 - BiomeManager::AssignBiomes()
 *   2 - ClimateZoneManager::CalculateClimateZones()
 *   3 - WaterSystemBuilder::BuildWaterBodies()
 *   4 - LandscapeBuilder::BuildLandscape()
 *   5 - ResourceDistributor::DistributeResources()
 */
UCLASS(BlueprintType, Blueprintable)
class PRIORDIUM_API AMapGenerator : public AActor
{
	GENERATED_BODY()

public:

	AMapGenerator();

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Generator|Components")
	UHeightmapGenerator* HeightmapGenerator;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Generator|Components")
	UBiomeManager* BiomeManager;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Generator|Components")
	ULandscapeBuilder* LandscapeBuilder;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Generator|Components")
	UWaterSystemBuilder* WaterSystemBuilder;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Generator|Components")
	UClimateZoneManager* ClimateZoneManager;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Generator|Components")
	UResourceDistributor* ResourceDistributor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Generator|Components")
	UTribeGenerator* TribeGenerator;

	/** Generation settings data asset. Assign in the editor Details Panel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generator")
	UMapGeneratorSettings* GeneratorSettings;

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/** Returns normalized generation progress [0.0, 1.0]. */
	UFUNCTION(BlueprintCallable, Category = "Map Generator")
	float GetGenerationProgress() const { return CurrentProgress; }

	/** Returns true while GenerateWorld() is running. */
	UFUNCTION(BlueprintCallable, Category = "Map Generator")
	bool IsGenerating() const { return bIsGenerating; }

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/**
	 * Executes a single generation pipeline step.
	 * Steps: 0=Heightmap, 1=Biomes, 2=Climate, 3=Water, 4=Landscape, 5=Resources, 6=Tribes.
	 * Returns false for out-of-range indices.
	 */
	UFUNCTION(BlueprintCallable, Category = "Map Generator")
	bool ExecuteGenerationStep(int32 StepIndex);

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/**
	 * Runs all 6 pipeline steps in sequence.
	 * Broadcasts OnGenerationCompleted on success.
	 * Broadcasts OnGenerationFailed if already generating, null settings, or step failure.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Map Generator")
	void GenerateWorld();

	/** Broadcast when GenerateWorld() finishes successfully. */
	UPROPERTY(BlueprintAssignable, Category = "Map Generator|Events")
	FOnGenerationCompleted OnGenerationCompleted;

	/** Broadcast when GenerateWorld() encounters an error. */
	UPROPERTY(BlueprintAssignable, Category = "Map Generator|Events")
	FOnGenerationFailed OnGenerationFailed;

	/** Broadcast after each step. */
	UPROPERTY(BlueprintAssignable, Category = "Map Generator|Events")
	FOnStepCompleted OnStepCompleted;

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/**
	 * Destroys all generated content (landscape, resources) and resets state.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Map Generator")
	void ClearGeneratedWorld();

	/**
	 * Regenerates only the tribes (clears existing ones first).
	 * Requires that GenerateWorld() has already been run so the heightmap
	 * and landscape are available.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Map Generator")
	void GenerateTribesOnly();

	/** If true, GenerateWorld() is called automatically on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generator")
	bool bAutoGenerateOnBeginPlay = false;

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/**
	 * Validates GeneratorSettings for completeness and valid value ranges.
	 * Returns false if any required field is missing or out of range.
	 */
	UFUNCTION(BlueprintCallable, Category = "Map Generator")
	bool ValidateSettings() const;

protected:

	virtual void BeginPlay() override;

private:

	static constexpr int32 TotalSteps = 7;

	bool  bIsGenerating   = false;
	float CurrentProgress = 0.0f;

	bool ExecuteStepHeightmap();
	bool ExecuteStepBiomes();
	bool ExecuteStepClimate();
	bool ExecuteStepWater();
	bool ExecuteStepLandscape();
	bool ExecuteStepResources();
	bool ExecuteStepTribes();

	void OnGenerationSuccess();
	void OnGenerationError(const FString& Reason);
};
