#pragma once

#include "core/scene/scene_manager.h"
#include <imgui.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Panel modular para edición de terreno planetario.
 *
 * No depende de sistemas runtime del motor: persiste configuración en
 * SceneObject::terrainSettings / properties para mantener compatibilidad.
 */
class PlanetTerrainEditorPanel {
public:
    PlanetTerrainEditorPanel() = default;
    ~PlanetTerrainEditorPanel() = default;

    void setScene(Haruka::SceneManager* scene);
    void onImGuiRender();

private:
    void refreshPlanetCandidates();
    std::shared_ptr<Haruka::SceneObject> getSelectedPlanet() const;
    static bool isPlanetLike(const Haruka::SceneObject& obj);

    void drawOverviewTab(Haruka::SceneObject& obj);
    void drawLodTab(Haruka::SceneObject& obj);
    void drawBiomesTab(Haruka::SceneObject& obj);
    void drawStreamingTab(Haruka::SceneObject& obj);
    void drawTerrainTab(Haruka::SceneObject& obj);
    void drawDebugTab(Haruka::SceneObject& obj);

    static void drawStringField(const char* label, std::string& value);
    static void drawDoubleField(const char* label, double& value, double speed = 0.1, double minValue = 0.0, double maxValue = 0.0);
    static void drawIntField(const char* label, int& value, int speed = 1, int minValue = 0, int maxValue = 0);
    static void drawDoubleVector(const char* label, std::vector<double>& values);
    static void drawStringVector(const char* label, std::vector<std::string>& values);
    static void drawTerrainLayerMap(std::unordered_map<std::string, Haruka::TerrainLayerSettings>& layers);

    Haruka::SceneManager* currentScene = nullptr;
    std::vector<std::string> planetNames;
    int selectedPlanetIndex = -1;
};
