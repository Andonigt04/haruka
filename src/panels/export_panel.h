#pragma once

#include "tasks/editor_task.h"
#include <string>
#include <memory>
#include <vector>

class EditorApplication;

/**
 * @brief Game export configuration panel.
 */
class ExportPanel {
public:
    /** @brief Constructs hidden export panel state. */
    ExportPanel();
    /** @brief Releases export panel resources. */
    ~ExportPanel();
    
    /** @brief Advances long-running export tasks once per frame. */
    void update();
    /** @brief Renders the export UI. */
    void render(EditorApplication* editorApp);
    /** @brief Shows the panel. */
    void show() { visible = true; }
    /** @brief Hides the panel. */
    void hide() { visible = false; }
    /** @brief Returns visibility state. */
    bool isVisible() const { return visible; }

private:
    struct ExportBatchState;

    bool visible = false;
    
    // UI state
    char gameName[256] = {0};
    char version[32] = {0};
    char author[256] = {0};
    char description[512] = {0};

    char shadersPath[512] = {0}; // Ruta de shaders a exportar
    char exportPath[512] = {0};
    
    int buildTypeSelected = 1;  // Release por defecto
    int platformSelected = 0;   // Linux por defecto
    
    bool includeDebugSymbols = false;
    bool optimizeAssets = true;
    bool stripUnusedContent = true;
    bool compressAssets = true;
    
    std::string exportStatus;
    bool exportStatusError = false;

    /** @brief Long-running export task wrapper. */
    class ExportTask final : public EditorTaskBase {
    public:
        explicit ExportTask(ExportPanel* panel);

        void reportProgress(float value, const std::string& message) { setProgress(value, message); }
        void reportComplete(const std::string& message) { complete(message); }
        void reportFail(const std::string& message) { fail(message); }

    protected:
        bool onStart(std::string& error) override;
        void onUpdate() override;
        void onCancel() override;

    private:
        ExportPanel* owner = nullptr;
    };

    std::unique_ptr<ExportTask> activeTask;
    EditorApplication* pendingEditorApp = nullptr;
    std::unique_ptr<ExportBatchState> batchState;
    void startExportTask(EditorApplication* editorApp);
    void updateExportTask();
    void finishExportTask();
    
    /** @brief Loads project export settings into the UI. */
    void loadProjectSettings(EditorApplication* editorApp);
    /** @brief Saves UI settings back into the project. */
    void saveProjectSettings(EditorApplication* editorApp);
    /** @brief Validates the current export configuration. */
    bool validateSettings() const;
    /** @brief Performs the actual export workflow. */
    void performExport(EditorApplication* editorApp);

    bool prepareExport(EditorApplication* editorApp, std::string& error);
    void setExportError(const std::string& message);
};
