#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace Haruka {

/**
 * @brief Procedural planet mesh generator (CPU/GPU with fallback).
 *
 * Builds a cube-sphere and applies layered terrain deformation.
 * Supports deterministic generation through explicit seed fields.
 * 
 * Key: Seed-based on-demand generation allows infinite terrains with minimal RAM.
 * Small chunks (1-5k verts) generated async on CPU, cached briefly, then discarded.
 */
class PlanetGenerator {
public:
    /**
     * @brief Generates one chunk tile from a cube-sphere face.
     * Generated async in CPU threads. Uses seed-based deterministic output.
     * 
     * CONFIGURABLE PLANETS:
     * - All parameters in PlanetConfig can be tweaked for any planet type
     * - Modify via SceneObject properties: "terrainEditor": { "seaLevel": 0.4, ... }
     * - No preset limits: complete freedom in configuration
     * - Same seed + config = deterministic output (reproducible)
     */

    /**
     * @brief Full generation configuration contract.
     *
     * All fields are deterministic inputs. Same config -> same output mesh.
     */
    struct PlanetConfig {
        /** @brief Base sphere radius used during mesh generation (normalized space). */
        float radius = 1.0f;
        /** @brief Cube-face subdivision exponent (`grid = 2^subdivisions`). */
        int subdivisions = 4;
        /** @brief Physical radius scale used for ratio-based deformation parameters. */
        float baseRadiusKm = 6371.0f;

        /** @name Layer seed set */
        ///@{
        int seedBase = 42;
        int seedContinents = 1337;
        int seedMacro = 2024;
        int seedDetail = 9001;
        ///@}

        /** @name Layer toggles and backend policy */
        ///@{
        bool enableContinents = true;
        bool enableMountains = true;
        bool useGPU = true;  // CPU better for small chunks; GPU for full planets
        ///@}

        /** @name Continents / oceans */
        ///@{
        float seaLevel = 0.52f;
        float continentFrequency = 1.2f;
        float continentWarpStrength = 0.15f;
        float continentHeightStrength = 0.06f;
        ///@}

        /** @name Macro terrain (ranges, plateaus, ridges) */
        ///@{
        float macroFrequency = 3.5f;
        float macroHeightStrength = 0.16f;
        ///@}

        /** @name Micro detail */
        ///@{
        float detailFrequency = 12.0f;
        float detailHeightStrength = 0.035f;
        ///@}

        /** @name Noise controls */
        ///@{
        int octavesContinents = 4;
        int octavesMacro = 5;
        int octavesDetail = 4;
        float persistence = 0.5f;
        float lacunarity = 2.0f;
        ///@}
    };

    /** @brief Output mesh container and basic height statistics. */
    struct PlanetData {
        /** @brief Generated vertex positions (normalized generation space). */
        std::vector<glm::vec3> vertices;
        /** @brief Per-vertex normals aligned with generated geometry. */
        std::vector<glm::vec3> normals;
        /** @brief Triangle index buffer (3 indices per triangle). */
        std::vector<unsigned int> indices;
        /** @brief Base radius used by generation. */
        float radius = 0.0f;
        /** @brief Minimum generated radial height. */
        float minHeight = 0.0f;
        /** @brief Maximum generated radial height. */
        float maxHeight = 0.0f;
    };

    /** @brief Chunk generation request over one cube-sphere face tile.
     * 
     * Seed-based deterministic generation: same key + config = same mesh always.
     * No storage needed: chunks are generated on-demand, cached briefly, discarded.
     * Re-entering chunk: regenerate from seed (CPU fast, deterministic).
     * 
     * Supports neighbor-aware LOD stitching for seamless multi-resolution terrain.
     */
    struct ChunkConfig {
        int face = 0;           ///< Cube face (0-5)
        int lod = 0;            ///< Level of detail (0=highest density)
        int tileX = 0;          ///< X position within face
        int tileY = 0;          ///< Y position within face
        int tilesPerFace = 1;   ///< Tiles per face

        int neighborLodN = 0;   ///< North neighbor LOD for stitching
        int neighborLodS = 0;   ///< South neighbor LOD for stitching
        int neighborLodE = 0;   ///< East neighbor LOD for stitching
        int neighborLodW = 0;   ///< West neighbor LOD for stitching
    };

    /** @brief Mesh payload for one generated chunk tile. */
    struct ChunkData {
        std::vector<glm::vec3> vertices;
        std::vector<glm::vec3> normals;
        std::vector<unsigned int> indices;
        float minHeight = 0.0f;
        float maxHeight = 0.0f;
    };

    /**
    * @brief Legacy convenience overload.
    * @param radius Base sphere radius.
    * @param subdivisions Cube-face subdivision exponent.
    * @param seed Root deterministic seed.
    * @param noiseScale Detail frequency proxy.
    * @param heightScale Detail amplitude proxy.
    * @param octaves Detail octave count.
     */
    static PlanetData generatePlanet(
        float radius,
        int subdivisions = 4,
        int seed = 42,
        float noiseScale = 1.0f,
        float heightScale = 0.1f,
        int octaves = 4
    );

    /**
     * @brief Generates a planet from full configuration.
     * Attempts GPU backend if enabled; otherwise uses CPU path.
     */
    static PlanetData generatePlanet(const PlanetConfig& config);
    /**
     * @brief Generates one chunk tile from a cube-sphere face.
     * Generated async in CPU threads. Uses seed-based deterministic output.
     */
    static ChunkData generateChunk(const PlanetConfig& config, const ChunkConfig& chunk);

    /**
     * @brief GPU compute implementation.
     * @param config Input generation config.
     * @param outData Output mesh container.
     * @return `true` on success, `false` if caller should fallback to CPU.
     */
    static bool tryGeneratePlanetGPU(const PlanetConfig& config, PlanetData& outData);

private:
    /** @brief Generates one cube face and appends vertices/indices to output. */
    static void generateFace(
        PlanetData& data,
        glm::vec3 faceNormal,
        glm::vec3 right,
        glm::vec3 up,
        const PlanetConfig& config,
        int faceSeedOffset
    );
};

}
