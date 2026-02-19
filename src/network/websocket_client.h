#pragma once

#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <thread>
#include <functional>

namespace Haruka {

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

typedef websocketpp::client<websocketpp::config::asio_client> WsClient;
typedef websocketpp::connection_hdl WsHandle;

class WebSocketClient {
public:
    WebSocketClient();
    ~WebSocketClient();
    
    bool connect(const std::string& uri);
    void disconnect();
    bool isConnected() const { return connected; }
    
    void send(const std::string& message);
    
    void setOpenCallback(std::function<void()> cb) { onOpen = cb; }
    void setMessageCallback(std::function<void(const std::string&)> cb) { onMessage = cb; }
    void setCloseCallback(std::function<void()> cb) { onClose = cb; }

private:
    WsClient client;
    WsHandle handle;
    bool connected = false;
    
    std::function<void()> onOpen;
    std::function<void(const std::string&)> onMessage;
    std::function<void()> onClose;
    
    void onOpenHandler(WsHandle hdl);
    void onMessageHandler(WsHandle hdl, WsClient::message_ptr msg);
    void onCloseHandler(WsHandle hdl);
};

}