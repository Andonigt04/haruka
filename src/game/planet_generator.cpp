#include "planet_generator.h"
#include "noise_generator.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

namespace Haruka {

namespace {
float saturate(float v) {
    return std::clamp(v, 0.0f, 1.0f);
}

float smoothstep(float edge0, float edge1, float x) {
    float t = saturate((x - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

bool readTextFile(const std::string& path, std::string& outText) {
    std::ifstream in(path);
    if (!in.is_open()) return false;
    outText.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return true;
}

GLuint compileComputeProgram(const std::string& source) {
    const char* src = source.c_str();
    GLuint cs = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(cs, 1, &src, nullptr);
    glCompileShader(cs);

    GLint ok = GL_FALSE;
    glGetShaderiv(cs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLchar log[2048] = {};
        glGetShaderInfoLog(cs, sizeof(log), nullptr, log);
        std::cerr << "[PlanetGenerator][GPU] Compute compile error: " << log << std::endl;
        glDeleteShader(cs);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, cs);
    glLinkProgram(prog);
    glDeleteShader(cs);

    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLchar log[2048] = {};
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::cerr << "[PlanetGenerator][GPU] Program link error: " << log << std::endl;
        glDeleteProgram(prog);
        return 0;
    }

    return prog;
}

void buildBaseCubeSphere(
    float radius,
    int subdivisions,
    std::vector<glm::vec3>& vertices,
    std::vector<unsigned int>& indices
) {
    vertices.clear();
    indices.clear();

    const int gridSize = 1 << subdivisions;
    const float step = 2.0f / static_cast<float>(gridSize);

    struct FaceBasis {
        glm::vec3 n;
        glm::vec3 r;
        glm::vec3 u;
    };

    const FaceBasis faces[6] = {
        {glm::vec3( 1, 0, 0), glm::vec3( 0, 0,-1), glm::vec3(0,1,0)},
        {glm::vec3(-1, 0, 0), glm::vec3( 0, 0, 1), glm::vec3(0,1,0)},
        {glm::vec3( 0, 1, 0), glm::vec3( 1, 0, 0), glm::vec3(0,0,-1)},
        {glm::vec3( 0,-1, 0), glm::vec3( 1, 0, 0), glm::vec3(0,0, 1)},
        {glm::vec3( 0, 0, 1), glm::vec3( 1, 0, 0), glm::vec3(0,1,0)},
        {glm::vec3( 0, 0,-1), glm::vec3(-1, 0, 0), glm::vec3(0,1,0)}
    };

    for (int f = 0; f < 6; ++f) {
        const int vertexOffset = static_cast<int>(vertices.size());
        const auto& fb = faces[f];

        for (int i = 0; i <= gridSize; ++i) {
            for (int j = 0; j <= gridSize; ++j) {
                const float x = -1.0f + i * step;
                const float y = -1.0f + j * step;
                glm::vec3 cube = fb.n + fb.r * x + fb.u * y;
                vertices.push_back(glm::normalize(cube) * radius);
            }
        }

        const int stride = gridSize + 1;
        for (int i = 0; i < gridSize; ++i) {
            for (int j = 0; j < gridSize; ++j) {
                const int a = vertexOffset + i * stride + j;
                const int b = a + 1;
                const int c = a + stride;
                const int d = c + 1;
                indices.push_back(a); indices.push_back(c); indices.push_back(b);
                indices.push_back(b); indices.push_back(c); indices.push_back(d);
            }
        }
    }
}
}

void PlanetGenerator::generateFace(
    PlanetData& data,
    glm::vec3 faceNormal,
    glm::vec3 right,
    glm::vec3 up,
    const PlanetConfig& config,
    int faceSeedOffset
) {
    int gridSize = 1 << config.subdivisions;
    float step = 2.0f / gridSize;
    int vertexOffset = data.vertices.size();
    
    // Generar vértices para esta cara
    for (int i = 0; i <= gridSize; ++i) {
        for (int j = 0; j <= gridSize; ++j) {
            float u = -1.0f + i * step;
            float v = -1.0f + j * step;
            
            // Posición en el cubo
            glm::vec3 cubePos = faceNormal + right * u + up * v;
            glm::vec3 normalized = glm::normalize(cubePos);

            // Deformación inicial en cero (solo ruido procedural)
            float baseDelta = 0.0f;

            // ---- Continentes y océanos ----
            float continentHeight = 0.0f;
            float continentMask = 0.0f;
            float coastMask = 1.0f;
            if (config.enableContinents) {
                float wx = NoiseGenerator::fBm(normalized,
                                               config.seedBase + faceSeedOffset + 11,
                                               3,
                                               config.persistence,
                                               config.lacunarity,
                                               config.continentFrequency * 0.7f);
                float wy = NoiseGenerator::fBm(normalized,
                                               config.seedBase + faceSeedOffset + 17,
                                               3,
                                               config.persistence,
                                               config.lacunarity,
                                               config.continentFrequency * 0.7f);
                float wz = NoiseGenerator::fBm(normalized,
                                               config.seedBase + faceSeedOffset + 23,
                                               3,
                                               config.persistence,
                                               config.lacunarity,
                                               config.continentFrequency * 0.7f);

                glm::vec3 warp = glm::vec3(wx, wy, wz) * config.continentWarpStrength;
                glm::vec3 cpos = glm::normalize(normalized + warp);

                float c = NoiseGenerator::fBm(cpos,
                                              config.seedContinents + faceSeedOffset,
                                              config.octavesContinents,
                                              config.persistence,
                                              config.lacunarity,
                                              config.continentFrequency);
                float c01 = (c + 1.0f) * 0.5f;

                // Costa ancha para transiciones suaves tierra/mar
                float coastWidth = 0.22f;
                continentMask = smoothstep(config.seaLevel - coastWidth, config.seaLevel + coastWidth, c01);

                float signedContinent = c01 - config.seaLevel;

                // Máscara de costa: cerca de costa reducimos amplitudes de macro/micro
                coastMask = smoothstep(0.03f, 0.18f, std::abs(signedContinent));

                // Tierra positiva y océano ligeramente hundido respecto a la base negativa.
                // Continentes deben ser suaves: amplitud mucho más pequeña que las montañas.
                float landPart = std::max(0.0f, signedContinent);
                float oceanPart = std::min(0.0f, signedContinent) * 0.12f;
                continentHeight = (landPart + oceanPart) * config.continentHeightStrength;
                continentMask = smoothstep(-0.10f, 0.10f, signedContinent);
            }

            // ---- Capa 3: montañas principales ----
            float macroHeight = 0.0f;
            float detailHeight = 0.0f;
            if (config.enableMountains) {
                float macro = NoiseGenerator::fBm(normalized,
                                                  config.seedMacro + faceSeedOffset,
                                                  config.octavesMacro,
                                                  config.persistence,
                                                  config.lacunarity,
                                                  config.macroFrequency);
                float macro01 = (macro + 1.0f) * 0.5f;
                float mountainMask = smoothstep(0.48f, 0.72f, macro01);
                float mountainRidge = 1.0f - std::abs(2.0f * macro01 - 1.0f);
                // Montañas fuertes solo sobre tierra; en zonas de costa, su influencia baja.
                float landInfluence = config.enableContinents ? (0.20f + 0.80f * continentMask) : 1.0f;
                macroHeight = mountainMask * mountainRidge * config.macroHeightStrength * landInfluence * coastMask;

                // ---- Capa 3: detalle fino de montañas ----
                float detail = NoiseGenerator::fBm(normalized,
                                                   config.seedDetail + faceSeedOffset,
                                                   config.octavesDetail,
                                                   config.persistence,
                                                   config.lacunarity,
                                                   config.detailFrequency);
                float detailSigned = detail * 0.5f + 0.5f;
                float detailMask = smoothstep(0.35f, 0.80f, macro01);
                detailHeight = detailMask * detailSigned * config.detailHeightStrength * landInfluence * coastMask;
            }

            float totalDelta = baseDelta + continentHeight + macroHeight + detailHeight;

            // La esfera base puede descender globalmente (nivel negativo para planetas genéricos)
            float height = 1.0f + totalDelta;
            height = std::max(0.2f, height);

            glm::vec3 vertex = normalized * config.radius * height;
            glm::vec3 normal = glm::normalize(vertex);
            
            data.vertices.push_back(vertex);
            data.normals.push_back(normal);
            
            // Actualizar min/max altura
            float vertexHeight = glm::length(vertex);
            data.minHeight = std::min(data.minHeight, vertexHeight);
            data.maxHeight = std::max(data.maxHeight, vertexHeight);
        }
    }
    
    // Generar índices para esta cara
    int stride = gridSize + 1;
    for (int i = 0; i < gridSize; ++i) {
        for (int j = 0; j < gridSize; ++j) {
            int a = vertexOffset + i * stride + j;
            int b = a + 1;
            int c = a + stride;
            int d = c + 1;
            
            // Triángulo 1
            data.indices.push_back(a);
            data.indices.push_back(c);
            data.indices.push_back(b);
            
            // Triángulo 2
            data.indices.push_back(b);
            data.indices.push_back(c);
            data.indices.push_back(d);
        }
    }
}

PlanetGenerator::PlanetData PlanetGenerator::generatePlanet(
    float radius,
    int subdivisions,
    int seed,
    float noiseScale,
    float heightScale,
    int octaves
) {
    PlanetConfig cfg;
    cfg.radius = radius;
    cfg.subdivisions = subdivisions;
    cfg.baseRadiusKm = 6371.0f;
    cfg.seedBase = seed;
    cfg.seedContinents = seed + 100;
    cfg.seedMacro = seed + 200;
    cfg.seedDetail = seed + 300;
    cfg.enableContinents = false;
    cfg.detailFrequency = std::max(1.0f, noiseScale);
    cfg.detailHeightStrength = heightScale;
    cfg.octavesDetail = octaves;
    return generatePlanet(cfg);
}

PlanetGenerator::PlanetData PlanetGenerator::generatePlanet(const PlanetConfig& config) {
    // =============================================================================
    // PLANET GENERATION DISPATCHER: CPU vs GPU con threshold de tamaño
    // =============================================================================
    // 
    // DECISIÓN GPU vs CPU basada en estimación de vértices:
    // - Vértices >= 100k: GPU probablemente más rápido (paralelismo > overhead)
    // - Vértices < 100k: CPU más rápido (overhead GPU no justificado)
    // - Fallback: siempre CPU si GPU falla
    // 
    // USO TÍPICO:
    // 1. Planetas completos (100k-1M verts): GPU
    // 2. Chunks individuales (1k-10k verts): CPU directo
    // 3. Testing/editor: CPU fallback
    // =============================================================================

    PlanetData resultData;
    
    // Estimar número de vértices para decisión
    const int gridSize = 1 << config.subdivisions;
    const size_t estimatedVerts = static_cast<size_t>(gridSize * gridSize * 6); // 6 caras
    const size_t GPU_THRESHOLD = 100000; // Threshold óptimo: ~100k vértices
    
    // Intentar GPU solo si hay suficientes vértices para justificar overhead
    if (config.useGPU && estimatedVerts >= GPU_THRESHOLD) {
        if (tryGeneratePlanetGPU(config, resultData)) {
            std::cout << "[PlanetGenerator] ✓ GPU generation success: "
                      << resultData.vertices.size() << " verts, "
                      << (resultData.indices.size() / 3) << " tris" << std::endl;
            return resultData;
        }
        std::cout << "[PlanetGenerator] ⚠ GPU generation failed, falling back to CPU." << std::endl;
    } else if (config.useGPU && estimatedVerts < GPU_THRESHOLD) {
        std::cout << "[PlanetGenerator] ℹ Skipping GPU (verts=" << estimatedVerts 
                  << " < threshold=" << GPU_THRESHOLD << "), using CPU." << std::endl;
    }
    
    // Path CPU (fallback, chunks, o planetas pequeños)
    PlanetData planet;
    planet.radius = config.radius;
    planet.minHeight = std::numeric_limits<float>::max();
    planet.maxHeight = std::numeric_limits<float>::lowest();
    
    // Generar 6 caras del cubo (proyectado a esfera)
    // Cara +X
    generateFace(planet, glm::vec3(1, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0),
                 config, 0);
    
    // Cara -X
    generateFace(planet, glm::vec3(-1, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0),
                 config, 1);
    
    // Cara +Y
    generateFace(planet, glm::vec3(0, 1, 0), glm::vec3(1, 0, 0), glm::vec3(0, 0, -1),
                 config, 2);
    
    // Cara -Y
    generateFace(planet, glm::vec3(0, -1, 0), glm::vec3(1, 0, 0), glm::vec3(0, 0, 1),
                 config, 3);
    
    // Cara +Z
    generateFace(planet, glm::vec3(0, 0, 1), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0),
                 config, 4);
    
    // Cara -Z
    generateFace(planet, glm::vec3(0, 0, -1), glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0),
                 config, 5);

