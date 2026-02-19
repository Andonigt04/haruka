#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <map>
#include <functional>

namespace Haruka {

using boost::asio::ip::tcp;

struct SocketSession {
    tcp::socket socket;
    std::string peerId;
    
    SocketSession(boost::asio::io_context& io_context)
        : socket(io_context) {}
};

class SocketServer {
public:
    SocketServer(boost::asio::io_context& io_context, int port);
    ~SocketServer();
    
    void start();
    void stop();
    bool isRunning() const { return running; }
    
    void broadcastMessage(const std::string& message);
    void sendMessage(const std::string& peerId, const std::string& message);
    
    void setMessageCallback(std::function<void(const std::string&, const std::string&)> cb) {
        messageCallback = cb;
    }

private:
    boost::asio::io_context& io_context;
    std::unique_ptr<tcp::acceptor> acceptor;
    bool running = false;
    std::map<std::string, std::shared_ptr<SocketSession>> sessions;
    std::function<void(const std::string&, const std::string&)> messageCallback;
    
    void asyncAccept();
    void handleAccept(std::shared_ptr<SocketSession> session,
                     const boost::system::error_code& error);
};

}