#pragma once

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>

/** @brief Description of one editable UI element. */
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

/**
 * @brief Visual UI layout builder panel.
 */
class UIBuilder {
public:
    /** @brief Constructs an empty UI builder. */
    UIBuilder() = default;
    
    /** @brief Draws the UI builder interface. */
    void onImGuiRender();
    
    /** @brief Adds a new UI element of the requested type. */
    UIElement* addElement(const std::string& type);
    /** @brief Removes an element by id. */
    void removeElement(const std::string& id);
    /** @brief Returns one element by id, if present. */
    UIElement* getElement(const std::string& id);
    
    /** @brief Saves the current UI layout to disk. */
    void saveUILayout(const std::string& path);
    /** @brief Loads a UI layout from disk. */
    void loadUILayout(const std::string& path);
    
    /** @brief Returns read-only access to the current element list. */
    const std::vector<std::shared_ptr<UIElement>>& getElements() const { return elements; }

private:
    std::vector<std::shared_ptr<UIElement>> elements;
    std::string selectedElementId;
    bool showProperties = true;
    bool showPreview = true;
};
