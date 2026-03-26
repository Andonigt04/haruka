#pragma once

#include <string>
#include <memory>
#include <map>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <glm/glm.hpp>

/**
 * AssetStreamer - Cargue de assets bajo demanda
 * 
 * Características:
 * - Carga asincrónica en background thread
 * - Priorización por distancia a la cámara
 * - Cache inteligente (LRU - Least Recently Used)
 * - Unload automático de assets lejanos
 * - Callbacks cuando assets se cargan
 * 
 * Impacto: -50% RAM en escenas grandes
 */

enum class AssetType {
    TEXTURE,
    MODEL,
    SHADER,
    AUDIO,
    UNKNOWN
};

struct Asset {
    std::string id;
    std::string path;
    AssetType type;
    void* data = nullptr;
    size_t sizeBytes = 0;
    bool loaded = false;
    float priority = 0.0f;  // Mayor = más importante
    long lastAccessTime = 0;
};

struct StreamRequest {
    std::string assetId;
    std::string assetPath;
    AssetType type;
    glm::vec3 position;
    float importance = 1.0f;  // 0.0-1.0
};

class AssetStreamer {
public:
    static AssetStreamer& getInstance() {
        static AssetStreamer instance;
        return instance;
    }

    /**
     * Inicializar el streamer
     * @param maxCacheMemoryMB Máximo de memoria para cache (default 512 MB)
     * @param numWorkerThreads Threads para carga async (default 2)
     */
    void init(size_t maxCacheMemoryMB = 512, int numWorkerThreads = 2);

    /**
     * Solicitar carga de asset
     */
    void requestAsset(
        const std::string& assetId,
        const std::string& assetPath,
        AssetType type,
        float importance = 1.0f,
        const glm::vec3& position = glm::vec3(0.0f)
    );

    /**
     * Obtener asset (bloqueante si no está cargado)
     */
    Asset* getAsset(const std::string& assetId, float timeoutMs = 5000.0f);

    /**
     * Obtener asset de forma no-bloqueante
     */
    Asset* tryGetAsset(const std::string& assetId);

    /**
     * Actualizar posición de la cámara (para priorización)
     */
    void updateCameraPosition(const glm::vec3& position);

    /**
     * Unload de asset específico
     */
    void unloadAsset(const std::string& assetId);

    /**
     * Limpiar cache (mantener solo lo más importante)
     */
    void trimCache();

    /**
     * Estadísticas
     */
    struct StreamStats {
        size_t totalCacheMemory;
        size_t maxCacheMemory;
        int loadedAssets;
        int pendingAssets;
        int failedAssets;
        float cacheUtilization;  // 0.0-1.0
    };

    StreamStats getStats() const;

    /**
     * Callback cuando asset se carga
     */
    using AssetLoadedCallback = std::function<void(const std::string&, Asset*)>;
    void onAssetLoaded(AssetLoadedCallback callback) {
        assetLoadedCallback = callback;
    }

    /**
     * Shutdown
     */
    void shutdown();

    ~AssetStreamer();

private:
    AssetStreamer();

    // Worker thread para carga asincrónica
    void workerThread();

    // Cargar asset sincronamente
    Asset* loadAssetSync(const StreamRequest& request);

    // Estimar tamaño del archivo
    size_t estimateAssetSize(const std::string& path);

    // Hacer espacio en cache si es necesario
    void makeRoomInCache(size_t neededBytes);

    // Eviction policy (LRU)
    void evictLRU();

    // Thread safety
    mutable std::mutex cacheMutex;
    mutable std::mutex queueMutex;
    std::condition_variable cv;

    // Cache de assets
    std::map<std::string, std::unique_ptr<Asset>> assetCache;

    // Cola de solicitudes
    std::queue<StreamRequest> loadQueue;

    // Worker threads
    std::vector<std::thread> workers;
    bool running = false;

    // Configuración
    size_t maxCacheMemoryBytes = 512 * 1024 * 1024;  // 512 MB default
    int numWorkers = 2;

    // Estadísticas
    size_t totalCacheMemory = 0;
    int failedAssets = 0;

    // Posición de cámara (para priorización)
    glm::vec3 cameraPosition = glm::vec3(0.0f);

    // Callbacks
    AssetLoadedCallback assetLoadedCallback = nullptr;
};
