#pragma once

#include <imgui.h>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <mutex>

enum class LogLevel {
    Info,
    Warning,
    Error
};

struct LogEntry {
    LogLevel level;
    std::string message;
    std::string timestamp;
};

class ConsolePanel {
public:
    ConsolePanel();
    ~ConsolePanel();
    
    void onImGuiRender();
    void clear();
    
    void addLog(LogLevel level, const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);

private:
    std::vector<LogEntry> logs;
    mutable std::mutex logsMutex;
    bool autoScroll = true;
    bool showInfo = true;
    bool showWarnings = true;
    bool showErrors = true;
    char filterBuffer[256] = "";
    
    ImVec4 getColorForLevel(LogLevel level);
    std::string getCurrentTimestamp();
};

class StreamCapture {
public:
    StreamCapture(std::ostream& stream, ConsolePanel* console, LogLevel level);
    ~StreamCapture();

private:
    class CaptureBuffer : public std::streambuf {
    public:
        CaptureBuffer(ConsolePanel* console, LogLevel level, std::streambuf* original);
        
    protected:
        int overflow(int c) override;
        std::streamsize xsputn(const char* s, std::streamsize n) override;
        
    private:
        ConsolePanel* console;
        LogLevel level;
        std::streambuf* originalBuf;
        std::string buffer;
        std::mutex bufferMutex;
    };
    
    std::ostream& stream;
    std::streambuf* originalBuf;
    CaptureBuffer captureBuffer;
};