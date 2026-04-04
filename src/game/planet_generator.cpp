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

            // ---- Capa 1: base negativa global ----
            float baseNegativeRatio = std::max(0.0f, config.baseNegativeDepthKm) / std::max(1.0f, config.baseRadiusKm);
            float baseDelta = -baseNegativeRatio;

            // ---- Capa 2: continentes y océanos ----
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
    cfg.baseNegativeDepthKm = 11.0f;
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
    PlanetData resultData;
    
    // Intentar generar con GPU si está habilitado
    if (config.useGPU) {
        if (tryGeneratePlanetGPU(config, resultData)) {
                std::cout << "[PlanetGenerator] GPU generation success: "
                          << resultData.vertices.size() << " verts, "
                          << (resultData.indices.size() / 3) << " tris" << std::endl;
            return resultData;  // GPU generación exitosa
        }
            std::cout << "[PlanetGenerator] GPU generation failed, falling back to CPU." << std::endl;
        // Si GPU falla, fallback a CPU (ver abajo)
    }
    
    // Path CPU (fallback o si GPU está deshabilitada)
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

PlanetGenerator::PlanetConfig PlanetGenerator::getPresetConfig(PlanetPreset preset) {
    PlanetConfig cfg;
    cfg.baseRadiusKm = 6371.0f;
    cfg.baseNegativeDepthKm = 11.0f;

    switch (preset) {
        case PlanetPreset::EARTH_LIKE:
            cfg.enableContinents = true;
            cfg.seaLevel = 0.52f;
            cfg.continentFrequency = 1.1f;
            cfg.continentWarpStrength = 0.14f;
            cfg.continentHeightStrength = 0.18f;
            cfg.macroFrequency = 3.2f;
            cfg.macroHeightStrength = 0.12f;
            cfg.detailFrequency = 12.0f;
            cfg.detailHeightStrength = 0.015f;
            break;
        case PlanetPreset::DESERT:
            cfg.enableContinents = true;
            cfg.seaLevel = 0.42f;
            cfg.continentFrequency = 1.4f;
            cfg.continentWarpStrength = 0.09f;
            cfg.continentHeightStrength = 0.14f;
            cfg.macroFrequency = 4.5f;
            cfg.macroHeightStrength = 0.08f;
            cfg.detailFrequency = 16.0f;
            cfg.detailHeightStrength = 0.02f;
            break;
        case PlanetPreset::ICE:
            cfg.enableContinents = true;
            cfg.seaLevel = 0.58f;
            cfg.continentFrequency = 0.95f;
            cfg.continentWarpStrength = 0.12f;
            cfg.continentHeightStrength = 0.10f;
            cfg.macroFrequency = 2.8f;
            cfg.macroHeightStrength = 0.10f;
            cfg.detailFrequency = 10.0f;
            cfg.detailHeightStrength = 0.012f;
            break;
    }

    return cfg;
}

