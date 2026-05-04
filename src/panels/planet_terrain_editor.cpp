#include "planet_terrain_editor.h"

#include "core/components/mesh_renderer_component.h"
#include "game/planetary_system.h"
#include "imgui.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <chrono>
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
constexpr int kHardMaxSubdiv = 30;

int getInteractiveSubdivisionCap(int preset) {
    (void)preset;
    return kHardMaxSubdiv;
}

const char* getQualityPresetLabel(int preset) {
    switch (preset) { case 0: return "Preview"; case 2: return "High"; default: return "Balanced"; }
}

float getMaxContinentFrequencyForSize(float baseRadiusKm, float minContinentSizeKm) {
    const float R = std::max(1.0f, baseRadiusKm);
    const float L = std::max(1.0f, minContinentSizeKm);
    return (kPi * R) / L;
}

std::pair<int, int> recommendChunkGridFromTriangleBudget(int subdivisions, int targetTrianglesPerChunk) {
    const int s = std::clamp(subdivisions, 1, 30);
    const double g = std::pow(2.0, static_cast<double>(s));
    const double totalTriangles = 12.0 * g * g;
    const double target = std::max(1000.0, static_cast<double>(targetTrianglesPerChunk));
    const double chunkCount = std::max(1.0, std::ceil(totalTriangles / target));

    int lat = static_cast<int>(std::round(std::sqrt(chunkCount * 0.5)));
    lat = std::clamp(lat, 2, 128);
    int lon = std::clamp(lat * 2, 2, 256);
    return {lat, lon};
}

float smoothstepf(float edge0, float edge1, float x);
float fBmHash(const glm::vec3& p, int seed, int octaves, float persistence, float lacunarity, float frequency);

float computePlateDelta(const glm::vec3& dir, int planetSeed, float plateScale, float plateBoundarySharpness, float plateReliefRatio) {
    const float pA = fBmHash(dir, planetSeed + 401, 2, 0.55f, 2.0f, std::max(0.01f, plateScale));
    const float pB = fBmHash(dir, planetSeed + 457, 2, 0.50f, 2.0f, std::max(0.01f, plateScale * 1.7f));
    const float pSigned = pA * 0.70f + pB * 0.30f;
    const float p01 = (pSigned + 1.0f) * 0.5f;
    const float boundary = 1.0f - std::abs(2.0f * p01 - 1.0f);
    const float bSharp = std::clamp(plateBoundarySharpness, 0.05f, 0.95f);
    const float ridgeMask = smoothstepf(1.0f - bSharp, 1.0f, boundary);
    return (pSigned * 0.65f + ridgeMask * 0.35f) * plateReliefRatio;
}

float hashNoise(const glm::vec3& p, int seed) {
    float n = glm::dot(p, glm::vec3(12.9898f + seed * 0.001f, 78.233f + seed * 0.002f, 37.719f + seed * 0.003f));
    return (std::sin(n) * 43758.5453f - std::floor(std::sin(n) * 43758.5453f)) * 2.0f - 1.0f;
}

float saturatef(float v) { return std::clamp(v, 0.0f, 1.0f); }

