#include "planet_terrain_editor.h"

#include <algorithm>
#include <cstdio>

namespace {
void drawTextDisabled(const char* label, const char* message) {
	ImGui::TextDisabled("%s: %s", label, message);
}
}

void PlanetTerrainEditorPanel::setScene(Haruka::SceneManager* scene) {
	currentScene = scene;
	selectedPlanetIndex = -1;
	planetNames.clear();
}

bool PlanetTerrainEditorPanel::isPlanetLike(const Haruka::SceneObject& obj) {
	return obj.type == "Planet" || obj.type == "CelestialBody" || obj.lodSettings.has_value() || obj.streamingSettings.has_value() || obj.terrainSettings.has_value();
}

void PlanetTerrainEditorPanel::refreshPlanetCandidates() {
	planetNames.clear();
	if (!currentScene) return;

	const auto& objects = currentScene->getAllObjects();
	for (const auto& objPtr : objects) {
		if (!objPtr) continue;
		const auto& obj = *objPtr;
		if (isPlanetLike(obj) || obj.flags.hasChunks) {
			planetNames.push_back(obj.name);
		}
	}

	if (planetNames.empty()) {
		selectedPlanetIndex = -1;
		return;
	}

	if (selectedPlanetIndex < 0 || selectedPlanetIndex >= (int)planetNames.size()) {
		selectedPlanetIndex = 0;
	}
}

std::shared_ptr<Haruka::SceneObject> PlanetTerrainEditorPanel::getSelectedPlanet() const {
	if (!currentScene || selectedPlanetIndex < 0 || selectedPlanetIndex >= (int)planetNames.size()) {
		return nullptr;
	}
	return currentScene->getObjectByName(planetNames[selectedPlanetIndex]);
}

void PlanetTerrainEditorPanel::drawStringField(const char* label, std::string& value) {
	char buffer[512] = {0};
	std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
	if (ImGui::InputText(label, buffer, sizeof(buffer))) {
		value = buffer;
	}
}

void PlanetTerrainEditorPanel::drawDoubleField(const char* label, double& value, double speed, double minValue, double maxValue) {
	float temp = static_cast<float>(value);
	if (minValue < maxValue) {
		if (ImGui::DragFloat(label, &temp, static_cast<float>(speed), static_cast<float>(minValue), static_cast<float>(maxValue))) {
			value = temp;
		}
	} else if (ImGui::DragFloat(label, &temp, static_cast<float>(speed))) {
		value = temp;
	}
}

void PlanetTerrainEditorPanel::drawIntField(const char* label, int& value, int speed, int minValue, int maxValue) {
	if (minValue < maxValue) {
		ImGui::DragInt(label, &value, static_cast<float>(speed), minValue, maxValue);
	} else {
		ImGui::DragInt(label, &value, static_cast<float>(speed));
	}
}

void PlanetTerrainEditorPanel::drawDoubleVector(const char* label, std::vector<double>& values) {
	if (!ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) return;
	for (size_t i = 0; i < values.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));
		drawDoubleField("Value", values[i], 0.1, 0.0, 0.0);
		ImGui::SameLine();
		if (ImGui::SmallButton("-")) {
			values.erase(values.begin() + static_cast<long>(i));
			ImGui::PopID();
			return;
		}
		ImGui::PopID();
	}
	if (ImGui::Button("Add value")) {
		values.push_back(0.0);
	}
}

void PlanetTerrainEditorPanel::drawStringVector(const char* label, std::vector<std::string>& values) {
	if (!ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) return;
	for (size_t i = 0; i < values.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));
		drawStringField("Asset", values[i]);
		ImGui::SameLine();
		if (ImGui::SmallButton("-")) {
			values.erase(values.begin() + static_cast<long>(i));
			ImGui::PopID();
			return;
		}
		ImGui::PopID();
	}
	if (ImGui::Button("Add asset")) {
		values.push_back("");
	}
}

void PlanetTerrainEditorPanel::drawTerrainLayerMap(std::unordered_map<std::string, Haruka::TerrainLayerSettings>& layers) {
	if (!ImGui::CollapsingHeader("Noise Layers", ImGuiTreeNodeFlags_DefaultOpen)) return;
	for (auto it = layers.begin(); it != layers.end(); ) {
		ImGui::PushID(it->first.c_str());
		ImGui::SeparatorText(it->first.c_str());
		drawDoubleField("Freq", it->second.freq, 0.01, 0.0, 0.0);
		drawIntField("Octaves", it->second.octaves, 1);
		drawDoubleField("Strength", it->second.strength, 0.01, 0.0, 0.0);
		if (ImGui::SmallButton("Remove layer")) {
			it = layers.erase(it);
			ImGui::PopID();
			continue;
		}
		ImGui::PopID();
		++it;
	}

	static char newLayerName[64] = {0};
	ImGui::InputText("New Layer Name", newLayerName, sizeof(newLayerName));
	if (ImGui::Button("Add Layer") && newLayerName[0] != '\0') {
		layers.emplace(newLayerName, Haruka::TerrainLayerSettings{});
		newLayerName[0] = '\0';
	}
}

