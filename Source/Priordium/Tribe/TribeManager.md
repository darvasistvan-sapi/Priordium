# TribeManager Dependencies

```mermaid
flowchart LR
  subgraph Fields
    F1[nearbyResources]
    F2[tribeMen]
    F3[storages]
    F4[resourceBaseClass]
    F5[tribeManTasks]
    F7[OccupiedResources]
    F8[storageResourcePathLengths]
    F9[storageResourcePaths]
  end

  subgraph Methods
    M1[calculateStorageResourcePathLengths]
    M2[CalculateStorageResourcePathLengths]
    M3[CalculateStorageResourcePathLength]
    M4[getFreeTribeMan]
    M5[collectResource]
    M6[getNearestResource]
    M7[getStorageNearestResource]
    M8[calculateNearbyResources]
    M9[calculateStorageResourcePaths]
    M10[BeginPlay]
    M11[CheckNullReferences]
    M12[checkNearbyResources]
    M13[checkOccupiedResources]
    M14[checkTribeMen]
    M15[checkStorages]
    M16[checkTribeManTasks]
    M17[checkStorageResourcePathLengths]
  end

  M1 --> M2
  M1 --> M8
  M2 --> M3
  M5 --> M4
  M5 --> M6
  M6 --> M7
  M9 --> F8
  M9 --> F9
  M10 --> M8
  M11 --> M12
  M11 --> M13
  M11 --> M14
  M11 --> M15
  M11 --> M16
  M11 --> M17

  M1 --> F4
  M1 --> F8
  M1 --> F3

  M2 --> F4
  M2 --> F8

  M3 --> F8

  M4 --> F2
  M4 --> F5

  M5 --> F5

  M6 --> F8

  M7 --> F8
  M7 --> F7

  M8 --> F1
  M8 --> F3
  M8 --> F4

  M10 --> F1

  M12 --> F1
  M13 --> F7
  M14 --> F2
  M15 --> F3
  M16 --> F5
  M17 --> F8

  F7 --> F1
  F8 --> F1
  F9 --> F1
  F9 --> F3
  F5 -. depends on keys from .-> F2
  F8 -. depends on keys from .-> F3
  F7 -. contains resources from .-> F8
```

## Notes

- `tribeManTasks` depends on `tribeMen` keys.
- `storageResourcePathLengths` depends on `storages` keys.
- `collectResource` orchestrates free worker selection, resource selection, and task assignment.
- `calculateNearbyResources` populates `nearbyResources` via sphere overlap for each storage within 10 km.
- `calculateStorageResourcePaths` flattens `storageResourcePathLengths` into a flat sorted array `storageResourcePaths`; call it after async path queries have completed.
- `BeginPlay` calls `calculateNearbyResources` on game start and logs the result.
- `CheckNullReferences` is the entry point for stale-reference cleanup; only runs the dependent checks if at least one entry was removed from `nearbyResources`, `tribeMen` or `storages`.
- `checkNearbyResources`, `checkOccupiedResources`, `checkTribeMen`, `checkStorages`, `checkTribeManTasks`, `checkStorageResourcePathLengths` each clean a single collection and return whether any entry was removed.