float smoothstepf(float edge0, float edge1, float x) {
    float t = saturatef((x - edge0) / std::max(1e-6f, edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

float fBmHash(const glm::vec3& p, int seed, int octaves, float persistence, float lacunarity, float frequency) {
    float amp = 1.0f, total = 0.0f, norm = 0.0f;
    glm::vec3 pp = p * frequency;
    for (int i = 0; i < octaves; ++i) {
        total += hashNoise(pp, seed + i * 31) * amp;
        norm += amp;
        amp *= persistence;
        pp *= lacunarity;
    }
    return norm > 1e-6f ? (total / norm) : 0.0f;
}

std::vector<glm::vec3> computeSmoothNormals(const std::vector<glm::vec3>& verts, const std::vector<unsigned int>& inds) {
    std::vector<glm::vec3> norms(verts.size(), glm::vec3(0.0f));
    if (verts.empty() || inds.size() < 3) return norms;

    for (size_t i = 0; i + 2 < inds.size(); i += 3) {
        unsigned int i0 = inds[i + 0];
        unsigned int i1 = inds[i + 1];
        unsigned int i2 = inds[i + 2];
        if (i0 >= verts.size() || i1 >= verts.size() || i2 >= verts.size()) continue;

        const glm::vec3 e1 = verts[i1] - verts[i0];
        const glm::vec3 e2 = verts[i2] - verts[i0];
        const glm::vec3 fn = glm::cross(e1, e2);
        float lenSq = glm::dot(fn, fn);
        if (lenSq <= 1e-20f) continue;

        norms[i0] += fn;
        norms[i1] += fn;
        norms[i2] += fn;
    }

    for (size_t i = 0; i < norms.size(); ++i) {
        float lenSq = glm::dot(norms[i], norms[i]);
        if (lenSq > 1e-20f) {
            norms[i] = glm::normalize(norms[i]);
        } else {
            float vlen = glm::length(verts[i]);
            norms[i] = (vlen > 1e-6f) ? (verts[i] / vlen) : glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }
    return norms;
}

struct LayeredDeformParams {
    float reliefKm;
    float baseRadiusKm;
    float plateReliefKm;
    float plateScale;
    float plateBoundarySharpness;
    float minContinentSizeKm;
    float continentFrequency;
    float detailFrequency;
    int planetSeed;
    bool enableContinents;
    bool enableMountains;
};

int applyLayeredDeformation(std::vector<glm::vec3>& verts, int chunkSeed, const LayeredDeformParams& p) {
    const float reliefRatio = std::clamp(p.reliefKm / std::max(1.0f, p.baseRadiusKm), 0.0f, 0.45f);
    const float plateReliefRatio = std::clamp(p.plateReliefKm / std::max(1.0f, p.baseRadiusKm), 0.0f, 0.45f);
    const float continentHeightStrength = reliefRatio * 0.90f;
    const float macroHeightStrength = reliefRatio * 0.45f;
    const float detailHeightStrength = reliefRatio * 0.20f;
    const float maxContinentFreq = getMaxContinentFrequencyForSize(p.baseRadiusKm, p.minContinentSizeKm);
    const float continentFreqEff = std::min(std::max(0.01f, p.continentFrequency), std::max(0.01f, maxContinentFreq));
    const float minSizeRatio = std::clamp(p.minContinentSizeKm / std::max(1.0f, kPi * p.baseRadiusKm), 0.01f, 1.0f);

    const float seaLevel = 0.52f;
    const float continentWarpStrength = 0.14f;
    const float macroFrequency = 3.2f;
    const float persistence = 0.5f;
    const float lacunarity = 2.0f;

    int touched = 0;
    for (auto& v : verts) {
        glm::vec3 dir = glm::normalize(v);
        float baseDelta = computePlateDelta(dir, p.planetSeed, p.plateScale, p.plateBoundarySharpness, plateReliefRatio);

        float continentMask = 0.0f;
        float coastMask = 1.0f;
        float continentHeight = 0.0f;
        if (p.enableContinents) {
            float wx = fBmHash(dir, chunkSeed + 11, 3, persistence, lacunarity, continentFreqEff * 0.7f);
            float wy = fBmHash(dir, chunkSeed + 17, 3, persistence, lacunarity, continentFreqEff * 0.7f);
            float wz = fBmHash(dir, chunkSeed + 23, 3, persistence, lacunarity, continentFreqEff * 0.7f);
            glm::vec3 warp = glm::vec3(wx, wy, wz) * continentWarpStrength;
            glm::vec3 cpos = glm::normalize(dir + warp);

            float c = fBmHash(cpos, chunkSeed + 101, 4, persistence, lacunarity, continentFreqEff);
            float c01 = (c + 1.0f) * 0.5f;
            float coastWidth = 0.22f;
            continentMask = smoothstepf(seaLevel - coastWidth, seaLevel + coastWidth, c01);
            float signedContinent = c01 - seaLevel;
            coastMask = smoothstepf(0.03f, 0.18f, std::abs(signedContinent));

            float landPart = std::max(0.0f, signedContinent);
            float oceanPart = std::min(0.0f, signedContinent) * 0.12f;

            const float largeFreq = std::max(0.005f, continentFreqEff * (0.18f + 0.30f * (1.0f - minSizeRatio)));
            const float largeNoise = fBmHash(cpos, p.planetSeed + 701, 3, 0.55f, 2.0f, largeFreq);
            const float large01 = (largeNoise + 1.0f) * 0.5f;
            const float t0 = 0.36f + 0.30f * minSizeRatio;
            const float t1 = std::min(0.98f, t0 + 0.22f);
            const float largeMask = smoothstepf(t0, t1, large01);

            const float landBoost = std::pow(std::max(0.0f, landPart), 0.75f);
            landPart = landBoost * (0.45f + 0.55f * largeMask);
            continentHeight = (landPart + oceanPart) * continentHeightStrength;
            continentMask = smoothstepf(-0.10f, 0.10f, signedContinent) * (0.55f + 0.45f * largeMask);
        }

        float macroHeight = 0.0f;
        float detailHeight = 0.0f;
        if (p.enableMountains) {
            float macro = fBmHash(dir, chunkSeed + 201, 5, persistence, lacunarity, macroFrequency);
            float macro01 = (macro + 1.0f) * 0.5f;
            float mountainMask = smoothstepf(0.48f, 0.72f, macro01);
            float mountainRidge = 1.0f - std::abs(2.0f * macro01 - 1.0f);
            float landInfluence = p.enableContinents ? (0.20f + 0.80f * continentMask) : 1.0f;
            macroHeight = mountainMask * mountainRidge * macroHeightStrength * landInfluence * coastMask;

            float detail = fBmHash(dir, chunkSeed + 301, 4, persistence, lacunarity, p.detailFrequency);
            float detailSigned = detail * 0.5f + 0.5f;
            float detailMask = smoothstepf(0.35f, 0.80f, macro01);
            detailHeight = detailMask * detailSigned * detailHeightStrength * landInfluence * coastMask;
        }

        float totalDelta = baseDelta + continentHeight + macroHeight + detailHeight;
        float newLen = std::max(0.2f, 1.0f + totalDelta);
        v = dir * newLen;
        touched++;
    }
    return touched;
}

nlohmann::json vec3ArrayToJson(const std::vector<glm::vec3>& values) {
    nlohmann::json result = nlohmann::json::array();
    for (const auto& value : values) result.push_back({value.x, value.y, value.z});
    return result;
}

std::vector<glm::vec3> jsonToVec3Array(const nlohmann::json& j) {
    std::vector<glm::vec3> values;
    if (!j.is_array()) return values;
    values.reserve(j.size());
    for (const auto& item : j) {
        if (item.is_array() && item.size() == 3)
            values.emplace_back(item[0].get<float>(), item[1].get<float>(), item[2].get<float>());
    }
    return values;
}

std::vector<unsigned int> jsonToIndexArray(const nlohmann::json& j) {
    std::vector<unsigned int> values;
    if (!j.is_array()) return values;
    values.reserve(j.size());
    for (const auto& item : j) values.push_back(item.get<unsigned int>());
    return values;
}

bool hasTerrainEditorTag(const Haruka::SceneObject& obj) {
    if (!obj.properties.is_object()) return false;
    if (!obj.properties.contains("terrainEditor")) return false;
    return obj.properties["terrainEditor"].is_object();
}

bool isChunkObject(const Haruka::SceneObject& obj) {
    if (!hasTerrainEditorTag(obj)) return false;
    return obj.properties["terrainEditor"].value("isChunk", false);
}

std::string getSourceName(const Haruka::SceneObject& obj, const std::string& fallback) {
    if (!hasTerrainEditorTag(obj)) return fallback;
    return obj.properties["terrainEditor"].value("source", fallback);
}

class TerrainSplitTask final : public EditorTaskBase {
public:
    explicit TerrainSplitTask(PlanetTerrainEditorPanel* panel) : EditorTaskBase("Terrain Split"), owner(panel) {}
protected:
    bool onStart(std::string& error) override {
        if (!owner) { error = "Invalid panel owner."; return false; }
        owner->beginSplitPlanetIntoChunksTask();
        if (!owner->isSplitTaskRunning()) { error = owner->getLastStatus(); return false; }
        setProgress(owner->getSplitTaskProgress(), owner->getSplitTaskStatus());
        return true;
    }
    void onUpdate() override {
        if (!owner) { fail("Panel owner lost."); return; }
        owner->tickSplitPlanetIntoChunksTask();
        setProgress(owner->getSplitTaskProgress(), owner->getSplitTaskStatus());
        if (!owner->isSplitTaskRunning()) {
            if (owner->didSplitTaskComplete()) complete(owner->getLastStatus());
            else if (owner->wasSplitTaskCancelled()) markCancelled(owner->getLastStatus());
            else fail(owner->getLastStatus());
        }
    }
    void onCancel() override { if (owner) owner->cancelSplitPlanetIntoChunksTask(); }
private:
    PlanetTerrainEditorPanel* owner = nullptr;
};
}

void PlanetTerrainEditorPanel::setScene(Haruka::Scene* scene) { currentScene = scene; if (currentScene) loadChunkDeltas(); }
void PlanetTerrainEditorPanel::setTargetObjectName(const std::string& name) { std::snprintf(targetObjectName, sizeof(targetObjectName), "%s", name.c_str()); }
void PlanetTerrainEditorPanel::setSelectedChunkId(int chunkId) { selectedChunkId = chunkId; refreshSelectedChunkSeed(); }
void PlanetTerrainEditorPanel::update() { if (activeTask && activeTask->isRunning()) activeTask->update(); if (activeTask && activeTask->isDone()) activeTask.reset(); }

void PlanetTerrainEditorPanel::refreshSelectedChunkSeed() {
    if (!currentScene) return;
    std::string sourceName = targetObjectName;
    for (const auto& obj : currentScene->getObjects()) {
        if (!isChunkObject(obj) || getSourceName(obj, "") != sourceName) continue;
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
        ImGui::Text("Planet sandbox: target -> generate -> chunk -> edit -> save");
        
        // Auto-detect selected planet
        if (ImGui::Button("🔍 Use Selected Object")) {
            if (currentScene && !currentScene->getObjects().empty()) {
                // Get first selected or use current target
                ImGui::OpenPopup("SelectPlanetPopup");
            }
        }
        ImGui::SameLine();
        
        // Popup para seleccionar planeta
        if (ImGui::BeginPopupModal("SelectPlanetPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Select a planet/terrain source:");
            ImGui::Separator();
            
            if (currentScene) {
                for (const auto& obj : currentScene->getObjects()) {
                    // Mostrar objetos con meshRenderer que no sean chunks
                    if (!obj.meshRenderer || isChunkObject(obj)) continue;
                    
                    bool selected = (std::string(targetObjectName) == obj.name);
                    if (ImGui::Selectable(obj.name.c_str(), selected)) {
                        setTargetObjectName(obj.name);
                        
                        // Cargar parámetros del objeto seleccionado
                        if (obj.properties.contains("terrainEditor") && 
                            obj.properties["terrainEditor"].contains("generator")) {
                            const auto& gen = obj.properties["terrainEditor"]["generator"];
                            generationSeed = gen.value("seed", generationSeed);
                            lastSyncedSeed = generationSeed;  // Sincronizar tracking
                            baseRadiusKm = gen.value("baseRadiusKm", baseRadiusKm);
                            continentFrequency = gen.value("continentFrequency", continentFrequency);
                            detailFrequency = gen.value("detailFrequency", detailFrequency);
                            enableContinents = gen.value("enableContinents", enableContinents);
                            enableMountains = gen.value("enableMountains", enableMountains);
                        }
                        
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            
            ImGui::Separator();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        
        if (activeTask && activeTask->isRunning()) {
            ImGui::Separator();
            ImGui::Text("Task in progress...");
            ImGui::ProgressBar(activeTask->getProgress(), ImVec2(-1.0f, 0.0f));
            if (!activeTask->getStatus().empty()) ImGui::TextWrapped("%s", activeTask->getStatus().c_str());
            if (ImGui::Button("Cancel")) activeTask->requestCancel();
            ImGui::Separator();
        }
        
        if (ImGui::CollapsingHeader("1) Main Actions", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::InputText("Target Object##main", targetObjectName, sizeof(targetObjectName));
            ImGui::SameLine();
            if (ImGui::Button("Sync↓##loadparams")) {
                // Cargar parámetros del objeto seleccionado
                if (currentScene) {
                    Haruka::SceneObject* obj = currentScene->getObject(targetObjectName);
                    if (obj && obj->properties.contains("terrainEditor")) {
                        const auto& te = obj->properties["terrainEditor"];
                        if (te.contains("generator")) {
                            const auto& gen = te["generator"];
                            generationSeed = gen.value("seed", generationSeed);
                            lastSyncedSeed = generationSeed;  // Sincronizar tracking
                            baseRadiusKm = gen.value("baseRadiusKm", baseRadiusKm);
                            continentFrequency = gen.value("continentFrequency", continentFrequency);
                            detailFrequency = gen.value("detailFrequency", detailFrequency);
                            enableContinents = gen.value("enableContinents", enableContinents);
                            enableMountains = gen.value("enableMountains", enableMountains);
                            macroFrequency = gen.value("macroFrequency", macroFrequency);
                            macroHeightStrength = gen.value("macroHeightStrength", macroHeightStrength);
                            continentHeightStrength = gen.value("continentHeightStrength", continentHeightStrength);
                            detailHeightStrength = gen.value("detailHeightStrength", detailHeightStrength);
                            lastStatus = "✓ Parameters loaded from selected planet";
                        }
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Sync↑##saveparams")) {
                // Guardar parámetros al objeto seleccionado
                if (currentScene) {
                    Haruka::SceneObject* obj = currentScene->getObject(targetObjectName);
                    if (obj) {
                        if (!obj->properties.contains("terrainEditor")) {
                            obj->properties["terrainEditor"] = nlohmann::json::object();
                        }
                        if (!obj->properties["terrainEditor"].contains("generator")) {
                            obj->properties["terrainEditor"]["generator"] = nlohmann::json::object();
                        }
                        auto& gen = obj->properties["terrainEditor"]["generator"];
                        gen["seed"] = generationSeed;
                        gen["baseRadiusKm"] = baseRadiusKm;
                        gen["continentFrequency"] = continentFrequency;
                        gen["detailFrequency"] = detailFrequency;
                        gen["enableContinents"] = enableContinents;
                        gen["enableMountains"] = enableMountains;
                        gen["macroFrequency"] = macroFrequency;
                        gen["macroHeightStrength"] = macroHeightStrength;
                        gen["continentHeightStrength"] = continentHeightStrength;
                        gen["detailHeightStrength"] = detailHeightStrength;
                        lastStatus = "✓ Parameters saved to selected planet";
                    }
                }
            }
            
            ImGui::BeginDisabled(activeTask && activeTask->isRunning());
            
            ImGui::Separator();
            ImGui::Text("Auto-Generation Status:");
            Haruka::SceneObject* sourceObj = currentScene ? currentScene->getObject(targetObjectName) : nullptr;
            if (sourceObj && sourceObj->meshRenderer) {
                const auto& sv = sourceObj->meshRenderer->getSourceVertices();
                float minR = std::numeric_limits<float>::max(), maxR = std::numeric_limits<float>::lowest();
                for (const auto& v : sv) { float r = glm::length(v); minR = std::min(minR, r); maxR = std::max(maxR, r); }
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "✓ Source Mesh: %zu vertices", sv.size());
                ImGui::Text("  Radius: %.1f to %.1f km", minR * baseRadiusKm, maxR * baseRadiusKm);
                
                // Status de auto-generation
                bool isPlanetRoot = sourceObj->properties.contains("terrainEditor") && 
                                   sourceObj->properties["terrainEditor"].value("isPlanetRoot", false);
                if (isPlanetRoot) {
                    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "⚡ Auto-chunking ENABLED");
                    ImGui::Text("  Chunks generating on-demand with LOD");
                    int tilesPerFace = sourceObj->properties["terrainEditor"].value("tilesPerFace", 4);
                    int maxLod = sourceObj->properties["terrainEditor"].value("maxLod", 2);
                    ImGui::Text("  Grid: %dx%d per face, LOD 0-%d", tilesPerFace, tilesPerFace, maxLod);
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "⚠ Not marked as planet root");
                    ImGui::Text("  Add: \"isPlanetRoot\": true to properties");
                }
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "⚠ Source mesh unavailable");
            }
            
            ImGui::EndDisabled();
        }
        
        if (ImGui::CollapsingHeader("2) Generator Config", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Seed-based deterministic generation");
            ImGui::Separator();
            
            // Seed principal
            ImGui::InputInt("Master Seed##main", &generationSeed);
            ImGui::SameLine();
            if (ImGui::Button("🎲 Random##seed")) { 
                static std::mt19937 rng{std::random_device{}()}; 
                generationSeed = static_cast<int>(rng() % 1000000); 
            }
            
            // Auto-sync seed change to selected planet (detectar cambios reales)
            if (generationSeed != lastSyncedSeed && currentScene) {
                Haruka::SceneObject* obj = currentScene->getObject(targetObjectName);
                if (obj) {
                    if (!obj->properties.contains("terrainEditor")) {
                        obj->properties["terrainEditor"] = nlohmann::json::object();
                    }
                    if (!obj->properties["terrainEditor"].contains("generator")) {
                        obj->properties["terrainEditor"]["generator"] = nlohmann::json::object();
                    }
                    obj->properties["terrainEditor"]["generator"]["seed"] = generationSeed;
                    lastSyncedSeed = generationSeed;  // Actualizar tracking
                    lastStatus = "✓ Seed updated to " + std::to_string(generationSeed) + " (chunks will regenerate on next load)";
                }
            }
            
            ImGui::Separator();
            ImGui::Text("🌍 Planet Scale");
            ImGui::DragFloat("Radius (km)##scale", &baseRadiusKm, 100.0f, 1.0f, 1e7f, "%.0f km");
            
            ImGui::Separator();
            ImGui::Text("🌊 Continents & Oceans");
            ImGui::Checkbox("Enable Continents", &enableContinents);
            if (enableContinents) {
                ImGui::SliderFloat("Continent Frequency##freq", &continentFrequency, 0.1f, 5.0f, "%.2f");
                ImGui::SliderFloat("Continent Height", &continentHeightStrength, 0.01f, 0.5f, "%.3f");
            }
            ImGui::SliderFloat("Sea Level##sea", &seaLevel, 0.0f, 1.0f, "%.3f");
            
            ImGui::Separator();
            ImGui::Text("⛰️ Macro Terrain (Mountains)");
            ImGui::Checkbox("Enable Mountains##macro", &enableMountains);
            if (enableMountains) {
                ImGui::SliderFloat("Mountain Frequency##mfreq", &macroFrequency, 0.5f, 10.0f, "%.2f");
                ImGui::SliderFloat("Mountain Height##mheight", &macroHeightStrength, 0.01f, 0.5f, "%.3f");
            }
            
            ImGui::Separator();
            ImGui::Text("🏔️ Detail & Fine Features");
            ImGui::SliderFloat("Detail Frequency##detail", &detailFrequency, 1.0f, 50.0f, "%.1f");
            ImGui::SliderFloat("Detail Height##dheight", &detailHeightStrength, 0.001f, 0.1f, "%.4f");
            ImGui::SliderInt("Detail Octaves##octaves", &detailOctaves, 1, 8);
            
            ImGui::Separator();
            ImGui::Text("Mesh Resolution");
            const int maxSubdivUI = kHardMaxSubdiv;
            ImGui::SliderInt("Mesh Resolution##subdiv", &meshSubdivisions, 4, maxSubdivUI);
            meshSubdivisions = std::clamp(meshSubdivisions, 1, maxSubdivUI);
            const int est = std::min(meshSubdivisions, 30);
            const double gs = std::pow(2.0, (double)est);
            ImGui::Text("Estimated: %.0f vertices | %.0f triangles", 6.0 * (gs + 1.0) * (gs + 1.0), 12.0 * gs * gs);
        }
        
        if (ImGui::CollapsingHeader("3) Chunks", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Auto chunk by triangle budget", &autoChunkByTriangleBudget);
            if (autoChunkByTriangleBudget) {
                ImGui::SliderInt("Target tris/chunk", &targetTrianglesPerChunk, 1000, 100000);
                ImGui::SliderInt("Max verts/chunk hard cap", &maxVerticesPerChunkHardCap, 10000, 200000);
                auto rec = recommendChunkGridFromTriangleBudget(meshSubdivisions, targetTrianglesPerChunk);
                latSections = rec.first;
                lonSections = rec.second;
                ImGui::Text("Recommended grid: lat=%d lon=%d", latSections, lonSections);
            } else {
                ImGui::SliderInt("Lat Sections", &latSections, 2, 64);
                ImGui::SliderInt("Lon Sections", &lonSections, 2, 128);
            }
            if (lastChunkCount > 0) {
                ImGui::Separator();
                ImGui::Text("Last split stats:");
                ImGui::Text("  Min verts/chunk: %d", lastMinVertsPerChunk);
                ImGui::Text("  Avg verts/chunk: %d", lastAvgVertsPerChunk);
                ImGui::Text("  Max verts/chunk: %d", lastMaxVertsPerChunk);
            }
            ImGui::Checkbox("Hide source##chunks", &hideSourceAfterChunking);
            ImGui::SameLine();
            ImGui::Checkbox("Copy material##chunks", &copyMaterial);
            ImGui::SameLine();
            ImGui::Checkbox("Visualize", &visualizeChunkSections);
        }
        
        if (ImGui::CollapsingHeader("4) Viewport & Brush Tools", ImGuiTreeNodeFlags_DefaultOpen)) {
            
            ImGui::Text("Viewport Info:");
            ImGui::Text("  Seed: %d | Radius: %.0f km", generationSeed, baseRadiusKm);
            ImGui::Text("  Continents: %s | Mountains: %s", enableContinents ? "ON" : "OFF", enableMountains ? "ON" : "OFF");
            ImGui::Text("  Chunks: %d x %d", latSections, lonSections);
            
            ImGui::Separator();
            ImGui::Text("Brush Parameters:");
            ImGui::InputInt("Chunk ID##brush", &selectedChunkId);
            ImGui::InputFloat3("Brush Direction##brush", brushDir);
            ImGui::SliderFloat("Brush Angle##brush", &brushAngleDeg, 1.0f, 90.0f);
            ImGui::DragFloat("Brush Strength (km)##brush", &brushDeltaKm, 0.05f, 0.001f, 50.0f, "%.3f");
            
            ImGui::Separator();
            if (ImGui::Button("Raise Terrain", ImVec2(-1, 0))) applyBrushToChunk(true);
            if (ImGui::Button("Lower Terrain", ImVec2(-1, 0))) applyBrushToChunk(false);
            
            ImGui::TextDisabled("Selecciona chunk y aplica con Raise/Lower");
        }
        
        if (ImGui::CollapsingHeader("5) Chunk Tools##advanced", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::InputInt("Chunk ID##advanced", &selectedChunkId)) refreshSelectedChunkSeed();
            ImGui::InputInt("Chunk Seed##advanced", &selectedChunkSeed);
            if (ImGui::Button("Apply Chunk Seed##advanced")) applySeedToChunk(selectedChunkId, selectedChunkSeed);
        }
        
        if (ImGui::CollapsingHeader("6) Persist", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Auto-save##persist", &autoSaveChunks);
            ImGui::SameLine();
            ImGui::Checkbox("Dirty only", &saveOnlyDirtyChunks);
            if (ImGui::Button("Save")) saveChunkDeltas();
            ImGui::SameLine();
            if (ImGui::Button("Load")) loadChunkDeltas();
            ImGui::SameLine();
            if (ImGui::Button("Save Settings")) savePlanetSettings();
            ImGui::SameLine();
            if (ImGui::Button("Load Settings")) loadPlanetSettings();
        }
        
        if (!lastStatus.empty()) {
            ImGui::Separator();
            ImGui::TextWrapped("%s", lastStatus.c_str());
            if (lastChunkCount > 0) ImGui::Text("Chunks: %d", lastChunkCount);
            if (lastModifiedChunkCount > 0) ImGui::Text("Saved: %d", lastModifiedChunkCount);
        }
    } catch (const std::exception& e) {
        lastStatus = std::string("Error: ") + e.what();
    } catch (...) {
        lastStatus = "Unknown error";
    }
    ImGui::End();
}

std::string PlanetTerrainEditorPanel::getDeltaFilePath(const std::string& sourceName) const {
    const fs::path base = fs::path("terrain_chunks") / (currentScene ? currentScene->getName() : "untitled");
    fs::create_directories(base);
    return (base / (sourceName + ".json")).string();
}

std::string PlanetTerrainEditorPanel::getSettingsFilePath(const std::string& sourceName) const {
    const fs::path base = fs::path("terrain_chunks") / (currentScene ? currentScene->getName() : "untitled");
    fs::create_directories(base);
    return (base / (sourceName + ".settings.json")).string();
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
    if (!chunkObj.meshRenderer) chunkObj.meshRenderer = std::make_shared<MeshRendererComponent>();
    if (delta.vertices.empty() || delta.indices.empty()) return;
    chunkObj.meshRenderer->setMesh(delta.vertices, delta.normals, delta.indices);
}

void PlanetTerrainEditorPanel::saveChunkDeltas() {
    lastModifiedChunkCount = 0;
    if (!currentScene) { lastStatus = "No active scene."; return; }
    std::string sourceName = targetObjectName;
    if (sourceName.empty()) { lastStatus = "Empty target name."; return; }
    
    nlohmann::json root;
    root["source"] = sourceName;
    root["scene"] = currentScene->getName();
    root["latSections"] = latSections;
    root["lonSections"] = lonSections;
    root["chunks"] = nlohmann::json::array();
    
    for (const auto& obj : currentScene->getObjects()) {
        if (!isChunkObject(obj) || getSourceName(obj, "") != sourceName || !obj.meshRenderer) continue;
        ChunkDelta delta;
        delta.chunkId = obj.properties["terrainEditor"].value("chunkId", -1);
        delta.vertices = obj.meshRenderer->getSourceVertices();
        delta.normals = obj.meshRenderer->getSourceNormals();
        delta.indices = obj.meshRenderer->getSourceIndices();
        if (delta.chunkId < 0 || delta.vertices.empty() || delta.indices.empty()) continue;
        if (saveOnlyDirtyChunks && !dirtyChunks.empty() && dirtyChunks.find(delta.chunkId) == dirtyChunks.end()) continue;
        chunkDeltas[delta.chunkId] = delta;
        root["chunks"].push_back(serializeChunkDelta(delta));
        lastModifiedChunkCount++;
    }
    std::ofstream out(getDeltaFilePath(sourceName), std::ios::trunc);
    if (!out.is_open()) { lastStatus = "Failed to open delta file."; return; }
    out << root.dump(4);
    lastStatus = "Chunk deltas saved.";
    if (saveOnlyDirtyChunks) dirtyChunks.clear();
}

void PlanetTerrainEditorPanel::loadChunkDeltas() {
    lastModifiedChunkCount = 0;
    if (!currentScene) { lastStatus = "No active scene."; return; }
    std::string sourceName = targetObjectName;
    if (sourceName.empty()) { lastStatus = "Empty target name."; return; }
    std::ifstream in(getDeltaFilePath(sourceName));
    if (!in.is_open()) { lastStatus = "No saved deltas."; return; }
    nlohmann::json root;
    in >> root;
    chunkDeltas.clear();
    dirtyChunks.clear();
    if (root.contains("chunks") && root["chunks"].is_array()) {
        for (const auto& entry : root["chunks"]) {
            ChunkDelta delta = deserializeChunkDelta(entry);
            if (delta.chunkId >= 0 && !delta.vertices.empty() && !delta.indices.empty()) chunkDeltas[delta.chunkId] = delta;
        }
    }
    for (auto& obj : currentScene->getObjectsMutable()) {
        if (!isChunkObject(obj) || getSourceName(obj, "") != sourceName) continue;
        int chunkId = obj.properties["terrainEditor"].value("chunkId", -1);
        auto it = chunkDeltas.find(chunkId);
        if (it != chunkDeltas.end()) { applyChunkDelta(obj, it->second); lastModifiedChunkCount++; }
    }
    lastStatus = "Chunk deltas loaded.";
}

void PlanetTerrainEditorPanel::savePlanetSettings() const {
    if (!currentScene) return;

    const std::string sourceName = targetObjectName;
    if (sourceName.empty()) return;

    // Containers + nlohmann::json keep the preset compact, portable, and easy to diff.
    nlohmann::json root;
    root["scene"] = currentScene->getName();
    root["source"] = sourceName;
    root["generator"] = {
        {"seed", generationSeed},
        {"baseRadiusKm", baseRadiusKm},
        {"plateReliefKm", plateReliefKm},
        {"plateScale", plateScale},
        {"plateBoundarySharpness", plateBoundarySharpness},
        {"minContinentSizeKm", minContinentSizeKm},
        {"reliefKm", reliefKm},
        {"continentFrequency", continentFrequency},
        {"detailFrequency", detailFrequency},
        {"enableContinents", enableContinents},
        {"enableMountains", enableMountains},
        {"useGPUGeneration", useGPUGeneration},
        {"generationQualityPreset", generationQualityPreset},
        {"ignoreSafetyLimits", ignoreSafetyLimits},
        {"meshSubdivisions", meshSubdivisions}
    };
    root["layout"] = {
        {"latSections", latSections},
        {"lonSections", lonSections},
        {"autoChunkByTriangleBudget", autoChunkByTriangleBudget},
        {"targetTrianglesPerChunk", targetTrianglesPerChunk},
        {"maxVerticesPerChunkHardCap", maxVerticesPerChunkHardCap},
        {"hideSourceAfterChunking", hideSourceAfterChunking},
        {"copyMaterial", copyMaterial},
        {"visualizeChunkSections", visualizeChunkSections},
        {"autoSaveChunks", autoSaveChunks},
        {"saveOnlyDirtyChunks", saveOnlyDirtyChunks}
    };

    std::ofstream out(getSettingsFilePath(sourceName), std::ios::trunc);
    if (!out.is_open()) return;
    out << root.dump(2);
}

void PlanetTerrainEditorPanel::loadPlanetSettings() {
    if (!currentScene) {
        lastStatus = "No active scene.";
        return;
    }

    const std::string sourceName = targetObjectName;
    if (sourceName.empty()) {
        lastStatus = "Empty target name.";
        return;
    }

    std::ifstream in(getSettingsFilePath(sourceName));
    if (!in.is_open()) {
        lastStatus = "No saved planet settings found.";
        return;
    }

    nlohmann::json root;
    in >> root;

    // Read with defaults so older preset files remain valid as fields evolve.
    if (root.contains("generator") && root["generator"].is_object()) {
        const auto& g = root["generator"];
        generationSeed = g.value("seed", generationSeed);
        baseRadiusKm = g.value("baseRadiusKm", baseRadiusKm);
        plateReliefKm = g.value("plateReliefKm", plateReliefKm);
        plateScale = g.value("plateScale", plateScale);
        plateBoundarySharpness = g.value("plateBoundarySharpness", plateBoundarySharpness);
        minContinentSizeKm = g.value("minContinentSizeKm", minContinentSizeKm);
        reliefKm = g.value("reliefKm", reliefKm);
        continentFrequency = g.value("continentFrequency", continentFrequency);
        detailFrequency = g.value("detailFrequency", detailFrequency);
        enableContinents = g.value("enableContinents", enableContinents);
        enableMountains = g.value("enableMountains", enableMountains);
        useGPUGeneration = g.value("useGPUGeneration", useGPUGeneration);
        generationQualityPreset = g.value("generationQualityPreset", generationQualityPreset);
        ignoreSafetyLimits = g.value("ignoreSafetyLimits", ignoreSafetyLimits);
        meshSubdivisions = g.value("meshSubdivisions", meshSubdivisions);
        meshSubdivisions = std::clamp(meshSubdivisions, 1, kHardMaxSubdiv);
    }

    if (root.contains("layout") && root["layout"].is_object()) {
        const auto& l = root["layout"];
        latSections = l.value("latSections", latSections);
        lonSections = l.value("lonSections", lonSections);
        autoChunkByTriangleBudget = l.value("autoChunkByTriangleBudget", autoChunkByTriangleBudget);
        targetTrianglesPerChunk = l.value("targetTrianglesPerChunk", targetTrianglesPerChunk);
        maxVerticesPerChunkHardCap = l.value("maxVerticesPerChunkHardCap", maxVerticesPerChunkHardCap);
        hideSourceAfterChunking = l.value("hideSourceAfterChunking", hideSourceAfterChunking);
        copyMaterial = l.value("copyMaterial", copyMaterial);
        visualizeChunkSections = l.value("visualizeChunkSections", visualizeChunkSections);
        autoSaveChunks = l.value("autoSaveChunks", autoSaveChunks);
        saveOnlyDirtyChunks = l.value("saveOnlyDirtyChunks", saveOnlyDirtyChunks);
    }

    lastStatus = "Planet settings loaded.";
}

void PlanetTerrainEditorPanel::captureSplitSourceState(const Haruka::SceneObject& source) {
    splitTask.sourceType = source.type;
    splitTask.sourceColor = source.color;
    splitTask.sourceIntensity = source.intensity;
    splitTask.sourceMaterial = source.material;
}

bool PlanetTerrainEditorPanel::prepareSplitTask(std::string& error) {
    splitTask.reset();
    lastChunkCount = 0;
    if (!currentScene) { error = "No active scene."; splitTask.phase = SplitTaskState::Phase::Failed; return false; }
    std::string sourceName = targetObjectName;
    if (sourceName.empty()) { error = "Target object name is empty."; splitTask.phase = SplitTaskState::Phase::Failed; return false; }
    Haruka::SceneObject* source = currentScene->getObject(sourceName);
    if (!source) { error = "Target object not found."; splitTask.phase = SplitTaskState::Phase::Failed; return false; }
    if (!source->meshRenderer) source->meshRenderer = std::make_shared<MeshRendererComponent>();
    
    latSections = std::clamp(latSections, 2, 128);
    lonSections = std::clamp(lonSections, 2, 256);
    meshSubdivisions = std::clamp(meshSubdivisions, 1, kHardMaxSubdiv);
    targetTrianglesPerChunk = std::clamp(targetTrianglesPerChunk, 1000, 100000);
    if (autoChunkByTriangleBudget) {
        auto rec = recommendChunkGridFromTriangleBudget(meshSubdivisions, targetTrianglesPerChunk);
        latSections = rec.first;
        lonSections = rec.second;
    }
    
    const bool regenerateSource = regenerateSourceOnNextSplit;
    regenerateSourceOnNextSplit = false;
    
    splitTask.sourceName = sourceName;
    splitTask.verts.clear();
    splitTask.normals.clear();
    splitTask.indices.clear();
    splitTask.triangleCursor = 0;
    splitTask.progress = 0.01f;
    splitTask.running = true;
    splitTask.status = regenerateSource ? "Generating source..." : "Preparing chunks...";
    
    auto& sceneObjects = currentScene->getObjectsMutable();
    for (int i = 0; i < (int)sceneObjects.size(); ++i) {
        if (sceneObjects[i].name == sourceName) { splitTask.sourceIndex = i; break; }
    }
    if (splitTask.sourceIndex < 0) { error = "Target disappeared."; splitTask.phase = SplitTaskState::Phase::Failed; splitTask.running = false; return false; }
    captureSplitSourceState(sceneObjects[splitTask.sourceIndex]);
    
    if (regenerateSource) {
        splitTask.phase = SplitTaskState::Phase::GenerateSource;
        const int cap = getInteractiveSubdivisionCap(generationQualityPreset);
        const char* label = getQualityPresetLabel(generationQualityPreset);
        const int requested = std::min(meshSubdivisions, kHardMaxSubdiv);
        splitTask.sourceFuture = std::async(std::launch::async, [this, requested, cap, label]() {
            int effective = std::min(requested, cap);
            
            SplitTaskState::SourceGenerationResult out;
            if (requested > effective) out.warning = "Subdivisions " + std::to_string(requested) + "->" + std::to_string(effective) + " (" + std::string(label) + ")";
            
            Haruka::PlanetarySystem::PlanetConfig cfg;
            cfg.radius = 1.0f;
            cfg.subdivisions = effective;
            cfg.baseRadiusKm = baseRadiusKm;
            cfg.seedBase = generationSeed;
            cfg.seedContinents = generationSeed + 100;
            cfg.seedMacro = generationSeed + 200;
            cfg.seedDetail = generationSeed + 300;
            cfg.enableContinents = false;
            cfg.enableMountains = false;
            cfg.continentFrequency = std::min(continentFrequency, std::max(0.01f, getMaxContinentFrequencyForSize(baseRadiusKm, minContinentSizeKm)));
            cfg.detailFrequency = detailFrequency; cfg.useGPU = false;
            cfg.continentHeightStrength = 0.0f;
            cfg.macroHeightStrength = 0.0f;
            cfg.detailHeightStrength = 0.0f;
            
            if (!planetarySystem) { out.error = "No PlanetarySystem instance set"; return out; }
            planetarySystem->generatePlanet(cfg, "EditorPreview");
            const auto* pdata = planetarySystem->getPlanetData("EditorPreview");
            Haruka::PlanetarySystem::PlanetData data;
            if (pdata) data = *pdata;
            if (data.vertices.empty() || data.indices.empty()) { out.error = "Planet generation failed."; return out; }

            LayeredDeformParams params{
                reliefKm,
                baseRadiusKm,
                plateReliefKm,
                plateScale,
                plateBoundarySharpness,
                minContinentSizeKm,
                continentFrequency,
                detailFrequency,
                generationSeed,
                enableContinents,
                enableMountains
            };
            applyLayeredDeformation(data.vertices, generationSeed, params);
            data.normals = computeSmoothNormals(data.vertices, data.indices);
            out.success = true; out.vertices = std::move(data.vertices); out.normals = std::move(data.normals); out.indices = std::move(data.indices);
            return out;
        });
        splitTask.sourceFutureValid = true;
    } else {
        const auto& verts = source->meshRenderer->getSourceVertices();
        const auto& normals = source->meshRenderer->getSourceNormals();
        const auto& indices = source->meshRenderer->getSourceIndices();
        if (verts.empty() || indices.size() < 3) {
            error = "Source mesh empty. Regenerate first.";
            splitTask.phase = SplitTaskState::Phase::Failed;
            splitTask.running = false;
            return false;
        }
        splitTask.verts = verts;
        splitTask.normals = normals;
        splitTask.indices = indices;
        splitTask.phase = SplitTaskState::Phase::ProcessTriangles;
        splitTask.status = "Using existing mesh...";
        splitTask.sourceFutureValid = false;
    }
    error.clear();
    return true;
}

void PlanetTerrainEditorPanel::processSplitTaskTriangles() {
    auto getChunkIdFromDir = [&](const glm::vec3& dir) {
        glm::vec3 d = glm::normalize(dir);
        float lat = std::asin(std::clamp(d.y, -1.0f, 1.0f));
        float lon = std::atan2(d.z, d.x);
        int latBin = static_cast<int>(((lat + kPi * 0.5f) / kPi) * latSections);
        int lonBin = static_cast<int>(((lon + kPi) / (2.0f * kPi)) * lonSections);
        return std::clamp(latBin, 0, latSections - 1) * lonSections + std::clamp(lonBin, 0, lonSections - 1);
    };
    auto remapVertex = [&](SplitTaskState::ChunkData& chunk, unsigned int oldIndex) -> unsigned int {
        auto it = chunk.remap.find(oldIndex);
        if (it != chunk.remap.end()) return it->second;
        unsigned int newIndex = static_cast<unsigned int>(chunk.localVerts.size());
        chunk.remap[oldIndex] = newIndex;
        chunk.localVerts.push_back(splitTask.verts[oldIndex]);
        if (!splitTask.normals.empty() && oldIndex < splitTask.normals.size())
            chunk.localNormals.push_back(splitTask.normals[oldIndex]);
        else
            chunk.localNormals.push_back(glm::normalize(splitTask.verts[oldIndex]));
        return newIndex;
    };
    
    constexpr size_t kTrianglesPerFrame = 12000;
    constexpr auto kTriangleBudget = std::chrono::milliseconds(3);
    const auto frameStart = std::chrono::steady_clock::now();
    size_t processed = 0;
    while (splitTask.triangleCursor + 2 < splitTask.indices.size() && processed < kTrianglesPerFrame) {
        unsigned int i0 = splitTask.indices[splitTask.triangleCursor + 0];
        unsigned int i1 = splitTask.indices[splitTask.triangleCursor + 1];
        unsigned int i2 = splitTask.indices[splitTask.triangleCursor + 2];
        splitTask.triangleCursor += 3;
        processed++;
        if (i0 >= splitTask.verts.size() || i1 >= splitTask.verts.size() || i2 >= splitTask.verts.size()) {
            splitTask.invalidTriangles++;
            continue;
        }
        glm::vec3 c = (splitTask.verts[i0] + splitTask.verts[i1] + splitTask.verts[i2]) * (1.0f / 3.0f);
        int chunkId = getChunkIdFromDir(c);
        auto& chunk = splitTask.chunks[chunkId];
        chunk.latBin = std::clamp(chunkId / lonSections, 0, latSections - 1);
        chunk.lonBin = std::clamp(chunkId % lonSections, 0, lonSections - 1);
        unsigned int n0 = remapVertex(chunk, i0);
        unsigned int n1 = remapVertex(chunk, i1);
        unsigned int n2 = remapVertex(chunk, i2);
        chunk.localIndices.push_back(n0);
        chunk.localIndices.push_back(n1);
        chunk.localIndices.push_back(n2);
        if ((processed & 0xFF) == 0) {
            const auto elapsed = std::chrono::steady_clock::now() - frameStart;
            if (elapsed >= kTriangleBudget) break;
        }
    }
    const float triProgress = splitTask.indices.empty() ? 1.0f : std::min(1.0f, static_cast<float>(splitTask.triangleCursor) / static_cast<float>(splitTask.indices.size()));
    splitTask.progress = 0.05f + triProgress * 0.55f;
    splitTask.status = "Partitioning...";
    if (splitTask.triangleCursor + 2 >= splitTask.indices.size()) {
        splitTask.chunkOrder.clear();
        splitTask.chunkOrder.reserve(splitTask.chunks.size());
        for (const auto& [chunkId, _] : splitTask.chunks) splitTask.chunkOrder.push_back(chunkId);

        // Source buffers no longer needed after partitioning.
        splitTask.verts.clear();
        splitTask.normals.clear();
        splitTask.indices.clear();

        splitTask.staleChunks.clear();
        for (const auto& obj : currentScene->getObjects()) {
            if (!isChunkObject(obj) || getSourceName(obj, "") != splitTask.sourceName) continue;
            int oldChunkId = obj.properties["terrainEditor"].value("chunkId", -1);
            if (splitTask.chunks.find(oldChunkId) == splitTask.chunks.end()) splitTask.staleChunks.push_back(obj.name);
        }
        splitTask.staleCursor = 0;
        splitTask.phase = SplitTaskState::Phase::RemoveStale;
    }
}

void PlanetTerrainEditorPanel::processSplitTaskStaleRemoval() {
    constexpr size_t kChunkDeletePerFrame = 24;
    size_t removed = 0;
    while (splitTask.staleCursor < splitTask.staleChunks.size() && removed < kChunkDeletePerFrame) {
        currentScene->removeObject(splitTask.staleChunks[splitTask.staleCursor]);
        splitTask.staleCursor++;
        removed++;
    }
    const float staleProgress = splitTask.staleChunks.empty() ? 1.0f : std::min(1.0f, static_cast<float>(splitTask.staleCursor) / static_cast<float>(splitTask.staleChunks.size()));
    splitTask.progress = 0.60f + staleProgress * 0.10f;
    splitTask.status = "Removing stale...";
    if (splitTask.staleCursor >= splitTask.staleChunks.size()) {
        splitTask.buildCursor = 0;
        splitTask.phase = SplitTaskState::Phase::BuildChunks;
    }
}

void PlanetTerrainEditorPanel::processSplitTaskChunkBuild() {
    constexpr size_t kChunkBuildPerFrame = 1;
    splitTask.sourceIndex = -1;
    auto& objs = currentScene->getObjectsMutable();
    for (int i = 0; i < (int)objs.size(); ++i) {
        if (objs[i].name == splitTask.sourceName) { splitTask.sourceIndex = i; break; }
    }
    if (splitTask.sourceIndex < 0) {
        splitTask.running = false;
        splitTask.phase = SplitTaskState::Phase::Failed;
        splitTask.status = "Target disappeared during build.";
        lastStatus = splitTask.status;
        return;
    }
    size_t builtThisFrame = 0;
    while (splitTask.buildCursor < splitTask.chunkOrder.size() && builtThisFrame < kChunkBuildPerFrame) {
        int chunkId = splitTask.chunkOrder[splitTask.buildCursor++];
        auto it = splitTask.chunks.find(chunkId);
        if (it == splitTask.chunks.end() || it->second.localIndices.empty()) continue;
        auto& data = it->second;
        const std::string chunkName = splitTask.sourceName + "_chunk_" + std::to_string(chunkId);
        Haruka::SceneObject* chunkObjPtr = currentScene->getObject(chunkName);
        if (!chunkObjPtr) {
            Haruka::SceneObject chunkObj;
            chunkObj.name = chunkName;
            chunkObj.type = splitTask.sourceType;
            chunkObj.position = glm::dvec3(0.0);
            chunkObj.rotation = glm::dvec3(0.0);
            chunkObj.scale = glm::dvec3(1.0);
            chunkObj.color = splitTask.sourceColor;
            chunkObj.intensity = splitTask.sourceIntensity;
            chunkObj.renderLayer = 1;
            chunkObj.parentIndex = splitTask.sourceIndex;
            if (copyMaterial) chunkObj.material = splitTask.sourceMaterial;
            chunkObj.meshRenderer = std::make_shared<MeshRendererComponent>();
            currentScene->addObject(chunkObj);
            chunkObjPtr = currentScene->getObject(chunkName);
            splitTask.created++;
        } else {
            splitTask.updated++;
        }
        if (chunkObjPtr) {
            if (!chunkObjPtr->meshRenderer) chunkObjPtr->meshRenderer = std::make_shared<MeshRendererComponent>();
            chunkObjPtr->type = splitTask.sourceType;
            chunkObjPtr->position = glm::dvec3(0.0);
            chunkObjPtr->rotation = glm::dvec3(0.0);
            chunkObjPtr->scale = glm::dvec3(1.0);
            chunkObjPtr->color = splitTask.sourceColor;
            chunkObjPtr->intensity = splitTask.sourceIntensity;
            chunkObjPtr->renderLayer = 1;
            chunkObjPtr->parentIndex = splitTask.sourceIndex;
            if (copyMaterial) chunkObjPtr->material = splitTask.sourceMaterial;
            chunkObjPtr->meshRenderer->setMesh(data.localVerts, data.localNormals, data.localIndices);
            chunkObjPtr->properties["terrainEditor"]["isChunk"] = true;
            chunkObjPtr->properties["terrainEditor"]["source"] = splitTask.sourceName;
            chunkObjPtr->properties["terrainEditor"]["chunkId"] = chunkId;
            chunkObjPtr->properties["terrainEditor"]["chunkFace"] = 0;
            chunkObjPtr->properties["terrainEditor"]["chunkLod"] = 0;
            chunkObjPtr->properties["terrainEditor"]["chunkX"] = data.lonBin;
            chunkObjPtr->properties["terrainEditor"]["chunkY"] = data.latBin;
            chunkObjPtr->properties["terrainEditor"]["chunkSeed"] = generationSeed + chunkId * 7919;
            chunkObjPtr->properties["terrainEditor"]["chunkTilesY"] = latSections;
            chunkObjPtr->properties["terrainEditor"]["chunkTilesX"] = lonSections;
            chunkObjPtr->properties["terrainEditor"]["isInstancedTerrainChunk"] = true;
            chunkObjPtr->properties["terrainEditor"]["elevationTexturing"] = true;
            if (visualizeChunkSections) chunkObjPtr->properties["terrainEditor"]["visualizeSections"] = true;
        }
        builtThisFrame++;
    }
    const float buildProgress = splitTask.chunkOrder.empty() ? 1.0f : std::min(1.0f, static_cast<float>(splitTask.buildCursor) / static_cast<float>(splitTask.chunkOrder.size()));
    splitTask.progress = 0.70f + buildProgress * 0.28f;
    splitTask.status = "Uploading...";
    if (splitTask.buildCursor >= splitTask.chunkOrder.size()) splitTask.phase = SplitTaskState::Phase::Finalize;
}

void PlanetTerrainEditorPanel::finalizeSplitTask() {
    if (Haruka::SceneObject* source = currentScene->getObject(splitTask.sourceName)) {
        source->properties["terrainEditor"]["disableRender"] = hideSourceAfterChunking;
        source->properties["terrainEditor"]["chunked"] = true;
        source->properties["terrainEditor"]["isPlanetRoot"] = true;
        source->properties["terrainEditor"]["chunkTilesY"] = latSections;
        source->properties["terrainEditor"]["chunkTilesX"] = lonSections;
        source->properties["terrainEditor"]["chunkFace"] = 0;
        source->properties["terrainEditor"]["chunkLod"] = 0;
        source->properties["terrainEditor"]["isInstancedTerrainSource"] = true;
        source->properties["terrainEditor"]["generator"]["seed"] = generationSeed;
        source->properties["terrainEditor"]["generator"]["baseRadiusKm"] = baseRadiusKm;
        source->properties["terrainEditor"]["generator"]["plateReliefKm"] = plateReliefKm;
        source->properties["terrainEditor"]["generator"]["plateScale"] = plateScale;
        source->properties["terrainEditor"]["generator"]["plateBoundarySharpness"] = plateBoundarySharpness;
        source->properties["terrainEditor"]["generator"]["minContinentSizeKm"] = minContinentSizeKm;
        source->properties["terrainEditor"]["generator"]["reliefKm"] = reliefKm;
        source->properties["terrainEditor"]["generator"]["continentFrequency"] = continentFrequency;
        source->properties["terrainEditor"]["generator"]["detailFrequency"] = detailFrequency;
        source->properties["terrainEditor"]["elevationTexturing"] = true;
        source->childrenIndices.clear();
        const auto& objs = currentScene->getObjects();
        for (int i = 0; i < (int)objs.size(); ++i) {
            const auto& o = objs[i];
            if (!isChunkObject(o) || getSourceName(o, "") != splitTask.sourceName) continue;
            source->childrenIndices.push_back(i);
        }
    }
    lastChunkCount = splitTask.created + splitTask.updated;
    lastStatus = (splitTask.created + splitTask.updated) > 0
        ? ("Created=" + std::to_string(splitTask.created) + " Updated=" + std::to_string(splitTask.updated))
        : "No chunks generated.";
    if (splitTask.invalidTriangles > 0) lastStatus += " [Invalid: " + std::to_string(splitTask.invalidTriangles) + "]";
    splitTask.progress = 1.0f;
    splitTask.status = lastStatus;
    splitTask.running = false;
    splitTask.phase = SplitTaskState::Phase::Completed;
    if (autoSaveChunks) saveChunkDeltas();
}

void PlanetTerrainEditorPanel::beginSplitPlanetIntoChunksTask() {
    std::string error;
    if (!prepareSplitTask(error)) lastStatus = error;
}

void PlanetTerrainEditorPanel::cancelSplitPlanetIntoChunksTask() {
    if (!splitTask.running) return;
    splitTask.running = false;
    splitTask.phase = SplitTaskState::Phase::Cancelled;
    splitTask.status = "Cancelled by user.";
    lastStatus = splitTask.status;
}

void PlanetTerrainEditorPanel::tickSplitPlanetIntoChunksTask() {
    if (!splitTask.running || !currentScene) return;
    switch (splitTask.phase) {
        case SplitTaskState::Phase::GenerateSource: {
            if (!splitTask.sourceFutureValid) {
                splitTask.running = false;
                splitTask.phase = SplitTaskState::Phase::Failed;
                splitTask.status = "Async not initialized.";
                lastStatus = splitTask.status;
                return;
            }
            auto ready = splitTask.sourceFuture.wait_for(std::chrono::milliseconds(0));
            splitTask.progress = std::max(splitTask.progress, 0.08f);
            splitTask.status = "Generating...";
            if (ready != std::future_status::ready) return;
            auto generated = splitTask.sourceFuture.get();
            splitTask.sourceFutureValid = false;
            if (!generated.success) {
                splitTask.running = false;
                splitTask.phase = SplitTaskState::Phase::Failed;
                splitTask.status = generated.error.empty() ? "Generation failed." : generated.error;
                lastStatus = splitTask.status;
                return;
            }
            Haruka::SceneObject* source = currentScene->getObject(splitTask.sourceName);
            if (!source) {
                splitTask.running = false;
                splitTask.phase = SplitTaskState::Phase::Failed;
                splitTask.status = "Target disappeared after generation.";
                lastStatus = splitTask.status;
                return;
            }
            // Keep generated data in split buffers; avoid uploading a giant source mesh before chunking.
            source->scale = glm::dvec3(std::max(1.0f, baseRadiusKm));
            if (!generated.warning.empty()) lastStatus = generated.warning;
            splitTask.verts = std::move(generated.vertices);
            splitTask.normals = std::move(generated.normals);
            splitTask.indices = std::move(generated.indices);
            if (splitTask.verts.empty() || splitTask.indices.size() < 3) {
                splitTask.running = false;
                splitTask.phase = SplitTaskState::Phase::Failed;
                splitTask.status = "Mesh empty after generation.";
                lastStatus = splitTask.status;
                return;
            }
            splitTask.triangleCursor = 0;
            splitTask.progress = 0.10f;
            splitTask.status = "Ready. Partitioning...";
            splitTask.phase = SplitTaskState::Phase::ProcessTriangles;
            break;
        }
        case SplitTaskState::Phase::ProcessTriangles: { processSplitTaskTriangles(); break; }
        case SplitTaskState::Phase::RemoveStale: { processSplitTaskStaleRemoval(); break; }
        case SplitTaskState::Phase::BuildChunks: { processSplitTaskChunkBuild(); break; }
        case SplitTaskState::Phase::Finalize: { finalizeSplitTask(); break; }
        default: break;
    }
}

void PlanetTerrainEditorPanel::splitPlanetIntoChunks() {
    if (activeTask && activeTask->isRunning()) { lastStatus = "Task already running."; return; }
    activeTask = std::make_unique<TerrainSplitTask>(this);
    if (!activeTask->start()) lastStatus = activeTask->getStatus();
}

void PlanetTerrainEditorPanel::restoreSourcePlanet() {
    lastChunkCount = 0;
    if (!currentScene) { lastStatus = "No active scene."; return; }
    std::string sourceName = targetObjectName;
    if (sourceName.empty()) { lastStatus = "Empty target name."; return; }
    std::vector<std::string> toRemove;
    std::string prefix = sourceName + "_chunk_";
    for (const auto& obj : currentScene->getObjects()) {
        if (obj.name.rfind(prefix, 0) == 0) toRemove.push_back(obj.name);
    }
    for (const auto& name : toRemove) currentScene->removeObject(name);
    if (Haruka::SceneObject* source = currentScene->getObject(sourceName)) {
        source->childrenIndices.clear();
        source->properties["terrainEditor"]["disableRender"] = false;
        source->properties["terrainEditor"]["chunked"] = false;
    }
    lastStatus = "Source restored.";
}

void PlanetTerrainEditorPanel::applyBrushToChunk(bool raise) {
    if (!currentScene) { lastStatus = "No active scene."; return; }
    std::string sourceName = targetObjectName;
    if (sourceName.empty()) { lastStatus = "Empty target name."; return; }
    Haruka::SceneObject* targetChunk = nullptr;
    for (auto& obj : currentScene->getObjectsMutable()) {
        if (!isChunkObject(obj) || getSourceName(obj, "") != sourceName) continue;
        int chunkId = obj.properties["terrainEditor"].value("chunkId", -1);
        if (chunkId == selectedChunkId) { targetChunk = &obj; break; }
    }
    if (!targetChunk || !targetChunk->meshRenderer) { lastStatus = "Chunk not found."; return; }
    auto verts = targetChunk->meshRenderer->getSourceVertices();
    auto inds = targetChunk->meshRenderer->getSourceIndices();
    if (verts.empty() || inds.empty()) { lastStatus = "Chunk mesh empty."; return; }
    glm::vec3 centerDir(brushDir[0], brushDir[1], brushDir[2]);
    float len = glm::length(centerDir);
    if (len < 1e-6f) { lastStatus = "Brush direction is zero."; return; }
    centerDir /= len;
    float cosThreshold = std::cos(glm::radians(std::clamp(brushAngleDeg, 0.1f, 179.0f)));
    float planetRadiusKm = 1.0f;
    if (Haruka::SceneObject* sourceObj = currentScene->getObject(sourceName)) {
        planetRadiusKm = std::max({std::abs((float)sourceObj->scale.x), std::abs((float)sourceObj->scale.y), std::abs((float)sourceObj->scale.z)});
    }
    if (planetRadiusKm < 1e-6f) planetRadiusKm = 1.0f;
    float deltaNorm = std::max(0.00001f, brushDeltaKm / planetRadiusKm);
    if (!raise) deltaNorm = -deltaNorm;
    int touched = 0;
    for (auto& v : verts) {
        glm::vec3 d = glm::normalize(v);
        if (glm::dot(d, centerDir) >= cosThreshold) { v += d * deltaNorm; touched++; }
    }
    if (touched == 0) { lastStatus = "Brush touched 0 vertices."; return; }
    std::vector<glm::vec3> norms = computeSmoothNormals(verts, inds);
    targetChunk->meshRenderer->setMesh(verts, norms, inds);
    int chunkId = targetChunk->properties["terrainEditor"].value("chunkId", -1);
    if (chunkId >= 0) dirtyChunks.insert(chunkId);
    lastStatus = "Brush: chunk " + std::to_string(chunkId) + " vertices=" + std::to_string(touched);
}

void PlanetTerrainEditorPanel::applySeedToChunk(int chunkId, int chunkSeed) {
    if (!currentScene) { lastStatus = "No active scene."; return; }
    std::string sourceName = targetObjectName;
    if (sourceName.empty()) { lastStatus = "Empty target name."; return; }
    Haruka::SceneObject* targetChunk = nullptr;
    for (auto& obj : currentScene->getObjectsMutable()) {
        if (!isChunkObject(obj) || getSourceName(obj, "") != sourceName) continue;
        if (obj.properties["terrainEditor"].value("chunkId", -1) == chunkId) { targetChunk = &obj; break; }
    }
    if (!targetChunk || !targetChunk->meshRenderer) { lastStatus = "Chunk not found."; return; }
    auto verts = targetChunk->meshRenderer->getSourceVertices();
    auto inds = targetChunk->meshRenderer->getSourceIndices();
    if (verts.empty() || inds.empty()) { lastStatus = "Chunk mesh empty."; return; }
    
    LayeredDeformParams params{
        reliefKm,
        baseRadiusKm,
        plateReliefKm,
        plateScale,
        plateBoundarySharpness,
        minContinentSizeKm,
        continentFrequency,
        detailFrequency,
        generationSeed,
        enableContinents,
        enableMountains
    };
    int touched = applyLayeredDeformation(verts, chunkSeed, params);
    std::vector<glm::vec3> norms = computeSmoothNormals(verts, inds);
    targetChunk->meshRenderer->setMesh(verts, norms, inds);
    targetChunk->properties["terrainEditor"]["chunkSeed"] = chunkSeed;
    dirtyChunks.insert(chunkId);
    selectedChunkSeed = chunkSeed;
    lastStatus = "Chunk seed applied: #" + std::to_string(chunkId) + " verts=" + std::to_string(touched);
}

void PlanetTerrainEditorPanel::applySeedToPlanet() {
    if (!currentScene) { lastStatus = "No active scene."; return; }
    if (std::string(targetObjectName).empty()) { lastStatus = "Empty target name."; return; }

    regenerateSourceOnNextSplit = true;
    splitPlanetIntoChunks();
    refreshSelectedChunkSeed();
}
void PlanetTerrainEditorPanel::saveGeneratorConfigToObject() {
    if (!currentScene) { lastStatus = "No active scene."; return; }
    
    Haruka::SceneObject* targetObj = currentScene->getObject(targetObjectName);
    if (!targetObj) { lastStatus = "Target object not found: " + std::string(targetObjectName); return; }
    
    // Inicializar estructura JSON si no existe
    if (!targetObj->properties.is_object()) {
        targetObj->properties = nlohmann::json::object();
    }
    if (!targetObj->properties.contains("terrainEditor")) {
        targetObj->properties["terrainEditor"] = nlohmann::json::object();
    }
    if (!targetObj->properties["terrainEditor"].contains("generator")) {
        targetObj->properties["terrainEditor"]["generator"] = nlohmann::json::object();
    }
    
    // Guardar configuración
    auto& gen = targetObj->properties["terrainEditor"]["generator"];
    gen["seed"] = generationSeed;
    gen["baseRadiusKm"] = baseRadiusKm;
    gen["continentFrequency"] = continentFrequency;
    gen["continentHeightStrength"] = continentHeightStrength;
    gen["continentWarpStrength"] = 0.15f;
    gen["seaLevel"] = seaLevel;
    gen["macroFrequency"] = macroFrequency;
    gen["macroHeightStrength"] = macroHeightStrength;
    gen["detailFrequency"] = detailFrequency;
    gen["detailHeightStrength"] = detailHeightStrength;
    gen["octavesDetail"] = detailOctaves;
    gen["enableContinents"] = enableContinents;
    gen["enableMountains"] = enableMountains;
    gen["persistence"] = 0.5f;
    gen["lacunarity"] = 2.0f;
    
    lastStatus = "✓ Generator config saved to " + std::string(targetObjectName);
}