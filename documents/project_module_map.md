# Haruka Project — Module Map (STL-style Contracts)

## `src/core`
**Role:** Runtime foundation (application, scene graph, camera, project, world coordinates).

### Key contracts
- `Scene` owns object collection and mutation APIs.
- Pointer/index stability must be documented across add/remove operations.
- Components in `core/components` are data-contract driven and serialization-aware.

## `src/renderer`
**Role:** Rendering backend and feature pipeline.

### Key contracts
- Distinguish resident GPU resources vs CPU metadata.
- Document pass dependencies (GBuffer -> lighting -> postprocess).
- Document shader path expectations and runtime lookup behavior.

## `src/game`
**Role:** Gameplay systems and procedural generation.

### Key contracts
- Deterministic seed mapping must be explicit.
- CPU/GPU fallback path must be externally observable.
- Planet/chunk generation must define input scale conventions.

## `src/editor`
**Role:** Tooling and interactive workflows.

### Key contracts
- UI actions must map 1:1 to subsystem calls.
- User-facing status should reflect true backend result, not optimistic success.
- Large operations require safety guardrails and clear override behavior.

## `src/io`
**Role:** Streaming/loading and asset I/O.

### Key contracts
- Cache behavior and eviction assumptions documented.
- Thread usage and callback guarantees documented.

## `src/physics`
**Role:** Collision/queries and physics simulation helpers.

### Key contracts
- Spatial structure invariants (`octree`, raycast assumptions).
- Update cadence and unit conventions.

## `src/audio`
**Role:** Audio playback/control.

### Key contracts
- Resource lifecycle (init/shutdown) and thread assumptions.

## `src/network`
**Role:** Client/server transport.

### Key contracts
- Connection lifecycle, retry/failure behavior, threading model.

## `src/database`
**Role:** DB access and persistence glue.

### Key contracts
- Connection ownership, query error handling, transaction assumptions.

## `src/test`
**Role:** Standalone validation executables.

### Key contracts
- Explicit setup/teardown and deterministic input datasets where possible.
