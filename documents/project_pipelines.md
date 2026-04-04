# Haruka Project — Critical Pipelines

## 1) Terrain Generation Pipeline
1. Editor action triggers terrain generation.
2. Config is assembled (seed, frequencies, relief, toggles).
3. Generator attempts GPU path.
4. On failure, generator falls back to CPU path.
5. Result mesh is assigned to source object.
6. Mesh may be split into chunks and attached as scene children.

### Contract points
- Seed determinism: same config -> same output.
- Fallback determinism: GPU failure still produces valid CPU result.
- Status messaging must indicate real execution path.

## 2) Scene/Chunk Persistence Pipeline
1. Chunk meshes are serialized as deltas.
2. Deltas are keyed by source object + chunk ID.
3. Load applies deltas only to matching source/chunks.

### Contract points
- Missing delta file is non-fatal.
- Partial saves (`dirty only`) must preserve unchanged chunks.

## 3) Deferred Rendering Pipeline
1. Geometry pass populates GBuffer.
2. Lighting pass consumes GBuffer.
3. Optional post effects (SSAO, bloom, tonemapping, etc.).

### Contract points
- Pass order is strict.
- Resource formats and sizes must match pass expectations.

## 4) Asset Streaming Pipeline
1. Request enters streamer queue.
2. Worker threads load/decode resources.
3. Main/render thread performs GPU upload.

### Contract points
- Explicit handoff point between worker CPU and render GPU stages.
- Cache limits and eviction policy must be documented.
