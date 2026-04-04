#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace Haruka {

/**
 * @brief Procedural planet mesh generator (CPU/GPU with fallback).
 *
 * Builds a cube-sphere and applies layered terrain deformation.
 * Supports deterministic generation through explicit seed fields.
 */
class PlanetGenerator {
public:
    /** @brief Built-in preset families for high-level planet style bootstrapping. */
    enum class PlanetPreset {
        EARTH_LIKE,
        DESERT,
        ICE
    };

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

        /** @brief Layer 1: global negative offset depth (km). */
        float baseNegativeDepthKm = 11.0f;

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
        /** @brief Try GPU compute path first; fallback to CPU on failure. */
        bool useGPU = true;
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
     *
     * Attempts GPU backend if enabled and available; otherwise uses CPU path.
     */
    static PlanetData generatePlanet(const PlanetConfig& config);

    /** @brief Returns tuned default values for a preset family. */
    static PlanetConfig getPresetConfig(PlanetPreset preset);

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
