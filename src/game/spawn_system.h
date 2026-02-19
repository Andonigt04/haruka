#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace Haruka {

class Character;

enum class SpawnType {
    PLAYER_START,      // Inicio de jugador nuevo
    PLAYER_RESPAWN,    // Respawn tras muerte
    NPC,               // NPCs
    MONSTER,           // Enemigos
    BOSS,              // Jefes
    CHECKPOINT         // Puntos de control
};

struct SpawnPoint {
    std::string id;
    SpawnType type;
    glm::dvec3 position;
    glm::vec3 rotation;
    float radius;           // Radio de spawn aleatorio
    std::string zoneId;
    int maxEntities;        // Máximo de entidades en este spawn
    int currentEntities;
    bool active;
    uint64_t cooldown;      // ms entre spawns
    uint64_t lastSpawn;
};

struct RespawnData {
    std::string userId;
    std::string characterId;
    glm::dvec3 deathPosition;
    std::string zoneId;
    uint64_t deathTime;
    int respawnTimer;       // segundos
    bool canRespawn;
};

class SpawnSystem {
public:
    SpawnSystem();
    ~SpawnSystem();
    
    // Spawn points
    void registerSpawnPoint(const SpawnPoint& point);
    void removeSpawnPoint(const std::string& id);
    SpawnPoint* getSpawnPoint(const std::string& id);
    std::vector<SpawnPoint> getSpawnPointsByType(SpawnType type, const std::string& zoneId = "");
    
    // Player spawn
    glm::dvec3 getPlayerSpawnPosition(const std::string& userId, const std::string& zoneId);
    glm::dvec3 getRandomSpawnPosition(SpawnType type, const std::string& zoneId);
    
    // Respawn
    void registerPlayerDeath(const std::string& userId, const glm::dvec3& position, const std::string& zoneId);
    bool canPlayerRespawn(const std::string& userId);
    glm::dvec3 getRespawnPosition(const std::string& userId);
    void respawnPlayer(const std::string& userId);
    
    // Checkpoint system
    void setPlayerCheckpoint(const std::string& userId, const std::string& checkpointId);
    std::string getPlayerCheckpoint(const std::string& userId);
    
    // Update
    void update(float deltaTime);
    
    // Callbacks
    void setSpawnCallback(std::function<void(const std::string&, const glm::dvec3&)> cb) {
        spawnCallback = cb;
    }
    void setRespawnCallback(std::function<void(const std::string&)> cb) {
        respawnCallback = cb;
    }

private:
    std::map<std::string, SpawnPoint> spawnPoints;
    std::map<std::string, RespawnData> pendingRespawns;
    std::map<std::string, std::string> playerCheckpoints; // userId -> checkpointId
    
    std::function<void(const std::string&, const glm::dvec3&)> spawnCallback;
    std::function<void(const std::string&)> respawnCallback;
    
    int defaultRespawnTime = 5; // segundos
    
    glm::dvec3 getRandomPositionInRadius(const glm::dvec3& center, float radius);
    SpawnPoint* findBestSpawnPoint(SpawnType type, const std::string& zoneId);
};

}