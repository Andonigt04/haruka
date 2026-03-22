#pragma once

#include <string>
#include <vector>
#include <functional>

struct ScriptFile {
    std::string name;
    std::string path;
    std::string content;
    bool modified = false;
    bool isOpen = false;
};

class ScriptingEditor {
public:
    ScriptingEditor() = default;
    
    void onImGuiRender();
    
    ScriptFile* openScript(const std::string& path);
    void closeScript(const std::string& path);
    void saveScript(const std::string& path);
    void compileScript(const std::string& path);
    
    const std::vector<ScriptFile>& getOpenScripts() const { return openScripts; }
    
    void setOnScriptCompiled(std::function<void(const std::string&, bool)> cb) {
        onScriptCompiled = std::move(cb);
    }

private:
    std::vector<ScriptFile> openScripts;
    int selectedScriptIndex = 0;
    std::string compileOutput;
    
    std::function<void(const std::string&, bool)> onScriptCompiled;
};
