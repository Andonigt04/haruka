#pragma once

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>

struct UIElement {
    std::string id;
    std::string type; // button, text, image, panel, etc
    glm::vec2 position;
    glm::vec2 size;
    std::string text;
    std::string imagePath;
    glm::vec4 color = glm::vec4(1.0f);
    bool visible = true;
    int zOrder = 0;
};

class UIBuilder {
public:
    UIBuilder() = default;
    
    void onImGuiRender();
    
    UIElement* addElement(const std::string& type);
    void removeElement(const std::string& id);
    UIElement* getElement(const std::string& id);
    
    void saveUILayout(const std::string& path);
    void loadUILayout(const std::string& path);
    
    const std::vector<std::shared_ptr<UIElement>>& getElements() const { return elements; }

private:
    std::vector<std::shared_ptr<UIElement>> elements;
    std::string selectedElementId;
    bool showProperties = true;
    bool showPreview = true;
};
