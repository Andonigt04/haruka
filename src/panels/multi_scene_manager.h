#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "core/scene.h"

/**
 * @brief Scene collection manager for opening, switching, and unloading scenes.
 */
class SceneManager {
public:
    /** @brief Constructs an empty scene manager. */
    SceneManager() = default;
    /** @brief Releases managed scenes. */
    ~SceneManager() = default;
    
    /** @name Scene management */
    ///@{
    Haruka::Scene* createScene(const std::string& name);
    Haruka::Scene* getScene(const std::string& name);
    Haruka::Scene* getActiveScene() const { return activeScene; }
    void setActiveScene(const std::string& name);
    void unloadScene(const std::string& name);
    void unloadAllScenes();
    
    const std::vector<std::string>& getLoadedScenes() const { return loadedSceneNames; }
    size_t getSceneCount() const { return loadedScenes.size(); }
    ///@}
    
    /** @name Lifecycle callbacks */
    ///@{
    void setOnSceneLoaded(std::function<void(Haruka::Scene*)> cb) { onSceneLoaded = std::move(cb); }
    void setOnSceneUnloaded(std::function<void(const std::string&)> cb) { onSceneUnloaded = std::move(cb); }
    void setOnActiveSceneChanged(std::function<void(Haruka::Scene*)> cb) { onActiveSceneChanged = std::move(cb); }
    ///@}

private:
    std::unordered_map<std::string, std::shared_ptr<Haruka::Scene>> loadedScenes;
    std::vector<std::string> loadedSceneNames;
    Haruka::Scene* activeScene = nullptr;
    
    std::function<void(Haruka::Scene*)> onSceneLoaded;
    std::function<void(const std::string&)> onSceneUnloaded;
    std::function<void(Haruka::Scene*)> onActiveSceneChanged;
};
