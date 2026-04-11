# Haruka Project — STL/C++ Documentation Standard

This document extends the STL/C++ documentation approach to the entire project.

## Scope
Applies to all source modules under `src/`:
- `core/`
- `renderer/`
- `game/`
- `editor/`
- `io/`
- `physics/`
- `audio/`
- `network/`
- `database/`
- `test/`

## Documentation Contract (per public API)
For each class/function exposed outside its translation unit:
1. **Intent** (`@brief`)
2. **Inputs/outputs**
3. **Preconditions / postconditions**
4. **Ownership / lifetime** (raw pointer, `shared_ptr`, reference semantics)
5. **Threading assumptions** (main thread, render thread, worker-safe)
6. **Error/fallback behavior**
7. **Serialization contract** (if applicable)

## Recommended Header Layout (STL style)
1. Class responsibility summary
2. Public API grouped by responsibility
3. Data members grouped with purpose comments
4. Invariants and constraints

## Naming and Semantics
- Keep existing names unless a refactor is explicitly requested.
- Use descriptive comments for domain terms (`chunk`, `scene`, `resident mesh`, `world pos`).
- Prefer deterministic descriptions for procedural systems (seed mapping, octave use, fallback paths).

## Ownership & Resource Rules
- Explicitly document who owns GPU resources and when they are released.
- For components storing both CPU+GPU representations, document synchronization points.
- For pointers into scene/object arrays, state index/pointer invalidation conditions.

## Serialization Rules
- Every serializable component must define:
  - required fields
  - optional fields and defaults
  - version-tolerance behavior for missing keys

## Error & Fallback Rules
- If runtime fallback exists (e.g., GPU->CPU), document trigger conditions and observable status.
- Error messages should be actionable and include subsystem context.

## Performance Notes Policy
Document complexity where it affects editor/runtime behavior:
- mesh/chunk operations: expected growth with subdivisions
- scene rebuild operations: object add/remove/update behavior
- CPU/GPU sync points: possible stalls

## Minimum Per-Module Documentation Artifacts
- One module summary section in `documents/project_module_map.md`
- One flow section for critical pipelines (render, terrain generation, scene persistence)

## Non-Goals
- No automatic code rewrites.
- No behavior changes introduced by documentation work.
