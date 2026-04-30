#include "multi_scene_manager.h"
#include <algorithm>
#include <iostream>

Haruka::Scene* SceneManager::createScene(const std::string& name) {
    if (loadedScenes.find(name) != loadedScenes.end()) {
        std::cerr << "Scene '" << name << "' already exists" << std::endl;
        return nullptr;
    }
    
    auto scene = std::make_shared<Haruka::Scene>(name);
    loadedScenes[name] = scene;
    loadedSceneNames.push_back(name);
    
    if (!activeScene) {
        activeScene = scene.get();
    }
    
    std::cout << "✓ Scene created: " << name << std::endl;
    if (onSceneLoaded) onSceneLoaded(scene.get());
    
    return scene.get();
}

Haruka::Scene* SceneManager::getScene(const std::string& name) {
    auto it = loadedScenes.find(name);
    if (it != loadedScenes.end()) {
        return it->second.get();
    }
    return nullptr;
}

void SceneManager::setActiveScene(const std::string& name) {
    auto scene = getScene(name);
    if (scene) {
        activeScene = scene;
        std::cout << "✓ Active scene: " << name << std::endl;
        if (onActiveSceneChanged) onActiveSceneChanged(scene);
    } else {
        std::cerr << "Scene '" << name << "' not found" << std::endl;
    }
}

void SceneManager::unloadScene(const std::string& name) {
    auto it = loadedScenes.find(name);
    if (it != loadedScenes.end()) {
        if (activeScene == it->second.get()) {
            activeScene = nullptr;
        }
        
        loadedScenes.erase(it);
        loadedSceneNames.erase(std::find(loadedSceneNames.begin(), loadedSceneNames.end(), name));
        
        std::cout << "✓ Scene unloaded: " << name << std::endl;
        if (onSceneUnloaded) onSceneUnloaded(name);
    }
}

void SceneManager::unloadAllScenes() {
    loadedScenes.clear();
    loadedSceneNames.clear();
    activeScene = nullptr;
    std::cout << "✓ All scenes unloaded" << std::endl;
}
