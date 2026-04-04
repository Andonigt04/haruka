#pragma once

#include "core/scene.h"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

/**
 * @brief Editor panel for layered planet generation and chunk workflows.
 *
 * Responsibilities:
 * - configure generation parameters
 * - trigger source planet regeneration
 * - split/restore chunk hierarchy
 * - persist and reload per-chunk deltas
 * - apply local seed/brush edits
 */
class PlanetTerrainEditorPanel {
public:
    /** @brief Binds the active scene context used by panel actions. */
    void setScene(Haruka::Scene* scene);
    /** @brief Draws and executes the ImGui panel UI each frame. */
    void onImGuiRender();
    /** @brief Updates selected chunk id for seed/brush operations. */
    void setSelectedChunkId(int chunkId);
    /** @brief Sets source object name targeted by terrain actions. */
    void setTargetObjectName(const std::string& name);

private:
    /** @brief Non-owning pointer to active scene. */
    Haruka::Scene* currentScene = nullptr;
    char targetObjectName[128] = "Earth";
    int latSections = 8;
    int lonSections = 16;
    bool hideSourceAfterChunking = true;
    bool copyMaterial = true;
    bool autoSaveChunks = true;
    bool saveOnlyDirtyChunks = true;
    bool visualizeChunkSections = false;

    // Planet generation configuration
    int generationSeed = 1337;
    float baseRadiusKm = 6371.0f;
    float baseNegativeDepthKm = 11.0f;
    float reliefKm = 21.0f;
    float continentFrequency = 0.95f;
    float detailFrequency = 11.5f;
    bool enableContinents = true;
    bool enableMountains = true;
    bool useGPUGeneration = true;  // Use compute shader generation
    bool ignoreSafetyLimits = false; // Allow bypassing safety limits
    int meshSubdivisions = 8; // triangles per planet: 12 * 4^subdivisions

    int selectedChunkId = 0;
    int selectedChunkSeed = 1337;
    float brushDir[3] = {0.0f, 1.0f, 0.0f};
    float brushAngleDeg = 12.0f;
    float brushDeltaKm = 0.5f;

    int lastChunkCount = 0;
    int lastModifiedChunkCount = 0;
    std::string lastStatus;

    /** @brief Regenerates source mesh and (re)creates chunk objects. */
    void splitPlanetIntoChunks();
    /** @brief Removes generated chunks and restores source visibility flags. */
    void restoreSourcePlanet();
    /** @brief Persists current chunk mesh deltas to disk. */
    void saveChunkDeltas();
    /** @brief Loads persisted chunk deltas and applies them to scene chunks. */
    void loadChunkDeltas();
    /** @brief Applies directional brush displacement to selected chunk. */
    void applyBrushToChunk(bool raise);
    /** @brief Re-seeds selected chunk with layered deformation model. */
    void applySeedToChunk(int chunkId, int chunkSeed);
    /** @brief Applies global seed by regenerating source + chunk split. */
    void applySeedToPlanet();
    /** @brief Syncs UI seed field with selected chunk metadata. */
    void refreshSelectedChunkSeed();

    /** @brief Serializable delta snapshot for one chunk mesh. */
    struct ChunkDelta {
        int chunkId = -1;
        std::vector<glm::vec3> vertices;
        std::vector<glm::vec3> normals;
        std::vector<unsigned int> indices;
    };

    std::unordered_map<int, ChunkDelta> chunkDeltas;
    std::unordered_set<int> dirtyChunks;
    /** @brief Resolves persistent delta file path for source object. */
    std::string getDeltaFilePath(const std::string& sourceName) const;
    /** @brief Applies one stored delta onto target chunk object. */
    void applyChunkDelta(Haruka::SceneObject& chunkObj, const ChunkDelta& delta) const;
    /** @brief Converts a chunk delta into JSON payload. */
    nlohmann::json serializeChunkDelta(const ChunkDelta& delta) const;
    /** @brief Builds a chunk delta from JSON payload. */
    ChunkDelta deserializeChunkDelta(const nlohmann::json& j) const;
};