    if (planet.minHeight == std::numeric_limits<float>::max()) {
        planet.minHeight = config.radius;
        planet.maxHeight = config.radius;
    }
    
    return planet;
}

PlanetGenerator::ChunkData PlanetGenerator::generateChunk(const PlanetConfig& config, const ChunkConfig& chunk) {
    // =============================================================================
    // CHUNK GENERATION: CPU Direct (Optimized)
    // =============================================================================
    // Los chunks son siempre pequeños (~256-4096 vértices), así que usar CPU
    // es más eficiente que overhead de GPU (contexto, buffers, dispatch).
    //
    // ESTRATEGIA:
    // - Chunks pequeños: CPU directo (mejor que GPU overhead)
    // - Para tú optimize generación en masa, ver: planetarySystem.cpp update loop
    // - Cada chunk genera 1-2 ms en CPU, amortizado en múltiples threads
    // =============================================================================
    
    ChunkData out;
    out.minHeight = std::numeric_limits<float>::max();
    out.maxHeight = std::numeric_limits<float>::lowest();

    const int face = std::clamp(chunk.face, 0, 5);
    // Asegurar detalle basado en LOD:
    // LOD 0 (cercano): 1024x1024 = 1M vértices
    // LOD 1: 512x512 = 256k vértices  
    // LOD 2: 256x256 = 64k vértices
    // LOD 3: 128x128 = 16k vértices
    // LOD 4+: 64x64 = 4k vértices
    const int baseChunkSubdiv = std::max(6, 10 - std::max(0, chunk.lod));  // Rango 6-10
    const int lodSubdiv = std::max(baseChunkSubdiv, config.subdivisions - std::max(0, chunk.lod));
    const int gridSize = 1 << lodSubdiv;
    const int tiles = std::max(1, chunk.tilesPerFace);

    const int tx = std::clamp(chunk.tileX, 0, tiles - 1);
    const int ty = std::clamp(chunk.tileY, 0, tiles - 1);

    // =========================================================================
    // DERIVAR SEMILLA ÚNICA POR CHUNK
    // =========================================================================
    // Estrategia: Usar seedBase del planeta como base principal
    // Mezclar suavemente con datos del chunk para variación local
    // Resultado: terreno consistente con variación por chunk
    const auto smoothHash = [](uint32_t x, uint32_t y, uint32_t z) -> uint32_t {
        uint32_t h = 2654435761U;
        h ^= (x >> 16) ^ x;       h *= 0x7feb352dU;
        h ^= (y >> 16) ^ y;       h *= 0x846ca68bU;
        h ^= (z >> 16) ^ z;       h *= 0x7feb352dU;
        h ^= h >> 16;
        return h;
    };
    
    // Semilla derivada del chunk: solo varía la posición, no la base del planeta
    const uint32_t chunkVariation = smoothHash(
        (static_cast<uint32_t>(face) << 16) | static_cast<uint32_t>(chunk.lod),
        (static_cast<uint32_t>(tx) << 16) | static_cast<uint32_t>(ty),
        0xdeadbeef
    );
    
    // Combinar: 90% seedBase + 10% variación del chunk
    // Así el terreno general es consistente, pero cada chunk tiene detalles únicos
    const uint32_t chunkSeed = config.seedBase ^ (chunkVariation >> 4);

    // Calcular bordes de forma segura y consistente
    // IMPORTANTE: Asegurar que los bordes de chunks adyacentes coincidan exactamente
    const int startX = (gridSize * tx) / tiles;
    const int startY = (gridSize * ty) / tiles;
    const int endX = (gridSize * (tx + 1)) / tiles;
    const int endY = (gridSize * (ty + 1)) / tiles;

    // Asegurar alineación perfecta entre chunks
    // Los bordes deben ser compartidos (no duplicados)
    const int cellsX = std::max(1, endX - startX);
    const int cellsY = std::max(1, endY - startY);
    const float step = 2.0f / static_cast<float>(gridSize);
    const float invStep = 1.0f / step;  // Para precisión

    struct FaceBasis { glm::vec3 n; glm::vec3 r; glm::vec3 u; };
    const FaceBasis faces[6] = {
        {glm::vec3( 1, 0, 0), glm::vec3( 0, 0,-1), glm::vec3(0,1,0)},
        {glm::vec3(-1, 0, 0), glm::vec3( 0, 0, 1), glm::vec3(0,1,0)},
        {glm::vec3( 0, 1, 0), glm::vec3( 1, 0, 0), glm::vec3(0,0,-1)},
        {glm::vec3( 0,-1, 0), glm::vec3( 1, 0, 0), glm::vec3(0,0, 1)},
        {glm::vec3( 0, 0, 1), glm::vec3( 1, 0, 0), glm::vec3(0,1,0)},
        {glm::vec3( 0, 0,-1), glm::vec3(-1, 0, 0), glm::vec3(0,1,0)}
    };
    const FaceBasis& fb = faces[face];

    out.vertices.reserve(static_cast<size_t>(cellsX + 1) * static_cast<size_t>(cellsY + 1));
    out.normals.reserve(static_cast<size_t>(cellsX + 1) * static_cast<size_t>(cellsY + 1));
    out.indices.reserve(static_cast<size_t>(cellsX) * static_cast<size_t>(cellsY) * 6ull);

    for (int ix = 0; ix <= cellsX; ++ix) {
        for (int iy = 0; iy <= cellsY; ++iy) {
            const int gx = startX + ix;
            const int gy = startY + iy;

            // Calcular posición U, V de manera precisa
            // Usar float para evitar errores de redondeo
            const float u = -1.0f + (static_cast<float>(gx) / static_cast<float>(gridSize)) * 2.0f;
            const float v = -1.0f + (static_cast<float>(gy) / static_cast<float>(gridSize)) * 2.0f;

            glm::vec3 cubePos = fb.n + fb.r * u + fb.u * v;
            glm::vec3 normalized = glm::normalize(cubePos);

            // IMPORTANTE: Usar semilla global basada en seedBase
            // El ruido de Perlin ya genera variación suave basada en 'normalized'
            // No usar hash por vértice - eso causa ruido muy alto
            // Solo variar por face para mantener consistencia
            const uint32_t faceHash = smoothHash(static_cast<uint32_t>(face), 0u, 0u);
            const uint32_t vertexSeed = config.seedBase ^ faceHash;

            // Deformación inicial en cero (solo ruido procedural)
            float baseDelta = 0.0f;

            float continentHeight = 0.0f;
            float continentMask = 0.0f;
            float coastMask = 1.0f;
            if (config.enableContinents) {
                float wx = NoiseGenerator::fBm(normalized,
                                               vertexSeed + 11,
                                               3,
                                               config.persistence,
                                               config.lacunarity,
                                               config.continentFrequency * 0.7f);
                float wy = NoiseGenerator::fBm(normalized,
                                               vertexSeed + 17,
                                               3,
                                               config.persistence,
                                               config.lacunarity,
                                               config.continentFrequency * 0.7f);
                float wz = NoiseGenerator::fBm(normalized,
                                               vertexSeed + 23,
                                               3,
                                               config.persistence,
                                               config.lacunarity,
                                               config.continentFrequency * 0.7f);

                glm::vec3 warp = glm::vec3(wx, wy, wz) * config.continentWarpStrength;
                glm::vec3 cpos = glm::normalize(normalized + warp);

                float c = NoiseGenerator::fBm(cpos,
                                              vertexSeed + 101,
                                              config.octavesContinents,
                                              config.persistence,
                                              config.lacunarity,
                                              config.continentFrequency);
                float c01 = (c + 1.0f) * 0.5f;
                float coastWidth = 0.22f;
                continentMask = smoothstep(config.seaLevel - coastWidth, config.seaLevel + coastWidth, c01);
                float signedContinent = c01 - config.seaLevel;
                coastMask = smoothstep(0.03f, 0.18f, std::abs(signedContinent));

                float landPart = std::max(0.0f, signedContinent);
                float oceanPart = std::min(0.0f, signedContinent) * 0.12f;
                continentHeight = (landPart + oceanPart) * config.continentHeightStrength;
                continentMask = smoothstep(-0.10f, 0.10f, signedContinent);
            }

            float macroHeight = 0.0f;
            float detailHeight = 0.0f;
            if (config.enableMountains) {
                float macro = NoiseGenerator::fBm(normalized,
                                                  vertexSeed + 201,
                                                  config.octavesMacro,
                                                  config.persistence,
                                                  config.lacunarity,
                                                  config.macroFrequency);
                float macro01 = (macro + 1.0f) * 0.5f;
                float mountainMask = smoothstep(0.48f, 0.72f, macro01);
                float mountainRidge = 1.0f - std::abs(2.0f * macro01 - 1.0f);
                float landInfluence = config.enableContinents ? (0.20f + 0.80f * continentMask) : 1.0f;
                macroHeight = mountainMask * mountainRidge * config.macroHeightStrength * landInfluence * coastMask;

                float detail = NoiseGenerator::fBm(normalized,
                                                   vertexSeed + 301,
                                                   config.octavesDetail,
                                                   config.persistence,
                                                   config.lacunarity,
                                                   config.detailFrequency);
                float detailSigned = detail * 0.5f + 0.5f;
                float detailMask = smoothstep(0.35f, 0.80f, macro01);
                detailHeight = detailMask * detailSigned * config.detailHeightStrength * landInfluence * coastMask;
            }

            float totalDelta = baseDelta + continentHeight + macroHeight + detailHeight;
            float height = std::max(0.2f, 1.0f + totalDelta);
            glm::vec3 vertex = normalized * config.radius * height;

            out.vertices.push_back(vertex);
            out.normals.push_back(glm::normalize(vertex));

            float vh = glm::length(vertex);
            out.minHeight = std::min(out.minHeight, vh);
            out.maxHeight = std::max(out.maxHeight, vh);
        }
    }

    // ==========================================================================
    // SUAVIZADO DE BORDES: Reducir detalle cerca de los bordes del chunk
    // ==========================================================================
    // Los bordes son compartidos con chunks vecinos. Para evitar discontinuidades:
    // - Aplicar fade-out suave en los 4 bordes
    // - Esto hace que la influencia del ruido fino (detail) disminuya cerca de bordes
    // - Los vecinos hacen lo mismo → transición suave
    //
    // El fade es suave: empieza a 0.2 del borde y llega a 0 en el borde exacto
    // ==========================================================================
    const float borderFadeWidth = 0.15f; // 15% del chunk es zona de transición
    
    for (int ix = 0; ix <= cellsX; ++ix) {
        for (int iy = 0; iy <= cellsY; ++iy) {
            size_t vertexIdx = static_cast<size_t>(ix * (cellsY + 1) + iy);
            
            // Factores de fade para cada borde (0 en borde, 1 en interior)
            float fadeX = std::min(
                static_cast<float>(ix) / (cellsX * borderFadeWidth),
                static_cast<float>(cellsX - ix) / (cellsX * borderFadeWidth)
            );
            float fadeY = std::min(
                static_cast<float>(iy) / (cellsY * borderFadeWidth),
                static_cast<float>(cellsY - iy) / (cellsY * borderFadeWidth)
            );
            
            float fadeFactor = saturate(std::min(fadeX, fadeY)) * 0.5f + 0.5f;
            
            // Reducir amplitud del terreno cerca de bordes (suaviza transiciones)
            // Pero preservar altura base
            glm::vec3& v = out.vertices[vertexIdx];
            glm::vec3 normalizedV = glm::normalize(v);
            float originalHeight = glm::length(v);
            float baseHeight = config.radius;
            float deformation = originalHeight - baseHeight;
            
            // Interpolar: en borde (fade=0.5) usar 50% deformación; en interior (fade=1) usar 100%
            float smoothedHeight = baseHeight + deformation * fadeFactor;
            v = normalizedV * smoothedHeight;
            out.normals[vertexIdx] = normalizedV;
        }
    }

    const int stride = cellsX + 1;
    for (int ix = 0; ix < cellsX; ++ix) {
        for (int iy = 0; iy < cellsY; ++iy) {
            unsigned int a = static_cast<unsigned int>(ix * stride + iy);
            unsigned int b = a + 1;
            unsigned int c = a + static_cast<unsigned int>(stride);
            unsigned int d = c + 1;

            out.indices.push_back(a); out.indices.push_back(c); out.indices.push_back(b);
            out.indices.push_back(b); out.indices.push_back(c); out.indices.push_back(d);
        }
    }


    // --- Neighbor-aware LOD stitching ---
    // Cierra las grietas entre chunks de diferente resolución.
    // =============================================================================
    // NEIGHBOR-AWARE LOD STITCHING
    // =============================================================================
    // Genera triángulos de stitching en los 4 bordes para conectar suavemente
    // con chunks vecinos de resolución diferente.
    //
    // LÓGICA:
    // - Si vecino es menos denso (neighborLod > chunk.lod):
    //   Genera triángulos que unan el borde actual (más denso) con el borde del
    //   vecino (menos denso), cerrando la grieta que resultaría de la diferencia
    //   de resolución.
    // - Si vecino es igual/más denso: no hacer nada (el vecino maneja su stitching)
    //
    // IMPLEMENTACIÓN:
    // - step = 2^(lodDelta): espaciado de vértices en el borde a coser
    // - Para cada par de vértices separados por 'step', generar triángulos
    //   que unan con los vértices intermedios del grid actual
    // - Los 4 bordes (N, E, S, W) tienen lógica similar pero con índices
    //   ajustados a sus coordenadas respectivas
    //
    // ORIENTACIÓN: Triángulos orientados CCW (counter-clockwise) visto desde afuera
    // para mantener consistencia con la malla principal.
    // =============================================================================
    
    auto addStitchEdge = [&](int edge, int neighborLod) {
        if (neighborLod <= chunk.lod) return; // Solo stitch si vecino es menos denso
        
        int lodDelta = neighborLod - chunk.lod;
        int step = 1 << lodDelta; // 2, 4, 8, etc. según diferencia de LOD
        
        if (edge == 0) {
            // ========== NORTE (y=0) ==========
            // Borde superior: fila de vértices en y=0
            // Conecta pares (v0, v1) separados por 'step' con intermedios
            for (int x = 0; x < cellsX; x += step) {
                if (x + step > cellsX) break; // Seguridad: no exceder límite
                unsigned int v0 = static_cast<unsigned int>(x * stride);           // izquierda
                unsigned int v1 = static_cast<unsigned int>((x + step) * stride);  // derecha
                
                // Triángulos que cierren el gap con vértices intermedios
                for (int dx = 1; dx < step && x + dx < cellsX; ++dx) {
                    unsigned int vm = static_cast<unsigned int>((x + dx) * stride);
                    out.indices.push_back(v0);
                    out.indices.push_back(vm);
                    out.indices.push_back(v1);
                }
            }
        } 
        else if (edge == 1) {
            // ========== ESTE (x=cellsX) ==========
            // Borde derecho: columna de vértices en x=cellsX
            for (int y = 0; y < cellsY; y += step) {
                if (y + step > cellsY) break;
                unsigned int v0 = static_cast<unsigned int>(cellsX * stride + y);           // abajo
                unsigned int v1 = static_cast<unsigned int>(cellsX * stride + (y + step)); // arriba
                
                for (int dy = 1; dy < step && y + dy < cellsY; ++dy) {
                    unsigned int vm = static_cast<unsigned int>(cellsX * stride + (y + dy));
                    out.indices.push_back(v0);
                    out.indices.push_back(v1);
                    out.indices.push_back(vm);
                }
            }
        }
        else if (edge == 2) {
            // ========== SUR (y=cellsY) ==========
            // Borde inferior: fila de vértices en y=cellsY
            for (int x = 0; x < cellsX; x += step) {
                if (x + step > cellsX) break;
                unsigned int v0 = static_cast<unsigned int>(x * stride + cellsY);           // izquierda
                unsigned int v1 = static_cast<unsigned int>((x + step) * stride + cellsY); // derecha
                
                for (int dx = 1; dx < step && x + dx < cellsX; ++dx) {
                    unsigned int vm = static_cast<unsigned int>((x + dx) * stride + cellsY);
                    out.indices.push_back(v0);
                    out.indices.push_back(v1);
                    out.indices.push_back(vm);
                }
            }
        }
        else if (edge == 3) {
            // ========== OESTE (x=0) ==========
            // Borde izquierdo: columna de vértices en x=0
            for (int y = 0; y < cellsY; y += step) {
                if (y + step > cellsY) break;
                unsigned int v0 = static_cast<unsigned int>(0 * stride + y);           // abajo
                unsigned int v1 = static_cast<unsigned int>(0 * stride + (y + step)); // arriba
                
                for (int dy = 1; dy < step && y + dy < cellsY; ++dy) {
                    unsigned int vm = static_cast<unsigned int>(0 * stride + (y + dy));
                    out.indices.push_back(v0);
                    out.indices.push_back(vm);
                    out.indices.push_back(v1);
                }
            }
        }
    };

    // Aplicar stitching en los 4 bordes según LOD de vecinos
    addStitchEdge(0, chunk.neighborLodN); // Norte
    addStitchEdge(1, chunk.neighborLodE); // Este
    addStitchEdge(2, chunk.neighborLodS); // Sur
    addStitchEdge(3, chunk.neighborLodW); // Oeste

    if (out.minHeight == std::numeric_limits<float>::max()) {
        out.minHeight = config.radius;
        out.maxHeight = config.radius;
    }

    return out;
}

bool PlanetGenerator::tryGeneratePlanetGPU(const PlanetConfig& config, PlanetData& outData) {
    // =============================================================================
    // GPU PLANET GENERATION via Compute Shader
    // =============================================================================
    // Requiere contexto OpenGL válido y shader compute disponible.
    // 
    // PIPELINE:
    // 1. Validar contexto GL y cargar shader compute
    // 2. Construir malla base cube-sphere sin deformación (rápido, CPU)
    // 3. Cargar vértices en SSBO (GPU memory)
    // 4. Despachär compute shader paralelo (cada thread procesa 1+ vértice)
    // 5. Recuperar vértices y normales deformados desde GPU
    // 
    // VENTAJAS:
    // - Paralelismo masivo: miles de threads procesando vértices simultáneamente
    // - Cálculos de ruido (fBm) en GPU muy rápido
    // - Ideal para planetas con 100k+ vértices
    // 
    // FALLBACK: Si falla cualquier paso, retorna false -> CPU path
    // =============================================================================
    
    // Validar contexto GL
    if (!glGetString(GL_VERSION)) {
        std::cerr << "[PlanetGenerator][GPU] No GL context available." << std::endl;
        return false;
    }

    // Paso 1: Construir malla base cube-sphere (sin deformación)
    std::vector<glm::vec3> baseVertices;
    std::vector<unsigned int> indices;
    buildBaseCubeSphere(config.radius, config.subdivisions, baseVertices, indices);
    if (baseVertices.empty() || indices.empty()) {
        std::cerr << "[PlanetGenerator][GPU] Base cube-sphere generation failed." << std::endl;
        return false;
    }

    // Paso 2: Cargar y compilar compute shader
    std::string shaderCode;
    const std::string shaderPathA = "shaders/planet_generation.comp";
    const std::string shaderPathB = "src/renderer/shaders/planet_generation.comp";
    if (!readTextFile(shaderPathA, shaderCode) && !readTextFile(shaderPathB, shaderCode)) {
        std::cerr << "[PlanetGenerator][GPU] Compute shader not found at " 
                  << shaderPathA << " or " << shaderPathB << std::endl;
        return false;
    }

    GLuint program = compileComputeProgram(shaderCode);
    if (!program) {
        std::cerr << "[PlanetGenerator][GPU] Compute shader compilation failed." << std::endl;
        return false;
    }

    // Paso 3: Preparar datos en SSBOs
    std::vector<glm::vec4> in(baseVertices.size());
    std::vector<glm::vec4> out(baseVertices.size(), glm::vec4(0.0f));
    std::vector<glm::vec4> outNormals(baseVertices.size(), glm::vec4(0.0f));
    for (size_t i = 0; i < baseVertices.size(); ++i) {
        in[i] = glm::vec4(baseVertices[i], 1.0f);
    }

    GLuint ssboIn = 0, ssboOut = 0, ssboNormals = 0;
    glGenBuffers(1, &ssboIn);
    glGenBuffers(1, &ssboOut);
    glGenBuffers(1, &ssboNormals);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboIn);
    glBufferData(GL_SHADER_STORAGE_BUFFER, in.size() * sizeof(glm::vec4), in.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboOut);
    glBufferData(GL_SHADER_STORAGE_BUFFER, out.size() * sizeof(glm::vec4), out.data(), GL_DYNAMIC_READ);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboNormals);
    glBufferData(GL_SHADER_STORAGE_BUFFER, outNormals.size() * sizeof(glm::vec4), outNormals.data(), GL_DYNAMIC_READ);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboIn);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssboOut);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssboNormals);

    // Paso 4: Configurar uniforms y validar shader
    glUseProgram(program);
    const GLint locSeaLevel = glGetUniformLocation(program, "u_seaLevel");
    const GLint locLegacyRelief = glGetUniformLocation(program, "u_reliefAmplitude");
    if (locSeaLevel < 0) {
        std::cerr << "[PlanetGenerator][GPU] Incompatible compute shader uniforms detected";
        if (locLegacyRelief >= 0) {
            std::cerr << " (legacy shader loaded)";
        }
        std::cerr << ". Falling back to CPU." << std::endl;
        glDeleteBuffers(1, &ssboIn);
        glDeleteBuffers(1, &ssboOut);
        glDeleteBuffers(1, &ssboNormals);
        glDeleteProgram(program);
        return false;
    }

    // Configurar uniforms
    glUniform1i(glGetUniformLocation(program, "u_numVertices"), static_cast<int>(baseVertices.size()));
    glUniform1f(locSeaLevel, config.seaLevel);
    glUniform1f(glGetUniformLocation(program, "u_continentFrequency"), config.continentFrequency);
    glUniform1f(glGetUniformLocation(program, "u_continentWarpStrength"), config.continentWarpStrength);
    glUniform1f(glGetUniformLocation(program, "u_continentHeightStrength"), config.continentHeightStrength);
    glUniform1i(glGetUniformLocation(program, "u_octavesContinents"), config.octavesContinents);
    glUniform1f(glGetUniformLocation(program, "u_macroFrequency"), config.macroFrequency);
    glUniform1f(glGetUniformLocation(program, "u_macroHeightStrength"), config.macroHeightStrength);
    glUniform1i(glGetUniformLocation(program, "u_octavesMacro"), config.octavesMacro);
    glUniform1f(glGetUniformLocation(program, "u_detailHeightStrength"), config.detailHeightStrength);
    glUniform1i(glGetUniformLocation(program, "u_octavesDetail"), config.octavesDetail);
    glUniform1f(glGetUniformLocation(program, "u_persistence"), config.persistence);
    glUniform1f(glGetUniformLocation(program, "u_lacunarity"), config.lacunarity);
    glUniform1i(glGetUniformLocation(program, "u_seedBase"), config.seedBase);
    glUniform1i(glGetUniformLocation(program, "u_seedContinents"), config.seedContinents);
    glUniform1i(glGetUniformLocation(program, "u_seedMacro"), config.seedMacro);
    glUniform1i(glGetUniformLocation(program, "u_seedDetail"), config.seedDetail);
    glUniform1f(glGetUniformLocation(program, "u_baseRadiusKm"), std::max(1.0f, config.baseRadiusKm));
    glUniform1i(glGetUniformLocation(program, "u_enableContinents"), config.enableContinents ? 1 : 0);
    glUniform1i(glGetUniformLocation(program, "u_enableMountains"), config.enableMountains ? 1 : 0);

    // Paso 5: Despachar compute shader
    // localSize: número de threads por work group (256 es estándar, eficiente)
    // groups: número de work groups necesarios para cubrir todos los vértices
    const GLuint localSize = 256;
    const GLuint groups = static_cast<GLuint>((baseVertices.size() + localSize - 1) / localSize);
    glDispatchCompute(groups, 1, 1);
    // Sincronizar: esperar a que todos los threads terminen y actualicen SSBOs
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    // Paso 6: Recuperar resultados desde GPU
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboOut);
    if (out.size() > 0) {
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, out.size() * sizeof(glm::vec4), out.data());
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboNormals);
    if (outNormals.size() > 0) {
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, outNormals.size() * sizeof(glm::vec4), outNormals.data());
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Limpiar recursos GPU
    glDeleteBuffers(1, &ssboIn);
    glDeleteBuffers(1, &ssboOut);
    glDeleteBuffers(1, &ssboNormals);
    glDeleteProgram(program);

    // Paso 7: Copiar datos a estructura de salida y calcular altura
    outData.vertices.resize(out.size());
    outData.normals.resize(outNormals.size());
    outData.indices = std::move(indices);
    outData.radius = config.radius;
    outData.minHeight = std::numeric_limits<float>::max();
    outData.maxHeight = std::numeric_limits<float>::lowest();

    if (out.size() != outNormals.size()) {
        std::cerr << "[PlanetGenerator][GPU] Size mismatch: vertices=" << out.size() 
                  << " normals=" << outNormals.size() << std::endl;
        return false;
    }

    // Convertir de vec4 a vec3 y normalizar
    for (size_t i = 0; i < out.size(); ++i) {
        outData.vertices[i] = glm::vec3(out[i]);
        glm::vec3 n = glm::vec3(outNormals[i]);
        outData.normals[i] = glm::length(n) > 1e-6f ? glm::normalize(n) : glm::normalize(outData.vertices[i]);

        float h = glm::length(outData.vertices[i]);
        outData.minHeight = std::min(outData.minHeight, h);
        outData.maxHeight = std::max(outData.maxHeight, h);
    }

    // Validación final
    if (outData.vertices.empty() || outData.indices.empty()) {
        std::cerr << "[PlanetGenerator][GPU] Output validation failed." << std::endl;
        return false;
    }
    if (outData.minHeight == std::numeric_limits<float>::max()) {
        outData.minHeight = config.radius;
        outData.maxHeight = config.radius;
    }

    return true;
}

}
