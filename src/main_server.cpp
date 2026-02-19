#include <iostream>
#include "network/network_manager.h"
#include "database/database_manager.h"
#include <chrono>
#include <thread>

int main(int argc, char** argv) {
    std::cout << "=== Haruka Server ===" << std::endl;
    
    if (argc < 2) {
        std::cerr << "Usage: HarukaServer [headmaster|cluster|webauth|anticheat|comms|zone] [options]" << std::endl;
        return 1;
    }
    
    std::string serverType(argv[1]);
    
    // Inicializar DBs según tipo de servidor
    auto& dbManager = Haruka::DatabaseManager::getInstance();
    
    if (serverType == "headmaster") {
        dbManager.initDatabase(Haruka::DatabaseType::HEADMASTER,
            "localhost", 5432, "haruka_headmaster", "haruka_user", "password");
        std::cout << "[DB] Headmaster database connected" << std::endl;
    }
    else if (serverType == "webauth") {
        dbManager.initDatabase(Haruka::DatabaseType::WEB,
            "localhost", 5432, "haruka_web", "haruka_user", "password");
        std::cout << "[DB] Web database connected" << std::endl;
    }
    else if (serverType == "comms") {
        dbManager.initDatabase(Haruka::DatabaseType::COMMS,
            "localhost", 5432, "haruka_comms", "haruka_user", "password");
        std::cout << "[DB] Comms database connected" << std::endl;
    }
    else if (serverType == "anticheat") {
        dbManager.initDatabase(Haruka::DatabaseType::ANTICHEAT,
            "localhost", 5432, "haruka_anticheat", "haruka_user", "password");
        std::cout << "[DB] Anticheat database connected" << std::endl;
    }
    
    // Inicializar servidor
    auto& netManager = Haruka::NetworkManager::getInstance();
    
    if (serverType == "headmaster") {
        netManager.initServer(Haruka::ServerType::HEADMASTER, 8080);
        std::cout << "[HEADMASTER] Running on port 8080" << std::endl;
        std::cout << "  - Persistencia de jugadores" << std::endl;
        std::cout << "  - Coordinación central" << std::endl;
    }
    else if (serverType == "cluster") {
        netManager.initServer(Haruka::ServerType::CLUSTER_MANAGER, 8083);
        std::cout << "[CLUSTER] Running on port 8083" << std::endl;
        std::cout << "  - Gestión de zonas" << std::endl;
        std::cout << "  - Balanceo de carga" << std::endl;
    }
    else if (serverType == "webauth") {
        netManager.initServer(Haruka::ServerType::WEB_AUTH, 8084);
        std::cout << "[WEB_AUTH] Running on port 8084" << std::endl;
        std::cout << "  - Autenticación de usuarios" << std::endl;
        std::cout << "  - Carga de perfiles web" << std::endl;
    }
    else if (serverType == "anticheat") {
        netManager.initServer(Haruka::ServerType::ANTICHEAT, 8085);
        std::cout << "[ANTICHEAT] Running on port 8085" << std::endl;
        std::cout << "  - Verificación de comportamiento" << std::endl;
        std::cout << "  - Detección de anomalías" << std::endl;
    }
    else if (serverType == "comms") {
        netManager.initServer(Haruka::ServerType::COMMS, 8082);
        std::cout << "[COMMS] Running on port 8082" << std::endl;
        std::cout << "  - Relay cliente <-> headmaster" << std::endl;
    }
    else if (serverType == "zone") {
        int zonePort = (argc > 2) ? std::stoi(argv[2]) : 8090;
        netManager.initServer(Haruka::ServerType::ZONE, zonePort);
        std::cout << "[ZONE] Running on port " << zonePort << std::endl;
        std::cout << "  - Servidor de zona individual" << std::endl;
    }
    else {
        std::cerr << "Invalid server type: " << serverType << std::endl;
        std::cerr << "Valid types: headmaster, cluster, webauth, anticheat, comms, zone" << std::endl;
        return 1;
    }
    
    // Loop del servidor
    while (true) {
        netManager.update(0.016);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    return 0;
}