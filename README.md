# Haruka Engine

A modern 3D rendering engine implementing advanced lighting techniques and real-time post-processing using OpenGL 4.6 with deferred shading architecture.

## Technologies

### 1. Deferred Shading

**Purpose**
Separates rendering into two phases: geometry and lighting. Enables hundreds of lights without exponential cost.

**How it Works**
Renders scene geometry to G-Buffer (multiple textures containing position, normal, albedo, etc.), then performs lighting calculations using only texture data instead of per-vertex calculations.

**Shaders**: `deferred_geom.vert/frag`, `deferred_light.frag`

---

### 2. PBR - Physically Based Rendering

**Purpose**
Implements realistic material properties based on physics using Cook-Torrance BRDF model.

**How it Works**
Combines diffuse (Lambertian) and specular (microfacet) components with roughness and metallic parameters to create materials that respond correctly to any lighting condition.

**Key Parameters**
- `roughness`: 0.0 (mirror-like) → 1.0 (diffuse)
- `metallic`: 0.0 (non-metal) → 1.0 (metal)

---

### 3. Normal Mapping

**Purpose**
Simulates surface detail without additional geometry using normal vector textures.

**How it Works**
Encodes surface normals in tangent space. During rendering, converts to world space using TBN matrix (Tangent, Bitangent, Normal), allowing complex details on flat surfaces.

**Benefit**: Thousands of polygons worth of visual detail from a single texture

---

### 4. Parallax Mapping

**Purpose**
Displaces UVs based on height map to simulate depth on flat surfaces.

**Techniques**
- Basic Parallax: simple UV offset
- Steep Parallax: ray-marching through height layers
- POM (Parallax Occlusion Mapping): maximum quality with occlusion

---

### 5. Shadow Mapping (Directional)

**Purpose**
Renders realistic shadows from directional light sources (sun, moon).

**How it Works**
1. Render scene from light's perspective to depth buffer
2. Compare fragment depth with stored depth to determine occlusion
3. PCF (Percentage Closer Filtering) samples multiple depths for soft shadow edges

---

### 6. Point Shadows (Omnidirectional)

**Purpose**
Renders shadows from point lights emitting in all directions (lamps, torches, explosions).

**How it Works**
Uses cubemap (6 faces) to store depth information from light's perspective. Geometry Shader renders to all 6 faces in single draw call. Adaptive PCF for smooth shadows.

**Technical**: 1024³ compressed depth cubemap per light

---

### 7. SSAO - Screen Space Ambient Occlusion

**Purpose**
Calculates soft shadows in crevices and contact points, improving depth perception without global illumination overhead.

**How it Works**
For each pixel:
1. Generate 64 random hemisphere vectors
2. Sample nearby G-Buffer positions
3. Count occluded samples (closer than current pixel)
4. Modulate ambient light with occlusion factor

**Performance**: O(width × height × samples), independent of geometry complexity

---

### 8. IBL - Image Based Lighting

#### 8.1 Diffuse IBL (Irradiance)

**Purpose**
Realistic ambient lighting from environment panorama. Blue sky produces blue ambient, sunset produces orange ambient.

**How it Works**
1. Load equirectangular HDRI (360° panorama)
2. Convert to cubemap
3. Convolve by integrating light over hemisphere per direction
4. Result: low-res cubemap (32×32) with ambient light data

#### 8.2 Specular IBL (Prefilter + BRDF LUT)

**Purpose**
Environment reflections on shiny surfaces (metals, chrome, water).

**Components**
- **Prefilter Map**: Cubemap with roughness pre-integrated in mipmaps
- **BRDF LUT**: 2D lookup table (roughness × view angle) for Fresnel interpolation

**Result**: Realistic specular reflections without ray-tracing

---

### 9. HDR + Tone Mapping

**Purpose**
Handles bright values > 1.0 without clipping, maps HDR range to displayable [0,1].

**How it Works**
Renders to `GL_RGB16F` (16-bit float = huge dynamic range), applies tone mapping operator post-process to compress to visible range.

**Benefits**
- Bright explosions don't saturate
- Dark caves show detail
- Realistic exposure simulation

**Operators**: Reinhard, ACES, Exposure-based

---

### 10. Bloom

**Purpose**
Glow effect around bright lights. Simulates lens imperfections and creates visual impact.

**Pipeline**
1. **Extract Pass**: Threshold pixels with luminance > 1.0
2. **Blur Pass**: Gaussian blur (horizontal + vertical) on bright pixels
3. **Composite Pass**: Additive blend back to main image

**Result**: Smooth glow halos around bright areas

---

### 11. RenderTarget System

**Purpose**
Modular FBO (Framebuffer Object) abstraction for composing rendering passes.

**Architecture**
Each pass outputs to its own RenderTarget (texture), next pass reads previous output. Enables flexible post-processing chains.

---

## Performance Characteristics

| Technique | Complexity | Lights | VRAM |
|-----------|-----------|--------|------|
| Forward Rendering | O(geometry × lights) | ~8 | Low |
| Deferred Shading | O(geometry + lights) | 100+ | High |
| SSAO | O(width × height × samples) | N/A | Low |
| Shadow Maps | O(light passes) | Per light | Medium |
| IBL | O(1) runtime | N/A | Medium |
| Bloom | O(width × height × blur) | N/A | Low |

---
