#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <libpq-fe.h>

namespace Haruka {

enum class DatabaseType {
    WEB,           // Usuarios web, tokens, sesiones
    HEADMASTER,    // Persistencia jugadores, inventarios, mundo
    COMMS,         // Mensajes, chat, logs
    ANTICHEAT      // Logs de detecciones, reportes
};

struct UserData {
    std::string userId;
    std::string username;
    std::string email;
    std::string passwordHash;
    std::string token;
    bool verified;
    uint64_t createdAt;
};

struct PlayerPersistence {
    std::string userId;
    std::string characterId;
    double posX, posY, posZ;
    std::string inventory;
    int level;
    int experience;
    uint64_t lastSave;
};

struct MessageLog {
    std::string messageId;
    std::string senderId;
    std::string recipientId;
    std::string content;
    uint64_t timestamp;
    bool delivered;
};

struct AnticheatLog {
    std::string logId;
    std::string userId;
    std::string detectionType;
    std::string data;
    uint64_t timestamp;
};

class DatabaseConnection {
public:
    DatabaseConnection(const std::string& connInfo);
    ~DatabaseConnection();
    
    bool connect();
    void disconnect();
    bool isConnected() const { return conn != nullptr; }
    
    PGresult* executeQuery(const std::string& query);
    bool executeUpdate(const std::string& query);
    
    std::string escapeString(const std::string& str);

private:
    PGconn* conn = nullptr;
    std::string connectionInfo;
};

class DatabaseManager {
public:
    static DatabaseManager& getInstance() {
        static DatabaseManager instance;
        return instance;
    }
    
    void initDatabase(DatabaseType type, const std::string& host, 
                     int port, const std::string& dbName,
                     const std::string& user, const std::string& password);
    
    DatabaseConnection* getConnection(DatabaseType type);
    
    // WEB DB
    bool createUser(const UserData& user);
    UserData* getUserByToken(const std::string& token);
    bool verifyUser(const std::string& userId, const std::string& token);
    
    // HEADMASTER DB
    bool savePlayerData(const PlayerPersistence& data);
    PlayerPersistence* loadPlayerData(const std::string& userId);
    bool updatePlayerPosition(const std::string& userId, double x, double y, double z);
    
    // COMMS DB
    bool saveMessage(const MessageLog& msg);
    std::vector<MessageLog> getUndeliveredMessages(const std::string& recipientId);
    bool markMessageDelivered(const std::string& messageId);
    
    // ANTICHEAT DB
    bool logDetection(const AnticheatLog& log);
    std::vector<AnticheatLog> getUserLogs(const std::string& userId);

private:
    DatabaseManager();
    std::map<DatabaseType, std::unique_ptr<DatabaseConnection>> connections;
};

}