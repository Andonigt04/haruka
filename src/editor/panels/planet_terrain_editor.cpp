#include "planet_terrain_editor.h"

#include "core/components/mesh_renderer_component.h"
#include "game/planet_generator.h"
#include "imgui.h"

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr double kSafeMaxVertices = 500000.0;
constexpr double kSafeMaxTriangles = 1000000.0;

float hashNoise(const glm::vec3& p, int seed) {
    float n = glm::dot(p, glm::vec3(12.9898f + seed * 0.001f, 78.233f + seed * 0.002f, 37.719f + seed * 0.003f));
    float s = std::sin(n) * 43758.5453f;
    return (s - std::floor(s)) * 2.0f - 1.0f;
}

float saturatef(float v) {
    return std::clamp(v, 0.0f, 1.0f);
}

float smoothstepf(float edge0, float edge1, float x) {
    float t = saturatef((x - edge0) / std::max(1e-6f, edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

float fBmHash(const glm::vec3& p, int seed, int octaves, float persistence, float lacunarity, float frequency) {
    float amp = 1.0f;
    float total = 0.0f;
    float norm = 0.0f;
    glm::vec3 pp = p * frequency;
    for (int i = 0; i < octaves; ++i) {
        total += hashNoise(pp, seed + i * 31) * amp;
        norm += amp;
        amp *= persistence;
        pp *= lacunarity;
    }
    return norm > 1e-6f ? (total / norm) : 0.0f;
}

nlohmann::json vec3ArrayToJson(const std::vector<glm::vec3>& values) {
    nlohmann::json result = nlohmann::json::array();
    for (const auto& value : values) {
        result.push_back({value.x, value.y, value.z});
    }
    return result;
}

std::vector<glm::vec3> jsonToVec3Array(const nlohmann::json& j) {
    std::vector<glm::vec3> values;
    if (!j.is_array()) return values;
    values.reserve(j.size());
    for (const auto& item : j) {
        if (item.is_array() && item.size() == 3) {
            values.emplace_back(item[0].get<float>(), item[1].get<float>(), item[2].get<float>());
        }
    }
    return values;
}

std::vector<unsigned int> jsonToIndexArray(const nlohmann::json& j) {
    std::vector<unsigned int> values;
    if (!j.is_array()) return values;
    values.reserve(j.size());
    for (const auto& item : j) {
        values.push_back(item.get<unsigned int>());
    }
    return values;
}

bool hasTerrainEditorTag(const Haruka::SceneObject& obj) {
    if (!obj.properties.is_object()) return false;
    if (!obj.properties.contains("terrainEditor")) return false;
    return obj.properties["terrainEditor"].is_object();
}

bool isChunkObject(const Haruka::SceneObject& obj) {
    if (!hasTerrainEditorTag(obj)) return false;
    const auto& te = obj.properties["terrainEditor"];
    return te.value("isChunk", false);
}

std::string getSourceName(const Haruka::SceneObject& obj, const std::string& fallback) {
    if (!hasTerrainEditorTag(obj)) return fallback;
    return obj.properties["terrainEditor"].value("source", fallback);
}
}

void PlanetTerrainEditorPanel::setScene(Haruka::Scene* scene) {
    currentScene = scene;
    if (currentScene) {
        loadChunkDeltas();
    }
}

void PlanetTerrainEditorPanel::setTargetObjectName(const std::string& name) {
    std::snprintf(targetObjectName, sizeof(targetObjectName), "%s", name.c_str());
}

void PlanetTerrainEditorPanel::setSelectedChunkId(int chunkId) {
    selectedChunkId = chunkId;
    refreshSelectedChunkSeed();
}

void PlanetTerrainEditorPanel::refreshSelectedChunkSeed() {
    if (!currentScene) return;
    std::string sourceName = targetObjectName;
    for (const auto& obj : currentScene->getObjects()) {
        if (!isChunkObject(obj)) continue;
        if (getSourceName(obj, "") != sourceName) continue;
        int chunkId = obj.properties["terrainEditor"].value("chunkId", -1);
        if (chunkId == selectedChunkId) {
            selectedChunkSeed = obj.properties["terrainEditor"].value("chunkSeed", generationSeed + chunkId * 7919);
            return;
        }
    }
}

void PlanetTerrainEditorPanel::onImGuiRender() {
    ImGui::Begin("Planet Terrain Editor");
    try {
        ImGui::Text("Planet sandbox workflow: target -> generate layers -> chunk -> edit -> save");

        if (ImGui::CollapsingHeader("1) Target & Main Actions", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::InputText("Target Object", targetObjectName, sizeof(targetObjectName));

            if (ImGui::Button("Apply Planet Seed (Regenerate)")) {
                applySeedToPlanet();
            }
            ImGui::SameLine();
            if (ImGui::Button("Instantiate Planet")) {
                splitPlanetIntoChunks();
            }
            ImGui::SameLine();
            if (ImGui::Button("Restore Source")) {
                restoreSourcePlanet();
            }

            Haruka::SceneObject* sourceObj = currentScene ? currentScene->getObject(targetObjectName) : nullptr;
            if (sourceObj && sourceObj->meshRenderer) {
                const auto& sv = sourceObj->meshRenderer->getSourceVertices();
                float minR = std::numeric_limits<float>::max();
                float maxR = std::numeric_limits<float>::lowest();
                for (const auto& v : sv) {
                    float r = glm::length(v);
                    minR = std::min(minR, r);
                    maxR = std::max(maxR, r);
                }
                ImGui::Text("Source mesh: %zu verts | r[min=%.5f max=%.5f]", sv.size(), minR, maxR);
            } else {
                ImGui::TextDisabled("Source mesh: unavailable");
            }
        }

        if (ImGui::CollapsingHeader("2) Planet Generator Layers", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::InputInt("Seed", &generationSeed);
            ImGui::SameLine();
            if (ImGui::Button("Randomize Seed")) {
                static std::mt19937 rng{std::random_device{}()};
                generationSeed = static_cast<int>(rng());
            }

            ImGui::DragFloat("Base Radius (km)", &baseRadiusKm, 1.0f, 1.0f, 1000000.0f, "%.1f");
            ImGui::DragFloat("Base Negative Depth (km)", &baseNegativeDepthKm, 0.1f, 0.0f, 1000.0f, "%.2f");
            ImGui::DragFloat("Relief (km)", &reliefKm, 0.1f, 0.0f, 1000.0f, "%.2f");

            ImGui::Checkbox("Enable Continents", &enableContinents);
            ImGui::SameLine();
            ImGui::Checkbox("Enable Mountains", &enableMountains);
            ImGui::Checkbox("Use GPU Generation", &useGPUGeneration);
            ImGui::Checkbox("Ignore Safety Limits (Unsafe)", &ignoreSafetyLimits);
            if (useGPUGeneration) {
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Compute shader generation enabled");
            } else {
                ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "CPU generation (slower)");
            }
            if (ignoreSafetyLimits) {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Unsafe mode ON: editor may freeze/crash on huge meshes.");
            }

            if (enableContinents) {
                ImGui::DragFloat("Continent Frequency", &continentFrequency, 0.01f, 0.01f, 10.0f, "%.3f");
            }
            if (enableMountains) {
                ImGui::DragFloat("Detail Frequency", &detailFrequency, 0.01f, 0.01f, 50.0f, "%.3f");
            }

            const int maxSubdivUI = ignoreSafetyLimits ? 600 : 8;
            ImGui::SliderInt("Mesh Subdivisions", &meshSubdivisions, 4, maxSubdivUI);
            meshSubdivisions = std::clamp(meshSubdivisions, 1, maxSubdivUI);

            // Estimación visual con tope para evitar overflow numérico en UI
            const int estimateSubdivisions = std::min(meshSubdivisions, 30);
            const double gridSize = std::pow(2.0, (double)estimateSubdivisions);
            const double estimatedTriangles = 12.0 * gridSize * gridSize;
            const double estimatedVertices = 6.0 * (gridSize + 1.0) * (gridSize + 1.0);
            const double approxSpacingKm = (static_cast<double>(kPi) * std::max(1.0f, baseRadiusKm)) / (2.0 * gridSize);
            ImGui::Text("Estimated base mesh: %.0f verts | %.0f tris", estimatedVertices, estimatedTriangles);
            ImGui::Text("Approx vertex spacing: %.3f km", approxSpacingKm);
            if (meshSubdivisions > estimateSubdivisions) {
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Estimate capped at subdivisions=%d (requested=%d).", estimateSubdivisions, meshSubdivisions);
            }
            if (!ignoreSafetyLimits && (estimatedVertices > kSafeMaxVertices || estimatedTriangles > kSafeMaxTriangles)) {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Unsafe mesh budget for editor: lower subdivisions.");
            } else if (meshSubdivisions >= 8) {
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "High density mode: can stutter on chunk split.");
            }
            ImGui::Text("Layer state: baseNegative=%.1f km | continents=%s | mountains=%s",
                        baseNegativeDepthKm,
                        enableContinents ? "ON" : "OFF",
                        enableMountains ? "ON" : "OFF");
        }

        if (ImGui::CollapsingHeader("3) Chunk Layout", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderInt("Lat Sections", &latSections, 2, 64);
            ImGui::SliderInt("Lon Sections", &lonSections, 2, 128);
            ImGui::Checkbox("Hide source after chunking", &hideSourceAfterChunking);
            ImGui::Checkbox("Copy material to chunks", &copyMaterial);
            ImGui::Checkbox("Visualize chunk sections", &visualizeChunkSections);
        }

        if (ImGui::CollapsingHeader("4) Chunk Seed + Sculpt", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::InputInt("Chunk ID", &selectedChunkId)) {
                refreshSelectedChunkSeed();
            }
            ImGui::InputInt("Chunk Seed", &selectedChunkSeed);
            if (ImGui::Button("Apply Chunk Seed")) {
                applySeedToChunk(selectedChunkId, selectedChunkSeed);
            }

            ImGui::InputFloat3("Brush Direction", brushDir);
            ImGui::SliderFloat("Brush Angle (deg)", &brushAngleDeg, 1.0f, 90.0f);
            ImGui::DragFloat("Delta (km)", &brushDeltaKm, 0.05f, 0.001f, 50.0f, "%.3f");
            if (ImGui::Button("Raise Chunk Brush")) {
                applyBrushToChunk(true);
            }
            ImGui::SameLine();
            if (ImGui::Button("Lower Chunk Brush")) {
                applyBrushToChunk(false);
            }
        }

        if (ImGui::CollapsingHeader("5) Persistence", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Auto-save chunk deltas", &autoSaveChunks);
            ImGui::Checkbox("Save only dirty chunks", &saveOnlyDirtyChunks);

            if (ImGui::Button("Save Chunk Deltas")) {
                saveChunkDeltas();
            }
            ImGui::SameLine();
            if (ImGui::Button("Load Chunk Deltas")) {
                loadChunkDeltas();
            }
            ImGui::SameLine();
            if (ImGui::Button("Save Dirty")) {
                bool prev = saveOnlyDirtyChunks;
                saveOnlyDirtyChunks = true;
                saveChunkDeltas();
                saveOnlyDirtyChunks = prev;
            }
        }

        if (!lastStatus.empty()) {
            ImGui::Separator();
            ImGui::TextWrapped("%s", lastStatus.c_str());
            if (lastChunkCount > 0) {
                ImGui::Text("Chunks created: %d", lastChunkCount);
            }
            if (lastModifiedChunkCount > 0) {
                ImGui::Text("Chunks with saved deltas: %d", lastModifiedChunkCount);
            }
        }
    } catch (const std::exception& e) {
        lastStatus = std::string("PlanetTerrainEditor error: ") + e.what();
    } catch (...) {
        lastStatus = "PlanetTerrainEditor unknown error";
    }
    ImGui::End();
}

std::string PlanetTerrainEditorPanel::getDeltaFilePath(const std::string& sourceName) const {
    std::string sceneName = currentScene ? currentScene->getName() : "untitled";
    fs::path base = fs::path("terrain_chunks") / sceneName;
    fs::create_directories(base);
    return (base / (sourceName + ".json")).string();
}

nlohmann::json PlanetTerrainEditorPanel::serializeChunkDelta(const ChunkDelta& delta) const {
    nlohmann::json j;
    j["chunkId"] = delta.chunkId;
    j["vertices"] = vec3ArrayToJson(delta.vertices);
    j["normals"] = vec3ArrayToJson(delta.normals);
    j["indices"] = delta.indices;
    return j;
}

PlanetTerrainEditorPanel::ChunkDelta PlanetTerrainEditorPanel::deserializeChunkDelta(const nlohmann::json& j) const {
    ChunkDelta delta;
    delta.chunkId = j.value("chunkId", -1);
    if (j.contains("vertices")) delta.vertices = jsonToVec3Array(j["vertices"]);
    if (j.contains("normals")) delta.normals = jsonToVec3Array(j["normals"]);
    if (j.contains("indices")) delta.indices = jsonToIndexArray(j["indices"]);
    return delta;
}

void PlanetTerrainEditorPanel::applyChunkDelta(Haruka::SceneObject& chunkObj, const ChunkDelta& delta) const {
    if (!chunkObj.meshRenderer) {
        chunkObj.meshRenderer = std::make_shared<MeshRendererComponent>();
    }
    if (delta.vertices.empty() || delta.indices.empty()) return;
    chunkObj.meshRenderer->setMesh(delta.vertices, delta.normals, delta.indices);
}

void PlanetTerrainEditorPanel::saveChunkDeltas() {
    lastModifiedChunkCount = 0;
    if (!currentScene) {
        lastStatus = "No active scene.";
        return;
    }

    std::string sourceName = targetObjectName;
    if (sourceName.empty()) {
        lastStatus = "Target object name is empty.";
        return;
    }

    nlohmann::json root;
    root["source"] = sourceName;
    root["scene"] = currentScene->getName();
    root["latSections"] = latSections;
    root["lonSections"] = lonSections;
    root["chunks"] = nlohmann::json::array();

    for (const auto& obj : currentScene->getObjects()) {
        if (!isChunkObject(obj)) continue;
        if (getSourceName(obj, "") != sourceName) continue;
        if (!obj.meshRenderer) continue;

        ChunkDelta delta;
        delta.chunkId = obj.properties["terrainEditor"].value("chunkId", -1);
        delta.vertices = obj.meshRenderer->getSourceVertices();
        delta.normals = obj.meshRenderer->getSourceNormals();
        delta.indices = obj.meshRenderer->getSourceIndices();

        if (delta.chunkId < 0 || delta.vertices.empty() || delta.indices.empty()) continue;
        if (saveOnlyDirtyChunks && !dirtyChunks.empty() && dirtyChunks.find(delta.chunkId) == dirtyChunks.end()) {
            continue;
        }
        chunkDeltas[delta.chunkId] = delta;
        root["chunks"].push_back(serializeChunkDelta(delta));
        lastModifiedChunkCount++;
    }

    std::ofstream out(getDeltaFilePath(sourceName), std::ios::trunc);
    if (!out.is_open()) {
        lastStatus = "Failed to open chunk delta file for writing.";
        return;
    }
    out << root.dump(4);
    lastStatus = "Chunk deltas saved.";
    if (saveOnlyDirtyChunks) {
        dirtyChunks.clear();
    }
}

void PlanetTerrainEditorPanel::loadChunkDeltas() {
    lastModifiedChunkCount = 0;
    if (!currentScene) {
        lastStatus = "No active scene.";
        return;
    }

    std::string sourceName = targetObjectName;
    if (sourceName.empty()) {
        lastStatus = "Target object name is empty.";
        return;
    }

    std::ifstream in(getDeltaFilePath(sourceName));
    if (!in.is_open()) {
        lastStatus = "No saved chunk deltas found yet.";
        return;
    }

    nlohmann::json root;
    in >> root;

    chunkDeltas.clear();
    dirtyChunks.clear();
    if (root.contains("chunks") && root["chunks"].is_array()) {
        for (const auto& entry : root["chunks"]) {
            ChunkDelta delta = deserializeChunkDelta(entry);
            if (delta.chunkId >= 0 && !delta.vertices.empty() && !delta.indices.empty()) {
                chunkDeltas[delta.chunkId] = delta;
            }
        }
    }

    for (auto& obj : currentScene->getObjectsMutable()) {
        if (!isChunkObject(obj)) continue;
        if (getSourceName(obj, "") != sourceName) continue;
        int chunkId = obj.properties["terrainEditor"].value("chunkId", -1);
        auto it = chunkDeltas.find(chunkId);
        if (it != chunkDeltas.end()) {
            applyChunkDelta(obj, it->second);
            lastModifiedChunkCount++;
        }
    }

    lastStatus = "Chunk deltas loaded and applied.";
}

void PlanetTerrainEditorPanel::splitPlanetIntoChunks() {
    lastChunkCount = 0;
    if (!currentScene) {
        lastStatus = "No active scene.";
        return;
    }

    std::string sourceName = targetObjectName;
    if (sourceName.empty()) {
        lastStatus = "Target object name is empty.";
        return;
    }

    Haruka::SceneObject* source = currentScene->getObject(sourceName);
    if (!source) {
        lastStatus = "Target object not found in scene.";
        return;
    }
    if (!source->meshRenderer) {
        source->meshRenderer = std::make_shared<MeshRendererComponent>();
    }

    latSections = std::clamp(latSections, 2, 128);
    lonSections = std::clamp(lonSections, 2, 256);
    meshSubdivisions = std::clamp(meshSubdivisions, 1, ignoreSafetyLimits ? 600 : 9);
    // El algoritmo base usa (1 << subdivisions) con int; proteger overflow/UB.
    const int effectiveSubdivisions = std::min(meshSubdivisions, 30);

    const double gridSize = std::pow(2.0, (double)effectiveSubdivisions);
    const double estimatedTriangles = 12.0 * gridSize * gridSize;
    const double estimatedVertices = 6.0 * (gridSize + 1.0) * (gridSize + 1.0);
    if (!ignoreSafetyLimits && (estimatedVertices > kSafeMaxVertices || estimatedTriangles > kSafeMaxTriangles)) {
        lastStatus = "Unsafe mesh budget for editor chunking. Lower Mesh Subdivisions.";
        return;
    }

    // Instanciar terreno procedural del planeta ANTES de dividir en chunks
    {
        auto cfg = Haruka::PlanetGenerator::getPresetConfig(Haruka::PlanetGenerator::PlanetPreset::EARTH_LIKE);
        cfg.radius = 1.0f;
        cfg.subdivisions = effectiveSubdivisions;
        cfg.baseRadiusKm = baseRadiusKm;
        cfg.baseNegativeDepthKm = baseNegativeDepthKm;
        cfg.seedBase = generationSeed;
        cfg.seedContinents = generationSeed + 100;
        cfg.seedMacro = generationSeed + 200;
        cfg.seedDetail = generationSeed + 300;
        cfg.enableContinents = enableContinents;
        cfg.enableMountains = enableMountains;
        cfg.baseNegativeDepthKm = baseNegativeDepthKm;
        cfg.continentFrequency = continentFrequency;
        cfg.detailFrequency = detailFrequency;
        cfg.useGPU = useGPUGeneration;

        // Conectar reliefKm al generador (antes no se usaba en la ruta global)
        const float reliefRatio = std::clamp(reliefKm / std::max(1.0f, baseRadiusKm), 0.0f, 0.45f);
        cfg.continentHeightStrength = enableContinents ? (reliefRatio * 0.35f) : 0.0f;
        cfg.macroHeightStrength = enableMountains ? (reliefRatio * 0.45f) : 0.0f;
        cfg.detailHeightStrength = enableMountains ? (reliefRatio * 0.20f) : 0.0f;

        auto data = Haruka::PlanetGenerator::generatePlanet(cfg);
        if (!data.vertices.empty() && !data.indices.empty()) {
            // Normalizar a un rango objetivo para que el relieve SIEMPRE se aplique de forma visible.
            float currentMinRatio = std::numeric_limits<float>::max();
            float currentMaxRatio = std::numeric_limits<float>::lowest();
            for (const auto& v : data.vertices) {
                float r = glm::length(v);
                currentMinRatio = std::min(currentMinRatio, r);
                currentMaxRatio = std::max(currentMaxRatio, r);
            }

            const float desiredMinRatio = std::max(0.2f, 1.0f - (std::max(0.0f, baseNegativeDepthKm) / std::max(1.0f, baseRadiusKm)));
            const float desiredMaxRatio = desiredMinRatio + (std::max(0.0f, reliefKm) / std::max(1.0f, baseRadiusKm));
            const float currentRange = std::max(1e-6f, currentMaxRatio - currentMinRatio);

            for (auto& v : data.vertices) {
                float len = glm::length(v);
                float t = (len - currentMinRatio) / currentRange;
                float newLen = desiredMinRatio + t * (desiredMaxRatio - desiredMinRatio);
                v = glm::normalize(v) * newLen;
            }

            data.normals.clear();
            data.normals.reserve(data.vertices.size());
            for (const auto& v : data.vertices) {
                data.normals.push_back(glm::normalize(v));
            }

            source->meshRenderer->setMesh(data.vertices, data.normals, data.indices);
            source->scale = glm::dvec3(std::max(1.0f, baseRadiusKm));

            float minR = std::numeric_limits<float>::max();
            float maxR = std::numeric_limits<float>::lowest();
            for (const auto& v : data.vertices) {
                float r = glm::length(v) * baseRadiusKm;
                minR = std::min(minR, r);
                maxR = std::max(maxR, r);
            }

            double meshSignature = 0.0;
            const size_t sigCount = std::min<size_t>(data.vertices.size(), 64);
            for (size_t i = 0; i < sigCount; ++i) {
                const auto& v = data.vertices[i];
                meshSignature += static_cast<double>(v.x) * 13.0 + static_cast<double>(v.y) * 17.0 + static_cast<double>(v.z) * 19.0;
            }

            lastStatus = std::string("Generated planet layers: base=" ) + std::to_string(baseNegativeDepthKm)
                + "km, relief=" + std::to_string(reliefKm)
                + "km, radius range=[" + std::to_string(minR) + ", " + std::to_string(maxR) + "] km"
                + " | seed=" + std::to_string(generationSeed)
                + " | subdiv(req/eff)=" + std::to_string(meshSubdivisions) + "/" + std::to_string(effectiveSubdivisions)
                + " | sig=" + std::to_string(meshSignature)
                + " | backend=" + (useGPUGeneration ? std::string("GPU (fallback CPU if needed)") : std::string("CPU"));
        } else {
            lastStatus = "Planet generation produced empty mesh (GPU/CPU path failed).";
            return;
        }
    }

    const auto& verts = source->meshRenderer->getSourceVertices();
    const auto& normals = source->meshRenderer->getSourceNormals();
    const auto& indices = source->meshRenderer->getSourceIndices();

    if (verts.empty() || indices.size() < 3) {
        lastStatus = "Mesh source data is empty. Regenerate or reload mesh first.";
        return;
    }

    int sourceIndex = -1;
    auto& sceneObjects = currentScene->getObjectsMutable();
    for (int i = 0; i < (int)sceneObjects.size(); ++i) {
        if (sceneObjects[i].name == sourceName) {
            sourceIndex = i;
            break;
        }
    }
    if (sourceIndex < 0) {
        lastStatus = "Target object disappeared while preparing chunks.";
        return;
    }

    const std::string sourceType = sceneObjects[sourceIndex].type;
    const glm::dvec3 sourceColor = sceneObjects[sourceIndex].color;
    const double sourceIntensity = sceneObjects[sourceIndex].intensity;
    std::shared_ptr<Haruka::MaterialComponent> sourceMaterial = sceneObjects[sourceIndex].material;

    struct ChunkData {
        std::vector<glm::vec3> localVerts;
        std::vector<glm::vec3> localNormals;
        std::vector<unsigned int> localIndices;
        std::unordered_map<unsigned int, unsigned int> remap;
        int latBin = 0;
        int lonBin = 0;
    };

    std::map<int, ChunkData> chunks; // orden estable por chunkId

    auto getChunkIdFromDir = [&](const glm::vec3& dir) {
        glm::vec3 d = glm::normalize(dir);
        float lat = std::asin(std::clamp(d.y, -1.0f, 1.0f));
        float lon = std::atan2(d.z, d.x);

        int latBin = static_cast<int>(((lat + kPi * 0.5f) / kPi) * latSections);
        int lonBin = static_cast<int>(((lon + kPi) / (2.0f * kPi)) * lonSections);
        latBin = std::clamp(latBin, 0, latSections - 1);
        lonBin = std::clamp(lonBin, 0, lonSections - 1);
        return latBin * lonSections + lonBin;
    };

    auto decodeChunkId = [&](int chunkId, int& outLat, int& outLon) {
        outLat = std::clamp(chunkId / lonSections, 0, latSections - 1);
        outLon = std::clamp(chunkId % lonSections, 0, lonSections - 1);
    };

    auto remapVertex = [&](ChunkData& chunk, unsigned int oldIndex) -> unsigned int {
        auto it = chunk.remap.find(oldIndex);
        if (it != chunk.remap.end()) return it->second;

        unsigned int newIndex = static_cast<unsigned int>(chunk.localVerts.size());
        chunk.remap[oldIndex] = newIndex;
        chunk.localVerts.push_back(verts[oldIndex]);
        if (!normals.empty() && oldIndex < normals.size()) {
            chunk.localNormals.push_back(normals[oldIndex]);
        } else {
            chunk.localNormals.push_back(glm::normalize(verts[oldIndex]));
        }
        return newIndex;
    };

    size_t invalidTriangles = 0;
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        unsigned int i0 = indices[i + 0];
        unsigned int i1 = indices[i + 1];
        unsigned int i2 = indices[i + 2];
        if (i0 >= verts.size() || i1 >= verts.size() || i2 >= verts.size()) {
            invalidTriangles++;
            continue;
        }

        glm::vec3 c = (verts[i0] + verts[i1] + verts[i2]) * (1.0f / 3.0f);
        int chunkId = getChunkIdFromDir(c);
        ChunkData& chunk = chunks[chunkId];
        decodeChunkId(chunkId, chunk.latBin, chunk.lonBin);

        unsigned int n0 = remapVertex(chunk, i0);
        unsigned int n1 = remapVertex(chunk, i1);
        unsigned int n2 = remapVertex(chunk, i2);

        chunk.localIndices.push_back(n0);
        chunk.localIndices.push_back(n1);
        chunk.localIndices.push_back(n2);
    }

    // Remove stale chunks (those no longer expected for current split config)
    std::vector<std::string> staleChunks;
    for (const auto& obj : currentScene->getObjects()) {
        if (!isChunkObject(obj)) continue;
        if (getSourceName(obj, "") != sourceName) continue;
        int oldChunkId = obj.properties["terrainEditor"].value("chunkId", -1);
        if (chunks.find(oldChunkId) == chunks.end()) {
            staleChunks.push_back(obj.name);
        }
    }
    for (const auto& staleName : staleChunks) {
        currentScene->removeObject(staleName);
    }

    // Recompute source index after removals (vector indices may shift)
    sourceIndex = -1;
    {
        auto& objs = currentScene->getObjectsMutable();
        for (int i = 0; i < (int)objs.size(); ++i) {
            if (objs[i].name == sourceName) {
                sourceIndex = i;
                break;
            }
        }
    }
    if (sourceIndex < 0) {
        lastStatus = "Target object disappeared after stale chunk cleanup.";
        return;
    }

    int created = 0;
    int updated = 0;
    for (auto& [chunkId, data] : chunks) {
        if (data.localIndices.empty()) continue;

        const std::string chunkName = sourceName + "_chunk_" + std::to_string(chunkId);
        Haruka::SceneObject* existing = currentScene->getObject(chunkName);
        Haruka::SceneObject* chunkObjPtr = existing;

        if (!chunkObjPtr) {
            Haruka::SceneObject chunkObj;
            chunkObj.name = chunkName;
            chunkObj.type = sourceType;
            chunkObj.modelPath = "";
            chunkObj.position = glm::dvec3(0.0);
            chunkObj.rotation = glm::dvec3(0.0);
            chunkObj.scale = glm::dvec3(1.0);
            chunkObj.color = sourceColor;
            chunkObj.intensity = sourceIntensity;
            chunkObj.renderLayer = 1;
            chunkObj.parentIndex = sourceIndex;
            if (copyMaterial) {
                chunkObj.material = sourceMaterial;
            }
            chunkObj.meshRenderer = std::make_shared<MeshRendererComponent>();
            currentScene->addObject(chunkObj);
            chunkObjPtr = currentScene->getObject(chunkName);
            created++;
        } else {
            updated++;
        }

        if (!chunkObjPtr) continue;
        if (!chunkObjPtr->meshRenderer) {
            chunkObjPtr->meshRenderer = std::make_shared<MeshRendererComponent>();
        }

        chunkObjPtr->type = sourceType;
        chunkObjPtr->modelPath.clear();
        chunkObjPtr->position = glm::dvec3(0.0);
        chunkObjPtr->rotation = glm::dvec3(0.0);
        chunkObjPtr->scale = glm::dvec3(1.0);
        chunkObjPtr->color = sourceColor;
        chunkObjPtr->intensity = sourceIntensity;
        chunkObjPtr->renderLayer = 1;
        chunkObjPtr->parentIndex = sourceIndex;
        if (copyMaterial) {
            chunkObjPtr->material = sourceMaterial;
        }

        chunkObjPtr->meshRenderer->setMesh(data.localVerts, data.localNormals, data.localIndices);

        chunkObjPtr->properties["terrainEditor"]["isChunk"] = true;
        chunkObjPtr->properties["terrainEditor"]["source"] = sourceName;
        chunkObjPtr->properties["terrainEditor"]["chunkId"] = chunkId;
        chunkObjPtr->properties["terrainEditor"]["latBin"] = data.latBin;
        chunkObjPtr->properties["terrainEditor"]["lonBin"] = data.lonBin;
        int chunkSeed = generationSeed + chunkId * 7919;
        chunkObjPtr->properties["terrainEditor"]["chunkSeed"] = chunkSeed;
        chunkObjPtr->properties["terrainEditor"]["latSections"] = latSections;
        chunkObjPtr->properties["terrainEditor"]["lonSections"] = lonSections;
        chunkObjPtr->properties["terrainEditor"]["isInstancedTerrainChunk"] = true;
        if (visualizeChunkSections) {
            chunkObjPtr->properties["terrainEditor"]["visualizeSections"] = true;
        }
    }

    if (Haruka::SceneObject* source = currentScene->getObject(sourceName)) {
        source->properties["terrainEditor"]["disableRender"] = hideSourceAfterChunking;
        source->properties["terrainEditor"]["chunked"] = true;
        source->properties["terrainEditor"]["latSections"] = latSections;
        source->properties["terrainEditor"]["lonSections"] = lonSections;
        source->properties["terrainEditor"]["isInstancedTerrainSource"] = true;
        source->properties["terrainEditor"]["generator"]["seed"] = generationSeed;
        source->properties["terrainEditor"]["generator"]["baseRadiusKm"] = baseRadiusKm;
        source->properties["terrainEditor"]["generator"]["reliefKm"] = reliefKm;
        source->properties["terrainEditor"]["generator"]["continentFrequency"] = continentFrequency;
        source->properties["terrainEditor"]["generator"]["detailFrequency"] = detailFrequency;

        // Rebuild jerarquía fuente -> chunks (índices consistentes tras inserciones/eliminaciones)
        source->childrenIndices.clear();
        const auto& objs = currentScene->getObjects();
        for (int i = 0; i < (int)objs.size(); ++i) {
            const auto& o = objs[i];
            if (!isChunkObject(o)) continue;
            if (getSourceName(o, "") != sourceName) continue;
            source->childrenIndices.push_back(i);
        }
    }

    lastChunkCount = created + updated;
    std::string statusMsg = (created + updated) > 0
        ? ("Chunk generation complete. created=" + std::to_string(created) + " updated=" + std::to_string(updated))
        : "No chunks were generated (check mesh data).";
    if (invalidTriangles > 0) {
        statusMsg += " [WARNING: " + std::to_string(invalidTriangles) + " invalid triangles skipped (bad indices from GPU/CPU)]";    
    }
    lastStatus = statusMsg;

    if (autoSaveChunks) {
        saveChunkDeltas();
    }
}

void PlanetTerrainEditorPanel::restoreSourcePlanet() {
    lastChunkCount = 0;
    if (!currentScene) {
        lastStatus = "No active scene.";
        return;
    }

    std::string sourceName = targetObjectName;
    if (sourceName.empty()) {
        lastStatus = "Target object name is empty.";
        return;
    }

    std::vector<std::string> toRemove;
    std::string prefix = sourceName + "_chunk_";
    for (const auto& obj : currentScene->getObjects()) {
        if (obj.name.rfind(prefix, 0) == 0) {
            toRemove.push_back(obj.name);
        }
    }
    for (const auto& name : toRemove) {
        currentScene->removeObject(name);
    }

    if (Haruka::SceneObject* source = currentScene->getObject(sourceName)) {
        source->childrenIndices.clear();
        source->properties["terrainEditor"]["disableRender"] = false;
        source->properties["terrainEditor"]["chunked"] = false;
    }

    lastStatus = "Source restored and generated chunks removed.";
}

void PlanetTerrainEditorPanel::applyBrushToChunk(bool raise) {
    if (!currentScene) {
        lastStatus = "No active scene.";
        return;
    }

    std::string sourceName = targetObjectName;
    if (sourceName.empty()) {
        lastStatus = "Target object name is empty.";
        return;
    }

    Haruka::SceneObject* targetChunk = nullptr;
    for (auto& obj : currentScene->getObjectsMutable()) {
        if (!isChunkObject(obj)) continue;
        if (getSourceName(obj, "") != sourceName) continue;
        int chunkId = obj.properties["terrainEditor"].value("chunkId", -1);
        if (chunkId == selectedChunkId) {
            targetChunk = &obj;
            break;
        }
    }

    if (!targetChunk || !targetChunk->meshRenderer) {
        lastStatus = "Chunk not found or without mesh.";
        return;
    }

    auto verts = targetChunk->meshRenderer->getSourceVertices();
    auto inds = targetChunk->meshRenderer->getSourceIndices();
    if (verts.empty() || inds.empty()) {
        lastStatus = "Chunk mesh source data is empty.";
        return;
    }

    glm::vec3 centerDir(brushDir[0], brushDir[1], brushDir[2]);
    float len = glm::length(centerDir);
    if (len < 1e-6f) {
        lastStatus = "Brush direction must be non-zero.";
        return;
    }
    centerDir /= len;

    float cosThreshold = std::cos(glm::radians(std::clamp(brushAngleDeg, 0.1f, 179.0f)));
    float planetRadiusKm = 1.0f;
    if (Haruka::SceneObject* sourceObj = currentScene->getObject(sourceName)) {
        planetRadiusKm = std::max(std::abs((float)sourceObj->scale.x), std::max(std::abs((float)sourceObj->scale.y), std::abs((float)sourceObj->scale.z)));
    }
    if (planetRadiusKm < 1e-6f) planetRadiusKm = 1.0f;
    float deltaNorm = std::max(0.00001f, brushDeltaKm / planetRadiusKm);
    if (!raise) deltaNorm = -deltaNorm;

    int touched = 0;
    for (auto& v : verts) {
        glm::vec3 d = glm::normalize(v);
        if (glm::dot(d, centerDir) >= cosThreshold) {
            v += d * deltaNorm;
            touched++;
        }
    }

    if (touched == 0) {
        lastStatus = "Brush touched 0 vertices (adjust angle/direction).";
        return;
    }

    std::vector<glm::vec3> norms;
    norms.reserve(verts.size());
    for (const auto& v : verts) {
        norms.push_back(glm::normalize(v));
    }

    targetChunk->meshRenderer->setMesh(verts, norms, inds);
    int chunkId = targetChunk->properties["terrainEditor"].value("chunkId", -1);
    if (chunkId >= 0) dirtyChunks.insert(chunkId);

    lastStatus = std::string("Brush applied to chunk ") + std::to_string(chunkId) + ", vertices touched: " + std::to_string(touched);
}

void PlanetTerrainEditorPanel::applySeedToChunk(int chunkId, int chunkSeed) {
    if (!currentScene) {
        lastStatus = "No active scene.";
        return;
    }

    std::string sourceName = targetObjectName;
    if (sourceName.empty()) {
        lastStatus = "Target object name is empty.";
        return;
    }

    Haruka::SceneObject* targetChunk = nullptr;
    for (auto& obj : currentScene->getObjectsMutable()) {
        if (!isChunkObject(obj)) continue;
        if (getSourceName(obj, "") != sourceName) continue;
        if (obj.properties["terrainEditor"].value("chunkId", -1) == chunkId) {
            targetChunk = &obj;
            break;
        }
    }

    if (!targetChunk || !targetChunk->meshRenderer) {
        lastStatus = "Chunk not found for seed apply.";
        return;
    }

    auto verts = targetChunk->meshRenderer->getSourceVertices();
    auto inds = targetChunk->meshRenderer->getSourceIndices();
    if (verts.empty() || inds.empty()) {
        lastStatus = "Chunk mesh source data is empty.";
        return;
    }

    const float reliefRatio = std::clamp(reliefKm / std::max(1.0f, baseRadiusKm), 0.0f, 0.45f);
    const float continentHeightStrength = reliefRatio * 0.35f;
    const float macroHeightStrength = reliefRatio * 0.45f;
    const float detailHeightStrength = reliefRatio * 0.20f;
    const float baseNegativeRatio = std::max(0.0f, baseNegativeDepthKm) / std::max(1.0f, baseRadiusKm);

    const float seaLevel = 0.52f;
    const float continentWarpStrength = 0.14f;
    const float macroFrequency = 3.2f;
    const float persistence = 0.5f;
    const float lacunarity = 2.0f;

    int touched = 0;
    for (auto& v : verts) {
        glm::vec3 dir = glm::normalize(v);

        // Capa 1: base negativa
        float baseDelta = -baseNegativeRatio;

        // Capa 2: continentes
        float continentMask = 0.0f;
        float coastMask = 1.0f;
        float continentHeight = 0.0f;
        if (enableContinents) {
            float wx = fBmHash(dir, chunkSeed + 11, 3, persistence, lacunarity, continentFrequency * 0.7f);
            float wy = fBmHash(dir, chunkSeed + 17, 3, persistence, lacunarity, continentFrequency * 0.7f);
            float wz = fBmHash(dir, chunkSeed + 23, 3, persistence, lacunarity, continentFrequency * 0.7f);
            glm::vec3 warp = glm::vec3(wx, wy, wz) * continentWarpStrength;
            glm::vec3 cpos = glm::normalize(dir + warp);

            float c = fBmHash(cpos, chunkSeed + 101, 4, persistence, lacunarity, continentFrequency);
            float c01 = (c + 1.0f) * 0.5f;
            float coastWidth = 0.22f;
            continentMask = smoothstepf(seaLevel - coastWidth, seaLevel + coastWidth, c01);
            float signedContinent = c01 - seaLevel;
            coastMask = smoothstepf(0.03f, 0.18f, std::abs(signedContinent));

            float landPart = std::max(0.0f, signedContinent);
            float oceanPart = std::min(0.0f, signedContinent) * 0.12f;
            continentHeight = (landPart + oceanPart) * continentHeightStrength;
            continentMask = smoothstepf(-0.10f, 0.10f, signedContinent);
        }

        // Capa 3: montañas + detalle
        float macroHeight = 0.0f;
        float detailHeight = 0.0f;
        if (enableMountains) {
            float macro = fBmHash(dir, chunkSeed + 201, 5, persistence, lacunarity, macroFrequency);
            float macro01 = (macro + 1.0f) * 0.5f;
            float mountainMask = smoothstepf(0.48f, 0.72f, macro01);
            float mountainRidge = 1.0f - std::abs(2.0f * macro01 - 1.0f);
            float landInfluence = enableContinents ? (0.20f + 0.80f * continentMask) : 1.0f;
            macroHeight = mountainMask * mountainRidge * macroHeightStrength * landInfluence * coastMask;

            float detail = fBmHash(dir, chunkSeed + 301, 4, persistence, lacunarity, detailFrequency);
            float detailSigned = detail * 0.5f + 0.5f;
            float detailMask = smoothstepf(0.35f, 0.80f, macro01);
            detailHeight = detailMask * detailSigned * detailHeightStrength * landInfluence * coastMask;
        }

        float totalDelta = baseDelta + continentHeight + macroHeight + detailHeight;
        float newLen = std::max(0.2f, 1.0f + totalDelta);
        v = dir * newLen;
        touched++;
    }

    std::vector<glm::vec3> norms;
    norms.reserve(verts.size());
    for (const auto& v : verts) norms.push_back(glm::normalize(v));

    targetChunk->meshRenderer->setMesh(verts, norms, inds);
    targetChunk->properties["terrainEditor"]["chunkSeed"] = chunkSeed;
    dirtyChunks.insert(chunkId);
    selectedChunkSeed = chunkSeed;
    lastStatus = std::string("Chunk seed applied: chunk ") + std::to_string(chunkId)
        + " seed " + std::to_string(chunkSeed)
        + " (layered model, vertices=" + std::to_string(touched) + ")";
}

void PlanetTerrainEditorPanel::applySeedToPlanet() {
    if (!currentScene) {
        lastStatus = "No active scene.";
        return;
    }
    if (std::string(targetObjectName).empty()) {
        lastStatus = "Target object name is empty.";
        return;
    }

    // Regeneración planetaria completa con semilla global (base + chunks)
    splitPlanetIntoChunks();
    refreshSelectedChunkSeed();
    // No sobreescribir estado de splitPlanetIntoChunks: ahí están errores/success reales.
}