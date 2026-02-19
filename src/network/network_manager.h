#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>

namespace Haruka {

enum class ServerType {
    HEADMASTER,      // Persistencia + coordinación central
    CLUSTER_MANAGER, // Organiza y distribuye zonas
    WEB_AUTH,        // Carga perfiles de usuarios web
    ANTICHEAT,       // Verificación de comportamiento
    COMMS,           // Comunicaciones cliente <-> headmaster
    ZONE             // Servidor de zona individual
};

struct PlayerData {
    std::string userId;
    std::string username;
    std::string email;
    glm::dvec3 position;
    glm::vec3 rotation;
    std::string currentZone;
    uint64_t lastUpdate;
    bool verified;
    bool authenticated;
};

struct ZoneInfo {
    std::string zoneId;
    std::string address;
    int port;
    int playerCount;
    int maxPlayers;
    float load; // 0.0 - 1.0
    bool active;
    glm::dvec3 worldPosition;
    double radius;
};

struct ServerInfo {
    std::string serverId;
    ServerType type;
    std::string address;
    int port;
    bool active;
};

struct NetworkMessage {
    std::string messageType;
    std::string senderId;
    std::string recipientId;
    std::string payload;
    uint64_t timestamp;
};

class NetworkClient {
public:
    NetworkClient();
    ~NetworkClient();
    
    bool connect(const std::string& serverAddr, int port);
    void disconnect();
    bool isConnected() const { return connected; }
    
    void sendPlayerData(const PlayerData& data);
    void sendPositionUpdate(glm::dvec3 pos, glm::vec3 rot);
    void handleMessage(const NetworkMessage& msg);
    void sendChatMessage(const std::string& senderName, const std::string& content);
    
    void setMessageCallback(std::function<void(const NetworkMessage&)> cb) {
        messageCallback = cb;
    }

private:
    bool connected = false;
    std::string serverId;
    std::function<void(const NetworkMessage&)> messageCallback;
};

class NetworkServer {
public:
    NetworkServer(ServerType type);
    ~NetworkServer();
    
    bool start(int port);
    void stop();
    bool isRunning() const { return running; }
    
    void registerServer(const ServerInfo& info);
    void broadcastMessage(const NetworkMessage& msg);
    void routeMessage(const NetworkMessage& msg);
    void handleChatMessage(const NetworkMessage& msg);
    void broadcastChat(const NetworkMessage& msg);
    
    // HEADMASTER: persistencia + coordinación
    void loadPlayerPersistence(const std::string& userId);
    void savePlayerPersistence(const PlayerData& data);
    void syncAllServers();
    
    // CLUSTER_MANAGER: gestión de zonas
    void registerZone(const ZoneInfo& zone);
    std::string assignPlayerToZone(const std::string& userId, glm::dvec3 position);
    void balanceZones();
    std::vector<ZoneInfo> getActiveZones();
    
    // WEB_AUTH: autenticación
    bool authenticateUser(const std::string& userId, const std::string& token);
    PlayerData loadUserProfile(const std::string& userId);
    
    // ANTICHEAT: verificación
    void checkPlayerBehavior(const std::string& userId, const PlayerData& data);
    bool validateMovement(glm::dvec3 oldPos, glm::dvec3 newPos, float deltaTime);
    
    // COMMS: relay
    void relayToHeadmaster(const NetworkMessage& msg);
    void relayToClient(const NetworkMessage& msg);
    
    // ZONE: servidor de zona
    void updateZonePlayers();
    void sendToCluster(const NetworkMessage& msg);

private:
    ServerType serverType;
    bool running = false;
    std::map<std::string, ServerInfo> connectedServers;
    std::map<std::string, PlayerData> players;
    std::map<std::string, ZoneInfo> zones;
    
    std::string headmasterAddress = "localhost";
    int headmasterPort = 8080;
    std::string clusterAddress = "localhost";
    int clusterPort = 8083;
};

class NetworkManager {
public:
    static NetworkManager& getInstance() {
        static NetworkManager instance;
        return instance;
    }
    
    NetworkClient* getClient() { return client.get(); }
    void initClient(const std::string& addr, int port);
    
    NetworkServer* getServer() { return server.get(); }
    void initServer(ServerType type, int port);
    
    void update(double deltaTime);

private:
    NetworkManager();
    std::unique_ptr<NetworkClient> client;
    std::unique_ptr<NetworkServer> server;
    double syncInterval = 0.1;
    double timeSinceSync = 0.0;
};

}