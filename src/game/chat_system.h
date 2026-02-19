#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <deque>
#include <map>
#include <cstdint>

namespace Haruka {

enum class ChatChannel {
    GLOBAL,
    LOCAL,
    PARTY,
    GUILD,
    WHISPER,
    SYSTEM
};

struct ChatMessage {
    std::string messageId;
    std::string senderId;
    std::string senderName;
    std::string recipientId;
    std::string content;
    ChatChannel channel;
    uint64_t timestamp;
    bool isCommand;
};

class ChatSystem {
public:
    ChatSystem();
    ~ChatSystem();
    
    void sendMessage(const std::string& content, ChatChannel channel = ChatChannel::GLOBAL);
    void sendWhisper(const std::string& recipientId, const std::string& content);
    void sendCommand(const std::string& command);
    
    void receiveMessage(const ChatMessage& msg);
    
    std::vector<ChatMessage> getHistory(ChatChannel channel = ChatChannel::GLOBAL, int maxMessages = 50);
    std::vector<ChatMessage> getWhisperHistory(const std::string& userId, int maxMessages = 50);
    void clearHistory(ChatChannel channel);
    
    void muteUser(const std::string& userId);
    void unmuteUser(const std::string& userId);
    bool isUserMuted(const std::string& userId);
    
    void blockChannel(ChatChannel channel);
    void unblockChannel(ChatChannel channel);
    bool isChannelBlocked(ChatChannel channel);
    
    void registerCommand(const std::string& command, std::function<void(const std::vector<std::string>&)> callback);
    void executeCommand(const std::string& input);
    
    void setMessageCallback(std::function<void(const ChatMessage&)> cb) { messageCallback = cb; }
    void setSendCallback(std::function<void(const ChatMessage&)> cb) { sendCallback = cb; }

private:
    std::deque<ChatMessage> globalHistory;
    std::deque<ChatMessage> localHistory;
    std::deque<ChatMessage> partyHistory;
    std::deque<ChatMessage> guildHistory;
    std::deque<ChatMessage> systemHistory;
    std::map<std::string, std::deque<ChatMessage>> whisperHistory;
    
    std::vector<std::string> mutedUsers;
    std::vector<ChatChannel> blockedChannels;
    
    std::map<std::string, std::function<void(const std::vector<std::string>&)>> commands;
    
    std::function<void(const ChatMessage&)> messageCallback;
    std::function<void(const ChatMessage&)> sendCallback;
    
    int maxHistorySize = 100;
    
    void addToHistory(const ChatMessage& msg);
    ChatMessage createMessage(const std::string& content, ChatChannel channel);
    std::vector<std::string> parseCommand(const std::string& input);
};

}