bool PlanetGenerator::tryGeneratePlanetGPU(const PlanetConfig& config, PlanetData& outData) {
    // Requiere contexto GL válido
    if (!glGetString(GL_VERSION)) {
        std::cerr << "[PlanetGenerator][GPU] No GL context available." << std::endl;
        return false;
    }

    // Construir malla base cube-sphere (sin deformación) y luego deformar en GPU
    std::vector<glm::vec3> baseVertices;
    std::vector<unsigned int> indices;
    buildBaseCubeSphere(config.radius, config.subdivisions, baseVertices, indices);
    if (baseVertices.empty() || indices.empty()) {
        std::cerr << "[PlanetGenerator][GPU] Base cube-sphere generation failed." << std::endl;
        return false;
    }

    std::string shaderCode;
    const std::string shaderPathA = "shaders/planet_generation.comp";
    const std::string shaderPathB = "src/renderer/shaders/planet_generation.comp";
    if (!readTextFile(shaderPathA, shaderCode) && !readTextFile(shaderPathB, shaderCode)) {
        std::cerr << "[PlanetGenerator][GPU] Compute shader not found." << std::endl;
        return false;
    }

    GLuint program = compileComputeProgram(shaderCode);
    if (!program) {
        return false;
    }

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

    glUniform1i(glGetUniformLocation(program, "u_numVertices"), static_cast<int>(baseVertices.size()));
    glUniform1f(locSeaLevel, config.seaLevel);
    glUniform1f(glGetUniformLocation(program, "u_continentFrequency"), config.continentFrequency);
    glUniform1f(glGetUniformLocation(program, "u_continentWarpStrength"), config.continentWarpStrength);
    glUniform1f(glGetUniformLocation(program, "u_continentHeightStrength"), config.continentHeightStrength);
    glUniform1i(glGetUniformLocation(program, "u_octavesContinents"), config.octavesContinents);
    glUniform1f(glGetUniformLocation(program, "u_macroFrequency"), config.macroFrequency);
    glUniform1f(glGetUniformLocation(program, "u_macroHeightStrength"), config.macroHeightStrength);
    glUniform1i(glGetUniformLocation(program, "u_octavesMacro"), config.octavesMacro);
    glUniform1f(glGetUniformLocation(program, "u_detailFrequency"), config.detailFrequency);
    glUniform1f(glGetUniformLocation(program, "u_detailHeightStrength"), config.detailHeightStrength);
    glUniform1i(glGetUniformLocation(program, "u_octavesDetail"), config.octavesDetail);
    glUniform1f(glGetUniformLocation(program, "u_persistence"), config.persistence);
    glUniform1f(glGetUniformLocation(program, "u_lacunarity"), config.lacunarity);
    glUniform1i(glGetUniformLocation(program, "u_seedBase"), config.seedBase);
    glUniform1i(glGetUniformLocation(program, "u_seedContinents"), config.seedContinents);
    glUniform1i(glGetUniformLocation(program, "u_seedMacro"), config.seedMacro);
    glUniform1i(glGetUniformLocation(program, "u_seedDetail"), config.seedDetail);
    glUniform1f(glGetUniformLocation(program, "u_baseRadiusKm"), std::max(1.0f, config.baseRadiusKm));
    glUniform1f(glGetUniformLocation(program, "u_baseNegativeDepthKm"), std::max(0.0f, config.baseNegativeDepthKm));
    glUniform1i(glGetUniformLocation(program, "u_enableContinents"), config.enableContinents ? 1 : 0);
    glUniform1i(glGetUniformLocation(program, "u_enableMountains"), config.enableMountains ? 1 : 0);

    const GLuint localSize = 256;
    const GLuint groups = static_cast<GLuint>((baseVertices.size() + localSize - 1) / localSize);
    glDispatchCompute(groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboOut);
    if (out.size() > 0) {
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, out.size() * sizeof(glm::vec4), out.data());
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboNormals);
    if (outNormals.size() > 0) {
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, outNormals.size() * sizeof(glm::vec4), outNormals.data());
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glDeleteBuffers(1, &ssboIn);
    glDeleteBuffers(1, &ssboOut);
    glDeleteBuffers(1, &ssboNormals);
    glDeleteProgram(program);

    outData.vertices.resize(out.size());
    outData.normals.resize(outNormals.size());
    outData.indices = std::move(indices);
    outData.radius = config.radius;
    outData.minHeight = std::numeric_limits<float>::max();
    outData.maxHeight = std::numeric_limits<float>::lowest();

    if (out.size() != outNormals.size()) {
        std::cerr << "[PlanetGenerator][GPU] Size mismatch: vertices=" << out.size() << " normals=" << outNormals.size() << std::endl;
        glDeleteBuffers(1, &ssboIn);
        glDeleteBuffers(1, &ssboOut);
        glDeleteBuffers(1, &ssboNormals);
        glDeleteProgram(program);
        return false;
    }

    for (size_t i = 0; i < out.size(); ++i) {
        outData.vertices[i] = glm::vec3(out[i]);
        glm::vec3 n = glm::vec3(outNormals[i]);
        outData.normals[i] = glm::length(n) > 1e-6f ? glm::normalize(n) : glm::normalize(outData.vertices[i]);

        float h = glm::length(outData.vertices[i]);
        outData.minHeight = std::min(outData.minHeight, h);
        outData.maxHeight = std::max(outData.maxHeight, h);
    }

    if (outData.vertices.empty() || outData.indices.empty()) {
        return false;
    }
    if (outData.minHeight == std::numeric_limits<float>::max()) {
        outData.minHeight = config.radius;
        outData.maxHeight = config.radius;
    }

    return true;
}

}
