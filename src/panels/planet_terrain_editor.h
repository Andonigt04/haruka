#pragma once

#include "core/scene.h"
#include "tasks/editor_task.h"
#include "game/planetary_system.h"
#include <nlohmann/json.hpp>
#include <future>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Haruka {
class MaterialComponent;
}

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
    /** @brief Inyecta el PlanetarySystem activo para generación y preview. */
    void setPlanetarySystem(Haruka::PlanetarySystem* system) { planetarySystem = system; }
    /** @brief Advances background generation tasks once per frame. */
    void update();
    /** @brief Draws and executes the ImGui panel UI each frame. */
    void onImGuiRender();
    /** @brief Updates selected chunk id for seed/brush operations. */
    void setSelectedChunkId(int chunkId);
    /** @brief Sets source object name targeted by terrain actions. */
    void setTargetObjectName(const std::string& name);

    /** @name Async split task API (for custom task wrappers) */
    ///@{
    void beginSplitPlanetIntoChunksTask();
    void tickSplitPlanetIntoChunksTask();
    void cancelSplitPlanetIntoChunksTask();

    bool isSplitTaskRunning() const { return splitTask.running; }
    bool didSplitTaskComplete() const { return splitTask.phase == SplitTaskState::Phase::Completed; }
    bool wasSplitTaskCancelled() const { return splitTask.phase == SplitTaskState::Phase::Cancelled; }
    float getSplitTaskProgress() const { return splitTask.progress; }
    std::string getSplitTaskStatus() const { return splitTask.status; }
    std::string getLastStatus() const { return lastStatus; }
    ///@}

