#include "ui_builder.h"
#include <imgui.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>
#include <cstdlib>
#include <sstream>

using json = nlohmann::json;

static int elementCounter = 0;

UIElement* UIBuilder::addElement(const std::string& type) {
    auto element = std::make_shared<UIElement>();
    element->type = type;
    
    // Generate unique ID
    std::stringstream ss;
    ss << type << "_" << (++elementCounter);
    element->id = ss.str();
    
    // Set defaults based on type
    element->size = {100, 50};
    if (type == "text") element->text = "Label";
    else if (type == "button") element->text = "Button";
    else if (type == "panel") element->size = {300, 200};
    
    elements.push_back(element);
    std::cout << "✓ UI Element added: " << type << std::endl;
    return element.get();
}

void UIBuilder::removeElement(const std::string& id) {
    auto it = std::find_if(elements.begin(), elements.end(),
        [&id](const auto& e) { return e->id == id; });
    
    if (it != elements.end()) {
        elements.erase(it);
        std::cout << "✓ UI Element removed" << std::endl;
    }
}

UIElement* UIBuilder::getElement(const std::string& id) {
    auto it = std::find_if(elements.begin(), elements.end(),
        [&id](const auto& e) { return e->id == id; });
    
    if (it != elements.end()) {
        return it->get();
    }
    return nullptr;
}

void UIBuilder::saveUILayout(const std::string& path) {
    json layout;
    layout["elements"] = json::array();
    
    for (const auto& elem : elements) {
        json obj;
        obj["id"] = elem->id;
        obj["type"] = elem->type;
        obj["position"] = {elem->position.x, elem->position.y};
        obj["size"] = {elem->size.x, elem->size.y};
        obj["text"] = elem->text;
        obj["imagePath"] = elem->imagePath;
        obj["color"] = {elem->color.x, elem->color.y, elem->color.z, elem->color.w};
        obj["visible"] = elem->visible;
        obj["zOrder"] = elem->zOrder;
        layout["elements"].push_back(obj);
    }
    
    std::ofstream file(path);
    if (file.is_open()) {
        file << layout.dump(2);
        file.close();
        std::cout << "✓ UI layout saved: " << path << std::endl;
    }
}

void UIBuilder::loadUILayout(const std::string& path) {
    std::ifstream file(path);
    if (file.is_open()) {
        json layout;
        file >> layout;
        file.close();
        
        elements.clear();
        for (const auto& obj : layout["elements"]) {
            auto elem = std::make_shared<UIElement>();
            elem->id = obj["id"];
            elem->type = obj["type"];
            elem->position = {obj["position"][0], obj["position"][1]};
            elem->size = {obj["size"][0], obj["size"][1]};
            elem->text = obj.value("text", "");
            elem->imagePath = obj.value("imagePath", "");
            auto c = obj["color"];
            elem->color = {c[0], c[1], c[2], c[3]};
            elem->visible = obj["visible"];
            elem->zOrder = obj["zOrder"];
            elements.push_back(elem);
        }
        std::cout << "✓ UI layout loaded: " << path << std::endl;
    }
}

void UIBuilder::onImGuiRender() {
    ImGui::SetNextWindowSize(ImVec2(1000, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("UI Builder")) {
        ImGui::End();
        return;
    }
    
    // Left panel: Elements list
    if (ImGui::BeginChild("ElementsList", ImVec2(250, 0), true)) {
        ImGui::Text("Elements");
        ImGui::Separator();
        
        if (ImGui::Button("Add Button", ImVec2(-1, 0))) addElement("button");
        if (ImGui::Button("Add Text", ImVec2(-1, 0))) addElement("text");
        if (ImGui::Button("Add Image", ImVec2(-1, 0))) addElement("image");
        if (ImGui::Button("Add Panel", ImVec2(-1, 0))) addElement("panel");
        
        ImGui::Separator();
        
        for (const auto& elem : elements) {
            bool selected = (elem->id == selectedElementId);
            if (ImGui::Selectable(elem->type.c_str(), selected)) {
                selectedElementId = elem->id;
            }
        }
        ImGui::EndChild();
    }
    
    ImGui::SameLine();
    
    // Right panel: Canvas + Properties
    if (ImGui::BeginChild("Canvas", ImVec2(0, 0), true)) {
        ImGui::Text("Canvas Editor");
        ImGui::Separator();
        
        // Preview area
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImVec2 canvasSize(500, 400);
        
        drawList->AddRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), 
                         IM_COL32(255, 255, 255, 255));
        
        // Draw elements
        for (const auto& elem : elements) {
            if (!elem->visible) continue;
            ImVec2 pos(canvasPos.x + elem->position.x, canvasPos.y + elem->position.y);
            ImVec2 size(elem->size.x, elem->size.y);
            ImU32 color = ImGui::GetColorU32(ImVec4(elem->color.x, elem->color.y, elem->color.z, elem->color.w));
            
            drawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), color);
            drawList->AddText(pos, color, elem->text.c_str());
        }
        
        ImGui::Dummy(canvasSize);
        
        ImGui::Separator();
        
        // Properties panel
        auto selectedElem = getElement(selectedElementId);
        if (selectedElem) {
            ImGui::Text("Properties: %s", selectedElem->type.c_str());
            ImGui::InputFloat2("Position##ui", &selectedElem->position.x);
            ImGui::InputFloat2("Size##ui", &selectedElem->size.x);
            ImGui::InputText("Text##ui", selectedElem->text.data(), selectedElem->text.capacity());
            ImGui::ColorEdit4("Color##ui", &selectedElem->color.x);
            ImGui::SliderInt("Z Order##ui", &selectedElem->zOrder, 0, 100);
            ImGui::Checkbox("Visible##ui", &selectedElem->visible);
            
            if (ImGui::Button("Delete Element")) {
                removeElement(selectedElem->id);
                selectedElementId.clear();
            }
        }
        
        ImGui::Separator();
        
        if (ImGui::Button("Save Layout", ImVec2(120, 0))) {
            saveUILayout("ui_layout.json");
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Layout", ImVec2(120, 0))) {
            loadUILayout("ui_layout.json");
        }
        
        ImGui::EndChild();
    }
    
    ImGui::End();
}
