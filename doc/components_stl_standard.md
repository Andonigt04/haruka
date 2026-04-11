# Core Components — STL/C++ Documentation Standard

This document defines the usage contracts and documentation style for components under `src/core/components`.

## Goal
- Document the public API using a C++/STL-oriented style:
  - clear class responsibility
  - preconditions and postconditions
  - ownership/lifetime notes
  - serialization behavior

## Documented Components
- `MaterialComponent`
- `MeshComponent`
- `MeshRendererComponent`
- `ModelComponent`
- `ScriptComponent`
- `TransformComponent`

## Contract Summary

### MaterialComponent
- Stores PBR parameters and texture paths.
- `toJSON()/fromJSON()` define the persistence contract.
- Does not load resources by itself.

### MeshComponent
- Path-reference component for mesh assets (`meshPath`).
- `toJson()/fromJson()` are tolerant to missing fields and use defaults.

### MeshRendererComponent
- Holds a resident mesh (`SimpleMesh`) plus CPU source data (`sourceVertices/normals/indices`).
- `setMesh(...)` keeps both representations in sync.
- `releaseMesh()` releases only the resident resource.
- Exposes cached and resident count getters.

### ModelComponent
- Reference to a full model asset (`modelPath`).
- Minimal JSON serialization (`type`, `modelPath`).

### ScriptComponent
- Script path reference (`scriptPath`).
- Inspector allows path editing.

### TransformComponent
- `position`, `rotation`, `scale` are `glm::dvec3`.
- Full transform serialization.
- `fromJson(...)` expects 3-element arrays per field.

## Applied Style Rules
- Class and method comments describe intent and contract.
- Public fields include explicit purpose.
- Editor/runtime behavior notes are included when relevant.
- No logic changes (documentation-only updates).

## Maintenance Notes
For new APIs in this folder:
1. Add a class comment (`@brief`).
2. Document each public method (input/output/effect).
3. State expected serialization behavior in `toJson()/fromJson()`.
4. Clarify ownership/lifetime for shared resources.
