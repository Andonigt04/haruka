#pragma once

#include "chat_system.h"
#include "network/network_manager.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <atomic>

namespace Haruka {

class InGameChat {
public:
    InGameChat(const std::string& playerName, int localPort, int remotePort);
    ~InGameChat();
    
    void init();
    void shutdown();
    
    void update(float deltaTime);
    void render();
    
    void sendMessage(const std::string& message);

    void setNetworkClient(NetworkClient* client) { networkClient = client; useNetwork = (client != nullptr); }
    void setUseNetwork(bool enabled) { useNetwork = enabled; }

    bool isOpen() const { return chatOpen; }
    void toggleChat() { chatOpen = !chatOpen; }
    void openChat() { chatOpen = true; }
    void closeChat() { chatOpen = false; }

private:
    std::string playerName;
    int localPort;
    int remotePort;

    NetworkClient* networkClient = nullptr;
    bool useNetwork = false;
    
    int sockfd = -1;
    sockaddr_in remoteAddr;
    std::atomic<bool> running{false};
    std::thread receiveThread;
    
    bool chatOpen = false;
    char inputBuffer[256] = {0};
    
    std::vector<std::string> messageHistory;
    int maxMessages = 50;
    
    void receiveMessages();
    void addMessage(const std::string& message);
};

}