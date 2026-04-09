#pragma once

#include <future>
#include <map>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "math_types.h"
#include "scene.h"
#include "world_system.h"
#include "game/planet_generator.h"

class RaycastSimple;

namespace Haruka {

class MaterialComponent;

struct TerrainStreamingStats {
    int visibleChunks = 0;
    int residentChunks = 0;
    int pendingChunkLoads = 0;
    int pendingChunkEvictions = 0;
    int residentMemoryMB = 0;
    int trackedChunks = 0;
    int maxMemoryMB = 0;
};

class TerrainStreamingSystem {
public:
    void update(Scene* scene,
                WorldSystem* worldSystem,
                RaycastSimple* raycastSystem,
                const WorldPos& cameraPos,
                const glm::mat4& viewProj,
                TerrainStreamingStats* outStats = nullptr);
    
    // Invalidar chunks cuando cambia la semilla (sin cambiar escena)
    void invalidateChunksForSeedChange() {
        currentSeedGeneration++;  // Incrementar sin limpiar
    }

private:
    static constexpr size_t MAX_READY_CHUNKS_CACHED = 64;

    struct TerrainChunkTemplate {
        std::string sourceName = "Earth";
        int chunkTilesY = 16;
        int chunkTilesX = 16;
        std::string type = "Mesh";
        glm::dvec3 color = glm::dvec3(1.0);
        double intensity = 1.0;
        int parentIndex = -1;
        std::shared_ptr<Haruka::MaterialComponent> material;
    };

    std::map<PlanetChunkKey, std::future<PlanetGenerator::ChunkData>> chunkGenJobs;
    std::map<PlanetChunkKey, PlanetGenerator::ChunkData> chunkReadyData;
    uint64_t currentSceneVersion = 0;
    uint32_t lastCheckedSeed = 0;  // Detectar cambios de semilla
    uint32_t currentSeedGeneration = 0;  // Versión de semilla actual para invalidación

    static bool isTerrainChunkObject(const SceneObject& obj);
    static SceneObject* findTerrainChunkByKey(Scene* scene, const PlanetChunkKey& key);
    static bool buildTerrainChunkTemplate(Scene* scene, TerrainChunkTemplate& outTpl);
    static SceneObject* createTerrainChunkForKey(Scene* scene, const PlanetChunkKey& key);
    static int findPlanetRootIndex(Scene* scene);
    static void addChunkAsChild(Scene* scene, SceneObject& chunk, int parentIdx);
    static void ensureTerrainChunkKeysAndGrid(Scene* scene, WorldSystem* worldSystem);
    static PlanetGenerator::PlanetConfig buildPlanetConfigFromChunkSource(const SceneObject& chunkObj, Scene* scene);
    static PlanetGenerator::ChunkConfig buildChunkConfigFromObjectAndKey(const SceneObject& chunkObj, const PlanetChunkKey& key, WorldSystem* worldSystem);
    static uint32_t calculateChunkSizeBytes(const SceneObject& chunk);
    static std::string makeChunkCollisionId(const PlanetChunkKey& key);
    static void buildCollisionProxy(const std::vector<glm::vec3>& verts,
                                    const std::vector<unsigned int>& inds,
                                    std::vector<glm::vec3>& outVerts,
                                    std::vector<unsigned int>& outInds);

    void pollChunkGenerationJobs();
    void invalidateStaleChunkJobs(uint64_t newSceneVersion);
};

} // namespace Haruka
