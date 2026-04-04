#pragma once

#include "core/scene.h"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

class PlanetTerrainEditorPanel {
public:
    void setScene(Haruka::Scene* scene);
    void onImGuiRender();
    void setSelectedChunkId(int chunkId);
    void setTargetObjectName(const std::string& name);

private:
    Haruka::Scene* currentScene = nullptr;
    char targetObjectName[128] = "Earth";
    int latSections = 8;
    int lonSections = 16;
    bool hideSourceAfterChunking = true;
    bool copyMaterial = true;
    bool autoSaveChunks = true;
    bool saveOnlyDirtyChunks = true;
    bool visualizeChunkSections = false;

    // Datos de instancia/configuración del terreno planetario
    int generationSeed = 1337;
    float baseRadiusKm = 6371.0f;
    float baseNegativeDepthKm = 11.0f;
    float reliefKm = 21.0f;
    float continentFrequency = 0.95f;
    float detailFrequency = 11.5f;
    bool enableContinents = true;
    bool enableMountains = true;
    bool useGPUGeneration = true;  // Usar compute shader para generación
    bool ignoreSafetyLimits = false; // Permitir sobrepasar límites de seguridad
    int meshSubdivisions = 8; // triángulos por planeta: 12 * 4^subdivisions

    int selectedChunkId = 0;
    int selectedChunkSeed = 1337;
    float brushDir[3] = {0.0f, 1.0f, 0.0f};
    float brushAngleDeg = 12.0f;
    float brushDeltaKm = 0.5f;

    int lastChunkCount = 0;
    int lastModifiedChunkCount = 0;
    std::string lastStatus;

    void splitPlanetIntoChunks();
    void restoreSourcePlanet();
    void saveChunkDeltas();
    void loadChunkDeltas();
    void applyBrushToChunk(bool raise);
    void applySeedToChunk(int chunkId, int chunkSeed);
    void applySeedToPlanet();
    void refreshSelectedChunkSeed();

    struct ChunkDelta {
        int chunkId = -1;
        std::vector<glm::vec3> vertices;
        std::vector<glm::vec3> normals;
        std::vector<unsigned int> indices;
    };

    std::unordered_map<int, ChunkDelta> chunkDeltas;
    std::unordered_set<int> dirtyChunks;
    std::string getDeltaFilePath(const std::string& sourceName) const;
    void applyChunkDelta(Haruka::SceneObject& chunkObj, const ChunkDelta& delta) const;
    nlohmann::json serializeChunkDelta(const ChunkDelta& delta) const;
    ChunkDelta deserializeChunkDelta(const nlohmann::json& j) const;
};