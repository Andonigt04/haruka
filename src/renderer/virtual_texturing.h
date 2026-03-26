#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>
#include <memory>
#include <vector>
#include <map>
#include <queue>
#include <unordered_map>
#include <string>

// Custom hash para glm::uvec2
namespace std {
    template<>
    struct hash<glm::uvec2> {
        size_t operator()(const glm::uvec2& v) const {
            return hash<unsigned int>()(v.x) ^ (hash<unsigned int>()(v.y) << 1);
        }
    };
}

/**
 * VirtualTexturing - Texturas ilimitadas con page-based streaming
 * 
 * Sistema de texturas virtual:
 * - Page-based virtual memory
 * - Feedback buffer para tracking de accesos
 * - Sparse texture (resident only loaded pages)
 * - Indirection texture (UV mapping)
 * 
 * Impacto: -80% VRAM, texturas ilimitadas
 */

struct PageRequest {
    glm::uvec2 page;      // Coordenada de página
    int mipLevel;         // Nivel mip
    float priority;       // Prioridad de carga
};

struct VTConfig {
    int pageSize = 128;
    int maxPageTableSize = 8192;
    int feedbackBufferSize = 512;
    int maxMipLevels = 8;
    int maxResidentPages = 1024;
    long long maxCacheMemory = 256LL * 1024 * 1024;
};

class VirtualTexturing {
public:
    VirtualTexturing();
    ~VirtualTexturing();

    /**
     * Inicializar virtual texturing
     */
    void init(const VTConfig& config = VTConfig());

    /**
     * Agregar texture virtual
     * @param textureId ID único
     * @param filePath Ruta al archivo
     * @param virtualSize Tamaño virtual en pixeles (ej: 8192x8192)
     */
    void addVirtualTexture(
        const std::string& textureId,
        const std::string& filePath,
        glm::uvec2 virtualSize
    );

    /**
     * Procesar feedback buffer
     * Llamar después de renderizar para actualizar páginas
     */
    void processFeedback();

    /**
     * Bindear virtual texture para rendering
     */
    void bindVirtualTexture(GLuint shaderProgram, const std::string& textureId);

    /**
     * Obtener indirection texture (UV → página)
     */
    GLuint getIndirectionTexture(const std::string& textureId) const;

    /**
     * Obtener physical texture (páginas cargadas)
     */
    GLuint getPhysicalTexture(const std::string& textureId) const;

    /**
     * Estadísticas
     */
    struct VTStats {
        int totalVirtualPages;
        int residentPages;
        int pageQueueSize;
        float cacheUtilization;
        int feedbackPixels;
        size_t totalMemory;
    };

    VTStats getStats(const std::string& textureId) const;

private:
    struct VirtualTextureData {
        std::string id;
        std::string filePath;
        glm::uvec2 virtualSize;
        glm::uvec2 pageTableSize;
        
        GLuint indirectionTexture = 0;  // Mapa de UV → página
        GLuint physicalTexture = 0;     // Texturas cargadas
        GLuint feedbackTexture = 0;     // Feedback buffer
        
        std::vector<GLuint> pageData;   // Datos de páginas
        std::queue<PageRequest> pageQueue;
        std::unordered_map<glm::uvec2, bool> residentPages;
        
        int residentPageCount = 0;
        size_t memoryUsage = 0;
    };

    VTConfig config;
    std::map<std::string, std::unique_ptr<VirtualTextureData>> virtualTextures;
    
    GLuint feedbackShader = 0;
    GLuint pageUploadShader = 0;
    
    size_t totalMemoryUsage = 0;
    std::queue<PageRequest> globalPageQueue;

    void createVirtualTextureGPUResources(VirtualTextureData& vt);
    void loadPhysicalPage(VirtualTextureData& vt, const PageRequest& request);
    void uploadPageToPhysical(VirtualTextureData& vt, const PageRequest& request);
    void updateIndirectionTexture(VirtualTextureData& vt);
    void makeRoomInCache(size_t neededBytes);
};
