#include <iostream>
#include <thread>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

class UDPChatClient {
public:
    UDPChatClient(int localPort, int remotePort, const std::string& name)
        : localPort(localPort), remotePort(remotePort), playerName(name) {
        
        // Crear socket UDP
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) {
            std::cerr << "Error creating socket" << std::endl;
            return;
        }
        
        // Bind local
        sockaddr_in localAddr;
        memset(&localAddr, 0, sizeof(localAddr));
        localAddr.sin_family = AF_INET;
        localAddr.sin_addr.s_addr = INADDR_ANY;
        localAddr.sin_port = htons(localPort);
        
        if (bind(sockfd, (sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
            std::cerr << "Error binding socket" << std::endl;
            close(sockfd);
            return;
        }
        
        // Remote address
        memset(&remoteAddr, 0, sizeof(remoteAddr));
        remoteAddr.sin_family = AF_INET;
        remoteAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        remoteAddr.sin_port = htons(remotePort);
        
        std::cout << "[" << playerName << "] Chat ready (local: " << localPort 
                  << ", remote: " << remotePort << ")" << std::endl;
    }
    
    ~UDPChatClient() {
        running = false;
        if (sockfd >= 0) close(sockfd);
    }
    
    void start() {
        running = true;
        
        // Thread para recibir mensajes
        std::thread receiveThread([this]() {
            char buffer[1024];
            sockaddr_in senderAddr;
            socklen_t senderLen = sizeof(senderAddr);
            
            while (running) {
                int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                               (sockaddr*)&senderAddr, &senderLen);
                
                if (n > 0) {
                    buffer[n] = '\0';
                    std::cout << "\n[RECEIVED] " << buffer << std::endl;
                    std::cout << "[" << playerName << "] > " << std::flush;
                }
            }
        });
        receiveThread.detach();
        
        // Thread para enviar mensajes
        std::cout << "[" << playerName << "] Type messages (or /quit to exit):" << std::endl;
        
        std::string input;
        while (running) {
            std::cout << "[" << playerName << "] > " << std::flush;
            std::getline(std::cin, input);
            
            if (input == "/quit") {
                running = false;
                break;
            }
            
            if (!input.empty()) {
                std::string message = playerName + ": " + input;
                sendto(sockfd, message.c_str(), message.length(), 0,
                      (sockaddr*)&remoteAddr, sizeof(remoteAddr));
            }
        }
    }

private:
    int sockfd = -1;
    int localPort;
    int remotePort;
    std::string playerName;
    sockaddr_in remoteAddr;
    bool running = false;
};

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cout << "Usage: ChatTest <name> <local_port> <remote_port>" << std::endl;
        std::cout << "Example:" << std::endl;
        std::cout << "  Terminal 1: ./ChatTest Player1 5000 5001" << std::endl;
        std::cout << "  Terminal 2: ./ChatTest Player2 5001 5000" << std::endl;
        return 1;
    }
    
    std::string name = argv[1];
    int localPort = std::stoi(argv[2]);
    int remotePort = std::stoi(argv[3]);
    
    std::cout << "=== Haruka Chat Test ===" << std::endl;
    
    UDPChatClient client(localPort, remotePort, name);
    client.start();
    
    return 0;
}