void PlanetTerrainEditorPanel::drawOverviewTab(Haruka::SceneObject& obj) {
	ImGui::Text("Object: %s", obj.name.c_str());
	ImGui::Text("Type: %s", obj.type.c_str());
	ImGui::Text("Chunks: %s", obj.flags.hasChunks ? "enabled" : "disabled");
	ImGui::Separator();

	if (!obj.lodSettings && !obj.streamingSettings && !obj.terrainSettings) {
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "This object has no typed terrain blocks. Validator/template should provide them.");
	}
}

void PlanetTerrainEditorPanel::drawLodTab(Haruka::SceneObject& obj) {
	if (!obj.lodSettings) {
		drawTextDisabled("LOD", "missing lodSettings for this object");
		return;
	}

	auto& lod = *obj.lodSettings;
	drawStringField("Type", lod.type);
	drawIntField("Max Depth", lod.maxDepth, 1);
	drawDoubleField("Split Threshold", lod.splitThreshold, 0.05, 0.0, 0.0);
	drawDoubleVector("Thresholds", lod.thresholds);
	drawStringVector("Assets", lod.assets);
}

void PlanetTerrainEditorPanel::drawTerrainTab(Haruka::SceneObject& obj) {
	if (!obj.terrainSettings) {
		drawTextDisabled("Terrain", "missing terrainSettings for this object");
		return;
	}

	auto& terrain = *obj.terrainSettings;
	drawStringField("Type", terrain.type);
	drawStringField("Shader", terrain.shader);
	drawIntField("Seed", terrain.seed, 1);
	drawIntField("Chunk Size", terrain.chunkSize, 1);
	drawTerrainLayerMap(terrain.layers);
}

void PlanetTerrainEditorPanel::drawBiomesTab(Haruka::SceneObject& obj) {
	(void)obj;
	ImGui::TextDisabled("Biome authoring pipeline will live here.");
	ImGui::BulletText("Height and humidity masks");
	ImGui::BulletText("Material blend rules");
	ImGui::BulletText("Runtime spawn zones");
}

void PlanetTerrainEditorPanel::drawStreamingTab(Haruka::SceneObject& obj) {
	if (!obj.streamingSettings) {
		drawTextDisabled("Streaming", "missing streamingSettings for this object");
		return;
	}

	auto& streaming = *obj.streamingSettings;
	drawStringField("Mode", streaming.mode);
	drawStringField("Priority", streaming.priority);
	ImGui::Checkbox("Enabled", &streaming.enabled);
}

void PlanetTerrainEditorPanel::drawDebugTab(Haruka::SceneObject& obj) {
	ImGui::TextDisabled("SceneObject::properties remains the metadata escape hatch.");
	ImGui::Text("Custom properties: %s", obj.properties.is_null() ? "empty" : "set");
}

void PlanetTerrainEditorPanel::onImGuiRender() {
	if (!ImGui::Begin("Planet Terrain Editor")) {
		ImGui::End();
		return;
	}

	if (!currentScene) {
		ImGui::TextUnformatted("No scene loaded");
		ImGui::End();
		return;
	}

	refreshPlanetCandidates();

	if (planetNames.empty()) {
		ImGui::TextUnformatted("No planet-like objects found in scene.");
		ImGui::TextDisabled("Tip: object type 'Planet' or has terrainSettings/hasChunks.");
		ImGui::End();
		return;
	}

	std::vector<const char*> labels;
	labels.reserve(planetNames.size());
	for (const auto& n : planetNames) labels.push_back(n.c_str());
	ImGui::Combo("Target Planet", &selectedPlanetIndex, labels.data(), (int)labels.size());

	auto selected = getSelectedPlanet();
	if (!selected) {
		ImGui::TextUnformatted("Selected planet is no longer valid.");
		ImGui::End();
		return;
	}

	if (ImGui::BeginTabBar("##terrain_tabs")) {
		if (ImGui::BeginTabItem("Overview")) {
			drawOverviewTab(*selected);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("LOD")) {
			drawLodTab(*selected);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Terrain")) {
			drawTerrainTab(*selected);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Biomes")) {
			drawBiomesTab(*selected);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Streaming")) {
			drawStreamingTab(*selected);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Debug")) {
			drawDebugTab(*selected);
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::End();
}