private:
    /** @brief Non-owning pointer to active scene. */
    Haruka::Scene* currentScene = nullptr;
    Haruka::PlanetarySystem* planetarySystem = nullptr;

    // Target / split layout
    char targetObjectName[128] = "Earth";
    int latSections = 8;
    int lonSections = 16;
    bool autoChunkByTriangleBudget = true;
    int targetTrianglesPerChunk = 12000;
        int maxVerticesPerChunkHardCap = 50000;
    bool hideSourceAfterChunking = true;
    bool copyMaterial = true;
    bool autoSaveChunks = true;
    bool saveOnlyDirtyChunks = true;
    bool visualizeChunkSections = false;

    // Generation parameters
    int generationSeed = 1337;
    int lastSyncedSeed = 1337;  // Rastrear cambios de seed
    float baseRadiusKm = 6371.0f;
    float continentFrequency = 1.2f;
    float continentHeightStrength = 0.06f;
    float seaLevel = 0.52f;
    float macroFrequency = 3.5f;
    float macroHeightStrength = 0.16f;
    float detailFrequency = 12.0f;
    float detailHeightStrength = 0.035f;
    int detailOctaves = 4;
    bool enableContinents = true;
    bool enableMountains = true;
    int meshSubdivisions = 4;
    
    // Legacy parameters (kept for compatibility)
    float plateReliefKm = 6.5f;
    float plateScale = 0.55f;
    float plateBoundarySharpness = 0.68f;
    float minContinentSizeKm = 1800.0f;
    float reliefKm = 18.0f;
    bool useGPUGeneration = false;
    int generationQualityPreset = 1;
    bool ignoreSafetyLimits = false;

    // Chunk tools
    int selectedChunkId = 0;
    int selectedChunkSeed = 1337;
    float brushDir[3] = {0.0f, 1.0f, 0.0f};
    float brushAngleDeg = 12.0f;
    float brushDeltaKm = 0.5f;
    float buildingRadius = 0.05f;              // Radio de visualización del edificio
    bool placingBuilding = false;              // Estado de colocación

    // Runtime UI state
    int lastChunkCount = 0;
    int lastModifiedChunkCount = 0;
    int lastMinVertsPerChunk = 0;
    int lastAvgVertsPerChunk = 0;
    int lastMaxVertsPerChunk = 0;
    std::string lastStatus;
    bool regenerateSourceOnNextSplit = false;

    /** @brief Regenerates source mesh and (re)creates chunk objects. */
    void splitPlanetIntoChunks();
    /** @brief Removes generated chunks and restores source visibility flags. */
    void restoreSourcePlanet();
    /** @brief Persists current chunk mesh deltas to disk. */
    void saveChunkDeltas();
    /** @brief Loads persisted chunk deltas and applies them to scene chunks. */
    void loadChunkDeltas();
    /** @brief Saves only planet generation/layout settings for compact replication. */
    void savePlanetSettings() const;
    /** @brief Loads planet generation/layout settings from the compact preset file. */
    void loadPlanetSettings();
    /** @brief Applies directional brush displacement to selected chunk. */
    void applyBrushToChunk(bool raise);
    /** @brief Re-seeds selected chunk with layered deformation model. */
    void applySeedToChunk(int chunkId, int chunkSeed);
    /** @brief Applies global seed by regenerating source + chunk split. */
    void applySeedToPlanet();
    /** @brief Syncs UI seed field with selected chunk metadata. */
    void refreshSelectedChunkSeed();
    /** @brief Saves current generator config to target object's JSON properties. */
    void saveGeneratorConfigToObject();
    
    /** @brief Serializable delta snapshot for one chunk mesh. */
    struct ChunkDelta {
        int chunkId = -1;
        std::vector<glm::vec3> vertices;
        std::vector<glm::vec3> normals;
        std::vector<unsigned int> indices;
    };

    // Caches for compact chunk-state tracking:
    // - unordered_map keeps chunk deltas addressable by id in O(1) average time
    // - unordered_set tracks dirty chunk ids without duplicates
    std::unordered_map<int, ChunkDelta> chunkDeltas;
    std::unordered_set<int> dirtyChunks;

    /** @brief Runtime state for incremental chunk generation. */
    struct SplitTaskState {
        struct ChunkData {
            std::vector<glm::vec3> localVerts;
            std::vector<glm::vec3> localNormals;
            std::vector<unsigned int> localIndices;
            std::unordered_map<unsigned int, unsigned int> remap;
            int latBin = 0;
            int lonBin = 0;
        };

        enum class Phase {
            Idle,
            GenerateSource,
            ProcessTriangles,
            RemoveStale,
            BuildChunks,
            Finalize,
            Completed,
            Failed,
            Cancelled
        };

        Phase phase = Phase::Idle;
        bool running = false;
        float progress = 0.0f;
        std::string status;

        std::string sourceName;
        std::string sourceType;
        glm::dvec3 sourceColor = glm::dvec3(1.0);
        double sourceIntensity = 1.0;
        std::shared_ptr<Haruka::MaterialComponent> sourceMaterial;

        std::vector<glm::vec3> verts;
        std::vector<glm::vec3> normals;
        std::vector<unsigned int> indices;
        size_t triangleCursor = 0;
        size_t invalidTriangles = 0;

        std::map<int, ChunkData> chunks;
        std::vector<int> chunkOrder;
        size_t buildCursor = 0;

        std::vector<std::string> staleChunks;
        size_t staleCursor = 0;

        int sourceIndex = -1;
        int created = 0;
        int updated = 0;

        struct SourceGenerationResult {
            bool success = false;
            std::string error;
            std::string warning;
            std::vector<glm::vec3> vertices;
            std::vector<glm::vec3> normals;
            std::vector<unsigned int> indices;
        };

        std::future<SourceGenerationResult> sourceFuture;
        bool sourceFutureValid = false;

        void reset() {
            phase = Phase::Idle;
            running = false;
            progress = 0.0f;
            status.clear();
            sourceName.clear();
            sourceType.clear();
            sourceColor = glm::dvec3(1.0);
            sourceIntensity = 1.0;
            sourceMaterial.reset();
            verts.clear();
            normals.clear();
            indices.clear();
            triangleCursor = 0;
            invalidTriangles = 0;
            chunks.clear();
            chunkOrder.clear();
            buildCursor = 0;
            staleChunks.clear();
            staleCursor = 0;
            sourceIndex = -1;
            created = 0;
            updated = 0;
            sourceFutureValid = false;
        }
    };

    SplitTaskState splitTask;
    std::unique_ptr<EditorTaskBase> activeTask;
    /** @brief Validates inputs and prepares split task source data. */
    bool prepareSplitTask(std::string& error);
    /** @brief Collects source object metadata used by later split stages. */
    void captureSplitSourceState(const Haruka::SceneObject& source);
    /** @brief Partitions triangle indices into chunk buckets for one frame slice. */
    void processSplitTaskTriangles();
    /** @brief Removes stale chunks that are no longer needed. */
    void processSplitTaskStaleRemoval();
    /** @brief Builds or updates chunk objects from prepared chunk data. */
    void processSplitTaskChunkBuild();
    /** @brief Finalizes chunk hierarchy and source metadata. */
    void finalizeSplitTask();
    /** @brief Resolves persistent delta file path for source object. */
    std::string getDeltaFilePath(const std::string& sourceName) const;
    /** @brief Resolves compact planet settings file path for source object. */
    std::string getSettingsFilePath(const std::string& sourceName) const;
    /** @brief Applies one stored delta onto target chunk object. */
    void applyChunkDelta(Haruka::SceneObject& chunkObj, const ChunkDelta& delta) const;
    /** @brief Converts a chunk delta into JSON payload. */
    nlohmann::json serializeChunkDelta(const ChunkDelta& delta) const;
    /** @brief Builds a chunk delta from JSON payload. */
    ChunkDelta deserializeChunkDelta(const nlohmann::json& j) const;
};
